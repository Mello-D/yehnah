/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork) DRIFT: an ambient voice, our own. Each note is six sine partials (PARTS: harmonics, octaves,
 * fifths, a bar's bell modes or glass's), each wandering on its own: its pitch a slow random walk (DRIFT cents, at
 * SPEED), its level another (FLICK: partials fading in and out), a fixed spread between them (DETUNE: slow beating);
 * TILT weighs them dark (the fundamental) to bright (the top ones); BLOOM brings the upper ones in later than the low
 * ones, each a little more; BREATH adds band-passed noise around the note. The track's ADSR shapes it (up to 10 s
 * each way): pads, drones, swells. Under the shimmer / cloud reverbs, a drone that never repeats.
 * EDIT 1  PARTS TILT DRIFT SPEED     EDIT 2  FLICK DETUNE BLOOM BREATH
 * The partials' state in a pool shared by the parts (dr_pool, DR_POOL voices; v->s[0] its slot + 1; a part's poly 6);
 * the wanders move once a block (a smooth glide to a new random place, then another), the sines from SINE (dsp.c
 * sine_i), summed in Q15. */

#include "fastmath.h"

enum { DR_HARM, DR_OCT, DR_FIFTH, DR_BELL, DR_GLASS, DR_N };
static const char *const N_DRPARTS[] = {"HARM", "OCT", "5TH", "BELL", "GLASS"};
#define DR_NP 6u                       /* partials a voice */
#define DR_POOL 10u                    /* voices sounding at once, all parts */
static const float DR_RATIO[DR_N][DR_NP] = {
    {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f},                       /* HARM: the harmonic series */
    {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f},                      /* OCT: octaves, one below */
    {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f},                       /* 5TH: fifths and octaves */
    {1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 0.5f},               /* BELL: a free bar's modes, a hum below */
    {1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 2.0f},                   /* GLASS: a glass's, its octave */
};
typedef struct {
    uint32_t ph[DR_NP];
    float pa[DR_NP], pb[DR_NP], q[DR_NP];   /* each partial's pitch wander: from, to (-1..1), the way there (0..1) */
    float aa[DR_NP], ab[DR_NP];        /* .. its level's (half a way behind: q + 0.5) */
    float g[DR_NP];                    /* .. its bloom (0 .. 1) */
    float n1, n2;                      /* BREATH's two one-poles */
    uint32_t rng;
    uint8_t owner, slot;               /* part + 1 (0 free), its voice */
} dr_voice_t;
static dr_voice_t dr_pool[DR_POOL];

static float dr_rnd(dr_voice_t *s)     /* -1 .. 1 */
{
    s->rng = s->rng * 1664525u + 1013904223u;
    return (float)(int32_t)s->rng * (1.0f / 2147483648.0f);
}
static dr_voice_t *dr_of(const track_t *t, const voice_t *v)
{
    uint32_t k = (uint32_t)v->s[0];
    dr_voice_t *s;
    if (!k || k > DR_POOL)
        return 0;
    s = &dr_pool[k - 1u];
    return s->owner == (uint8_t)((uint32_t)(t - trk) % NPART + 1u) && s->slot == (uint8_t)(v - t->v) ? s : 0;
}
static int dr_busy(const dr_voice_t *s)  /* its voice still sounds (and is still its) */
{
    const track_t *t;
    const voice_t *v;
    if (!s->owner)
        return 0;
    t = &trk[(s->owner - 1u) % NPART];
    v = &t->v[s->slot % NVOICE];
    return v->active && (uint32_t)v->s[0] == (uint32_t)(s - dr_pool) + 1u && trk[(s->owner - 1u) % NPART].engine == ENGI_DRIFT;
}

static void drift_note_on(track_t *t, voice_t *v)
{
    dr_voice_t *s = dr_of(t, v);
    uint32_t i;
    int fresh = !s;
    if (!s) {                                           /* a pool entry: a free one, else the first (the oldest seldom) */
        for (i = 0; i < DR_POOL && dr_busy(&dr_pool[i]); i++)
            ;
        s = &dr_pool[i < DR_POOL ? i : (v->age * 7u) % DR_POOL];
        v->s[0] = (int32_t)(s - dr_pool) + 1;
        s->owner = (uint8_t)((uint32_t)(t - trk) % NPART + 1u);
        s->slot = (uint8_t)(v - t->v);
        s->rng = 0x9E3779B9u ^ (v->age * 2654435761u);
        s->n1 = s->n2 = 0.0f;
        for (i = 0; i < DR_NP; i++) {                   /* (each partial its own phase and place in its wanders) */
            s->ph[i] = (uint32_t)(dr_rnd(s) * 2147483647.0f) << 1;
            s->pa[i] = dr_rnd(s);
            s->pb[i] = dr_rnd(s);
            s->aa[i] = dr_rnd(s);
            s->ab[i] = dr_rnd(s);
            s->q[i] = 0.5f + 0.5f * dr_rnd(s);
        }
    }
    if (!v->env_out || fresh)                           /* (a new note blooms again; a voice still sounding (retriggered, stolen
                                                         * in place) keeps its partials' levels: resetting them was a click) */
        for (i = 0; i < DR_NP; i++)
            s->g[i] = (float)clamp(t->p[P_E6], 0, 127) ? (i ? 0.0f : 0.3f) : 1.0f;
}

static void drift_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    dr_voice_t *s = dr_of(t, v);
    const int16_t *p = t->p;
    const float *R = DR_RATIO[clamp(p[P_E0], 0, DR_N - 1)];
    float tilt = 2.6f - 2.4f * (float)clamp(p[P_E1], 0, 127) * (1.0f / 127.0f);   /* (the partials fall as r^-tilt) */
    float dc = (float)clamp(p[P_E2], 0, 127) * (60.0f / 127.0f);   /* DRIFT: up to 60 cents */
    float sp = (float)clamp(p[P_E3], 0, 127) * (1.0f / 127.0f);
    float rate = 0.02f + 1.5f * sp * sp;                           /* SPEED: 0.02 .. 1.5 Hz */
    float k1 = rate * (CTL / (float)FS), fl = (float)clamp(p[P_E4], 0, 127) * (1.0f / 127.0f);
    float det = (float)clamp(p[P_E5], 0, 127) * (20.0f / 127.0f);   /* DETUNE: up to +-20 cents */
    float bl = (float)clamp(p[P_E6], 0, 127) * (1.0f / 127.0f), br = (float)clamp(p[P_E7], 0, 127) * (1.0f / 127.0f);
    float inc0 = (float)m->inc, norm = 0.0f, nc;
    int32_t amp[DR_NP], na;
    uint32_t inc[DR_NP], i, j;
    if (!s)
        return;
    for (j = 0; j < DR_NP; j++) {
        float a = fm_minf(fm_powf(R[j], -tilt), 1.0f), c, w, u, q0 = s->q[j];   /* (OCT's partial below: no louder) */
        norm += a;
        s->q[j] += k1 * (0.7f + 0.12f * (float)j);      /* (a smooth glide to a new random place, each partial its pace) */
        if (s->q[j] >= 1.0f) {
            s->q[j] -= 1.0f;
            s->pa[j] = s->pb[j];
            s->pb[j] = dr_rnd(s);
        }
        if (q0 < 0.5f && s->q[j] >= 0.5f) {             /* (the level's, half a way behind) */
            s->aa[j] = s->ab[j];
            s->ab[j] = dr_rnd(s);
        }
        u = s->q[j];
        c = s->pa[j] + (s->pb[j] - s->pa[j]) * u * u * (3.0f - 2.0f * u);
        u = u < 0.5f ? u + 0.5f : u - 0.5f;
        w = s->aa[j] + (s->ab[j] - s->aa[j]) * u * u * (3.0f - 2.0f * u);
        if (bl > 0.0f)                                  /* BLOOM: the partial j in over ~bl * (1 + 2 j) s */
            s->g[j] += (1.0f - s->g[j]) * (CTL / (float)FS) / (0.05f + bl * bl * (1.0f + 2.0f * (float)j));
        a *= (1.0f - fl * (0.5f + 0.5f * w)) * s->g[j];
        c = c * dc + det * (float)((j & 1u) ? 1 : -1) * (float)(j + 1u) * 0.5f;
        w = inc0 * R[j] * fm_exp2f(c * (1.0f / 1200.0f));
        inc[j] = w < 2147483648.0f ? (uint32_t)w : 0u;  /* (past Nyquist: silent) */
        amp[j] = inc[j] ? (int32_t)(a * 32767.0f) : 0;
    }
    nc = 0.62f / (norm > 0.0f ? norm : 1.0f);           /* (the sum at full: ~0.62, as the other voices) */
    for (j = 0; j < DR_NP; j++)
        amp[j] = (int32_t)((float)amp[j] * nc);
    na = (int32_t)(br * br * 9000.0f);                  /* BREATH: noise band-passed round the note */
    {
        float f = fm_minf(inc0 * (1.0f / 4294967296.0f) * 6.2831853f * 3.0f, 0.9f), n1 = s->n1, n2 = s->n2;
        uint32_t r = s->rng;
        for (i = 0; i < n; i++) {
            int32_t y = 0;
            for (j = 0; j < DR_NP; j++) {
                y += (amp[j] * sine_i(s->ph[j])) >> 15;
                s->ph[j] += inc[j];
            }
            if (na) {
                float x;
                r = r * 1664525u + 1013904223u;
                x = (float)(int32_t)r * (1.0f / 2147483648.0f);
                n1 += f * (x - n1);
                n2 += f * 0.25f * (n1 - n2);
                y += (int32_t)((n1 - n2) * (float)na);
            }
            out[i] += voice_amp(y, m, i) << 1;
        }
        s->n1 = fm_flush(n1);
        s->n2 = fm_flush(n2);
        s->rng = r;
    }
}

static const preset_t DRIFT_PRESETS[] YEHNAH_RES = {
    /* name, {PARTS TILT DRIFT SPEED FLICK DETUNE BLOOM BREATH}, {A D S R} (long: the track's), fenv, mono, sends */
    {"DRIFT PAD", {DR_HARM, 50, 40, 30, 30, 20, 30, 0}, {90, 100, 120, 110}, 0, 0, FX(0, 40, 30, 90), PAT(5),
     XP(P_PHAMT + 1, 50, P_PHRATE + 1, 20, P_PHFB + 1, 40)},
    {"GLASS DRONE", {DR_GLASS, 80, 25, 15, 60, 10, 80, 0}, {110, 100, 127, 120}, 0, 0, FX(0, 30, 40, 100), PAT(5)},
    {"OCTAVE CLOUD", {DR_OCT, 40, 30, 20, 50, 30, 60, 0}, {100, 100, 127, 115}, 0, 0, FX(0, 50, 35, 95), PAT(5)},
    {"FIFTHS", {DR_FIFTH, 55, 35, 25, 40, 25, 40, 0}, {90, 100, 120, 110}, 0, 0, FX(0, 40, 30, 90), PAT(5)},
    {"BELL HAZE", {DR_BELL, 70, 20, 10, 70, 15, 0, 0}, {0, 100, 100, 110}, 0, 0, FX(0, 20, 40, 100), PAT(7)},
    {"BREATH PAD", {DR_HARM, 30, 30, 25, 30, 15, 40, 90}, {95, 100, 120, 110}, 0, 0, FX(0, 40, 30, 95), PAT(5),
     XP(P_PHAMT + 1, 40, P_PHRATE + 1, 18, P_PHFB + 1, 30)},
    {"WARM HUM", {DR_HARM, 15, 20, 45, 80, 10, 20, 20}, {70, 100, 120, 100}, 0, 0, FX(0, 50, 20, 80), PAT(5)},
    {"DEEP DRONE", {DR_OCT, 20, 25, 10, 40, 25, 50, 30}, {120, 100, 127, 112}, 0, 0, FX(0, 30, 30, 100), PAT(5),
     XP(P_TRANS + 1, -12)},
    {"SHIMMER SWELL", {DR_OCT, 110, 30, 60, 90, 20, 127, 0}, {110, 100, 127, 115}, 0, 0, FX(0, 40, 45, 110), PAT(5)},
    {"DRIFT KEYS", {DR_HARM, 60, 15, 30, 20, 15, 0, 0}, {0, 90, 60, 80}, 0, 0, FX(0, 30, 30, 70), PAT(6)},
    {"DRIFT BASS", {DR_HARM, 30, 10, 20, 15, 10, 0, 0}, {0, 80, 110, 40}, 0, 1, FX(0, 0, 10, 20), PAT(2)},
    {"WIND GLASS", {DR_GLASS, 90, 40, 70, 100, 20, 40, 80}, {100, 100, 127, 120}, 0, 0, FX(0, 30, 50, 110), PAT(5)},
    {"SLOW BLOOM", {DR_FIFTH, 75, 20, 15, 50, 15, 127, 0}, {127, 100, 127, 112}, 0, 0, FX(0, 40, 35, 100), PAT(5)},
    {"INIT", {0, 50, 40, 30, 30, 20, 30, 0}, {0, 64, 127, 30}, 0, 0, FX(0, 0, 0, 0)},   /* (chord-harp fork) the EDIT defaults, dry */
};
static const char DRIFT_PT[] = "PPPPEPPPPKBXPN";
_Static_assert(sizeof DRIFT_PT == NELEM(DRIFT_PRESETS) + 1u, "a type a preset");
/* (engine 21: its favourites in favorites.factory[5]'s upper half, favorites.c favorite_byte: 128 presets at most) */
_Static_assert(NELEM(DRIFT_PRESETS) <= 128u, "DRIFT's favourites");
/* (2026-11-11) the SOUND page's rows (core.h sound_row_t): the engine's eight settings, then params.c's generic rows */
static const sound_row_t DRIFT_ROWS[] = {
    {"SOURCE", {P_E0, P_E1, 0xFF}, {0, 0, 0}},
    {"MOTION", {P_E2, P_E3, P_E4}, {0, 0, 0}},
    {"AIR", {P_E5, P_E6, P_E7}, {0, 0, 0}},
};

static const engine_t ENG_DRIFT = {
    .rows = DRIFT_ROWS,
    .nrows = (uint8_t)(sizeof DRIFT_ROWS / sizeof DRIFT_ROWS[0]),
    .name = "DRIFT",
    .page_title = {"DRIFT", "AIR"},
    .edit = {
        {"PARTS", F_ENUM, 0, DR_N - 1, DR_HARM, N_DRPARTS, 0},
        {"TILT", F_PCT, 0, 127, 50, 0, 0},
        {"DRIFT", F_PCT, 0, 127, 40, 0, 0},
        {"SPEED", F_PCT, 0, 127, 30, 0, 0},
        {"FLICK", F_PCT, 0, 127, 30, 0, 0},
        {"DETUNE", F_PCT, 0, 127, 20, 0, 0},
        {"BLOOM", F_PCT, 0, 127, 30, 0, 0},
        {"BREATH", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = DRIFT_PRESETS,
    .npresets = NELEM(DRIFT_PRESETS),
    .ptype = DRIFT_PT,
    .note_on = drift_note_on,
    .render = drift_render,
    .knob = {P_E1, P_E2, P_E4, P_E7},
    .poly = 6,
};
