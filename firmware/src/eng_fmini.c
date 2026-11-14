/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D
 * Derived from FMini-1's eng_fmini.c (ruppert/fmini-1, Copyright (C) 2026 Guilherme Ruppert, GPL-3.0-only; a fork of Felucca) */
/* FMINI (2026-11-11): FMini-1's Minimoog engine, ported. Its sound is its filter: a 24 dB transistor ladder calibrated to
 * the Minimoog (tables from FMini-1's tools/fmini_filter.py: FMINI_CUT FMINI_K FMINI_TRIM FMINI_G FMINI_GX), solved with
 * zero-delay feedback, a tanh in front of every stage, the emphasis through a lag pole.
 *   three oscillators   TRI SHARK SAW SQR PLS-W PLS-N SIN (polyBLEP), RANGE LO 32' 16' 8' 4' 2', OSC 2 / 3 detuned in cents
 *   mixer               a squared level each (a level of 0 is off) and the noise (white or pink); the sum overloads softly
 *   filter              CUT, EMPH, KBD (the keyboard at 1 V/oct), the contour CNTR (up to 5 octaves; F.ATK, F.DEC, F.SUS, the
 *                       release takes F.DEC), MOD (the LFO or noise, to the pitch and / or the filter); optional high-pass output
 *   loudness            the part's ADSR (the voice's ENV page)
 * What differs from FMini-1: its 200 patches are presets (eng_fmini_data.h, any of them one browse step away); a preset's
 * values go into track parameters (pset) instead of its own patch store, so projects, parameter locks, the editor and
 * the EDIT knobs reach them all; the list page (ui_lzlist.c) edits them. The mod wheel, the pitch bend, the glide and the
 * GLO TUNING are the host's. Not ported: FMini-1's arpeggiator / FX per patch, its sequencer, its own pages.
 * EDIT 1  CUT EMPH CNTR F.ATK    EDIT 2  F.DEC F.SUS MOD KBD
 * Voice state: s[0..3] the ladder (Q8), s[4] the feedback's lag pole (Q8), s[5] s[6] the oscillators' drift (cents * 256),
 * s[7] the contour (stage << 24 | level, Q23). Per part (in .pool): the noise, the smoothed CUT / EMPH / CNTR. */
#define FMINI_NFACTORY 201u
#define FGP(r1, r2, r3, w1, w2, w3, f2, f3, l1, l2, l3, ln, s1, s2, tn, mm, kb, gl, d2l, d2h, d3l, d3h, bl, bh) \
    {r1, r2, r3, w1, w2, w3, f2, f3, l1, l2, l3, ln, s1, s2, tn, mm, kb, gl, d2l, d2h, d3l, d3h, bl, bh}
#define FG_SIZE 32u
enum {                           /* a factory patch's bytes (FMini-1's) */
    FG_R1, FG_R2, FG_R3, FG_W1, FG_W2, FG_W3, FG_F2, FG_F3, FG_L1, FG_L2, FG_L3, FG_LN, FG_SW1, FG_SW2, FG_TUNE, FG_MMIX,
    FG_KBD, FG_GLID, FG_DC2L, FG_DC2H, FG_DC3L, FG_DC3H, FG_BPML, FG_BPMH
};
enum { FS_O1 = 1, FS_O2 = 2, FS_O3 = 4, FS_NZ = 8, FS_PINK = 16, FS_MOSC = 32, FS_MFLT = 64 };   /* FG_SW1 */
enum { FTG_GLIDE = 1, FTG_SQR = 2, FTG_HP = 4 };                                                   /* FG_SW2 */
#include "eng_fmini_data.h"             /* FMINI_FACTORY FMINI_CATS FMINI_PRESETS FMINI_X: tools/gen_fmini_data.py */
#include "eng_fmini_filter.h"           /* the ladder's laws: FMINI_CUT FMINI_K FMINI_TRIM FMINI_G FMINI_GX */

/* a patch's DET 2 / 3 in cents: the signed 10-bit magnitude in the two 7-bit bytes at lo (low 7 bits, then bits 7..9 and the sign at bit 3) */
static int32_t fmini_det(const uint8_t *g, uint32_t lo)
{
    int32_t m = g[lo] | (int32_t)(g[lo + 1] & 7) << 7;
    return g[lo + 1] & 8 ? -m : m;
}

/* preset pi's patch into the track values p (the hook of engines.c preset_values; E0..E6 and the rest are the preset's) */
static uint32_t fmini_pset(int16_t *p, uint32_t pi)
{
    const uint8_t *g = FMINI_FACTORY[pi % FMINI_NFACTORY];
    const fmini_x_t *x = &FMINI_X[pi % FMINI_NFACTORY];
    uint32_t i;
    for (i = P_LN0; i <= P_LN6; i++)                 /* the packed fields start clear */
        p[i] = 0;
    fmx_set(p, FX_R1, g[FG_R1]); fmx_set(p, FX_R2, g[FG_R2]); fmx_set(p, FX_R3, g[FG_R3]);
    fmx_set(p, FX_W1, g[FG_W1]); fmx_set(p, FX_W2, g[FG_W2]); fmx_set(p, FX_W3, g[FG_W3]);
    fmx_set(p, FX_L1, (g[FG_SW1] & FS_O1) ? g[FG_L1] : 0);
    fmx_set(p, FX_L2, (g[FG_SW1] & FS_O2) ? g[FG_L2] : 0);
    fmx_set(p, FX_L3, (g[FG_SW1] & FS_O3) ? g[FG_L3] : 0);
    fmx_set(p, FX_LN, (g[FG_SW1] & FS_NZ) ? g[FG_LN] : 0);
    fmx_set(p, FX_D2, (int16_t)fmini_det(g, FG_DC2L));
    fmx_set(p, FX_D3, (int16_t)fmini_det(g, FG_DC3L));
    fmx_set(p, FX_TU, (int16_t)((int32_t)g[FG_TUNE] - 64));
    fmx_set(p, FX_MM, g[FG_MMIX]);
    fmx_set(p, FX_PK, (g[FG_SW1] & FS_PINK) != 0);
    fmx_set(p, FX_MO, (g[FG_SW1] & FS_MOSC) != 0);
    fmx_set(p, FX_MF, (g[FG_SW1] & FS_MFLT) != 0);
    fmx_set(p, FX_SQ, (g[FG_SW2] & FTG_SQR) != 0);
    fmx_set(p, FX_HP, (g[FG_SW2] & FTG_HP) != 0);
    p[P_E7] = g[FG_KBD];
    p[P_GLIDE] = (g[FG_SW2] & FTG_GLIDE) ? g[FG_GLID] : 0;
    p[P_LRATE] = x->lfo[0];
    p[P_LWAVE] = x->lfo[1];
    p[P_LFADE] = x->lfo[2];
    p[P_LD_PIT] = x->mod[0];
    p[P_LD_AMP] = x->mod[1];
    p[P_ED_PIT] = x->mod[2];
    p[P_TRIM] = (int16_t)((int32_t)x->level - 104 + 10);   /* FMini-1 levelled each patch with LEVEL (default 104, 1/2 dB steps); +5 dB: the host mix is louder than its own */
    if (!x->arp[0])                                  /* a patch with no arpeggiator leaves the track's */
        return 0;
    for (i = 0; i < 4u; i++)                         /* MODE RATE OCT GATE */
        p[P_AMODE + i] = x->arp[i];
    p[P_ASWING] = x->arp[4];
    p[P_APROB] = x->arp[5];
    p[P_AHOLD] = 0;
    return 1;
}

/* per part, in .pool (zeroed at boot): the noise generator, its pink filter and this block's noise, the smoothed knobs */
static int32_t fmini_nst[NTRK] __attribute__((section(".pool")));
static int32_t fmini_pk[NTRK][3] __attribute__((section(".pool")));
static int32_t fmini_nbuf[NTRK][CTL] __attribute__((section(".pool")));    /* +-32768: one source, all the part's voices */
static int32_t fmini_sm[NTRK][3] __attribute__((section(".pool")));        /* CUT, EMPH, CNTR (Q16): the encoder's steps become ramps */
static int32_t fmini_mn[NTRK] __attribute__((section(".pool")));           /* the modulation noise of this block, +-16384 */
static uint8_t fmini_sm_ok __attribute__((section(".pool")));              /* bit tr: fmini_sm[tr] follows the knobs (clear = snap) */

static inline uint32_t fmini_noise(uint32_t tr)
{
    if (!fmini_nst[tr])
        fmini_nst[tr] = (int32_t)(0x2F6E2B1u + tr * 0x9E3779B1u);
    return noise32(&fmini_nst[tr]);
}

/* once a block and part, before its voices: the knobs' smoothing, the noise (white or pink: Kellet's filter), the modulation noise */
static void fmini_block(track_t *t)
{
    uint32_t tr = (uint32_t)(t - trk), i;
    static const uint8_t KN[3] = {P_E0, P_E1, P_E2};
    uint32_t j;
    if (tr >= NTRK)
        return;
    for (i = 0; i < NVOICE && !t->v[i].active; i++)
        ;
    for (j = 0; j < 3; j++) {            /* one encoder click = 1/127 glides: a one-pole per block (200 ms), exact landing; silent = snap */
        int32_t want = (int32_t)clamp(t->p[KN[j]], 0, 127) << 16, d = want - fmini_sm[tr][j], st;
        if (i == NVOICE || !(fmini_sm_ok & (1u << tr))) {
            fmini_sm[tr][j] = want;
            continue;
        }
        st = d >= 0 ? (int32_t)(((int64_t)d * 238) >> 16) : -(int32_t)(((int64_t)-d * 238) >> 16);
        if (!st)
            st = d > 0 ? 1 : d < 0 ? -1 : 0;
        fmini_sm[tr][j] += st;
    }
    if (i == NVOICE) {
        fmini_sm_ok &= (uint8_t)~(1u << tr);
        return;                                              /* nothing sounds: no noise to make */
    }
    fmini_sm_ok |= (uint8_t)(1u << tr);
    fmini_mn[tr] = (int32_t)(fmini_noise(tr) >> 17) - 16384;
    if (fmx_get(t->p, FX_LN) > 0) {
        int32_t b0 = fmini_pk[tr][0], b1 = fmini_pk[tr][1], b2 = fmini_pk[tr][2];
        for (i = 0; i < CTL; i++) {
            int32_t w = (int32_t)(fmini_noise(tr) >> 16) - 32768;   /* white, +-32768 */
            if (fmx_get(t->p, FX_PK)) {
                b0 += ((w * 3245) >> 15) - ((b0 * 77) >> 15);              /* 0.0990 in, 0.99765 kept */
                b1 += ((w * 9716) >> 15) - ((b1 * 1212) >> 15);            /* 0.2965 in, 0.963 kept */
                b2 += ((w * 34495) >> 15) - ((b2 * 14090) >> 15);          /* 1.0527 in, 0.57 kept */
                w = (b0 + b1 + b2 + ((w * 6056) >> 15)) >> 1;
            }
            fmini_nbuf[tr][i] = w;
        }
        fmini_pk[tr][0] = b0;
        fmini_pk[tr][1] = b1;
        fmini_pk[tr][2] = b2;
    }
}

/* ------------------------------------------------------------------ voice --- */
static inline int32_t fmini_shark(uint32_t ph)            /* a triangle that rises for 3/4 of the cycle */
{
    uint32_t x = ph >> 16;
    return x < 49152u ? (int32_t)((x * 43691u) >> 15) - 32768 : 32767 - (int32_t)((x - 49152u) * 4u);
}

/* one oscillator of the mix: mix[i] += its sample at level g (Q15); a loop per wave */
static void fmini_wave(uint32_t w, uint32_t *php, uint32_t inc, int32_t g, int32_t *mix, uint32_t n)
{
    uint32_t ph = *php, i;
    switch (w) {
    case 0:
        for (i = 0; i < n; i++) {
            mix[i] += mulq15(osc_tri(ph), g);
            ph += inc;
        }
        break;
    case 1:
        for (i = 0; i < n; i++) {
            mix[i] += mulq15(fmini_shark(ph), g);
            ph += inc;
        }
        break;
    case 2:
        for (i = 0; i < n; i++) {
            mix[i] += mulq15(osc_saw(ph, inc), g);
            ph += inc;
        }
        break;
    case 3:
        for (i = 0; i < n; i++) {
            mix[i] += mulq15(osc_pulse(ph, inc, 0x80000000u), g);
            ph += inc;
        }
        break;
    case 4:
        for (i = 0; i < n; i++) {
            mix[i] += mulq15(osc_pulse(ph, inc, 0x55555555u), g);
            ph += inc;
        }
        break;
    case 6:                                             /* SIN: the sine table (dsp.c), no harmonics to fold back */
        for (i = 0; i < n; i++) {
            mix[i] += mulq15(osc_sine(ph), g);
            ph += inc;
        }
        break;
    default:
        for (i = 0; i < n; i++) {
            mix[i] += mulq15(osc_pulse(ph, inc, 0x1C71C71Cu), g);
            ph += inc;
        }
        break;
    }
    *php = ph;
}

/* range of an oscillator in 1/16 semitones (LO: very low) */
static int32_t fmini_range16(uint32_t r)
{
    static const int16_t R[6] = {-48 * 16, -24 * 16, -12 * 16, 0, 12 * 16, 24 * 16};
    return R[r % 6u];
}

static void fmini_note_on(track_t *t, voice_t *v)
{
    if (!v->env && !v->env_out) {                          /* from silence: the ladder and the contour from 0 */
        uint32_t tr = (uint32_t)(t - trk) % NTRK;
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = 0;
        v->s[4] = 0;
        v->s[7] = 0;
        if (t->p[P_E1] >= 96)                                         /* it will sing: a nudge to start from (the hardware has its own noise) */
            v->s[0] = (fmini_noise(tr) & 0x80000000u) ? 700 * 256 : -700 * 256;
        v->s[5] = (int32_t)(fmini_noise(tr) >> 20) - 2048;   /* the VCOs start a little apart: drift, cents * 256 */
        v->s[6] = (int32_t)(fmini_noise(tr) >> 20) - 2048;
    }
    v->s[7] = (int32_t)(1u << 24) | (v->s[7] & 0xFFFFFF);  /* the contour attacks from where it is */
}

/* the contour, once a block: stage 1 attack, 2 decay to the sustain, 3 release (the DECAY time); level Q23 */
static int32_t fmini_contour(const int16_t *p, voice_t *v)
{
    uint32_t st = (uint32_t)v->s[7] >> 24;
    int32_t e = v->s[7] & 0xFFFFFF, sus = (int32_t)p[P_E5] << 16, k = (int32_t)ENV_EXP[p[P_E4] & 127];
    if (!v->gate && st && st != 3u)
        st = 3;
    switch (st) {
    case 1:
        {   /* the capacitor charges toward a level above full: a concave rise that ends in a corner, as in the Minimoog */
            int32_t step = (int32_t)(ENV_LIN[p[P_E3] & 127] >> 1), d = (int32_t)(((int64_t)((5 << 21) - e) * step * 824) >> 32);
            e += d < 1 ? 1 : d;
        }
        if (e >= (1 << 23)) {
            e = 1 << 23;
            st = 2;
        }
        break;
    case 2:
        e += mulq16(sus - e, (uint32_t)k);                  /* the DECAY time is the drop from the peak to the SUSTAIN level, whatever it is */
        break;
    case 3:
        e -= mulq16(e, (uint32_t)k);                        /* the RELEASE takes the DECAY time */
        break;
    default:
        break;
    }
    v->s[7] = (int32_t)(st << 24) | (e & 0xFFFFFF);
    return e >> 8;                                          /* Q15 */
}

/* a * k / 65536 rounded to the nearest (mulq16 floors: four stages of it leave a bias that a quiet, low ladder cannot sing against) */
static inline int32_t fmini_mulr(int32_t a, uint32_t k)
{
    return (int32_t)(((a >> 16) * (int32_t)k) + (int32_t)(((uint32_t)(a & 0xFFFF) * k + 32768u) >> 16));
}

/* The filter, after the Minimoog's (FMini-1's tools/fmini_filter.py has the laws and the reasoning): four equal one-pole
 * stages, each a differential pair (tanh in front), the last stage's output fed back to the first through a lag pole (the
 * emphasis network), solved without a delay in the loop (each stage is a trapezoidal integrator, G = g / (1 + g), and the
 * feedback is found from the stages' states before the sample is processed). The filter sings at 1.578 times its -3 dB
 * corner when the feedback k reaches 2.92. The knobs and the pitch work in 1/16 semitones of the singing pitch (MIDI x 16):
 * CUTOFF is the dial -5 .. +5 (FMINI_CUT), the keyboard adds 1 V/oct from F (MIDI 41) at KBD 127, the contour up to 5
 * octaves; EMPHASIS is the dial 0 .. 10 (FMINI_K) with the hardware's passband loss (FMINI_TRIM).
 * Q formats: signals +-32768 (tanh(1)); the states Q8; G Q16; the prediction's coefficients c Q14; k Q12. */
static void fmini_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t mix[CTL];
    int32_t env15, mod, lfo, lc, p16, fine, k12, G16, gx16, c0, c1, c2, c3, inv14, kinv12, trim12;
    int32_t l1 = 0, l2 = 0, l3 = 0, ln = 0, dr1, dr2, dr3;
    uint32_t hp = fmx_get(p, FX_HP) ? 1u : 0u;      /* the output: LP (the last stage) or HP ((1 - LP)^4 of the same stages) */
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i;
    uint32_t inc1, inc2, inc3;
    int32_t s0 = v->s[0], s1 = v->s[1], s2 = v->s[2], s3 = v->s[3], sx = v->s[4];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    if (n > CTL)
        n = CTL;
    env15 = fmini_contour(p, v);

    /* modulation: the LFO (the square with SQUARE) or noise held for the block, MOD deep */
    lfo = mulq15(t->lfo_val, t->lfo_fade);
    if (fmx_get(p, FX_SQ))
        lfo = lfo >= 0 ? 32767 : -32767;
    mod = mulq15(lfo, 32767 - (int32_t)fmx_get(p, FX_MM) * 258) + mulq15(fmini_mn[tr] << 1, (int32_t)fmx_get(p, FX_MM) * 258);
    mod = mulq15(mod, (int32_t)p[P_E6] * 258);                          /* MOD: Q15 of its depth */

    /* oscillator pitches */
    p16 = m->pitch16 + (fmx_get(p, FX_MO) ? (mod * 16) >> 14 : 0);        /* full depth: +-2 semitones */
    fine = m->fine + (int32_t)fmx_get(p, FX_TU) * 237 / 64;                        /* TUNE: +-100 cents */
    {   /* the VCOs wander a few cents (temperature, the integrators): a slow random walk per oscillator, about +-2 cents */
        int32_t d1 = v->s[5], d2 = v->s[6];
        d1 += ((int32_t)(fmini_noise(tr) >> 20) - 2048 >> 6) - (d1 >> 11);
        d2 += ((int32_t)(fmini_noise(tr) >> 20) - 2048 >> 6) - (d2 >> 11);
        v->s[5] = d1;
        v->s[6] = d2;
        dr1 = d1 >> 8;
        dr2 = d2 >> 8;
        dr3 = (d1 - d2) >> 9;
    }
    inc1 = cents_inc(clamp(p16 + fmini_range16((uint32_t)fmx_get(p, FX_R1)), 0, 2047), dr1, fine);
    inc2 = cents_inc(clamp(p16 + fmini_range16((uint32_t)fmx_get(p, FX_R2)), 0, 2047), (int32_t)fmx_get(p, FX_D2) + dr2, fine);
    inc3 = cents_inc(clamp(p16 + fmini_range16((uint32_t)fmx_get(p, FX_R3)), 0, 2047), (int32_t)fmx_get(p, FX_D3) + dr3, fine);

    /* mixer levels: a squared curve */
    l1 = (int32_t)(fmx_get(p, FX_L1) * fmx_get(p, FX_L1) * 2);
    l2 = (int32_t)(fmx_get(p, FX_L2) * fmx_get(p, FX_L2) * 2);
    l3 = (int32_t)(fmx_get(p, FX_L3) * fmx_get(p, FX_L3) * 2);
    ln = (int32_t)(fmx_get(p, FX_LN) * fmx_get(p, FX_LN) * 2);
    for (i = 0; i < n; i++)
        mix[i] = 0;
    if (l1)
        fmini_wave((uint32_t)fmx_get(p, FX_W1), &ph0, inc1, l1, mix, n);
    else
        ph0 += inc1 * n;
    if (l2)
        fmini_wave((uint32_t)fmx_get(p, FX_W2), &ph1, inc2, l2, mix, n);
    else
        ph1 += inc2 * n;
    if (l3)
        fmini_wave((uint32_t)fmx_get(p, FX_W3), &ph2, inc3, l3, mix, n);
    else
        ph2 += inc3 * n;
    if (ln)
        for (i = 0; i < n; i++)
            mix[i] += mulq15(fmini_nbuf[tr][i], ln);

    /* the singing pitch (MIDI x 16): the CUTOFF knob's dial, the contour, the keyboard, the modulation, the host's (LFO / matrix / ENV routings) */
    {
        int32_t kn = fmini_sm[tr][0], i0 = kn >> 16, fr = (kn >> 8) & 255;
        lc = FMINI_CUT[i0];
        if (i0 < 127)
            lc += ((FMINI_CUT[i0 + 1] - lc) * fr) >> 8;
    }
    lc += (m->cutoff * 55) >> 10;                                                                           /* the host's steps (1/14 octave) in 1/16 semitones */
    lc += (int32_t)(((int64_t)env15 * fmini_sm[tr][2] / (127 * 65536)) * 960) >> 15;                          /* the contour: up to 5 octaves */
    lc += ((v->pitch_cur - FMINI_NOTE0 * 16) * (int32_t)p[P_E7] * 516) >> 16;                                /* the keyboard: 1 V/oct at 127, 0 V on F */
    if (fmx_get(p, FX_MF))
        lc += (mod * 547) >> 15;                                                                            /* +-2.85 octaves at full depth */
    lc = clamp(lc, FMINI_GM0 * 16, FMINI_FOSC_MAX16);
    {   /* the stage pole G and the lag pole's from the tables (one entry a semitone, linear between), the feedback solved ahead */
        uint32_t ix = (uint32_t)((lc >> 4) - FMINI_GM0), fr = (uint32_t)lc & 15u, gq, gxq, g2, g3, g4, om;
        gq = FMINI_G[ix] + (uint32_t)(((int32_t)(FMINI_G[ix + 1] - FMINI_G[ix]) * (int32_t)fr) >> 4);
        gxq = FMINI_GX[ix] + (uint32_t)(((int32_t)(FMINI_GX[ix + 1] - FMINI_GX[ix]) * (int32_t)fr) >> 4);
        G16 = (int32_t)((gq + 128u) >> 8);
        gx16 = (int32_t)((gxq + 128u) >> 8);
        om = 65536u - (uint32_t)G16;
        g2 = ((uint32_t)G16 * (uint32_t)G16) >> 16;
        g3 = (g2 * (uint32_t)G16) >> 16;
        g4 = (g3 * (uint32_t)G16) >> 16;
        c3 = (int32_t)(om >> 2);                                   /* y4 = G^4 u + c0 s0 + c1 s1 + c2 s2 + c3 s3 */
        c2 = (int32_t)(((uint32_t)G16 * om) >> 18);
        c1 = (int32_t)((g2 * om) >> 18);
        c0 = (int32_t)((g3 * om) >> 18);
        {   /* EMPHASIS: the feedback gain k and the passband's loss, from the knob (Q16) */
            int32_t kn = fmini_sm[tr][1], i0 = kn >> 16, fk = (kn >> 8) & 255, den;
            k12 = FMINI_K[i0];
            trim12 = FMINI_TRIM[i0];
            if (i0 < 127) {
                k12 += ((FMINI_K[i0 + 1] - k12) * fk) >> 8;
                trim12 += ((FMINI_TRIM[i0 + 1] - trim12) * fk) >> 8;
            }
            den = 65536 + ((k12 * (int32_t)((((uint32_t)gxq >> 8) * g4) >> 16)) >> 12);        /* 1 + k Gx G^4: the loop around the whole sample */
            inv14 = (int32_t)((1u << 30) / (uint32_t)den);
            kinv12 = (k12 * inv14) >> 14;
        }
    }

    for (i = 0; i < n; i++) {
        int32_t x = softclip(mix[i]), S, fbc, u, xin, v1, v2, v3, v4, y1, y2, y3, y4, w, o;   /* the mixer overloads softly */
        S = (c0 * ((s0 + 128) >> 8) + c1 * ((s1 + 128) >> 8) + c2 * ((s2 + 128) >> 8) + c3 * ((s3 + 128) >> 8)) >> 14;   /* what the stages already hold */
        fbc = sx + fmini_mulr((S << 8) - sx, (uint32_t)gx16);              /* through the lag pole (Q8) */
        u = ((x * inv14) >> 14) - ((kinv12 * ((fbc + 128) >> 8)) >> 12);  /* the input less the feedback, the loop solved */
        xin = softclip(u) << 8;                                              /* the first stage's input (Q8) */
        v1 = fmini_mulr(xin - s0, (uint32_t)G16);                          /* each stage is a differential pair: tanh in front of it */
        y1 = v1 + s0;
        s0 = y1 + v1;
        v2 = fmini_mulr((softclip((y1 + 128) >> 8) << 8) - s1, (uint32_t)G16);
        y2 = v2 + s1;
        s1 = y2 + v2;
        v3 = fmini_mulr((softclip((y2 + 128) >> 8) << 8) - s2, (uint32_t)G16);
        y3 = v3 + s2;
        s2 = y3 + v3;
        v4 = fmini_mulr((softclip((y3 + 128) >> 8) << 8) - s3, (uint32_t)G16);
        y4 = v4 + s3;
        s3 = y4 + v4;
        w = fmini_mulr(y4 - sx, (uint32_t)gx16);                           /* the lag pole follows the output */
        sx += w + w;
        /* HP: the stages are one-pole lows H; (1 - H)^4 = 1 - 4H + 6H^2 - 4H^3 + H^4 on the stages' outputs (linear, the states as they are) */
        o = hp ? xin - 4 * y1 + 6 * y2 - 4 * y3 + y4 : y4;
        out[i] += voice_amp((((o + 128) >> 8) * trim12) >> 12, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = s0;
    v->s[1] = s1;
    v->s[2] = s2;
    v->s[3] = s3;
    v->s[4] = sx;
}

_Static_assert(sizeof FMINI_CATS == FMINI_NFACTORY + 1u, "a type a preset");

/* (2026-11-11) the SOUND page's rows: the whole patch (the settings past E0..E7 are VF ids, bit fields in P_LN0..6) */
static const sound_row_t FMINI_ROWS[] = {
    {"OSC 1", {VF(FX_R1), VF(FX_W1), VF(FX_L1)}, {"RANGE", "WAVE", "LEVEL"}},
    {"OSC 2", {VF(FX_R2), VF(FX_W2), VF(FX_L2)}, {"RANGE", "WAVE", "LEVEL"}},
    {"OSC 3", {VF(FX_R3), VF(FX_W3), VF(FX_L3)}, {"RANGE", "WAVE", "LEVEL"}},
    {"TUNING", {VF(FX_D2), VF(FX_D3), VF(FX_TU)}, {"DETUNE 2", "DETUNE 3", "TUNE"}},
    {"NOISE", {VF(FX_LN), VF(FX_PK), 0xFF}, {"LEVEL", "COLOR", ""}},
    {"FILTER", {P_E0, P_E1, P_E7}, {"CUTOFF", "EMPHASIS", "KEYBOARD"}},
    {"CONTOUR", {P_E2, P_E3, P_E4}, {"AMOUNT", "ATTACK", "DECAY"}},
    {"SUSTAIN", {P_E5, VF(FX_HP), 0xFF}, {"SUSTAIN", "OUTPUT", ""}},
    {"MOD", {P_E6, VF(FX_MM), VF(FX_SQ)}, {"AMOUNT", "MIX", "LFO SHAPE"}},
    {"MOD TO", {VF(FX_MO), VF(FX_MF), 0xFF}, {"TO OSC", "TO FILTER", ""}},
};

static const engine_t ENG_MINI = {
    .rows = FMINI_ROWS,
    .nrows = (uint8_t)(sizeof FMINI_ROWS / sizeof FMINI_ROWS[0]),
    .name = "MINI",
    .page_title = {"FILTER", "MORE"},
    .edit = {
        {"CUT", F_PCT, 0, 127, 64, 0, 0},
        {"EMPH", F_PCT, 0, 127, 40, 0, 0},
        {"CNTR", F_PCT, 0, 127, 64, 0, 0},
        {"F.ATK", F_TIME, 0, 127, 0, 0, 0},
        {"F.DEC", F_TIME, 0, 127, 64, 0, 0},
        {"F.SUS", F_PCT, 0, 127, 40, 0, 0},
        {"MOD", F_PCT, 0, 127, 0, 0, 0},
        {"KBD", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = FMINI_PRESETS,
    .npresets = FMINI_NFACTORY,
    .ptype = FMINI_CATS,
    .trim = 0,
    .note_on = fmini_note_on,
    .render = fmini_render,
    .block = fmini_block,
    .pset = fmini_pset,
    .knob = {P_E0, P_E1, P_E2, P_E4},
    .poly = 4,
    .keep = 0x13,            /* the ladder's first two stages and the lag pole (s[0], s[1], s[4]) */
};
