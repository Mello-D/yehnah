/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork) MODAL: struck tines and bars, our own, on KALIMBA's resonators (eng_kalimba.c: its pool, its pluck,
 * its modes; MODAL's materials there too, MDL_MATS, and its pickup and tremolo in kalimba_render). An electric piano's tine, the mallet bars and a piano, each a bank of two-pole resonators
 * at its modes: RHODES (a tine and its tone bar: the first mode strong, a bell-like partial high up that dies fast; the
 * pickup's growl from BARK), VIBES (aluminium bars tuned 1 : 4 : 10, long), MARIMBA (rosewood tuned 1 : 4 : 10, the
 * tube under it holding the first mode, the upper ones short), XYLO (rosewood tuned 1 : 3 : 6, short and bright), GLOCK
 * (steel bars free at both ends, 1 : 2.76 : 5.40 : 8.93, ringing), CELESTA (the same struck softly, in a box).
 *   HARD  the mallet or hammer: soft (felt, yarn) to hard (rubber, brass); with velocity
 *   DECY  how long it rings; TONE how fast the upper modes die (bright .. dull); DAMP a damper on it at key up
 *   BARK  the pickup (RHODES: its growl as it is hit harder; the bars: a little grit)
 *   TREM  the tremolo: the vibraphone's motor, the suitcase's (0 off, then slow .. fast)
 *   CLIK  the strike's noise
 * EDIT 1  MODEL HARD DECY TONE        EDIT 2  BARK TREM DAMP CLIK
 * Its own envelope (ownenv), as KALIMBA's. The tremolo's phase is the part's (mdl_trem, once a block). */

static void modal_block(track_t *t)
{
    uint32_t part = (uint32_t)(t - trk) % NPART;
    float hz = t->p[P_E5] > 0 ? 1.0f + (float)t->p[P_E5] * (7.0f / 127.0f) : 0.0f;   /* TREM: 1 .. 8 Hz */
    mdl_trem[part] += hz * (float)CTL / (float)FS;
    if (mdl_trem[part] >= 1.0f)
        mdl_trem[part] -= 1.0f;
}

static const preset_t MODAL_PRESETS[] YEHNAH_RES = {
    /* name, {MODEL HARD DECY TONE BARK TREM DAMP CLIK}, {A D S R} (not used: its own envelope), fenv, mono, sends */
    {"RHODES", {MDL_RHODES, 50, 70, 60, 40, 0, 60, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 0, 30)},
    {"RHODES BARK", {MDL_RHODES, 80, 64, 70, 100, 0, 60, 40}, {0, 64, 127, 40}, 0, 0, FX(10, 0, 0, 25)},
    {"SUITCASE", {MDL_RHODES, 55, 72, 60, 50, 45, 60, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 20, 10, 35)},
    {"SOFT EP", {MDL_RHODES, 25, 80, 45, 15, 0, 50, 10}, {0, 64, 127, 40}, 0, 0, FX(0, 30, 10, 45)},
    {"BRIGHT EP", {MDL_RHODES, 90, 60, 90, 70, 0, 60, 50}, {0, 64, 127, 40}, 0, 0, FX(0, 20, 15, 30)},
    {"VIBES", {MDL_VIBES, 50, 90, 60, 0, 0, 0, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 10, 45)},
    {"SOFT VIBES", {MDL_VIBES, 20, 100, 40, 0, 30, 0, 10}, {0, 64, 127, 40}, 0, 0, FX(0, 20, 20, 60)},
    {"MARIMBA", {MDL_MARIMBA, 45, 64, 64, 0, 0, 0, 40}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 35)},
    {"BASS MARIMBA", {MDL_MARIMBA, 35, 80, 50, 0, 0, 0, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 30), XP(P_TRANS + 1, -12)},
    {"XYLOPHONE", {MDL_XYLO, 80, 64, 70, 0, 0, 0, 70}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 30)},
    {"GLOCKENSPIEL", {MDL_GLOCK, 80, 70, 80, 0, 0, 0, 60}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 15, 45)},
    {"CELESTA", {MDL_CELESTA, 40, 64, 60, 0, 0, 30, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 10, 50)},
    {"MUSIC BOX", {MDL_GLOCK, 60, 80, 100, 0, 0, 0, 40}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 20, 60), XP(P_TRANS + 1, 12)},
    /* PIANO: a stiff-string bank (eng_kalimba.c km_coefs): two strings a hair apart (the prompt sound, then the
     * aftersound), partials 2..7 stretched, the hammer at 1/8 of the string */
    {"GRAND PIANO", {MDL_PIANO, 55, 70, 60, 0, 0, 70, 30}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 0, 30)},
    {"UPRIGHT", {MDL_PIANO, 60, 55, 50, 0, 0, 80, 45}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 20)},
    {"SOFT PIANO", {MDL_PIANO, 25, 75, 40, 0, 0, 60, 10}, {0, 64, 127, 40}, 0, 0, FX(0, 20, 0, 40)},
    {"BRIGHT PIANO", {MDL_PIANO, 90, 65, 85, 0, 0, 70, 50}, {0, 64, 127, 40}, 0, 0, FX(0, 10, 0, 25)},
    {"FELT PIANO", {MDL_PIANO, 8, 60, 25, 0, 0, 50, 5}, {0, 64, 127, 40}, 0, 0, FX(0, 25, 0, 45)},
    {"INIT", {0, 50, 70, 60, 40, 0, 60, 30}, {0, 64, 127, 30}, 0, 0, FX(0, 0, 0, 0)},   /* (chord-harp fork) the EDIT defaults, dry */
};
static const char MODAL_PT[] = "IIIIIMMMMMEEEIIIIIN";
_Static_assert(sizeof MODAL_PT == NELEM(MODAL_PRESETS) + 1u, "a type a preset");
/* (engine 25: its favourites in favorites.factory[9]'s upper half, favorites.c favorite_byte: 128 presets at most) */
_Static_assert(NELEM(MODAL_PRESETS) <= 128u, "MODAL's favourites");
static const engine_t ENG_MODAL = {
    .name = "MODAL",
    .page_title = {"BAR", "PLAY"},
    .edit = {
        {"MODEL", F_ENUM, 0, MDL_N - 1, MDL_RHODES, N_MDMODEL, 0},
        {"HARD", F_PCT, 0, 127, 50, 0, 0},
        {"DECY", F_PCT, 0, 127, 70, 0, 0},
        {"TONE", F_PCT, 0, 127, 60, 0, 0},
        {"BARK", F_PCT, 0, 127, 40, 0, 0},
        {"TREM", F_PCT, 0, 127, 0, 0, 0},
        {"DAMP", F_PCT, 0, 127, 60, 0, 0},
        {"CLIK", F_PCT, 0, 127, 30, 0, 0},
    },
    .presets = MODAL_PRESETS,
    .npresets = NELEM(MODAL_PRESETS),
    .ptype = MODAL_PT,
    .note_on = kalimba_note_on,
    .render = kalimba_render,
    .block = modal_block,
    .knob = {P_E1, P_E2, P_E4, P_E5},
    .ownenv = 1,
    .done = kalimba_done,        /* (s[0], its pool entry: KALIMBA's) */
};
