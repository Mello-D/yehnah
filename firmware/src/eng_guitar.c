/* SPDX-License-Identifier: GPL-3.0-only
 * (chord-harp fork) GUITAR: a plucked string by Karplus-Strong, our own (after Jaffe and Smith's extensions, 1983).
 * Each voice is a string: a delay line one period long, round which the pluck runs, losing a little of its top each
 * pass (a one-zero averaging filter) and a little of everything (the loop gain, from DECAY), tuned to the fraction of a
 * sample by a first-order allpass. The pluck fills the line: noise darkened by how hard it is plucked (PLUCK and the
 * velocity: a soft finger a dark one, a pick a bright one), minus itself a fraction of the string along (PICK: the
 * comb of the pick position, near the bridge thin and twangy, over the neck round). Then the body: two resonances
 * (the air in it and the top) for NYLON and STEEL, the pickup's for ELEC; its drive and cabinet are the part's DIST
 * (AMP) and FILTER, as the presets set them. MUTE is the palm on the strings by the bridge (short, darker, a thud);
 * DAMP the fretting hand let go (0: it rings on, an open string).
 * EDIT 1  MODEL PICK PLUCK DECY      EDIT 2  TONE BODY MUTE DAMP
 * A guitar's six strings: GT_POOL lines shared by the parts (v->s[0] its slot + 1; a part's poly 6), 16-bit, in the
 * pool (.pool); a note below D2 (the line's longest) plays an octave up. The string and its body run at half the rate
 * (GT_SR: its top 11 kHz, half the work and half the line), the output interpolated back up. In float (the core's FPU;
 * fastmath.h). Its own envelope (ownenv): the ADSR is not used; the strum is HARP's. */
#include "fastmath.h"

enum { GT_NYLON, GT_STEEL, GT_ELEC, GT_N };
static const char *const N_GTMODEL[] = {"NYLON", "STEEL", "ELEC"};
#define GT_POOL 6u                     /* strings sounding at once, all parts */
#define GT_LEN 304u                    /* a line's samples: D2 (73.4 Hz) and up */
#define GT_SR 22050.0f                 /* the string's rate: half the output's */
typedef struct {                       /* a model: its decay, its pluck, its body (two resonances: f, Q, level) */
    float t60, bright, loss;           /* T60 of A2 (s); the pluck's brightness; TONE's averaging at its middle */
    float bf[2], bq[2], bg[2];
} gt_model_t;
static const gt_model_t GT_MODELS[GT_N] = {
    {4.0f, 0.55f, 0.42f, {98.0f, 205.0f}, {6.0f, 5.0f}, {0.9f, 0.6f}},      /* NYLON: dark, a round body */
    {6.0f, 0.85f, 0.30f, {110.0f, 230.0f}, {7.0f, 6.0f}, {0.7f, 0.5f}},     /* STEEL: bright, longer */
    {7.0f, 0.75f, 0.26f, {2600.0f, 0.0f}, {1.4f, 1.0f}, {0.6f, 0.0f}},      /* ELEC: no body, the pickup's peak */
};
typedef struct {
    float ap_c, ap_x1, ap_y1;          /* the tuning allpass: its coefficient, last input, last output */
    float z1, g, s;                    /* the loss filter's last input; the loop gain; its averaging (0 .. 0.5) */
    float b[2][5], bz[2][2];           /* the body's band-passes (b0 b1 b2 a1 a2) and their states */
    float bmix, dc_x, dc_y;            /* how much body; the DC blocker */
    float prev;                        /* the last half-rate output (the interpolation's) */
    float level, period;               /* the loudness lately (the steal); the line's length now (samples) */
    int32_t cents, nz;                 /* the pitch it is tuned for; the noise */
    uint16_t w, len;                   /* the write position; the integer delay */
    uint8_t on, quiet, owner, slot, rel, model, oct;   /* owner: part + 1 (0 free), slot: its voice; rel: DAMP done */
} gt_voice_t;
static gt_voice_t gt_pool[GT_POOL];
static int16_t gt_line[GT_POOL][GT_LEN] __attribute__((section(".pool")));

#define GT_NI static __attribute__((noinline))
/* the voice's pool entry, 0 = none (another voice took it) */
static gt_voice_t *gt_of(const track_t *t, const voice_t *v)
{
    uint32_t k = (uint32_t)v->s[0];
    gt_voice_t *s;
    if (!k || k > GT_POOL)
        return 0;
    s = &gt_pool[k - 1u];
    return s->owner == (uint8_t)((uint32_t)(t - trk) % NPART + 1u) && s->slot == (uint8_t)(v - t->v) ? s : 0;
}
static inline float gt_noise(int32_t *nz) { return (float)(int32_t)noise32(nz) * (1.0f / 2147483648.0f); }
static inline float gt_k(const track_t *t, uint32_t i) { return (float)clamp(t->p[P_E0 + i], 0, 127) * (1.0f / 127.0f); }

/* a band-pass (RBJ, constant peak gain), its coefficients */
GT_NI void gt_bp(float *c, float f, float q)
{
    float w = FM_TWO_PI * fm_clampf(f, 20.0f, 9000.0f) * (1.0f / GT_SR), sn = fm_sinf(w), cs = fm_cosf(w);
    float al = sn / (2.0f * q), a0 = 1.0f / (1.0f + al);
    c[0] = al * a0; c[1] = 0.0f; c[2] = -al * a0;
    c[3] = -2.0f * cs * a0; c[4] = (1.0f - al) * a0;
}
/* the string for its pitch (cents) and the knobs: its length, the allpass's fraction, the loss and the loop gain */
GT_NI void gt_tune(const track_t *t, gt_voice_t *s, int32_t cents)
{
    const gt_model_t *m = &GT_MODELS[s->model];
    float f0 = 8.17579892f * fm_exp2f((float)cents * (1.0f / 1200.0f)), p, frac, t60, mute = gt_k(t, 6);
    uint32_t oct = 0;
    while (f0 * fm_exp2f((float)oct) < GT_SR / (float)(GT_LEN - 3u) && oct < 4u)
        oct++;                                          /* (below D2: an octave up) */
    f0 *= fm_exp2f((float)oct);
    s->oct = (uint8_t)oct;
    s->cents = cents;
    s->s = fm_clampf(m->loss + (0.5f - gt_k(t, 4)) * 0.5f + mute * 0.2f, 0.02f, 0.5f);   /* TONE: less averaging, brighter */
    p = GT_SR / f0 - s->s;                              /* the loop: the line, the loss filter's s, the allpass's frac */
    s->len = (uint16_t)clamp((int32_t)(p - 0.1f), 2, GT_LEN - 2);
    frac = p - (float)s->len;
    s->ap_c = (1.0f - frac) / (1.0f + frac);
    s->period = p;
    t60 = m->t60 * fm_exp2f((gt_k(t, 3) - 0.5f) * 4.0f) * fm_powf(f0 * (1.0f / 110.0f), -0.45f);   /* DECY: x0.25 .. x4 */
    t60 *= 1.0f - mute * 0.96f;                         /* MUTE: the palm, down to ~4 % */
    if (s->rel)                                         /* DAMP: the fretting hand let go */
        t60 = fm_minf(t60, 0.04f + 2.0f * (1.0f - gt_k(t, 7)));
    t60 = fm_clampf(t60, 0.03f, 8.0f);                  /* (falls silent within ~10 s, as Yehnah's voices must) */
    s->g = fm_expf(-6.9078f / (t60 * f0));              /* -60 dB over t60: a pass a period */
}

static void guitar_note_on(track_t *t, voice_t *v)
{
    gt_voice_t *s = gt_of(t, v);
    const gt_model_t *m;
    float bright, vg = (float)v->vel * (1.0f / 127.0f), lp = 0.0f, amp, mute = gt_k(t, 6);
    uint32_t i, n, pk, k;
    int16_t *L;
    if (!s) {                                           /* a string: a free one, else the quietest */
        uint32_t best = 0;
        float lo = 1e9f;
        for (i = 0; i < GT_POOL && gt_pool[i].on; i++)
            if (gt_pool[i].level < lo) {
                lo = gt_pool[i].level;
                best = i;
            }
        s = &gt_pool[i < GT_POOL ? i : best];
        s->on = 0;
        v->s[0] = (int32_t)(s - gt_pool) + 1;
        s->owner = (uint8_t)((uint32_t)(t - trk) % NPART + 1u);
        s->slot = (uint8_t)(v - t->v);
        s->nz = (int32_t)(0x2545F491u + v->age * 2654435761u);
    }
    L = gt_line[s - gt_pool];
    s->model = (uint8_t)clamp(t->p[P_E0], 0, GT_N - 1);
    m = &GT_MODELS[s->model];
    s->rel = 0;
    gt_tune(t, s, (int32_t)v->note * 100);
    if (!s->on) {                                       /* (a string at rest: its filters too; a ringing one keeps its body's) */
        s->ap_x1 = s->ap_y1 = s->z1 = s->dc_x = s->dc_y = s->prev = 0.0f;
        s->bz[0][0] = s->bz[0][1] = s->bz[1][0] = s->bz[1][1] = 0.0f;
    }
    for (k = 0; k < 2u; k++)                            /* the body (or the pickup) */
        if (m->bg[k] > 0.0f)
            gt_bp(s->b[k], m->bf[k], m->bq[k]);
    s->bmix = gt_k(t, 5) * 2.0f;                        /* BODY: 64 the model's */
    /* the pluck: PLUCK and the velocity its brightness (a one-pole's coefficient), MUTE a duller one */
    bright = fm_clampf(m->bright * (0.35f + 1.3f * gt_k(t, 2)) * (0.6f + 0.6f * vg) * (1.0f - 0.6f * mute), 0.04f, 1.0f);
    amp = vg * (0.55f + 0.45f * vg) * 0.9f;
    n = s->len;
    pk = (uint32_t)((float)n * (0.04f + 0.46f * gt_k(t, 1)) + 0.5f);   /* PICK: 0.04 (the bridge) .. 0.5 of the string */
    if (pk < 1u) pk = 1u;
    {   /* the line (the samples to come out next, oldest first): noise, darkened, then minus itself pk along (the pick's
         * comb, from the end back: what it takes is still the noise). A re-pluck takes the string as the pick does */
        uint32_t at0 = s->w + GT_LEN - n;
        for (i = 0; i < n; i++) {
            lp += bright * (gt_noise(&s->nz) - lp);
            L[(at0 + i) % GT_LEN] = (int16_t)clamp((int32_t)(lp * amp * 16384.0f), -32767, 32767);
        }
        for (i = n; i-- > pk;) {
            int32_t y = (int32_t)L[(at0 + i) % GT_LEN] - (int32_t)L[(at0 + i - pk) % GT_LEN];
            L[(at0 + i) % GT_LEN] = (int16_t)clamp(y, -32767, 32767);
        }
    }
    s->on = 1;
    s->quiet = 0;
    s->level = vg;
}

static void guitar_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    gt_voice_t *s = gt_of(t, v);
    float amp = (float)m->amp0, damp = (float)(m->amp1 - m->amp0) * (1.0f / CTL), pk = 0.0f;
    const float kq = (float)VOICE_FS * 1.5f / (32768.0f * 16384.0f);
    float apc, apx, apy, z1, g, sv, bm, dcx, dcy, b0[5], b1[5], z[2][2], prev = s ? s->prev : 0.0f;
    uint32_t j, w, len, k;
    int16_t *L;
    if (!s || !s->on)
        return;
    {   /* the pitch: the key's, with the part's bend, tune and vibrato (m->pitch16, 1/16 semitone) */
        int32_t c = m->pitch16 * 100 / 16 + m->fine * 100 / 236;
        if (!v->gate && !s->rel && t->p[P_E7] > 0) {    /* DAMP: the key let go */
            s->rel = 1;
            s->cents = c + 1;                           /* (retuned below with it) */
        }
        if (c != s->cents)
            gt_tune(t, s, c);
    }
    L = gt_line[s - gt_pool];
    apc = s->ap_c, apx = s->ap_x1, apy = s->ap_y1, z1 = s->z1, g = s->g, sv = s->s, dcx = s->dc_x, dcy = s->dc_y;
    bm = s->bmix;
    w = s->w, len = s->len;
    for (k = 0; k < 5u; k++)
        b0[k] = s->b[0][k], b1[k] = s->b[1][k];
    z[0][0] = s->bz[0][0], z[0][1] = s->bz[0][1], z[1][0] = s->bz[1][0], z[1][1] = s->bz[1][1];
    {
        const gt_model_t *md = &GT_MODELS[s->model];
        float g0 = md->bg[0] * bm, g1 = md->bg[1] * bm;
        for (j = 0; j < n; j += 2u, amp += 2.0f * damp) {   /* (a string sample a pair of output samples) */
            float x = (float)L[(w + GT_LEN - len) % GT_LEN], y, o, bp;
            y = g * ((1.0f - sv) * x + sv * z1);        /* the loss: the averaging, the loop gain */
            z1 = x;
            o = apc * y + apx - apc * apy;              /* the fraction: the allpass */
            apx = y;
            apy = o;
            L[w] = (int16_t)(o > 32767.0f ? 32767.0f : o < -32767.0f ? -32767.0f : o);
            w = w + 1u < GT_LEN ? w + 1u : 0u;
            dcy = x - dcx + 0.995f * dcy;               /* the string's output, its DC out */
            dcx = x;
            o = dcy;
            if (g0 > 0.0f) {                            /* the body: band-passes added (transposed direct form II) */
                bp = b0[0] * dcy + z[0][0];
                z[0][0] = b0[1] * dcy - b0[3] * bp + z[0][1];
                z[0][1] = b0[2] * dcy - b0[4] * bp;
                o += bp * g0;
            }
            if (g1 > 0.0f) {
                bp = b1[0] * dcy + z[1][0];
                z[1][0] = b1[1] * dcy - b1[3] * bp + z[1][1];
                z[1][1] = b1[2] * dcy - b1[4] * bp;
                o += bp * g1;
            }
            pk = fm_fabsf(o) > pk ? fm_fabsf(o) : pk;
            out[j] += (int32_t)((prev + o) * 0.5f * amp * kq);   /* (halfway, then the sample) */
            if (j + 1u < n)
                out[j + 1u] += (int32_t)(o * (amp + damp) * kq);
            prev = o;
        }
    }
    s->ap_x1 = apx, s->ap_y1 = fm_flush(apy), s->z1 = z1, s->dc_x = dcx, s->dc_y = fm_flush(dcy), s->prev = fm_flush(prev);
    s->bz[0][0] = fm_flush(z[0][0]), s->bz[0][1] = fm_flush(z[0][1]), s->bz[1][0] = fm_flush(z[1][0]),
    s->bz[1][1] = fm_flush(z[1][1]);
    s->w = (uint16_t)w;
    pk *= 1.0f / 16384.0f;
    s->level = pk > s->level ? pk : s->level * 0.9f + pk * 0.1f;
    if (pk < 3e-5f) {
        if (++s->quiet > 48u)                           /* ~35 ms of nothing: the string has stopped */
            s->on = 0;
    } else {
        s->quiet = 0;
    }
}
static int guitar_done(track_t *t, voice_t *v)
{
    gt_voice_t *s = gt_of(t, v);
    return !s || !s->on;
}

static const preset_t GUITAR_PRESETS[] YEHNAH_RES = {
    /* name, {MODEL PICK PLUCK DECY TONE BODY MUTE DAMP}, {A D S R} (not used: its own envelope), fenv, mono, sends;
     * ELEC's drive and cabinet: the part's DIST (AMP) and FILTER (LP), XP */
    {"NYLON", {GT_NYLON, 40, 40, 64, 55, 64, 0, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 30)},
    {"STEEL", {GT_STEEL, 25, 70, 64, 64, 64, 0, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 0, 30)},
    {"FOLK STRUM", {GT_STEEL, 30, 60, 70, 60, 80, 0, 20}, {0, 64, 127, 40}, 0, 0, FX(0, 20, 0, 36)},
    {"PALM MUTE", {GT_STEEL, 20, 75, 64, 64, 64, 95, 60}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 18)},
    {"CLEAN ELEC", {GT_ELEC, 22, 70, 64, 66, 64, 0, 40}, {0, 64, 127, 40}, 0, 0, FX(0, 30, 20, 26)},
    {"JAZZ BOX", {GT_ELEC, 60, 35, 60, 35, 50, 0, 50}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 30),
     XP(P_FCUT + 1, 88)},
    {"CRUNCH", {GT_ELEC, 18, 80, 64, 70, 64, 0, 40}, {0, 64, 127, 40}, 0, 0, FX(45, 0, 15, 24),
     XP(P_DTYPE + 1, 5, P_FCUT + 1, 96)},
    {"OVERDRIVE", {GT_ELEC, 15, 85, 72, 72, 64, 0, 30}, {0, 64, 127, 40}, 0, 0, FX(80, 0, 25, 26),
     XP(P_DTYPE + 1, 5, P_FCUT + 1, 90, P_DTONE + 1, 10)},
    {"CHUG", {GT_ELEC, 12, 90, 64, 70, 64, 90, 70}, {0, 64, 127, 40}, 0, 1, FX(85, 0, 0, 14),
     XP(P_DTYPE + 1, 5, P_FCUT + 1, 88)},
    /* (chord-harp fork, appended) a banjo-ish twang, an ambient electric (long, into the delay and reverb), a funk
     * clean, a blues drive, a fuzz lead */
    {"TWANG", {GT_STEEL, 8, 110, 30, 100, 30, 0, 60}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 10, 20)},
    {"AMBIENT GTR", {GT_ELEC, 40, 40, 100, 50, 64, 0, 20}, {0, 64, 127, 40}, 0, 0, FX(0, 40, 50, 90)},
    {"FUNK CLEAN", {GT_ELEC, 15, 80, 50, 80, 64, 30, 70}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 0, 15)},
    {"BLUES DRIVE", {GT_ELEC, 25, 75, 70, 60, 64, 0, 40}, {0, 64, 127, 40}, 0, 0, FX(55, 0, 20, 30),
     XP(P_DTYPE + 1, 5, P_FCUT + 1, 92)},
    {"FUZZ LEAD", {GT_ELEC, 15, 90, 80, 70, 64, 0, 30}, {0, 64, 127, 40}, 0, 1, FX(100, 0, 30, 30),
     XP(P_DTYPE + 1, 1, P_FCUT + 1, 85)},
    /* (chord-harp fork, 2026-10-27) the old and the far plucked strings (ESTAMPIE's LUTE, FuMi-1's KOTO) */
    {"LUTE", {GT_NYLON, 30, 55, 50, 70, 80, 0, 35}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 35)},
    {"OUD", {GT_NYLON, 20, 80, 45, 60, 90, 0, 45}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 30)},
    {"KOTO", {GT_STEEL, 15, 100, 60, 90, 40, 0, 50}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 10, 40)},
    {"SHAMISEN", {GT_STEEL, 5, 120, 30, 110, 20, 0, 70}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 25)},
    {"INIT", {1, 25, 70, 64, 64, 64, 0, 30}, {0, 64, 127, 30}, 0, 0, FX(0, 0, 0, 0)},   /* (chord-harp fork) the EDIT defaults, dry */
};
static const char GUITAR_PT[] = "GGGGGGGGGGGGGGGGGGN";
_Static_assert(sizeof GUITAR_PT == NELEM(GUITAR_PRESETS) + 1u, "a type a preset");
/* (engine 19: its favourites in favorites.factory[3]'s upper half, favorites.c favorite_byte: 128 presets at most) */
_Static_assert(NELEM(GUITAR_PRESETS) <= 128u, "GUITAR's favourites");
static const engine_t ENG_GUITAR = {
    .name = "GUITAR",
    .page_title = {"STRING", "HAND"},
    .edit = {
        {"MODEL", F_ENUM, 0, GT_N - 1, GT_STEEL, N_GTMODEL, 0},
        {"PICK", F_PCT, 0, 127, 25, 0, 0},
        {"PLUCK", F_PCT, 0, 127, 70, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
        {"BODY", F_PCT, 0, 127, 64, 0, 0},
        {"MUTE", F_PCT, 0, 127, 0, 0, 0},
        {"DAMP", F_PCT, 0, 127, 30, 0, 0},
    },
    .presets = GUITAR_PRESETS,
    .npresets = NELEM(GUITAR_PRESETS),
    .ptype = GUITAR_PT,
    .note_on = guitar_note_on,
    .render = guitar_render,
    .knob = {P_E1, P_E2, P_E3, P_E6},
    .poly = GT_POOL,
    .ownenv = 1,
    .done = guitar_done,         /* (s[0], its pool entry: voice.c leaves it, keep 0) */
};
