/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* MINI (2026-11-10): a Minimoog-style monosynth voice, after the three-oscillator Moog instruments as FMini-1
 * (ruppert/fmini-1, GPL-3.0, the Minimoog Model D on the FM-1) models them; the circuit here is our own, small, on the
 * ladder of dsp.c (a four-pole transistor ladder, the Moog's) and ACID's polyBLEP oscillators.
 *   three oscillators   OSC 1 at the note, OSC 2 detuned (DET, up to ~27 cents), OSC 3 an octave below (SUB); all of
 *                       the one WAVE (SAW TRI SQR PULSE), band-limited (polyBLEP)
 *   mixer               the three and the NOISE, summed hot into the ladder: its saturation is the Moog's "overload"
 *   filter              the 24 dB ladder: CUT, EMPH (the emphasis, up to singing), the contour CNTR deep, FDEC long
 *                       (an instant attack, a decay to a sustain while the key is held, the same decay after it),
 *                       the keyboard following the cutoff by two thirds
 *   loudness            the part's ADSR (the voice's ENV page)
 * EDIT 1  WAVE DET SUB NOIS     EDIT 2  CUT EMPH CNTR FDEC
 * Voice state: ph[0..2] the oscillators; s[0..3] the ladder (Q12), s[4] the filter contour (Q15), s[5] the noise
 * generator, s[7] the high-pass (DC) */
static const char *const N_MINI_WAVE[] = {"SAW", "TRI", "SQR", "PULSE"};

static void mini_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->env && !v->env_out) {                    /* from silence: the filter clear, the oscillators free */
        v->ph[0] = v->ph[1] = v->ph[2] = 0;
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = v->s[7] = 0;
        v->s[5] = (int32_t)(0x2F6E2B1u + v->age * 0x9E3779B1u) | 1;
    }
    v->s[4] = 32767;                                 /* the contour restarts (not on a legato note) */
}

static inline int32_t mini_tri(uint32_t ph)
{
    int32_t x = (int32_t)(ph >> 16);
    return (x < 32768 ? x : 65535 - x) * 2 - 32767;
}
static inline int32_t mini_osc(uint32_t wave, uint32_t ph, uint32_t inc)
{
    switch (wave) {
    case 1: return mini_tri(ph);
    case 2: return (hy_saw(ph, inc) - hy_saw(ph + 0x80000000u, inc)) >> 1;
    case 3: return (hy_saw(ph, inc) - hy_saw(ph + 0xC0000000u, inc)) >> 1;
    default: return hy_saw(ph, inc);
    }
}

static void mini_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, wave = (uint32_t)p[P_E0] & 3u, inc = m->inc;
    uint32_t ph1 = v->ph[0], ph2 = v->ph[1], ph3 = v->ph[2];
    uint32_t inc2 = inc + (uint32_t)(((uint64_t)inc * (uint32_t)p[P_E1]) >> 13), inc3 = inc >> 1;
    int32_t sub = p[P_E2] * 258, nz = p[P_E3] * 258;                   /* Q15 levels */
    int32_t fenv = v->s[4], hp = v->s[7], rs = v->s[5];
    int32_t ls[4] = {v->s[0], v->s[1], v->s[2], v->s[3]};
    int32_t cut, res, tg = v->gate ? 9000 : 0;
    ladder_t lad;
    /* the contour, once a block: to the sustain while held, to nothing after; FDEC sets the time */
    fenv = tg + (int32_t)(((int64_t)(fenv - tg) * acid_block_k(40u + (uint32_t)p[P_E7] * 65u / 100u)) >> 16);
    /* cutoff (0..127 << 8, ~14 steps an octave): CUT + CNTR x the contour + the keyboard (2/3 of the pitch above F2) + the part */
    cut = (p[P_E4] << 8) + ((fenv * p[P_E6]) >> 8) + ((m->pitch16 - 41 * 16) * 12) + m->cutoff;
    res = clamp(p[P_E5], 0, 118);
    ladder_coef(&lad, cut, res);
    for (i = 0; i < n; i++, ph1 += inc, ph2 += inc2, ph3 += inc3) {
        int32_t s = mini_osc(wave, ph1, inc) + mini_osc(wave, ph2, inc2) + mulq15(mini_osc(wave, ph3, inc3), sub);
        if (nz) {                                                       /* the noise: white */
            rs = rs * 1664525 + 1013904223;
            s += mulq15(rs >> 16, nz);
        }
        s = clamp(s >> 1, -65536, 65536);                               /* the mixer, hot: the ladder's input saturates */
        s = ladder_tick(&lad, s, ls);
        hp += (s - hp) >> 9;                                            /* the DC out (~14 Hz) */
        s -= hp;
        out[i] += mulq15(mulq15(s, amp_at(m, i)), VOICE_FS) << 1;
    }
    v->ph[0] = ph1, v->ph[1] = ph2, v->ph[2] = ph3;
    v->s[0] = ls[0], v->s[1] = ls[1], v->s[2] = ls[2], v->s[3] = ls[3];
    v->s[4] = fenv, v->s[5] = rs, v->s[7] = hp;
}

static const preset_t MINI_PRESETS[] YEHNAH_RES = {
    /* {WAVE, DET, SUB, NOIS, CUT, EMPH, CNTR, FDEC}, {A D S R}, ENV -> FILTER (the part's: 0), mono; glide */
    {"MOOG BASS", {0, 20, 70, 0, 28, 60, 80, 40}, {0, 70, 100, 15}, 0, 1, FX(0, 0, 0, 4), XP(P_TRANS + 1, -12)},
    {"FAT BASS", {0, 40, 100, 0, 22, 40, 70, 50}, {0, 80, 110, 20}, 0, 1, FX(0, 0, 0, 4), XP(P_TRANS + 1, -12, P_GLIDE + 1, 14)},
    {"SUB BASS", {1, 10, 127, 0, 20, 20, 40, 30}, {0, 60, 120, 25}, 0, 1, FX(0, 0, 0, 2), XP(P_TRANS + 1, -24)},
    {"WOW BASS", {2, 25, 60, 0, 12, 85, 110, 60}, {0, 90, 90, 20}, 0, 1, FX(0, 0, 0, 6), XP(P_TRANS + 1, -12)},
    {"PLUCK BASS", {0, 15, 60, 0, 14, 70, 100, 22}, {0, 40, 0, 15}, 0, 1, FX(0, 0, 0, 4), XP(P_TRANS + 1, -12)},
    {"MOOG LEAD", {0, 45, 40, 0, 44, 55, 50, 70}, {0, 90, 110, 30}, 0, 1, FX(0, 0, 16, 14), XP(P_GLIDE + 1, 30)},
    {"PORTA LEAD", {0, 60, 30, 0, 52, 40, 40, 80}, {0, 100, 110, 35}, 0, 1, FX(0, 4, 20, 16), XP(P_GLIDE + 1, 60, P_MWDST + 1, MWD_VIB)},
    {"SQR LEAD", {2, 30, 50, 0, 40, 45, 55, 70}, {0, 80, 110, 30}, 0, 1, FX(0, 0, 18, 14), XP(P_GLIDE + 1, 20)},
    {"SINGING", {1, 55, 30, 0, 36, 100, 45, 90}, {10, 100, 120, 40}, 0, 1, FX(0, 8, 20, 20), XP(P_GLIDE + 1, 40, P_MWDST + 1, MWD_VIB)},
    {"BRASS", {0, 50, 50, 0, 24, 30, 90, 55}, {30, 80, 100, 25}, 0, 0, FX(0, 8, 0, 14)},
    {"SYNTH PAD", {0, 70, 30, 0, 30, 25, 40, 100}, {90, 100, 110, 90}, 0, 0, FX(0, 20, 20, 30)},
    {"FUNK", {2, 20, 80, 0, 26, 75, 100, 28}, {0, 45, 40, 15}, 0, 1, FX(0, 0, 0, 4), XP(P_TRANS + 1, -12)},
    {"WIND", {3, 10, 0, 90, 40, 110, 60, 110}, {80, 100, 100, 90}, 0, 1, FX(0, 8, 20, 36), XP(P_GLIDE + 1, 10)},
    {"ZAP", {0, 90, 20, 30, 20, 90, 127, 18}, {0, 30, 0, 12}, 0, 1, FX(0, 0, 14, 12), XP(P_TRANS + 1, 12)},
    {"INIT", {0, 20, 70, 0, 60, 30, 60, 60}, {0, 64, 127, 30}, 0, 1, FX(0, 0, 0, 0)},   /* the EDIT defaults, dry */
};
static const char MINI_PT[] = "BBBBBLLLLPPBWSN";
_Static_assert(sizeof MINI_PT == NELEM(MINI_PRESETS) + 1u, "a type a preset");
static const int8_t MINI_TRIM[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
_Static_assert(sizeof MINI_TRIM == NELEM(MINI_PRESETS), "a trim a preset");
_Static_assert(NELEM(MINI_PRESETS) <= 128u, "MINI's favourites");

static const engine_t ENG_MINI = {
    .name = "MINI",
    .page_title = {"OSC", "VCF"},
    .edit = {
        {"WAVE", F_ENUM, 0, 3, 0, N_MINI_WAVE, 0},
        {"DET", F_PCT, 0, 127, 20, 0, 0},
        {"SUB", F_PCT, 0, 127, 70, 0, 0},
        {"NOIS", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 60, 0, 0},
        {"EMPH", F_PCT, 0, 127, 30, 0, 0},
        {"CNTR", F_PCT, 0, 127, 60, 0, 0},
        {"FDEC", F_PCT, 0, 127, 60, 0, 0},
    },
    .presets = MINI_PRESETS,
    .npresets = NELEM(MINI_PRESETS),
    .ptype = MINI_PT,
    .trim = MINI_TRIM,
    .note_on = mini_note_on,
    .render = mini_render,
    .knob = {P_E4, P_E5, P_E6, P_E7},
    .poly = 4,
    .keep = 0xAF,            /* the ladder (s[0..3]), the noise (s[5]), the high-pass (s[7]) */
};
