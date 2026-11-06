/* SPDX-License-Identifier: GPL-3.0-only
 * The tine of FiMba-1 (jadamsowers/fm1-fimba, firmware/src/dsp/kalimba.c, GPL-3.0): a kalimba by modal synthesis.
 * Modifications (chord-harp fork): one voice of Yehnah's engine API (FiMba plays its 16 tines itself), the voices from a
 * shared pool, its body (resonators, buzzers, the hole's wah), effects, layouts and patterns left out; the release
 * damping and the click on knobs. */
/* KALIMBA: a tine (or a bar) struck by a thumb. Each voice is a bank of KM_NMODE two-pole resonators, the bending modes
 * of a clamped-free bar (and a twin of the first a hair away: the beating of a tine that bends in two planes). A pluck
 * is a half-sine force pulse: a soft thumb a long one, which cannot excite the high modes; a nail a short one (and a
 * click), which can. MAT the material: its modes' ratios, levels and decays. A hard pluck swings wide and rises in
 * pitch a little until it settles (GLID, tension modulation); WORN gives each tine its own quirks (tuning, sustain, the
 * beat, its upper modes); DAMP a finger on it as the key is let go (0: it rings on).
 * EDIT 1  MAT HARD DECY TONE        EDIT 2  GLID WORN DAMP CLIK
 * In float (the core's FPU; fastmath.h). Its own envelope (ownenv): the ADSR is not used. A voice's resonators live in a
 * pool shared by the parts (km_pool, KM_POOL voices: as many as the shared budget sounds), v->s[0] its slot + 1. */
#include "fastmath.h"

#define KM_NMODE 8                     /* resonators per voice: the first mode, its twin, six more (MODAL's PIANO uses all) */
#define KM_TAB 5                       /* modes a material's table gives (the first, its twin, three more) */
#define KM_POOL 14u                    /* tines sounding at once, all parts (FULL_VOICES 12, and two fading) */
enum { MAT_STEEL, MAT_BRASS, MAT_BRONZE, MAT_ALU, MAT_BAMBOO, MAT_GLASS, MAT_N };
static const char *const N_KMAT[] = {"STEEL", "BRASS", "BRONZE", "ALUM", "BAMBOO", "GLASS"};
/* a material: its modes (ratio to the first, level), the first mode's T60 at C4 (shorter up the keyboard by
 * (f / C4)^-pitch_k), how much faster the upper modes die (loss), the twin's detuning and level, the nail click.
 * Ratios: a uniform clamped-free bar has its bending modes at 1, 6.27, 17.55, 34.39 (Euler-Bernoulli); a real tine is
 * thinner at the tip and bent up, which pulls them down a little, more so in soft brass. Bronze and glass are played
 * as bars free at both ends (1, 2.76, 5.40, 8.93), a bell's brighter spectrum. Bamboo is short-lived and woody */
typedef struct {
    float ratio[KM_TAB - 1], amp[KM_TAB - 1];
    float t60, loss, pitch_k, beat, beat_amp, click;
} km_material_t;
static const km_material_t KM_MATS[MAT_N] = {
    {{1.0028f, 5.90f, 16.2f, 31.5f}, {0.35f, 0.30f, 0.10f, 0.040f}, 5.0f, 0.10f, 0.55f, 0.0028f, 0.35f, 0.50f},
    {{1.0020f, 5.60f, 15.3f, 29.8f}, {0.30f, 0.22f, 0.06f, 0.020f}, 3.6f, 0.20f, 0.50f, 0.0020f, 0.30f, 0.30f},
    {{1.0015f, 2.76f, 5.40f, 8.93f}, {0.40f, 0.45f, 0.25f, 0.120f}, 4.5f, 0.06f, 0.60f, 0.0015f, 0.40f, 0.60f},
    {{1.0040f, 6.27f, 17.55f, 34.4f}, {0.30f, 0.50f, 0.22f, 0.100f}, 2.6f, 0.12f, 0.50f, 0.0040f, 0.30f, 0.70f},
    {{1.0000f, 4.60f, 11.8f, 22.3f}, {0.00f, 0.30f, 0.12f, 0.050f}, 0.55f, 0.90f, 0.35f, 0.0f, 0.0f, 0.25f},
    {{1.0010f, 3.01f, 6.24f, 10.4f}, {0.20f, 0.55f, 0.35f, 0.200f}, 2.0f, 0.01f, 0.70f, 0.0010f, 0.20f, 0.90f},
};

/* (chord-harp fork) MODAL's (eng_modal.c): the same resonators, an electric piano's tine and the mallet bars */
enum { MDL_RHODES, MDL_VIBES, MDL_MARIMBA, MDL_XYLO, MDL_GLOCK, MDL_CELESTA, MDL_PIANO, MDL_N };
static const char *const N_MDMODEL[] = {"RHODES", "VIBES", "MARIMBA", "XYLO", "GLOCK", "CELESTA", "PIANO"};
/* {twin, mode 2, 3, 4 ratios}, {their levels}, t60 at C4, loss, pitch_k, beat, beat level, click (km_material_t) */
static const km_material_t MDL_MATS[MDL_N] = {
    {{1.0008f, 7.11f, 19.9f, 3.00f}, {0.25f, 0.20f, 0.04f, 0.02f}, 4.5f, 0.70f, 0.60f, 0.0008f, 0.25f, 0.15f},
    {{1.0005f, 4.00f, 10.0f, 20.2f}, {0.08f, 0.22f, 0.07f, 0.015f}, 6.0f, 0.22f, 0.50f, 0.0005f, 0.08f, 0.30f},
    {{1.0000f, 3.92f, 9.20f, 16.0f}, {0.00f, 0.16f, 0.04f, 0.010f}, 1.3f, 1.60f, 0.70f, 0.0f, 0.0f, 0.45f},
    {{1.0000f, 3.00f, 6.00f, 9.90f}, {0.00f, 0.28f, 0.11f, 0.040f}, 0.7f, 1.20f, 0.60f, 0.0f, 0.0f, 0.60f},
    {{1.0010f, 2.76f, 5.40f, 8.93f}, {0.10f, 0.26f, 0.18f, 0.090f}, 4.0f, 0.10f, 0.50f, 0.0010f, 0.10f, 0.70f},
    {{1.0006f, 2.76f, 5.40f, 8.93f}, {0.10f, 0.14f, 0.06f, 0.020f}, 2.2f, 0.45f, 0.55f, 0.0006f, 0.10f, 0.25f},
    /* PIANO: only the twin (the second string, a hair away) comes from here; partials 2..7 are n sqrt(1 + B n^2) in
     * km_coefs (stretched), t60 at C4 for the aftersound, loss, pitch_k */
    {{1.0006f, 0.0f, 0.0f, 0.0f}, {0.55f, 0.0f, 0.0f, 0.0f}, 5.0f, 0.45f, 0.50f, 0.0006f, 0.55f, 0.35f},
};
static float mdl_trem[NPART];                          /* MODAL's tremolo phase (turns), per part (modal_block) */
/* the voice's material: KALIMBA's MAT, or MODAL's MODEL */
static const km_material_t *km_mat(const track_t *t)
{
    return t->engine == ENGI_MODAL ? &MDL_MATS[clamp(t->p[P_E0], 0, MDL_N - 1)] : &KM_MATS[clamp(t->p[P_E0], 0, MAT_N - 1)];
}

typedef struct {
    float b1[KM_NMODE], b2[KM_NMODE], g[KM_NMODE];    /* y = b1 y1 + b2 y2 + g x */
    float y1[KM_NMODE], y2[KM_NMODE];
    float w0[KM_NMODE], rr[KM_NMODE], amp[KM_NMODE];  /* each mode at rest: frequency (rad), radius, level */
    float c0[KM_NMODE], s0[KM_NMODE];                 /* cos and sin of w0: the glide moves from them (and the silence test) */
    float hard;                                       /* the hammer / mallet now, 0..1 (PIANO's partials) */
    float damp;                                       /* < 1 while a finger is on it (folded into the radii) */
    float ex_amp, ex_ph, ex_inc;                      /* the pluck: a half-sine pulse, ex_ph 0..0.5 turns */
    float click, click_dec;                           /* the nail: a burst of noise */
    float glide, glide_pend, glide_rise;              /* cents sharp now; to come, and how much more each block */
    float level;
    float dc;                                         /* (MODAL) BARK's DC blocker */
    int32_t cents, nz;                                /* the pitch the modes are set for; the click's noise */
    uint8_t on, nm, quiet, contact, rise_n, live, owner, slot;   /* owner: part + 1 (0 free), slot: its voice */
    uint8_t rel;                                      /* DAMP's finger on it (the key let go) */
} km_voice_t;
static km_voice_t km_pool[KM_POOL];

/* Re-plucking a ringing tine: the thumb lands on it, damping it to ~0.55 over KM_CONTACT_N blocks, then slips off
 * (the new pluck). Done through the resonators (their decay), never a jump in the waveform */
#define KM_CONTACT_N 6u                               /* blocks of CTL (32): ~4.4 ms */
#define KM_CONTACT_D 0.99689f                         /* per sample: 0.55 over 192 samples */
#define KM_FINGER_D 0.99978f                          /* a finger left on it (DAMP) */
#define KM_GLIDE_K 0.98400f                           /* a block: exp(-32 / (0.045 s FS)), the glide's settling */

/* the voice's pool entry, 0 = none (another voice took it) */
static km_voice_t *km_of(const track_t *t, const voice_t *v)
{
    uint32_t k = (uint32_t)v->s[0];
    if (!k || k > KM_POOL)
        return 0;
    km_voice_t *s = &km_pool[k - 1u];
    return s->owner == (uint8_t)((uint32_t)(t - trk) % NPART + 1u) && s->slot == (uint8_t)(v - t->v) ? s : 0;
}
/* the tine's quirk i, -1..1 (WORN: the same each time it is plucked, from its pitch) */
static float km_wear(int32_t cents, uint32_t i)
{
    uint32_t h = (uint32_t)cents * 2654435761u ^ i * 0x9E3779B9u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return (float)(h & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f;
}

/* The resonators' settings, from their frequency (with the glide) and radius: b1 and g follow the frequency, b2 the
 * radius. A ringing mode simply carries on from its last two outputs, the smoothest join a retune can have */
static void km_freq(km_voice_t *v)
{
    const float e = v->glide * 0.000577791f;           /* cents -> the fraction the frequency rises */
    int k;
    for (k = 0; k < KM_NMODE; k++) {
        float w = v->w0[k] * (1.0f + e), r = v->rr[k], c, sn;
        if (k >= v->nm || v->amp[k] <= 0.0f || w > 0.45f * FM_TWO_PI) {
            v->b1[k] = v->b2[k] = v->g[k] = 0.0f;
            continue;
        }
        if (e == 0.0f) {
            c = v->c0[k];
            sn = v->s0[k];
        } else {   /* cos and sin of w0 + d from those of w0: the glide is small (d < 0.07 rad), to d^3 */
            const float d = v->w0[k] * e, d2 = 0.5f * d * d, d3 = d * d2 * (1.0f / 3.0f);
            c = v->c0[k] * (1.0f - d2) - v->s0[k] * (d - d3);
            sn = v->s0[k] * (1.0f - d2) + v->c0[k] * (d - d3);
        }
        v->b1[k] = 2.0f * r * c;
        v->b2[k] = -r * r;
        v->g[k] = v->amp[k] * sn;                     /* an impulse of area 1 rings at amplitude amp */
    }
}
/* the modes for the voice's pitch, its material, DECY and TONE (damp: the finger) */
static __attribute__((noinline)) void km_coefs(const track_t *t, km_voice_t *v)
{
    const int16_t *p = t->p;
    const km_material_t *m = km_mat(t);
    float f0 = 8.17579892f * fm_exp2f((float)v->cents * (1.0f / 1200.0f));
    float dscale = fm_exp2f((float)(clamp(p[P_E2], 0, 127) - 64) * 0.0315f),
          tonefac = fm_exp2f((float)(64 - clamp(p[P_E3], 0, 127)) * 0.0315f), t1, lvl = 1.0f, upper = 1.0f, spread = 1.0f;
    float worn = t->engine == ENGI_MODAL ? 0.0f : (float)clamp(p[P_E5], 0, 127) * (1.0f / 127.0f);   /* (MODAL: TREM) */
    const int pn = t->engine == ENGI_MODAL && clamp(p[P_E0], 0, MDL_N - 1) == MDL_PIANO;
    float pb = 0.0f, pslope = 0.0f;
    int k, nm = 0;
    if (pn) {                                           /* the string's stiffness B: least at the tenor, up both ways */
        float d = (float)v->cents * 0.01f - 52.0f;
        pb = 1.5e-4f * fm_exp2f(fm_fabsf(d) * (0.8f / 12.0f));
        pslope = 1.7f - 1.1f * v->hard;
        lvl = 0.5f;                                     /* -6 dB: the strike (every partial in phase, twin on top) peaked at 0.8 of full
                                                         * scale at velocity 127, over the master limiter's 0.55: it ducked the note ~4 dB
                                                         * for 100 ms, a double hit */
    }
    if (worn > 0.0f) {                                  /* this tine's quirks */
        f0 *= fm_exp2f(worn * 12.0f * km_wear(v->cents, 0) * (1.0f / 1200.0f));
        dscale *= 1.0f + 0.35f * worn * km_wear(v->cents, 1);
        spread = 1.0f + 1.5f * worn * km_wear(v->cents, 2);
        upper = 1.0f + 0.5f * worn * km_wear(v->cents, 3);
        lvl = 1.0f + 0.2f * worn * km_wear(v->cents, 4);
    }
    t1 = m->t60 * dscale * fm_powf(f0 * (1.0f / 261.63f), -m->pitch_k);
    if (t1 > 6.5f)                                      /* (chord-harp fork) at most 6.5 s: a low tine falls silent
                                                         * (-94 dB) within ~10 s, as Yehnah's voices must (tests/regress.c) */
        t1 = 6.5f;
    for (k = 0; k < KM_NMODE; k++) {
        float ratio = k ? (k < KM_TAB ? m->ratio[k - 1] : 0.0f) : 1.0f, amp = k ? (k < KM_TAB ? m->amp[k - 1] : 0.0f) : 1.0f,
              f, t60, r;
        if (k == 1)                                     /* the twin: its beat */
            ratio = 1.0f + (ratio - 1.0f) * spread;
        if (pn && k >= 2) {   /* PIANO: partial n of a stiff string, n sqrt(1 + B n^2); the hammer strikes at 1/8 of the
                               * string, so partial 8 is missing and the rest follow sin(n pi / 8); a softer hammer
                               * leaves the high ones weak */
            const float n = (float)k;
            ratio = n * fm_sqrtf(1.0f + pb * n * n);
            amp = 1.1f * fm_fabsf(fm_sin_turns(n * 0.0625f)) * fm_powf(n, -pslope);
        }
        if (k >= 2)
            amp *= upper;
        amp *= lvl;
        f = f0 * ratio;
        if (k == 1 && m->beat_amp <= 0.0f)
            amp = 0.0f;
        v->amp[k] = 0.0f;
        if (f > 0.45f * (float)FS || amp <= 0.0f)
            continue;
        t60 = pn && k == 0 ? t1 * 0.22f : t1;          /* PIANO: the first string's prompt sound, then the aftersound */
        if (k >= 2)
            t60 /= 1.0f + m->loss * tonefac * (ratio - 1.0f);
        if (t60 < 0.01f)
            t60 = 0.01f;
        r = fm_expf(-6.9077553f / (t60 * (float)FS)) * v->damp;
        v->w0[k] = FM_TWO_PI * f / (float)FS;
        v->c0[k] = fm_cosf(v->w0[k]);
        v->s0[k] = fm_sinf(v->w0[k]);
        v->rr[k] = r;
        v->amp[k] = amp;
        nm = k + 1;
    }
    v->nm = (uint8_t)nm;
    km_freq(v);
}

static void kalimba_note_on(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    const km_material_t *m = km_mat(t);
    km_voice_t *s = km_of(t, v);
    uint32_t i;
    int again = s && s->on;
    if (!s) {                                           /* a pool entry: a free one, else the quietest */
        uint32_t best = 0;
        float lo = 1e9f;
        for (i = 0; i < KM_POOL && km_pool[i].on; i++)
            if (km_pool[i].level < lo) {
                lo = km_pool[i].level;
                best = i;
            }
        s = &km_pool[i < KM_POOL ? i : best];
        s->on = 0;
        again = 0;
        v->s[0] = (int32_t)(s - km_pool) + 1;
        s->owner = (uint8_t)((uint32_t)(t - trk) % NPART + 1u);
        s->slot = (uint8_t)(v - t->v);
        s->nz = (int32_t)(0x2545F491u + v->age * 2654435761u);
        s->dc = 0.0f;
    }
    float hard = fm_clampf((float)clamp(p[P_E1], 0, 127) * (1.0f / 127.0f) + (float)((int32_t)v->vel - 90) *
                           (0.35f / 127.0f), 0.0f, 1.0f),
          /* a pluck is the tine slipping off the thumb: the force falls away in tp, slow for the pad of a thumb
           * (1.2 ms: the upper modes hardly start), quick for a nail (0.05 ms: they all ring) */
          tp = (t->engine == ENGI_MODAL && clamp(p[P_E0], 0, MDL_N - 1) == MDL_PIANO ?
                0.0025f * fm_exp2f(-3.0f * hard) :     /* (PIANO: a felt hammer's contact, ~1 ms at HARD 50: a rounder strike) */
                0.0012f * fm_exp2f(-4.5f * hard)), vg = (float)v->vel * (1.0f / 127.0f), a, nsm;
    if (!again)
        for (i = 0; i < KM_NMODE; i++)
            s->y1[i] = s->y2[i] = 0.0f;
    s->cents = (int32_t)v->note * 100;
    s->hard = hard;
    s->live = 0xFFu;                                    /* every mode, until it falls silent */
    s->damp = again ? KM_CONTACT_D : 1.0f;              /* ringing: the thumb lands on it first */
    s->contact = again ? (uint8_t)KM_CONTACT_N : 0u;
    s->rel = 0;
    km_coefs(t, s);
    nsm = tp * (float)FS;                               /* pulse samples */
    if (nsm < 1.0f)
        nsm = 1.0f;
    a = vg * vg * 0.55f;
    {   /* sharp at first: tension, as it swings wide. A ringing tine takes it as the thumb slips off */
        float gl = t->engine == ENGI_MODAL ? 0.0f :   /* (MODAL: BARK there, no glide) */
                   (float)clamp(p[P_E4], 0, 127) * (40.0f / 127.0f) * vg * vg * (0.4f + 0.6f * hard);
        s->glide_rise = 0.0f;
        s->rise_n = 0;
        if (again) {
            s->glide_pend = gl;
        } else {
            s->glide = gl;
            s->glide_pend = 0.0f;
            km_freq(s);
        }
    }
    s->ex_inc = 0.5f / nsm;                             /* half a turn over the contact */
    s->ex_ph = again ? -(float)(KM_CONTACT_N * CTL) * s->ex_inc : 0.0f;   /* re-pluck: it slips off after the contact */
    s->ex_amp = a * (FM_PI * 0.5f) / nsm;               /* area a: the first mode rings at ~a */
    s->click = a * m->click * hard * hard * 0.12f * (float)clamp(p[P_E7], 0, 127) * (1.0f / 64.0f);
    s->click_dec = fm_expf(-1.0f / (0.0025f * (float)FS));
    s->on = 1;
    s->quiet = 0;
    s->level = vg;
}
static int kalimba_done(track_t *t, voice_t *v)
{
    km_voice_t *s = km_of(t, v);
    return !s || !s->on;
}

static void kalimba_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    km_voice_t *s = km_of(t, v);
    float vb[CTL], xb[CTL], pk = 0.0f, amp = (float)m->amp0, damp = (float)(m->amp1 - m->amp0) * (1.0f / CTL);
    const float kq = (float)VOICE_FS * 1.6f / 32768.0f;
    uint32_t j;
    int k, excite;
    if (!s || !s->on)
        return;
    {   /* the pitch: the key's, with the part's bend, tune and vibrato (m->pitch16, 1/16 semitone) */
        int32_t c = m->pitch16 * 100 / 16 + m->fine * 100 / 236;   /* (fine: 1/4096 of the increment, ~1.7 ct a step) */
        if (c != s->cents && s->ex_ph >= 0.0f) {
            s->cents = c;
            km_coefs(t, s);
        }
    }
    if (!v->gate && t->p[P_E6] > 0 && !s->rel) {        /* DAMP: a finger on it as the key is let go */
        float d = 1.0f - (1.0f - KM_FINGER_D) * (float)t->p[P_E6] * (1.0f / 16.0f);   /* (127: ~0.1 s) */
        s->contact = 0;
        s->rel = 1;
        s->damp = d < 0.99f ? 0.99f : d;
        km_coefs(t, s);
    }
    excite = s->ex_ph < 0.5f;
    if (excite) {                                       /* the pulse (ex_ph < 0: the thumb still resting on it) */
        float ph = s->ex_ph;
        for (j = 0; j < n; j++) {
            xb[j] = ph >= 0.0f && ph < 0.5f ? s->ex_amp * fm_sin_turns(ph) : 0.0f;
            ph += s->ex_inc;
        }
        s->ex_ph = ph;
    }
    for (j = 0; j < n; j++)
        vb[j] = 0.0f;
    for (k = 0; k < s->nm; k++) {
        float y1 = s->y1[k], y2 = s->y2[k];
        const float b1 = s->b1[k], b2 = s->b2[k], g = s->g[k];
        if (g == 0.0f || !(s->live >> k & 1u))
            continue;
        if (excite) {
            for (j = 0; j < n; j++) {
                float y = b1 * y1 + b2 * y2 + g * xb[j];
                y2 = y1;
                y1 = y;
                vb[j] += y;
            }
        } else {
            for (j = 0; j < n; j++) {
                float y = b1 * y1 + b2 * y2;
                y2 = y1;
                y1 = y;
                vb[j] += y;
            }
        }
        s->y1[k] = fm_flush(y1);
        s->y2[k] = fm_flush(y2);
        if (!excite) {   /* below ~-110 dB it is silence: skip it until the next pluck. Its amplitude C from the last
                          * two outputs: (C sin w)^2 = (y1 sin w)^2 + (y1 cos w - r y2)^2 */
            const float sw = s->s0[k], q = y1 * s->c0[k] - s->rr[k] * y2;
            if (y1 * y1 * sw * sw + q * q < 1e-11f * sw * sw) {
                s->live &= (uint8_t)~(1u << k);
                s->y1[k] = s->y2[k] = 0.0f;
            }
        }
    }
    if (s->click > 1e-6f && s->ex_ph >= 0.0f) {         /* the nail's tick, as it slips off */
        float c = s->click;
        for (j = 0; j < n; j++) {
            vb[j] += c * ((float)(int32_t)noise32(&s->nz) * (1.0f / 2147483648.0f));
            c *= s->click_dec;
        }
        s->click = c;
    }
    if (t->engine == ENGI_MODAL) {                      /* (chord-harp fork) MODAL: the pickup (BARK), the tremolo */
        float b = (float)clamp(t->p[P_E4], 0, 127) * (1.2f / 127.0f), dc = s->dc, tr = 1.0f;
        if (t->p[P_E5] > 0) {                           /* (TREM: ~-6 dB deep, the part's phase) */
            float w = fm_sin_turns(mdl_trem[(uint32_t)(t - trk) % NPART]);
            tr = 0.75f + 0.25f * w;
        }
        for (j = 0; j < n; j++) {                       /* an asymmetric pickup: the second harmonic, more as it swings */
            float o = vb[j];
            o += b * o * fm_fabsf(o) * 0.8f + b * o * o * 0.6f;
            dc += (o - dc) * 0.0015f;
            vb[j] = (o - dc) * tr;
        }
        s->dc = fm_flush(dc);
    }
    for (j = 0; j < n; j++, amp += damp) {
        float o = vb[j], a = fm_fabsf(o);
        pk = a > pk ? a : pk;
        out[j] += (int32_t)(o * amp * kq);
    }
    if (s->contact && !--s->contact) {                  /* the thumb slips off: free again, and stretched */
        if (s->damp == KM_CONTACT_D)
            s->damp = 1.0f;
        s->glide_rise = s->glide_pend * 0.125f;         /* over eight blocks (~6 ms): a ringing tine eases into it */
        s->glide_pend = 0.0f;
        km_coefs(t, s);
    }
    if (s->glide_rise > 0.0f) {                         /* rising (a re-pluck) */
        s->glide += s->glide_rise;
        if (++s->rise_n >= 8u)
            s->glide_rise = 0.0f, s->rise_n = 0;
        km_freq(s);
    } else if (s->glide != 0.0f && s->ex_ph >= 0.0f) {  /* then it settles as the swing narrows: ~45 ms */
        s->glide = s->glide > 0.05f ? s->glide * KM_GLIDE_K : 0.0f;
        km_freq(s);
    }
    s->level = pk > s->level ? pk : s->level * 0.9f + pk * 0.1f;
    if (pk < 2e-5f && !excite) {
        if (++s->quiet > 48u)                           /* ~35 ms of nothing: the tine has stopped */
            s->on = 0;
    } else {
        s->quiet = 0;
    }
}

static const preset_t KALIMBA_PRESETS[] YEHNAH_RES = {
    /* name, {MAT HARD DECY TONE GLID WORN DAMP CLIK}, {A D S R} (not used: its own envelope), fenv, mono, sends */
    {"KALIMBA", {MAT_STEEL, 57, 64, 70, 38, 0, 0, 64}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 35)},
    {"MBIRA", {MAT_STEEL, 70, 80, 80, 50, 50, 0, 90}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 0, 40)},
    {"SOFT THUMB", {MAT_STEEL, 20, 64, 50, 25, 0, 0, 20}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 40)},
    {"BRASS TINE", {MAT_BRASS, 57, 64, 64, 38, 0, 0, 64}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 35)},
    {"BRONZE BELL", {MAT_BRONZE, 60, 90, 80, 20, 0, 0, 60}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 20, 50)},
    {"ALU BARS", {MAT_ALU, 64, 64, 70, 10, 0, 0, 64}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 10, 35)},
    {"BAMBOO", {MAT_BAMBOO, 64, 64, 50, 15, 20, 0, 50}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 25)},
    {"GLASS", {MAT_GLASS, 70, 80, 90, 10, 0, 0, 80}, {0, 64, 127, 40}, 0, 0, FX(0, 20, 20, 55)},
    {"MUTED TINE", {MAT_STEEL, 57, 64, 64, 38, 0, 90, 64}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 25)},
    /* (chord-harp fork, appended) mallets and bells from the same bars: vibes (the LFO on the level: the motor), a
     * wind chimes, a glass harp, a deep tine (an octave down), a gamelan, a plink (BAMBOO is the marimba already) */
    {"VIBES", {MAT_ALU, 50, 100, 60, 0, 0, 0, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 30, 20, 50),
     XP(P_LRATE + 1, 64, P_LD_AMP + 1, 30)},
    {"WIND CHIMES", {MAT_ALU, 80, 127, 110, 0, 30, 0, 50}, {0, 64, 127, 40}, 0, 0, FX(0, 20, 40, 90)},
    {"GLASS HARP", {MAT_GLASS, 30, 110, 70, 0, 0, 0, 10}, {0, 64, 127, 40}, 0, 0, FX(0, 40, 30, 80)},
    {"DEEP TINE", {MAT_STEEL, 40, 90, 40, 38, 0, 0, 40}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 10, 45), XP(P_TRANS + 1, -12)},
    {"GAMELAN", {MAT_BRONZE, 80, 100, 90, 0, 60, 0, 70}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 25, 60)},
    {"PLINK", {MAT_GLASS, 110, 30, 110, 0, 0, 40, 100}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 35, 30)},
    /* (chord-harp fork, 2026-10-27) hand drums (Del Rio's DJEMBE CAJON UDU): bamboo struck low, GLID the skin's
     * pitch settling */
    {"DJEMBE", {MAT_BAMBOO, 80, 30, 45, 40, 50, 30, 80}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 25), XP(P_TRANS + 1, -12)},
    {"INIT", {0, 57, 64, 70, 38, 0, 0, 64}, {0, 64, 127, 30}, 0, 0, FX(0, 0, 0, 0)},   /* (chord-harp fork) the EDIT defaults, dry */
};
static const char KALIMBA_PT[] = "UUUUEMMEUMEEUMUDN";
_Static_assert(sizeof KALIMBA_PT == NELEM(KALIMBA_PRESETS) + 1u, "a type a preset");
/* (engine 17: its favourites in favorites.factory[1]'s upper half, favorites.c favorite_byte: 128 presets at most) */
_Static_assert(NELEM(KALIMBA_PRESETS) <= 128u, "KALIMBA's favourites");
static const engine_t ENG_KALIMBA = {
    .name = "KALIMBA",
    .page_title = {"TINE", "PLAY"},
    .edit = {
        {"MAT", F_ENUM, 0, MAT_N - 1, MAT_STEEL, N_KMAT, 0},
        {"HARD", F_PCT, 0, 127, 57, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 70, 0, 0},
        {"GLID", F_PCT, 0, 127, 38, 0, 0},
        {"WORN", F_PCT, 0, 127, 0, 0, 0},
        {"DAMP", F_PCT, 0, 127, 0, 0, 0},
        {"CLIK", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = KALIMBA_PRESETS,
    .npresets = NELEM(KALIMBA_PRESETS),
    .ptype = KALIMBA_PT,
    .note_on = kalimba_note_on,
    .render = kalimba_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .ownenv = 1,
    .done = kalimba_done,        /* (s[0], its pool entry: voice.c leaves it, keep 0) */
};
