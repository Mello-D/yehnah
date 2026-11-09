/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 3dSam (Clipper), after Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca) */
/* (chord-harp fork, 2026-11-04) Clipper's HYBRID (isod89/clipper-fm1 7bc15a9, GPL-3.0), its DSP and its patches
 * verbatim. Ours: the patch is read from the flash (HY_FACTORY[PTCH], no copy a part: no web-editor patches, Clipper's
 * hy_in / hy_set_patch / editor_hybrid.c left out); the voices' state in a pool shared by the parts (hy_pool, HY_POOL;
 * v->s[0] its entry + 1, as PLAITS'), in .pool; 4 voices a part (HY_POLY: SUPER is 7 saws an oscillator); the
 * controllers from the track (t->mw, t->at, ex_off; the bend: midi_control.c hy_bend). */
/* HYBRID: the Clipper voice. Three oscillators, two filters, two envelopes and two LFOs of its own per voice,
 * and a 16-slot modulation matrix, on top of what every engine gets from voice.c (the amplitude ADSR, the
 * part's LFO, glide, unison, the pitch envelope).
 *
 * The patch is the sound (hy_patch_t, HY_PATCH_SIZE bytes): every synth part has one. A factory patch is chosen
 * by PTCH (EDIT P_E3; read in the flash, its matrix compiled in the audio ISR at the start of a block: hy_block).
 * On the device the eight EDIT values play the patch:
 *   CUT  RES  FENV   offsets on both filters' cutoff, resonance and envelope amount (-64..63)
 *   PTCH the factory patch
 *   M1..M4  four macro knobs: sources of the matrix (the patch says what each one does)
 *
 * Per sample, per voice: each oscillator renders a CTL block into its own buffer (one switch a block, not a
 * sample), the mix goes through the drive and the filters, then the amplitude. Everything else (the matrix,
 * the envelopes, the LFOs, the cutoffs and the oscillator increments) runs once per control tick (CTL samples).
 *
 * Oscillator types (shape: what the SHP value does):
 *   SAW    band-limited saw (polyBLEP)
 *   PULSE  band-limited pulse, SHP the width (50 % .. 6 %)
 *   TRI    triangle
 *   SINE   sine, SHP self feedback (sine -> saw-like, as a DX7 operator's feedback)
 *   MORPH  SHP sweeps sine -> triangle -> saw -> square
 *   SUPER  seven detuned saws (the 90s supersaw: its detune and mix curves), SHP the detune
 *   FM     a sine phase-modulated by the oscillator before it (osc 1: by osc 3 of the last block), SHP the index
 *   SYNC   a saw hard-synced to osc 1, SHP its pitch above osc 1 (0 .. +48 semitones)
 *   FOLD   a sine through a sine wavefolder, SHP the fold
 *   NOISE  white noise, SHP darkens it (one-pole low-pass)
 * Filter types: OFF, LP12, LP24 (two stages, the resonance on the second), BP, HP, NOTCH, PEAK; routing SER
 * (F1 -> F2), PAR (F1 + F2, BAL between them) or SPLIT (osc 1 and the ring into F1, osc 2 + 3 into F2).
 *
 * State: (chord-harp fork) the voices' hy_voice_t in hy_pool; a part keeps only the patch it compiled (hy_act). */

#define HY_NOSC 3u
#define HY_NSLOT 16u
#define HY_SUPER 7u

enum { HO_OFF, HO_SAW, HO_PULSE, HO_TRI, HO_SINE, HO_MORPH, HO_SUPER, HO_FM, HO_SYNC, HO_FOLD, HO_NOISE, HO_N };
enum { HF_OFF, HF_LP12, HF_LP24, HF_BP, HF_HP, HF_NOTCH, HF_PEAK, HF_MOOG, HF_N };   /* (MOOG: Clipper 0.2) */
enum { HR_SER, HR_PAR, HR_SPLIT, HR_N };
enum { HW_SINE, HW_TRI, HW_SAW, HW_SQR, HW_SH, HW_SMOOTH, HW_N };   /* the voices' LFO waves */
/* matrix sources: Q15, bipolar (LFOs, KEY, BEND, RAND) or unipolar (the rest) */
enum {
    HS_NONE, HS_ENV2, HS_ENV3, HS_AENV, HS_LFO1, HS_LFO2, HS_LFO3, HS_VEL, HS_KEY, HS_MW, HS_AT, HS_BEND,
    HS_EXPR, HS_RAND, HS_M1, HS_M2, HS_M3, HS_M4, HS_ONE, HS_N
};
/* matrix destinations */
enum {
    HD_NONE, HD_PITCH, HD_P1, HD_P2, HD_P3, HD_SH1, HD_SH2, HD_SH3, HD_LV1, HD_LV2, HD_LV3, HD_RING,
    HD_CUT, HD_CUT1, HD_CUT2, HD_RES1, HD_RES2, HD_DRIVE, HD_AMP, HD_LR2, HD_LR3, HD_FBAL, HD_N
};
#define HL_KEY 1u                /* LFO flags: restart at each note */
#define HL_SYNC 2u               /*            RATE in note values at the song tempo (HY_SYNC_DIV) */
#define HOF_RESET 1u             /* oscillator flags: phase 0 at each note (else free: phases kept / random) */
#define HOF_FIXED 2u             /*                   no keyboard tracking: the pitch of C4 on every key */

typedef struct {
    uint8_t type;
    int8_t coarse;               /* semitones -48..48 */
    int8_t fine;                 /* cents -64..63 */
    uint8_t level;               /* 0..127 */
    uint8_t shape;               /* 0..127 */
    uint8_t flags;               /* HOF_* */
} hy_osc_t;
typedef struct {
    uint8_t type;                /* HF_* */
    uint8_t cut;                 /* 0..127 (30 Hz .. 16 kHz) */
    uint8_t res;                 /* 0..127 */
    int8_t env;                  /* ENV2 -> cutoff, -64..63 */
    int8_t key;                  /* keyboard tracking, 64 = 100 % (an octave up opens it an octave) */
    uint8_t vel;                 /* velocity -> cutoff, 0..127 */
} hy_flt_t;
typedef struct {
    uint8_t wave, rate, flags, fade;   /* HW_*, 0..127 (or a HY_SYNC_DIV index), HL_*, fade-in 0..127 */
} hy_lfo_t;
typedef struct {
    uint8_t src, via, dst;       /* HS_*, HS_* (0 = none: the amount alone), HD_* */
    int8_t amt;                  /* -64..63 */
} hy_slot_t;
typedef struct {
    hy_osc_t osc[HY_NOSC];
    uint8_t ring;                /* osc 1 x osc 2, 0..127 */
    uint8_t drift;               /* analog drift of the pitch, 0..127 */
    uint8_t drive;               /* before the filters, 0..127 */
    uint8_t route;               /* HR_* */
    uint8_t fbal;                /* PAR: F1 0 .. 127 F2 */
    hy_flt_t flt[2];
    uint8_t env2[4], env3[4];    /* A D S R, the ADSR's scales (voice.c env_tick) */
    hy_lfo_t lfo[2];             /* LFO 2, LFO 3 (LFO 1 is the part's) */
    hy_slot_t mod[HY_NSLOT];
    uint8_t bend;                /* pitch bend range, semitones 0..24 */
    uint8_t level;               /* 0..127, 100 = unity */
    uint8_t rsv;                 /* 0 (room for later) */
    char name[10];
} hy_patch_t;
#define HY_PATCH_SIZE 128u
_Static_assert(sizeof(hy_patch_t) == HY_PATCH_SIZE, "hy_patch_t is the editor's byte layout");

typedef struct {
    uint32_t ph[HY_NOSC];
    uint32_t sph[HY_NOSC][HY_SUPER - 1u];   /* SUPER: the six detuned saws */
    int32_t fb[HY_NOSC];         /* last sample (FM feedback, osc 1's FM from osc 3) */
    int32_t nlp[HY_NOSC];        /* NOISE colour */
    int32_t f[2][4];             /* filters: two SVF stages each (ic1, ic2), or MOOG's four (Q4) */
    int32_t e[2];                /* ENV2, ENV3: Q24 */
    uint8_t est[2];              /* their stages: 0 idle, 1 attack, 2 decay / sustain, 3 release */
    uint8_t ftype[2];            /* the filters' types their states belong to (a change clears them) */
    int32_t lrm[2];              /* the matrix's LFO 2 / 3 RATE rows of the last tick (rate units) */
    uint32_t lph[2];             /* LFO 2, 3 */
    int32_t lval[2], lprev[2], lnext[2], lfade[2];
    int32_t rnd;                 /* RAND: one value per note, Q15 bipolar */
    int32_t drift, dtgt;         /* drift: 1/4096 semitone, and where it is going */
    uint32_t rs;                 /* (0.7) the voice's own random numbers while it renders (S&H, drift): rng() is one
                                  * for the whole firmware, and the second core renders some of the voices */
    int32_t nz;                  /* noise state */
} hy_voice_t;
#define HY_POOL 12u              /* (chord-harp fork) voices sounding at once, all parts (voice.c FULL_VOICES 12) */
#define HY_POLY 4u               /* (chord-harp fork) a part's voices (as JUNO's) */
static hy_voice_t hy_pool[HY_POOL] __attribute__((section(".pool")));   /* (zeroed at boot) */
static uint8_t hy_owner[HY_POOL] __attribute__((section(".pool")));     /* part + 1 (0 free) */
static uint8_t hy_vslot[HY_POOL] __attribute__((section(".pool")));     /* its voice */
static int16_t hy_bend[NPART] __attribute__((section(".pool")));        /* the MIDI bend, Q15 (midi_control.c) */
static struct {                                  /* the patch read once when it comes (hy_compile): */
    uint8_t seen;                                /* (chord-harp fork) PTCH compiled + 1 (0 = none yet) */
    uint8_t n, slot[HY_NSLOT];                   /* the matrix slots in use */
    uint32_t src;                                /* bit per source they read (HS_*): LFOs nobody reads stay still */
} hy_act[NPART] __attribute__((section(".pool")));

/* 2^32 CTL 96 / (60 FS): a constant (folded by the compiler) */
#define HY_SYNC_K ((uint32_t)(((uint64_t)CTL * 96u << 32) / (60u * (uint64_t)FS)))
/* tempo-synced LFO rates: the period in 1/96 of a beat (1/32 .. 8 bars) */
static const uint16_t HY_SYNC_DIV[] = {12, 16, 24, 32, 48, 64, 96, 128, 192, 288, 384, 576, 768, 1536, 3072};
#define HY_NSYNC NELEM(HY_SYNC_DIV)

#include "hybrid_patches.h"     /* HY_FACTORY[], HY_NFACTORY, N_HY_PATCH[], HY_PRESETS[] */
/* (chord-harp fork) part t's patch now: PTCH's, in the flash */
static const hy_patch_t *hy_of(const track_t *t)
{
    return &HY_FACTORY[(uint32_t)clamp(t->p[P_E3], 0, (int32_t)HY_NFACTORY - 1)];
}

/* ------------------------------------------------------------ helpers --- */
/* a state-variable filter that also gives the band and the high-pass (Simper; tsvf_coef's scaling) */
typedef struct { int32_t a1, a2, a3, k; } hsvf_t;
static inline void hsvf_coef(hsvf_t *c, int32_t cut, int32_t k)   /* cut 0..127 << 8; k damping, Q12 */
{
    int32_t i, g, den;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    den = 4096 + ((g * (g + k)) >> 12);
    c->a1 = (int32_t)((4096u << 13) / (uint32_t)den);
    c->a2 = (c->a1 * g) >> 12;
    c->a3 = (c->a2 * g) >> 12;
    c->k = k;
}
static inline int32_t hy_damp(int32_t res) { return 8192 - clamp(res, 0, 127) * 7600 / 127; }   /* 2.0 .. 0.145 */

/* one SVF stage over a buffer: one loop per output (the choice made once a block, not per sample). Inputs within
 * +-60000 (the mix is, by construction; hybrid_render bounds a filter's output going into another): with the states
 * clamped at +-150000 every product fits in 32 bits at any cutoff and resonance (checked over the whole table) */
/* Clipper 0.2: the states saturate softly above 96000 (a tanh knee towards 150000) instead of being cut there:
 * a screaming resonance rounds off like an analog one instead of clipping */
static inline int32_t hsvf_sat(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a <= 96000)
        return x;
    a = 96000 + ((softclip(((a - 96000) * 155) >> 8) * 54000) >> 15);   /* tanh((a - 96000) / 54000) */
    return x < 0 ? -a : a;
}
#define HSVF_LOOP(OUT)                                                                                         \
    for (i = 0; i < n; i++) {                                                                                  \
        int32_t in = x[i], v3 = in - ic2;                                                                      \
        int32_t v1 = (a1 * ic1 + a2 * v3) >> 13;                                                               \
        int32_t v2 = ic2 + ((a2 * ic1 + a3 * v3) >> 13);                                                       \
        ic1 = hsvf_sat(2 * v1 - ic1);                                                                          \
        ic2 = hsvf_sat(2 * v2 - ic2);                                                                          \
        x[i] = (OUT);                                                                                          \
    }
static void hsvf_run(const hsvf_t *c, int32_t *x, uint32_t n, int32_t *st, uint32_t mode)   /* 0 LP .. 4 PEAK */
{
    int32_t ic1 = st[0], ic2 = st[1], a1 = c->a1, a2 = c->a2, a3 = c->a3, k = c->k;
    uint32_t i;
    switch (mode) {
    case 0: HSVF_LOOP(v2) break;
    case 1: HSVF_LOOP(v1) break;
    case 2: HSVF_LOOP(in - ((k * v1) >> 12) - v2) break;
    case 3: HSVF_LOOP(in - ((k * v1) >> 12)) break;
    default: HSVF_LOOP(2 * v2 - in + ((k * v1) >> 12)) break;             /* LP - HP */
    }
    st[0] = ic1;
    st[1] = ic2;
}

/* MOOG: the ladder (dsp.c ladder_tick) over a buffer, its four stages in st[0..3] */
static void hy_ladder(const ladder_t *c, int32_t *x, uint32_t n, int32_t *st)
{
    int32_t s[4] = {st[0], st[1], st[2], st[3]};
    uint32_t i;
    for (i = 0; i < n; i++)
        x[i] = ladder_tick(c, x[i], s);
    st[0] = s[0], st[1] = s[1], st[2] = s[2], st[3] = s[3];
}

/* a filter of the patch over a buffer (type HF_*, coefficients made for this block) */
static void hy_filter(uint32_t type, const hsvf_t *c, const hsvf_t *c2, const ladder_t *lc, int32_t *x, uint32_t n,
                      int32_t *st)
{
    switch (type) {
    case HF_MOOG: hy_ladder(lc, x, n, st); break;
    case HF_LP12: hsvf_run(c, x, n, st, 0); break;
    case HF_LP24:
        hsvf_run(c2, x, n, st, 0);                   /* (the first stage flat, the resonance on the second) */
        hsvf_run(c, x, n, st + 2, 0);
        break;
    case HF_BP: hsvf_run(c, x, n, st, 1); break;
    case HF_HP: hsvf_run(c, x, n, st, 2); break;
    case HF_NOTCH: hsvf_run(c, x, n, st, 3); break;
    case HF_PEAK: hsvf_run(c, x, n, st, 4); break;
    default: break;
    }
}

/* (chord-harp fork) hy_inc: engines.c's (Clipper's, taken for the other engines first) */

/* ENV2 / ENV3: the ADSR of voice.c env_tick on its own value, gated by the key; Q15 out */
static int32_t hy_env(int32_t *e, uint8_t *st, const uint8_t *adsr, int gate)
{
    int32_t sus = (int32_t)(adsr[2] & 127u) << 17;
    if (!gate && *st && *st != 3u)
        *st = 3;
    switch (*st) {
    case 1:
        *e += (int32_t)ENV_LIN[adsr[0] & 127u];
        if (*e >= (1 << 24)) {
            *e = 1 << 24;
            *st = 2;
        }
        break;
    case 2:
        *e += mulq16(sus - *e, ENV_EXP[adsr[1] & 127u]);
        break;
    case 3:
        *e -= mulq16(*e, ENV_EXP[adsr[3] & 127u]);
        if (*e < (1 << 12)) {
            *e = 0;
            *st = 0;
        }
        break;
    default:
        *e = 0;
        break;
    }
    return *e >> 9;
}

/* a voice LFO's value now (Q15 bipolar); advances it one control tick */
static uint32_t hy_vr(hy_voice_t *hv)               /* (0.7) the voice's own LCG (rs), seeded at its note */
{
    hv->rs = hv->rs * 1664525u + 1013904223u;
    return hv->rs;
}
static int32_t hy_lfo(const hy_lfo_t *L, hy_voice_t *hv, uint32_t k, int32_t rate_mod)
{
    uint32_t inc, old = hv->lph[k];
    int32_t r = clamp((int32_t)L->rate + rate_mod, 0, 127), y;
    if (L->flags & HL_SYNC) {                        /* a period of HY_SYNC_DIV / 96 beats at the song tempo */
        uint32_t d = HY_SYNC_DIV[(uint32_t)r * HY_NSYNC / 128u];
        /* 2^32 per period, CTL samples a tick: inc = 2^32 CTL 96 BPM / (60 FS d) = HY_SYNC_K BPM / d, in 32 bits
         * (HY_SYNC_K BPM < 1.2e9; the FM-1 has no 64-bit divide: no __udivdi3) */
        inc = HY_SYNC_K * (uint32_t)song.g[G_BPM] / d;
    } else {
        inc = LFO_INC[r];
    }
    hv->lph[k] += inc;
    if (hv->lph[k] < old) {                          /* a new cycle: the next random value */
        hv->lprev[k] = hv->lnext[k];
        hv->lnext[k] = (int32_t)(hy_vr(hv) >> 16) - 32768;
    }
    switch (L->wave) {
    case HW_TRI: y = osc_tri(hv->lph[k]); break;
    case HW_SAW: y = (int32_t)(hv->lph[k] >> 16) - 32768; break;
    case HW_SQR: y = hv->lph[k] < 0x80000000u ? 32767 : -32767; break;
    case HW_SH: y = hv->lnext[k]; break;
    case HW_SMOOTH: y = hv->lprev[k] + mulq15(hv->lnext[k] - hv->lprev[k], (int32_t)(hv->lph[k] >> 17)); break;
    default: y = osc_sine(hv->lph[k]); break;
    }
    if (hv->lfade[k] < 32767) {
        int32_t s = (int32_t)(ENV_LIN[L->fade & 127u] >> 9);
        hv->lfade[k] = L->fade ? clamp(hv->lfade[k] + (s ? s : 1), 0, 32767) : 32767;
    }
    return mulq15(y, hv->lfade[k]);
}

/* ------------------------------------------------------- oscillators --- */
/* the 90s supersaw (after Adam Szabo's analysis of the JP-8000): seven saws, their offsets at full detune
 * (Q16 of the frequency), the detune curve and the side saws' level against the centre's */
static const int32_t SUPER_OFF[HY_SUPER - 1u] = {-7210, -4121, -1279, 1305, 4074, 7042};
static int32_t super_detune(uint32_t shape)       /* SHP 0..127 -> 0..65536, finer at the low end */
{
    int32_t x = (int32_t)shape * 516;                /* 0..65532 */
    return (int32_t)(((int64_t)x * x >> 16) * 3 + x) >> 2;
}

/* (chord-harp fork) hy_saw: dsp.c's (Clipper's, as here) */

/* Clipper 0.2: how far an index (FM, feedback, fold) may go at this pitch before its partials pass 20 kHz and fold
 * back: (20 kHz / f0 - 1) x k (k: the index units per unit of bandwidth / f0). One 32-bit divide a block. */
#define HY_FM_K 10                /* FM: 127 = ~1.98 cycles = 12.4 rad; Carson: (beta + 1) f0 < 20 kHz */
#define HY_FB_K 90                /* SINE feedback: g 4064 = ~0.5 cycle; tuned on tests/alias_test.c (2.4 kHz) */
static int32_t hy_index_max(uint32_t inc, int32_t k)
{
    int32_t r = (int32_t)(1948000000u / (inc >> 8));             /* 20 kHz / f0, x 256 */
    return r > 256 ? ((r - 256) * k) >> 8 : 0;
}

/* osc k of the voice into y[n] (Q15); prev: the buffer of the oscillator before (FM), m1: osc 1's start
 * phase and increment (SYNC) */
static void hy_osc_run(const hy_osc_t *o, hy_voice_t *hv, uint32_t k, uint32_t inc, int32_t shape, int32_t *y,
                       uint32_t n, const int32_t *prev, uint32_t m1ph, uint32_t m1inc)
{
    uint32_t ph = hv->ph[k], i;
    int32_t sh = clamp(shape, 0, 127 << 8);          /* 0..127 << 8 */
    switch (o->type) {
    case HO_SAW:
        for (i = 0; i < n; i++, ph += inc)
            y[i] = hy_saw(ph, inc);
        break;
    case HO_PULSE: {
        uint32_t pw = 0x80000000u + (uint32_t)sh * 0x7000u;          /* 50 % .. ~6 % */
        for (i = 0; i < n; i++, ph += inc)
            y[i] = (hy_saw(ph, inc) - hy_saw(ph + pw, inc)) >> 1;
        break;
    }
    case HO_TRI:                                     /* (band-limited: polyBLAMP at the corners, dsp.c) */
        for (i = 0; i < n; i++, ph += inc)
            y[i] = osc_tri_bl(ph, inc);
        break;
    case HO_SINE: {                                  /* self feedback: the mean of the last two outputs */
        int32_t fb = hv->fb[k], last = hv->nlp[k], g = sh >> 3;          /* 0..4064 */
        if (inc >= 65536u)                           /* (Clipper 0.2: held on the high notes, see hy_index_max) */
            g = clamp(g, 0, hy_index_max(inc, HY_FB_K));
        for (i = 0; i < n; i++, ph += inc) {
            int32_t s = osc_sine(ph + ((uint32_t)(((fb + last) >> 1) * g) << 4));
            last = fb;
            fb = s;
            y[i] = s;
        }
        hv->fb[k] = fb;
        hv->nlp[k] = last;
        break;
    }
    case HO_MORPH: {                                 /* sine -> tri -> saw -> square, crossfaded */
        int32_t seg = sh / (43 << 8), x = ((sh - seg * (43 << 8)) * 762) >> 8;   /* x Q15 within the segment */
        if (seg > 2) {
            seg = 2;
            x = 32767;
        }
        if (seg == 0) {
            for (i = 0; i < n; i++, ph += inc) {
                int32_t a = osc_sine(ph);
                y[i] = a + mulq15(osc_tri_bl(ph, inc) - a, x);
            }
        } else if (seg == 1) {
            for (i = 0; i < n; i++, ph += inc) {
                int32_t a = osc_tri_bl(ph, inc);
                y[i] = a + mulq15(hy_saw(ph, inc) - a, x);
            }
        } else {
            for (i = 0; i < n; i++, ph += inc) {
                int32_t a = hy_saw(ph, inc);
                y[i] = a + mulq15(((a - hy_saw(ph + 0x80000000u, inc)) >> 1) - a, x);
            }
        }
        break;
    }
    case HO_SUPER: {
        int32_t d = super_detune((uint32_t)sh >> 8), dq = d >> 1;          /* dq: 0..32768 */
        /* the centre saw's level falls and the side saws' rises with the detune (Szabo's fits, simplified) */
        int32_t cg = 32767 - mulq15(dq, 18000), sg = 2000 + mulq15(dq, 16000);
        uint32_t sinc[HY_SUPER - 1u], j;
        for (j = 0; j < HY_SUPER - 1u; j++)
            sinc[j] = inc + (uint32_t)(int32_t)(((int64_t)(int32_t)(inc >> 4) * mulq16(SUPER_OFF[j], (uint32_t)d)) >> 12);
        int32_t acc[CTL];                            /* (the side saws one at a time: their phase stays in a register;
                                                      * 0.7: on the stack, the voices render on two cores) */
        for (i = 0; i < n; i++)
            acc[i] = 0;
        for (j = 0; j < HY_SUPER - 1u; j++) {
            uint32_t q = hv->sph[k][j], qi = sinc[j];
            for (i = 0; i < n; i++, q += qi)
                acc[i] += hy_saw(q, qi);
            hv->sph[k][j] = q;
        }
        for (i = 0; i < n; i++, ph += inc)           /* (six saws at sg / 2 each: the sum stays in range) */
            y[i] = mulq15(hy_saw(ph, inc), cg) + mulq15(acc[i] >> 1, sg);
        break;
    }
    case HO_FM: {                                    /* phase modulation by the buffer before, up to ~2 cycles */
        int32_t g = sh >> 8;
        if (inc >= 65536u)                           /* (Clipper 0.2: Carson's bandwidth kept under 20 kHz) */
            g = clamp(g, 0, hy_index_max(inc, HY_FM_K));
        for (i = 0; i < n; i++, ph += inc)
            y[i] = osc_sine(ph + ((uint32_t)(prev[i] * g) << 11));
        break;
    }
    case HO_SYNC:
        if (k == 0) {                                /* (osc 1 has no master: a saw) */
            for (i = 0; i < n; i++, ph += inc)
                y[i] = hy_saw(ph, inc);
            break;
        }
        for (i = 0; i < n; i++) {
            uint32_t was = m1ph;
            m1ph += m1inc;
            if (m1ph < was && m1inc >= 4096u) {      /* the master wrapped in this sample: restart, by the fraction
                                                      * (in 32 bits: the master's overshoot / its increment, Q12) */
                /* Clipper 0.2: band-limited. The saw falls by h (its value then) at t samples before this one: the
                 * polyBLEP residual of that step, scaled by h, on this sample ((1 - t)^2 / 2) and the one before
                 * (t^2 / 2; the first sample of a block cannot reach back: one reset in 32 there) */
                uint32_t q12 = m1ph / (m1inc >> 12), at = ph - (inc >> 12) * q12;
                int32_t h = (int32_t)(at >> 16), tq = (int32_t)(q12 << 3), a = 32768 - tq;
                ph = (inc >> 12) * q12;
                y[i] = (int32_t)(ph >> 16) - 32768 + ((((a * a) >> 16) * h) >> 15);
                if (i)
                    y[i - 1] -= (((tq * tq) >> 16) * h) >> 15;
            } else {
                y[i] = hy_saw(ph, inc);
            }
            ph += inc;
        }
        break;
    case HO_FOLD: {                                  /* sin(pi/2 g sin x), g 1 .. 8 */
        int32_t g = 256 + ((sh * 7) >> 7);           /* Q8 */
        /* Clipper 0.2: its partials reach about (pi / 2 g + 2) f0: g is held where they would pass 20 kHz (the
         * fold opens up less on the high notes, as nothing folds back into the band) */
        if (inc >= 65536u) {
            int32_t b = (int32_t)(1948000000u / (inc >> 8)) - 512;    /* (pi / 2 g)max x 256: 20 kHz / f0 - 2 */
            int32_t gl = (b * 163) >> 8;                               /* x 2 / pi: g Q8 */
            g = clamp(g, 256, gl > 256 ? gl : 256);
        }
        for (i = 0; i < n; i++, ph += inc)
            y[i] = osc_sine((uint32_t)((osc_sine(ph) * g) >> 8) << 15);
        break;
    }
    case HO_NOISE: {
        int32_t st = hv->nz, lp = hv->nlp[k], c = 32767 - ((sh >> 8) * 250);
        for (i = 0; i < n; i++) {
            int32_t w = (int32_t)(noise32(&st) >> 17) - 16384;
            lp += mulq15((w << 1) - lp, c);
            y[i] = lp;
        }
        hv->nz = st;
        hv->nlp[k] = lp;
        break;
    }
    default:
        for (i = 0; i < n; i++)
            y[i] = 0;
        break;
    }
    hv->ph[k] = ph;
    if (o->type != HO_SINE)
        hv->fb[k] = y[n - 1u];
}

/* ------------------------------------------------------------ patch --- */
/* audio ISR: the slots of part p's patch that do something, and the sources they read */
static void hy_compile(uint32_t p, const hy_patch_t *P)
{
    uint32_t k, n = 0, src = 0;
    for (k = 0; k < HY_NSLOT; k++) {
        const hy_slot_t *s = &P->mod[k];
        if (!s->src || !s->dst || !s->amt)
            continue;
        hy_act[p].slot[n++] = (uint8_t)k;
        src |= 1u << s->src | 1u << s->via;
    }
    hy_act[p].n = (uint8_t)n;
    hy_act[p].src = src;
}

/* audio ISR, once per block and part (before its voices, on the core that renders them): PTCH's patch compiled */
static void hy_block(track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), slot;
    if (p >= NPART)
        return;
    slot = (uint32_t)(hy_of(t) - HY_FACTORY);
    if (hy_act[p].seen != slot + 1u) {
        hy_compile(p, &HY_FACTORY[slot]);
        hy_act[p].seen = (uint8_t)(slot + 1u);
    }
}

/* (chord-harp fork) the voice's pool entry, or 0 (none, or taken by another voice since) */
static hy_voice_t *hy_voice(const track_t *t, const voice_t *v)
{
    uint32_t k = (uint32_t)v->s[0];
    if (t < &trk[0] || t >= &trk[NPART] || !k || k > HY_POOL)
        return 0;
    k--;
    return hy_owner[k] == (uint8_t)(t - trk + 1) && hy_vslot[k] == (uint8_t)(v - t->v) ? &hy_pool[k] : 0;
}
static int hy_busy(uint32_t k)                       /* its voice still sounds (and is still its, on HYBRID) */
{
    const track_t *t;
    const voice_t *v;
    if (!hy_owner[k])
        return 0;
    t = &trk[(hy_owner[k] - 1u) % NPART];
    v = &t->v[hy_vslot[k] % NVOICE];
    return v->active && (uint32_t)v->s[0] == k + 1u && t->engine == ENGI_HYBRID;
}
static hy_voice_t *hy_take(track_t *t, voice_t *v)   /* a free entry, else one by the voice's age (seldom) */
{
    uint32_t k;
    for (k = 0; k < HY_POOL && hy_busy(k); k++)
        ;
    if (k == HY_POOL)
        k = (v->age * 7u) % HY_POOL;
    hy_owner[k] = (uint8_t)(t - trk + 1);
    hy_vslot[k] = (uint8_t)(v - t->v);
    v->s[0] = (int32_t)k + 1;
    memset(&hy_pool[k], 0, sizeof hy_pool[k]);
    return &hy_pool[k];
}

static void hybrid_note_on(track_t *t, voice_t *v)
{
    const hy_patch_t *P = hy_of(t);
    hy_voice_t *hv = hy_voice(t, v);
    uint32_t k, j;
    int fresh = !v->env && !v->env_out;
    if (t < &trk[0] || t >= &trk[NPART])
        return;
    if (!hv) {                                       /* (chord-harp fork) its pool entry: a new one starts afresh */
        hv = hy_take(t, v);
        fresh = 1;
    }
    if (fresh) {                                     /* a voice from silence: filters clear, envelopes from 0 */
        for (k = 0; k < 2u; k++) {
            for (j = 0; j < 4u; j++)
                hv->f[k][j] = 0;
            hv->e[k] = 0;
        }
        if (!hv->nz)
            hv->nz = 0x2468ACE + (int32_t)v->age;
        for (k = 0; k < HY_NOSC; k++) {              /* free phases: somewhere else each note (no phasing) */
            hv->ph[k] = rng();
            for (j = 0; j < HY_SUPER - 1u; j++)
                hv->sph[k][j] = rng();
            hv->fb[k] = hv->nlp[k] = 0;
        }
        hv->drift = hv->dtgt = 0;
    }
    for (k = 0; k < HY_NOSC; k++)
        if (P->osc[k].flags & HOF_RESET)
            hv->ph[k] = 0;
    hv->est[0] = hv->est[1] = 1;                     /* the envelopes attack from where they are */
    for (k = 0; k < 2u; k++)
        if ((P->lfo[k].flags & HL_KEY) || fresh) {
            hv->lph[k] = (P->lfo[k].flags & HL_KEY) ? 0 : rng();
            hv->lfade[k] = 0;
            if (fresh)
                hv->lprev[k] = hv->lnext[k] = (int32_t)(rng() >> 16) - 32768;
        }
    hv->rnd = (int32_t)(rng() >> 16) - 32768;
    hv->rs = (uint32_t)hv->rnd * 2654435761u ^ hv->ph[0] ^ (uint32_t)v->note << 24;   /* (0.7: no rng() more) */
}

/* the source values of the matrix for this voice and tick (Q15) */
static void hy_sources(const track_t *t, const voice_t *v, const hy_voice_t *hv, const vmod_t *m, int32_t e2,
                       int32_t e3, int32_t l2, int32_t l3, int32_t *s)
{
    s[HS_NONE] = 0;
    s[HS_ENV2] = e2;
    s[HS_ENV3] = e3;
    s[HS_AENV] = m->envq15;
    s[HS_LFO1] = mulq15(t->lfo_val, t->lfo_fade);
    s[HS_LFO2] = l2;
    s[HS_LFO3] = l3;
    s[HS_VEL] = v->vel * 258;
    s[HS_KEY] = clamp(((int32_t)v->note - 60) * 546, -32768, 32767);
    s[HS_MW] = t->mw * 258;                          /* (chord-harp fork: the track's controllers; no poly AT) */
    s[HS_AT] = t->at * 258;
    s[HS_BEND] = hy_bend[(uint32_t)(t - trk) % NPART];
    s[HS_EXPR] = (127 - t->ex_off) * 258;
    s[HS_RAND] = hv->rnd;
    s[HS_M1] = t->p[P_E4] * 258;
    s[HS_M2] = t->p[P_E5] * 258;
    s[HS_M3] = t->p[P_E6] * 258;
    s[HS_M4] = t->p[P_E7] * 258;
    s[HS_ONE] = 32767;
}

static void hybrid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t part = (uint32_t)(t - trk) % NPART, k, i;
    const hy_patch_t *P = &HY_FACTORY[(hy_act[part].seen ? hy_act[part].seen - 1u : 0u) % HY_NFACTORY];   /* (hy_block's) */
    hy_voice_t *hv = hy_voice(t, v);
    int32_t src[HS_N], md[HD_N], e2, e3, l2, l3;
    static struct {                                  /* (static: the audio ISR's stack stays small; one set a core,
                                                      * core.h MC_CPU; chord-harp fork: in .pool) */
        int32_t buf[HY_NOSC][CTL], a[CTL], b[CTL];
    } hy_scr[2] __attribute__((section(".pool")));
    int32_t (*buf)[CTL] = hy_scr[MC_CPU() & 1u].buf, *a = hy_scr[MC_CPU() & 1u].a, *b = hy_scr[MC_CPU() & 1u].b;
    uint32_t inc[HY_NOSC];
    hsvf_t fc[2], fc2[2];
    ladder_t lc[2];
    int32_t lv[HY_NOSC], gain, ring, drive, dw, bal;
    if (!hv || n > CTL)
        return;
    for (k = 0; k < HD_N; k++)
        md[k] = 0;
    /* the envelopes and LFOs (the LFO rates first: their own matrix rows, from last tick's values) */
    e2 = hy_env(&hv->e[0], &hv->est[0], P->env2, v->gate);
    e3 = hy_env(&hv->e[1], &hv->est[1], P->env3, v->gate);
    l2 = (hy_act[part].src >> HS_LFO2) & 1u ? hy_lfo(&P->lfo[0], hv, 0, hv->lrm[0]) : 0;
    l3 = (hy_act[part].src >> HS_LFO3) & 1u ? hy_lfo(&P->lfo[1], hv, 1, hv->lrm[1]) : 0;
    hy_sources(t, v, hv, m, e2, e3, l2, l3, src);
    /* the matrix: amount x source (x via); pitch rows on a square curve (fine vibrato, wide sweeps) */
    for (k = 0; k < hy_act[part].n; k++) {
        const hy_slot_t *s = &P->mod[hy_act[part].slot[k]];
        int32_t x, am;
        am = s->amt * 520;                           /* -33280..32760 */
        if (s->dst <= HD_P3)
            am = (s->amt * (s->amt < 0 ? -s->amt : s->amt)) * 8;    /* +-32768 at 64: quadratic */
        x = mulq15(src[s->src], clamp(am, -32768, 32767));
        if (s->via)
            x = mulq15(x, src[s->via]);
        md[s->dst] += x;
    }
    hv->lrm[0] = (md[HD_LR2] * 127) >> 15;
    hv->lrm[1] = (md[HD_LR3] * 127) >> 15;
    /* drift: a slow random walk of the pitch, per voice (up to ~+-25 cents at 127) */
    if (P->drift) {
        if (!(hy_vr(hv) & 63u))
            hv->dtgt = (int32_t)((hy_vr(hv) >> 16) & 0xFFFFu) - 32768;
        hv->drift += (hv->dtgt - hv->drift) >> 6;
    } else {
        hv->drift = 0;
    }
    /* the oscillators: pitch (coarse, fine, the matrix, the bend, the drift), shape and level for this tick */
    {
        int32_t pall = md[HD_PITCH] * 3 + ((hv->drift * (int32_t)P->drift) >> 12);   /* 1/4096 semitone (the
                                                                                         * bend: voice.c) */
        for (k = 0; k < HY_NOSC; k++) {
            const hy_osc_t *o = &P->osc[k];
            int32_t off = pall + md[HD_P1 + k] * 3 + o->coarse * 4096 + o->fine * 41;   /* (a cent ~41/4096) */
            int32_t p16 = (o->flags & HOF_FIXED) ? 60 * 16 : m->pitch16;
            if (k && P->drift)                       /* each oscillator drifts a little apart */
                off += (hv->drift * (int32_t)P->drift * (int32_t)(k == 1u ? 3 : -2)) >> 16;
            if (o->type == HO_SYNC && k)             /* SYNC: SHP raises the slave over its master */
                off += ((int32_t)o->shape * 48 * 4096) / 127 + ((md[HD_SH1 + k] * 48) >> 3);
            inc[k] = hy_inc(p16, m->fine, off);
            lv[k] = clamp((int32_t)o->level * 129 + md[HD_LV1 + k], 0, 16384);          /* level 127 = 1/2 */
        }
    }
    ring = clamp((int32_t)P->ring * 258 + md[HD_RING], 0, 32767);
    /* render the oscillators: 1, then 2 (FM from 1, SYNC to 1), then 3 (FM from 2); a silent one is not
     * rendered (its phase runs on) unless the ring or the FM of the next one reads it */
    {
        uint32_t ph1 = hv->ph[0];
        int32_t f3[CTL];                             /* (0.7: on the stack, the voices render on two cores) */
        for (k = 0; k < HY_NOSC; k++) {
            const hy_osc_t *o = &P->osc[k];
            int32_t shp = ((int32_t)o->shape << 8) + ((md[HD_SH1 + k] * 127) >> 7);
            const int32_t *prev = k ? buf[k - 1u] : 0;
            if (!o->type || (!lv[k] && !(k < 2u && ring) && !(k + 1u < HY_NOSC && P->osc[k + 1u].type == HO_FM))) {
                if (o->type) {                       /* silent: the phase still runs (no jump when it comes back) */
                    hv->ph[k] += inc[k] * n;
                    lv[k] = 0;
                } else if (k + 1u < HY_NOSC && P->osc[k + 1u].type == HO_FM) {
                    for (i = 0; i < n; i++)          /* (OFF before an FM: it modulates by nothing) */
                        buf[k][i] = 0;
                }
                continue;
            }
            if (!k && o->type == HO_FM) {            /* osc 1's FM: from osc 3's last sample, as a constant offset */
                for (i = 0; i < n; i++)
                    f3[i] = hv->fb[2];
                prev = f3;
            }
            hy_osc_run(o, hv, k, inc[k], shp, buf[k], n, prev, ph1, inc[0]);
        }
    }
    /* the mix, at half scale (room for the resonance): each oscillator at its level into A (SPLIT: osc 2 and 3
     * into B), the ring into A; then the drive */
    drive = clamp((int32_t)P->drive + (md[HD_DRIVE] * 127 >> 15), 0, 127);
    dw = drive * 258;
    {
        uint32_t na = 0, nb = 0;
        for (k = 0; k < HY_NOSC; k++) {
            int32_t g = lv[k] >> 1, *d;
            if (!P->osc[k].type || !g)
                continue;
            d = P->route == HR_SPLIT && k ? b : a;
            if (d == a ? !na++ : !nb++)
                for (i = 0; i < n; i++)
                    d[i] = mulq15(buf[k][i], g);
            else
                for (i = 0; i < n; i++)
                    d[i] += mulq15(buf[k][i], g);
        }
        if (ring && P->osc[0].type && P->osc[1].type) {
            int32_t g = ring >> 2;                   /* (the inputs bounded: a SUPER reaches ~2x full scale) */
            if (!na++)
                for (i = 0; i < n; i++)
                    a[i] = mulq15(mulq15(clamp(buf[0][i], -32767, 32767), clamp(buf[1][i], -32767, 32767)), g);
            else
                for (i = 0; i < n; i++)
                    a[i] += mulq15(mulq15(clamp(buf[0][i], -32767, 32767), clamp(buf[1][i], -32767, 32767)), g);
        }
        if (!na)
            for (i = 0; i < n; i++)
                a[i] = 0;
        if (P->route == HR_SPLIT && !nb)
            for (i = 0; i < n; i++)
                b[i] = 0;
    }
    if (drive) {                                     /* (the full-scale signal is 2 a: its drive, halved) */
        int32_t dg = 32768 + drive * 1536;           /* 1x .. 7x */
        for (i = 0; i < n; i++)
            a[i] += mulq15((softclip(((a[i] >> 2) * (dg >> 2)) >> 10) >> 1) - a[i], dw);
        if (P->route == HR_SPLIT)
            for (i = 0; i < n; i++)
                b[i] += mulq15((softclip(((b[i] >> 2) * (dg >> 2)) >> 10) >> 1) - b[i], dw);
    }
    /* the filters: cutoff = the patch + the macro + ENV2 + keyboard + velocity + the part (voice.c) + the matrix */
    for (k = 0; k < 2u; k++) {
        const hy_flt_t *f = &P->flt[k];
        int32_t cut, res, env = f->env + t->p[P_E2];
        if (!f->type)
            continue;
        if (hv->ftype[k] != f->type) {               /* another type: its states mean something else (scale too) */
            hv->ftype[k] = f->type;
            hv->f[k][0] = hv->f[k][1] = hv->f[k][2] = hv->f[k][3] = 0;
        }
        cut = ((int32_t)f->cut << 8) + (t->p[P_E0] << 8) + ((e2 * env) >> 6) +
              (((m->pitch16 - 60 * 16) * f->key * 75) >> 8) + ((v->vel * (int32_t)f->vel) >> 1) + m->cutoff +
              ((md[HD_CUT] + md[k ? HD_CUT2 : HD_CUT1]) * 127 >> 7);
        res = (int32_t)f->res + t->p[P_E1] + ((md[k ? HD_RES2 : HD_RES1] * 127) >> 15);
        if (f->type == HF_MOOG) {
            ladder_coef(&lc[k], cut, clamp(res, 0, 127));
            continue;
        }
        hsvf_coef(&fc[k], cut, hy_damp(res));
        if (f->type == HF_LP24)
            hsvf_coef(&fc2[k], cut, 5793);           /* (the first stage: Butterworth, k = sqrt 2) */
    }
    bal = clamp((int32_t)P->fbal * 258 + md[HD_FBAL], 0, 32767);
    if (P->route == HR_SER) {
        hy_filter(P->flt[0].type, &fc[0], &fc2[0], &lc[0], a, n, hv->f[0]);
        if (P->flt[1].type) {
            if (P->flt[0].type)                      /* F1's output into F2: bounded (a resonant PEAK reaches ~5x; every
                                                      * SVF product stays in 32 bits with inputs within +-60000) */
                for (i = 0; i < n; i++)
                    a[i] = clamp(a[i], -60000, 60000);
            hy_filter(P->flt[1].type, &fc[1], &fc2[1], &lc[1], a, n, hv->f[1]);
        }
    } else {
        if (P->route == HR_PAR)
            for (i = 0; i < n; i++)
                b[i] = a[i];
        hy_filter(P->flt[0].type, &fc[0], &fc2[0], &lc[0], a, n, hv->f[0]);
        hy_filter(P->flt[1].type, &fc[1], &fc2[1], &lc[1], b, n, hv->f[1]);
        if (P->route == HR_PAR)                      /* (resonant outputs reach ~+-300000: bounded, then the balance
                                                      * at a quarter scale, every product in 32 bits) */
            for (i = 0; i < n; i++) {
                int32_t x = clamp(a[i], -65535, 65535), y = clamp(b[i], -65535, 65535);
                a[i] = x + (mulq15((y - x) >> 2, bal) << 2);
            }
        else
            for (i = 0; i < n; i++)
                a[i] += b[i];
    }
    /* the amplitude: the patch level, the matrix's AMP (it only lowers: tremolo, a VCA on a source), the
     * voice's ramp (vmod_t amp0 -> amp1 over the block) and VOICE_FS in one factor a sample; the soft knee
     * above half scale (only resonance peaks get there). out += 4 knee(a) gain amp VOICE_FS (as ANALOG's 2 x) */
    gain = mulq15(clamp((int32_t)P->level * 328, 0, 41600), clamp(32767 + md[HD_AMP], 0, 32767));
    {
        int32_t h0 = mulq15(mulq15(m->amp0, gain), VOICE_FS), h1 = mulq15(mulq15(m->amp1, gain), VOICE_FS);
        int32_t hq = h0 << CTL_LOG2, dh = h1 - h0;
        for (i = 0; i < n; i++, hq += dh) {
            int32_t y = a[i];
            if (y > 16000 || y < -16000)
                y = soft_knee(y, 16000);
            out[i] += (y * (hq >> CTL_LOG2)) >> 13;
        }
    }
}

/* EDIT */
/* (after Clipper 1.2) UNISON (voice.c unison_spread): the oscillators set to phase 0 at each note (RESET) moved
 * by r (Q32) of a turn of the note, each by its share (its interval; FIXED: C4's): the others' phases are random
 * each note already */
static void hybrid_spread(track_t *t, voice_t *v, uint32_t r)
{
    const hy_patch_t *P = hy_of(t);
    hy_voice_t *hv = hy_voice(t, v);
    uint32_t k, b;
    if (!hv)
        return;
    b = hy_inc(v->pitch16, 0, 0);
    for (k = 0; k < HY_NOSC; k++)
        if ((P->osc[k].flags & HOF_RESET) && b) {
            const hy_osc_t *o = &P->osc[k];
            uint32_t ik = hy_inc((o->flags & HOF_FIXED) ? 60 * 16 : v->pitch16, 0, o->coarse * 4096 + o->fine * 41);
            hv->ph[k] += (uint32_t)(((uint64_t)r * ik) / b);
        }
}

/* (chord-harp fork) each preset's type (ui.c PTYPE: Clipper's banks, its bells BELL) and Clipper's level trim
 * (preset_trim_clipper.h, engine 12) */
static const char HY_PT[] = "PPSBLEKBBLRPUXUBLPPBLUENBLPLPBEL"
                          "RSLKKLPKKOWEPLUBPVRX";
_Static_assert(sizeof HY_PT == NELEM(HY_PRESETS) + 1u, "a type a preset");
static const int8_t HY_TRIM[] = {-2, -14, 2, 1, -1, -7, -13, 3, -1, 6, 4, 14, 8, 2, 8, 5,
                                 6, -7, -1, -2, 11, 15, -7, 3, 8, 2, -7, 0, -13, -8, -6, 3,
                                 1, -3, 2, 1, 7, 0, -2, 2, 7, -3, -2, 0, -1, 0, 9, 1, -4, -2, 4, 16};
_Static_assert(sizeof HY_TRIM == NELEM(HY_PRESETS), "a trim a preset");
_Static_assert(NELEM(HY_PRESETS) <= 128u, "HYBRID's favourites");
_Static_assert(HY_NFACTORY <= 255u, "hy_act.seen");

static const engine_t ENG_HYBRID = {
    .name = "HYBRID",
    .page_title = {"FILT", "MACRO"},
    .edit = {
        {"CUT", F_BIPCT, -64, 63, 0, 0, 0},
        {"RES", F_BIPCT, -64, 63, 0, 0, 0},
        {"FENV", F_BIPCT, -64, 63, 0, 0, 0},
        {"PTCH", F_ENUM, 0, HY_NFACTORY - 1, 0, N_HY_PATCH, 0},
        {"M1", F_PCT, 0, 127, 0, 0, 0},
        {"M2", F_PCT, 0, 127, 0, 0, 0},
        {"M3", F_PCT, 0, 127, 0, 0, 0},
        {"M4", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = HY_PRESETS,
    .npresets = NELEM(HY_PRESETS),
    .ptype = HY_PT,
    .trim = HY_TRIM,
    .note_on = hybrid_note_on,
    .render = hybrid_render,
    .knob = {P_E0, P_E1, P_E4, P_E5},
    .poly = HY_POLY,
    .keep = 0x01,                                /* s[0]: its pool entry */
    .block = hy_block,
};
