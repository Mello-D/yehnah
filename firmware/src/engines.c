/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SEQ > PATTERNS) and the parts' sounds at power-on. */
#include "dsp.c"

/* (chord-harp fork, Clipper's FX > FILTER > MODL) the filter model of the engines with a low-pass of their own */
static inline int32_t fm_model(const track_t *t)
{
    return clamp(t->p[P_FMODEL], FM_SVF, FM_LAST);
}

/* (chord-harp fork: Clipper's engines.c, fbd8434) UNISON's start-phase spread (voice.c unison_spread): r (Q32: a turn
 * of the note at pitch p16, 1/16 semitone) in turns of an oscillator d16 (1/16 semitone) above it, the share of the same
 * time; per note */
static uint32_t uni_turns(uint32_t r, int32_t p16, int32_t d16)
{
    uint32_t b = pitch_inc((uint32_t)clamp(p16, 0, 2047));
    return b ? (uint32_t)(((uint64_t)r * pitch_inc((uint32_t)clamp(p16 + d16, 0, 2047))) / b) : r;
}

/* (chord-harp fork: Clipper's eng_hybrid.c hy_inc) the phase increment of pitch16 (1/16 semitone) + off (1/4096
 * semitone), fine on it (1/4096: vmod_t.fine) */
static uint32_t hy_inc(int32_t pitch16, int32_t fine, int32_t off)
{
    int32_t q = (pitch16 << 8) + off, f;
    uint32_t inc;
    if (q < 0)
        q = 0;
    if (q > (2047 << 8))
        q = 2047 << 8;
    inc = pitch_inc((uint32_t)(q >> 8));
    f = fine + (((q & 255) * 3792) >> 16);
    if (f)
        inc += (uint32_t)((int32_t)(inc >> 12) * f);
    return inc;
}

#include "eng_analog.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_formant.c"
#include "eng_wheel.c"
#include "eng_drum.c"           /* DRUM: the 8-lane DRUM-X kit (drumx_voice.c) */
#if YEHNAH_NOISE
#include "eng_noise.c"
#endif
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#if YEHNAH_ANIMAL
#include "eng_animal.c"         /* (0.8.3) ANIMAL: animal calls (CMOS PULSE WHSTL WING GLOTT) */
#endif
#include "eng_juno.c"           /* (chord-harp fork) JUNO: Clipper's Juno-6 / 60 / 106 (GPL-3.0) */
#include "eng_omni.c"           /* (chord-harp fork) OMNI: FoMni's Omnichord voices (GPL-3.0; Chordian's, CC0) */
#include "eng_kalimba.c"        /* (chord-harp fork) KALIMBA: FiMba-1's tine (GPL-3.0) */
#include "eng_guitar.c"         /* (chord-harp fork) GUITAR: a plucked string, nylon, steel, electric */
#include "eng_acid.c"           /* (chord-harp fork) ACID: Clipper's TB-303 (GPL-3.0) */
#include "eng_fmini.c"          /* (2026-11-11) FMINI: FMini-1's Minimoog engine and its 200 patches (GPL-3.0) */
#include "eng_drift.c"          /* (chord-harp fork) DRIFT: wandering partials, ambient */
#include "eng_plaits.c"         /* (chord-harp fork) PLAITS: four of Mutable Instruments Plaits' models (MIT, via Optimist) */
#include "eng_strings.c"        /* (chord-harp fork) STRINGS: Clipper's string machine (GPL-3.0, via Optimist) */
#include "eng_vps.c"            /* (chord-harp fork) VPS: Clipper's vector phaseshaping (GPL-3.0, via Optimist) */
#include "eng_modal.c"          /* (chord-harp fork) MODAL: struck tines and bars on KALIMBA's resonators */
#include "eng_wseq.c"           /* (chord-harp fork) WSEQ: Clipper's wave sequencing (GPL-3.0, via Optimist) */
#include "eng_terrain.c"        /* (chord-harp fork) TERRAIN: Clipper's wave terrain (GPL-3.0, fbd8434) */
#include "eng_vector.c"         /* (chord-harp fork) VECTOR: Clipper's vector synthesis (GPL-3.0, fbd8434) */
#include "eng_hybrid.c"         /* (chord-harp fork) HYBRID: Clipper's 3 oscillators, 2 filters, the matrix (GPL-3.0, 7bc15a9) */
#include "eng_digital.c"        /* DIGITAL: four-operator FM (chord-harp fork, 2026-10-30: Clipper's again, GPL-3.0) */

/* PHYS (engine 9): retired in TONIC (its 50 KB of voice state went to the song and the drums); SAMPLE (4), GRAIN (8) and
 * SLICE (13): retired in Yehnah (core.h eng_gone). The numbers stay reserved, as DIGITAL's: the stores name engines by
 * index and the table is append-only. ENG_GONE below has no presets and is never offered (eng_ok); a sound of theirs
 * that arrives anyway -- a project, a user preset, the editor's PRESET / SET G_ENGSEL, a backup -- plays as ANALOG's
 * first preset (eng_live; project.c proj_phys_gone, ui.c set_engine_of, ui_input.c). A PHYS DRUM of before 1.0 and a
 * SAMPLE PERC still load as DRUM first (core.h drum_from_phys, drum_from_perc) */
static void phys_gone_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void phys_gone_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_GONE = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {
        {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0},
    },
    .note_on = phys_gone_note_on,
    .render = phys_gone_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
#define ENGI_PHYS_TO 0u          /* the engine a retired engine's sound plays as: ANALOG (its first preset) */

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_GONE,                   /* 4: reserved (SAMPLE, retired in Yehnah) */
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_GONE,                   /* 6: reserved (TRIO, folded into ANALOG in 0.4: core.h analog_from_trio) */
    &ENG_WHEEL,                  /* 7 */
    &ENG_GONE,                   /* 8: reserved (GRAIN, retired in Yehnah) */
    &ENG_GONE,                   /* 9: reserved (PHYS, retired in TONIC: its sounds play as ANALOG) */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
#if YEHNAH_NOISE
    &ENG_NOISE,                  /* 11 */
#else
    &ENG_GONE,                   /* 11: reserved (NOISE, not built in the chord-harp fork: YEHNAH_NOISE=1 builds it) */
#endif
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
    &ENG_GONE,                   /* 13: reserved (SLICE, retired in Yehnah) */
#if YEHNAH_ANIMAL
    &ENG_ANIMAL,                 /* 14 (ENGI_ANIMAL, 0.8.3) */
#else
    &ENG_GONE,                   /* 14: reserved (ANIMAL, not built in the chord-harp fork: YEHNAH_ANIMAL=1 builds it) */
#endif
    &ENG_JUNO,                   /* 15 (ENGI_JUNO, chord-harp fork) */
    &ENG_OMNI,                   /* 16 (ENGI_OMNI, chord-harp fork) */
    &ENG_KALIMBA,                /* 17 (ENGI_KALIMBA, chord-harp fork) */
    &ENG_MINI,                   /* 18 (ENGI_MINI, 2026-11-10: the slot LCD-4 left) */
    &ENG_GUITAR,                 /* 19 (ENGI_GUITAR, chord-harp fork) */
    &ENG_ACID,                   /* 20 (ENGI_ACID, chord-harp fork) */
    &ENG_DRIFT,                  /* 21 (ENGI_DRIFT, chord-harp fork) */
    &ENG_PLAITS,                 /* 22 (ENGI_PLAITS, chord-harp fork) */
    &ENG_STRINGS,                /* 23 (ENGI_STRINGS, chord-harp fork) */
    &ENG_VPS,                    /* 24 (ENGI_VPS, chord-harp fork) */
    &ENG_MODAL,                  /* 25 (ENGI_MODAL, chord-harp fork) */
    &ENG_WSEQ,                   /* 26 (ENGI_WSEQ, chord-harp fork) */
    &ENG_TERRAIN,                /* 27 (ENGI_TERRAIN, chord-harp fork) */
    &ENG_VECTOR,                 /* 28 (ENGI_VECTOR, chord-harp fork) */
    &ENG_HYBRID,                 /* 29 (ENGI_HYBRID, chord-harp fork) */
};

/* (chord-harp fork, Clipper's) the presets that set the arp: MODE, RATE, OCT, GATE (out of preset_t, 2026-11-04: 3 of
 * 543 set one, 2 KB of zeros); any other preset sets none (the track's goes on: ui.c preset_values) */
static const struct { const preset_t *p; uint8_t arp[4]; } PRESET_ARP[] = {
    {&LOFI_PRESETS[2], {1, 2, 2, 40}},           /* 8BIT ARP */
    {&JUNO_PRESETS[11], {1, 2, 2, 70}},          /* ARP PLUCK */
    {&JUNO_PRESETS[16], {18, 2, 2, 60}},         /* TURING JUNO (TURNG: our 18, Clipper's 15) */
};
static const uint8_t *preset_arp(const preset_t *pr)
{
    uint32_t i;
    for (i = 0; i < NELEM(PRESET_ARP); i++)
        if (PRESET_ARP[i].p == pr)
            return PRESET_ARP[i].arp;
    return 0;
}

/* (Yehnah, 2026-11-12) the presets that bring an FX scene (params.c N_SCENE: 1 ROOM .. 10 REVRS) with them: the reverb and
 * delay buses are song-wide, so loading one of these sets the whole bus, as a USER preset saved with an FX scene does
 * (upreset.c). A side table as PRESET_ARP, not a field of preset_t (543 presets); any other preset leaves the bus alone */
static const struct { const preset_t *p; uint8_t scene; } PRESET_SCENE[] = {
    {&JUNO_PRESETS[NELEM(JUNO_PRESETS) - 6u], 4},    /* DREAM HAZE: SHIMR */
    {&JUNO_PRESETS[NELEM(JUNO_PRESETS) - 5u], 2},    /* GAZE WALL: HALL */
    {&JUNO_PRESETS[NELEM(JUNO_PRESETS) - 4u], 10},   /* GAZE SWELL: REVRS */
    {&JUNO_PRESETS[NELEM(JUNO_PRESETS) - 3u], 8},    /* NEON DRIVE: PING */
    {&JUNO_PRESETS[NELEM(JUNO_PRESETS) - 2u], 7},    /* OUTRUN PAD: TAPE */
    {&ANALOG_PRESETS[NELEM(ANALOG_PRESETS) - 3u], 8},    /* NEON SAW: PING */
    {&ANALOG_PRESETS[NELEM(ANALOG_PRESETS) - 2u], 2},    /* GAZE FUZZ: HALL */
    {&STRINGS_PRESETS[NELEM(STRINGS_PRESETS) - 2u], 4},  /* DREAM STR: SHIMR */
    {&DRIFT_PRESETS[NELEM(DRIFT_PRESETS) - 2u], 10},     /* GAZE DRIFT: REVRS */
};
/* the same by name inside one engine's table (SC: the table, the preset's name, the scene) */
#define SC(arr, nm, sc) {arr, (uint16_t)NELEM(arr), nm, sc}
static const struct { const preset_t *arr; uint16_t n; const char *name; uint8_t scene; } PRESET_SCENE_NM[] = {
    SC(DRIFT_PRESETS, "SHIMMER SWELL", 4), SC(DRIFT_PRESETS, "GLASS DRONE", 4), SC(DRIFT_PRESETS, "BELL HAZE", 4),
    SC(DRIFT_PRESETS, "OCTAVE CLOUD", 5), SC(DRIFT_PRESETS, "WIND GLASS", 5), SC(DRIFT_PRESETS, "DEEP DRONE", 2),
    SC(DRIFT_PRESETS, "SLOW BLOOM", 2),
    SC(FORMANT_PRESETS, "AIR CHOIR", 2), SC(FORMANT_PRESETS, "CHOIR AAH", 2), SC(FORMANT_PRESETS, "GHOST VOX", 4),
    SC(GUITAR_PRESETS, "AMBIENT GTR", 4), SC(GUITAR_PRESETS, "SURF TWANG", 3), SC(GUITAR_PRESETS, "SHOEGAZE GTR", 2),
    SC(GUITAR_PRESETS, "TAPE ECHO GTR", 7),
    SC(KALIMBA_PRESETS, "WIND CHIMES", 4), SC(KALIMBA_PRESETS, "GLASS HARP", 4),
    SC(OMNI_PRESETS, "DREAM HARP", 4),
    SC(VECTOR_PRESETS, "SPACE DRONE", 5),
    SC(PLAITS_PRESETS, "RAIN GLASS", 5),
    SC(ANALOG_PRESETS, "ATMOS PAD", 2),
    SC(JUNO_PRESETS, "DREAM PAD", 2),
};
static uint32_t preset_scene(const preset_t *pr)
{
    uint32_t i, k;
    for (i = 0; i < NELEM(PRESET_SCENE); i++)
        if (PRESET_SCENE[i].p && PRESET_SCENE[i].p == pr)
            return PRESET_SCENE[i].scene;
    for (i = 0; i < NELEM(PRESET_SCENE_NM); i++)
        if (pr >= PRESET_SCENE_NM[i].arr && pr < PRESET_SCENE_NM[i].arr + PRESET_SCENE_NM[i].n) {
            const char *a = pr->name, *b = PRESET_SCENE_NM[i].name;
            for (k = 0; a[k] && a[k] == b[k]; k++) {}
            if (!a[k] && !b[k])
                return PRESET_SCENE_NM[i].scene;
        }
    return 0;
}

/* (chord-harp fork) a preset's extra values (preset_t.x), after the rest of its sound */
static void preset_extras(int16_t *p, const preset_t *pr)
{
    uint32_t i;
    for (i = 0; i + 1u < NELEM(pr->x) && pr->x[i] > 0; i += 2u)
        if ((uint32_t)pr->x[i] - 1u < P_E0)
            p[pr->x[i] - 1] = pr->x[i + 1];
}


/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with YEHNAH_FM4 it comes last). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them. (chord-harp fork, 2026-10-26) A to Z by name (was
 * 0.9.9's DRUM-X, VOICE, ANIMAL first, as the parts at power-on; those are TRK_DEF's own); PROGRAM CHANGE keeps
 * 0.9.8's (ENGINE_PC_ORDER) */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    ENGI_ACID, ENGI_ANALOG,
#if YEHNAH_ANIMAL
    ENGI_ANIMAL,
#endif
    ENGI_DIGITAL,
    ENGI_DRIFT, ENGI_DRUM, ENGI_FM6, ENGI_MINI, ENGI_GUITAR, ENGI_HYBRID, ENGI_JUNO, ENGI_KALIMBA, ENGI_LOFI, ENGI_MODAL,
#if YEHNAH_NOISE
    ENGI_NOISE,
#endif
    ENGI_OMNI, ENGI_PHASE, ENGI_PLAITS, ENGI_STRINGS, ENGI_TERRAIN, ENGI_VECTOR, ENGI_VOICE, ENGI_VPS, ENGI_WHEEL, ENGI_WSEQ,
};
/* (0.9.9) PROGRAM CHANGE n loads the n + 1-th sound of the PRESETS list in the order of 0.9.8 (ANIMAL last: 0.8.3's
 * numbers kept), so a MIDI rig's program numbers name the sounds they named before the display order changed; the
 * PRESETS list itself follows ENGINE_ORDER (ui.c preset_all_at_in) */
static const uint8_t ENGINE_PC_ORDER[NENG_SHOWN] = {
    ENGI_ANALOG, ENGI_FM6, ENGI_DIGITAL,
    ENGI_PHASE, ENGI_LOFI, ENGI_VOICE, ENGI_WHEEL,
#if YEHNAH_NOISE
    ENGI_NOISE,
#endif
    ENGI_DRUM,
#if YEHNAH_ANIMAL
    ENGI_ANIMAL,
#endif
    ENGI_JUNO, ENGI_OMNI, ENGI_KALIMBA, ENGI_GUITAR, ENGI_ACID, ENGI_DRIFT,   /* (chord-harp fork: after the rest) */
    ENGI_PLAITS, ENGI_STRINGS, ENGI_VPS, ENGI_MODAL, ENGI_WSEQ, ENGI_TERRAIN, ENGI_VECTOR, ENGI_HYBRID, ENGI_MINI,
};

/* the engines one can pick, in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e) { return e < NENGINES && !eng_gone(e); }
/* the engine a stored engine number plays on: a retired one ANALOG, a bad number ANALOG */
static uint32_t eng_live(uint32_t e)
{
    return e >= NENGINES ? 0u : eng_gone(e) ? ENGI_PHYS_TO : e;
}
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    e = eng_live(e);                             /* (PHYS: ANALOG) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* factory sequence patterns: SEQ > PATTERNS loads one into the selected track (ui.c pat_load); a preset
 * only suggests one with PAT(n), loading a sound never touches the steps. Absolute notes, loaded as they
 * are (DRUM plays them as GM drums, SLICE as slices; SCL TRANS and OCT transpose): 0 = rest;
 * flags 1 = accent, 2 = slide, 4 = tie (holds the previous note). Names: at most 8 characters */
#define T_ 4
static const struct {
    const char *name;
    uint8_t note[16], flags[16];
} PATTERNS[] YEHNAH_RES = {   /* (chord-harp fork: the resource region, res.c) */
    {"ACID", {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50},          /* 1 */
     {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1}},
    {"OFFBEAT", {0, 36, 0, 36, 0, 36, 0, 48, 0, 36, 0, 36, 0, 39, 0, 43},          /* 2 bass */
     {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},
    {"MELODY", {60, 0, 67, 0, 72, 67, 0, 64, 62, 0, 69, 0, 74, 69, 0, 67},         /* 3 pluck */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"LEAD", {72, 0, 0, 74, 0, 0, 76, 0, 79, 0, 76, 0, 74, 0, 0, 0},               /* 4 */
     {1, T_, 0, 2, T_, 0, 0, 0, 1, 0, 2, 0, 0, T_, T_, 0}},
    {"PAD", {60, 0, 0, 0, 0, 0, 0, 0, 57, 0, 0, 0, 55, 0, 0, 0},                   /* 5 long notes */
     {0, T_, T_, T_, T_, T_, T_, 0, 0, T_, T_, 0, 0, T_, T_, 0}},
    {"KEYS", {0, 0, 60, 0, 0, 63, 0, 0, 0, 0, 60, 0, 0, 65, 0, 63},                /* 6 offbeat stabs */
     {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}},
    {"BELL", {72, 0, 0, 79, 0, 0, 84, 0, 0, 0, 76, 0, 0, 0, 0, 0},                 /* 7 sparse */
     {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"SUB", {36, 0, 0, 0, 0, 0, 0, 36, 0, 0, 34, 0, 0, 0, 0, 0},                   /* 8 low, held */
     {1, T_, T_, T_, 0, 0, 0, 0, 0, 0, 0, T_, T_, T_, 0, 0}},
    /* SLICE (eng_slice.c): note = C4 + slice */
    {"CHOP", {60, 61, 62, 67, 64, 65, 60, 69, 68, 70, 62, 67, 72, 72, 74, 64},     /* 9 16 slices re-ordered */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"STUTTER", {60, 60, 61, 61, 62, 0, 63, 63, 64, 65, 65, 0, 66, 66, 66, 67},    /* 10 8 slices, repeats */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"SLICES", {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75},   /* 11 in order */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* DRUM (General MIDI: 36 kick, 38 snare, 42 closed / 46 open hi-hat) */
    {"BEAT", {36, 42, 42, 42, 38, 42, 36, 42, 36, 42, 42, 36, 38, 42, 46, 42},     /* 12 */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* 16ths up a C minor arpeggio, twice: the line the ARP presets (RAVE, ARP 8BIT, ARP LEAD) suggest
     * now that the arpeggiator is the track's and a preset no longer switches it on */
    {"ARP", {48, 51, 55, 60, 63, 67, 72, 75, 48, 51, 55, 60, 63, 67, 72, 75},       /* 13 */
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
};
#undef T_
#define NPATTERNS (RES_OK() ? (uint32_t)(sizeof PATTERNS / sizeof PATTERNS[0]) : 0u)   /* (none while the resource region
                                                         * is missing: the users' only) */

/* (0.9.9) the parts at power-on, INIT SYSTEM and NEW PROJECT (engine, preset, PATTERNS[n - 1] in the sequencer,
 * 0 = empty: all are). (chord-harp fork, 2026-11-02, the user's) T1 JUNO's pad for the chords, T2 ANALOG's bass, T3
 * ANALOG's lead, T4 DRUM's 909 kit (not DRUM-X; LCD-4 909 since 2026-11-05), T5 LOFI's game lead, T6 FM6's electric piano. Each its preset's sound,
 * dry (ui.c power_sound_of: no DIST CHOR DLY REV send, the effects are the player's to add) */
#define TRK_PR_JUNO 0u           /* JUNO 106 PAD (eng_juno.c JUNO_PRESETS) */
#define TRK_PR_ANALOG 21u        /* MINI BASS (eng_analog.c: Clipper's, the MOOG ladder) */
#define TRK_PR_LEAD 26u          /* PROPH LEAD (eng_analog.c) */
#define TRK_PR_KIT 10u           /* LCD-4 909 (eng_drum.c DRUM_PRESETS; 909 CLUB dropped 2026-11-05) */
static const uint8_t TRK_DEF[NPART][3] = {{ENGI_JUNO, TRK_PR_JUNO, 0}, {ENGI_ANALOG, TRK_PR_ANALOG, 0},
                                          {ENGI_ANALOG, TRK_PR_LEAD, 0}, {ENGI_DRUM, TRK_PR_KIT, 0},
                                          {ENGI_LOFI, 0, 0}, {ENGI_FM6, 0, 0}};
/* JUNO 106 PAD, MINI BASS, PROPH LEAD, LCD-4 909, GAME LEAD, TINE EP */
/* the parts at power-on through 0.9.8 (the same columns): what an old project's tracks it had no room for load as
 * (project.c proj_new_tracks, PROJ_DEF_SOUND: format 1's tracks 2..4, a 4-track format's T5 / T6): never change it.
 * ANALOG ACID, FM6 PAD (was DIGITAL PAD), LOFI PULSE LD (3: BYTE first), DRUM KIT; (0.6) PHASE's first, WHEEL's first */
static const uint8_t TRK_OLD[NPART][3] = {{0, 11, 0}, {ENGI_FM6, 4, 0}, {3, 3, 0}, {ENGI_DRUM, 0, 0}, {2, 0, 0}, {7, 0, 0}};
/* (chord-harp fork, 2026-10-30: Clipper's presets; old projects need not load, the user's rule) T1 ANALOG SYN BRASS (was
 * ACID: its place now Clipper's ACID 303, TRANS and GLIDE its own), FM6 SOFT PAD, LOFI NES BASS, PHASE CZ BASS, WHEEL
 * SOUL ORGAN */
static uint32_t trk_old_engine(uint32_t i) { return TRK_OLD[i % NPART][0]; }
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
