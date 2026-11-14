/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* YEHNAH core types: tracks, voices, engines, parameters.
 * Four tracks, each a synth part: its own engine, preset, parameters, voices and 64-step
 * pattern. The parts share a measured-load budget of sounding voices (voice.c). Drums are the DRUM
 * engine (General MIDI map) on any part; the SAMPLE engine's PERC set is retired (drum_from_perc).
 * Sections: sizes, parameters, voices and engines, tracks and the song, system. */
#include <stdint.h>

/* ------------------------------------------------------------ sizes --- */
#define NVOICE 8                 /* slots per part; shared admission budget is separate (voice.c) */
#define NPART 6                  /* synth parts: tracks 1..6 (0.6: was 4) */
#define NTRK NPART               /* tracks (the formats and the protocol count these): every track is a part */
#define NSTEP 64
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms at 44.1 kHz (a key waits 0..1 half, then plays 1 half later) */
#define YEHNAH_FM4 0            /* (chord-harp fork, 2026-10-30) 0: the old DIGITAL's operator pages (P_FM1_ATK ..) gone;
                                  * DIGITAL (engine 1) is Clipper's again, a plain engine (its EDIT pages only) */
#ifndef FELUCCA_FXSCENE
#define FELUCCA_FXSCENE 0       /* (2026-11-06) 1: FX > SCENE and SAVE > USER FX (the FX scenes, params.c) are in the build; 0: both pages are
                                 * gone, a user preset stores no scene (its byte stays 0) and loads none */
#endif
#ifndef FELUCCA_MACROPAGE
#define FELUCCA_MACROPAGE 0     /* (2026-11-06) 1: HOME > MACRO (M1..M4 on the knobs, ui_lens.c CE_HOME) is in HOME's chain; 0: hidden (the macros
                                 * themselves stay: the FX held layer's MACROS keys, the matrix, scenes) */
#endif
#ifndef FELUCCA_AMBIENT
#define FELUCCA_AMBIENT 0       /* (chord-harp fork, 2026-11-05) 1: ARP > AMBIENT (amb.c) is in the build: its page, its keys' pool, its
                                 * tick. 0 (now): out; the params P_AMB..P_ASPC stay (numbering stable, all OFF), the code stays in the tree */
#endif
#ifndef YEHNAH_HORT
#define YEHNAH_HORT 1           /* (chord-harp fork, 2026-11-14) 1 (default): DRUM's MACHINE HORT (drum_hort.c: Hortator's drum models, seven kit presets) is in the build. 0: out (flash) */
#endif
#define YEHNAH_FM4CONV 0        /* (chord-harp fork, 2026-10-30) no conversion of DIGITAL's sounds to FM6: DIGITAL is back */
#ifndef YEHNAH_ANIMAL
#define YEHNAH_ANIMAL 1         /* (chord-harp fork) ANIMAL (eng_animal.c): built again (2026-10-25, ~7.6 KB, room from the
                                  * slimmed English dictionary); 0 leaves it out: engine 14 reserved, its sounds play as
                                  * ANALOG's first (eng_gone). It needs NOISE (noise_inc) */
#endif
#ifndef YEHNAH_NOISE
#define YEHNAH_NOISE 1          /* (chord-harp fork) NOISE (eng_noise.c): built again (2026-10-25, ambient textures;
                                  * ~2.7 KB); 0 leaves it out, engine 11 reserved. ANIMAL needs it (noise_inc) */
#endif
#define NENGINES 30              /* the stores' engine numbers 0..29 (engines.c ENGINES[], append-only; 0.8.3: 14 ANIMAL;
                                  * chord-harp fork: 15 JUNO, 16 OMNI, 17 KALIMBA, 18 LCD-4 (retired), 19 GUITAR, 20 ACID, 21 DRIFT,
                                  * 22 PLAITS, 23 STRINGS, 24 VPS, 25 MODAL, 26 WSEQ, 27 TERRAIN, 28 VECTOR, 29 HYBRID. A user preset's favourite and the
                                  * editor's FAV_GET / FAV_SET name NENGINES (was 15); favorites.c favorite_byte: engines
                                  * 16..30 in rows 0..14's upper half) */
#define ENGI_DIGITAL 1u          /* (chord-harp fork, 2026-10-30: Clipper's DIGITAL, a plain engine again) */
#define NENG_SHOWN (NENGINES - 5 - !YEHNAH_ANIMAL - !YEHNAH_NOISE)   /* the engines one can pick: PRESETS,
                                                * the EDIT layer, the editor,
                                                * in the display order of engines.c ENGINE_ORDER (PHYS 9, and in
                                                * Yehnah SAMPLE 4, GRAIN 8, SLICE 13, the fork's
                                                * LCD-4 18: retired) */
/* (0.6.71) the second core (hal/fm1_multicore.h; fx.c mix_block hands it some synth tracks). MC_CPU(): the core
 * running this, for the scratch each core has its own of (fx.c part_buf and the mix, mod.c's matrix, fm6_core.c's
 * buses, voice.c's filter flags). The host (tests): no second core; mc_host runs its jobs at once, as core 1 */
#ifndef YEHNAH_DUAL_CORE
#define YEHNAH_DUAL_CORE 0
#endif
#if YEHNAH_DUAL_CORE
#define MC_CPU() fm1_cnum()
#else
static uint8_t mc_host;
#ifndef MC_HOST_STORAGE
#define MC_HOST_STORAGE
#endif
static MC_HOST_STORAGE uint8_t mc_host_cpu; /* the concurrent host test uses thread-local core identity */
#define MC_CPU() ((uint32_t)mc_host_cpu)
#endif
/* (0.6.78) the hottest audio code in RAM, for core 0 (yehnah.c YEHNAH_HOTRAM): app.ld puts these functions'
 * sections (-ffunction-sections) in .hot_text by name, so the compiler builds them as before (this clang inlines
 * nothing across a section attribute, not even a static inline). Core 1 keeps running the flash copies (fx.c
 * mix_part_m, fx_run): it now and then reads a word of RAM wrong (hal/fm1_multicore.h) */
#ifndef YEHNAH_HOTRAM
#define YEHNAH_HOTRAM 1
#endif
#include "res_decl.h"                   /* (chord-harp fork) YEHNAH_RES, RES_OK, ENG_NP: the resource region (res.c) */
#define UP_SLOTS 32u             /* user presets (upreset.c) */
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------- parameters --- */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
    F_ONOFF, F_OCT, F_STEPS,
    F_PCTINV                     /* (0.6.37) a share of 127 shown and turned the other way: 0 is 100 % */
};

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;

enum {                          /* per-track parameters */
    P_LEVEL,
    P_ATK, P_DEC, P_SUS, P_REL,
    P_ED_FLT, P_ED_PIT, P_ED_SHP, P_ED_FX,     /* P_ED_FX (once unused): 0.4's ENV LOOP, P_ELOOP */
    P_LRATE, P_LWAVE, P_LPHASE, P_LFADE,
    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP,
    P_AMODE, P_ARATE, P_AOCT, P_AGATE,
    P_ASWING, P_APROB, P_AHOLD, P_AORDER,
    P_ROOT, P_SCALE, P_QUANT, P_TRANS,
    P_SLEN, P_SDIV, P_SSWING, P_SGATE,
    P_DIST, P_CHOR, P_DLY, P_REV,
    P_VOICE, P_GLIDE, P_PAN, P_MUTE,
    P_GLMODE, P_PRIO, P_ALLOC, P_DETUNE,
    P_SLCR, P_SLPAT, P_SLRATE, P_SLDEPTH,      /* SLICER insert (slicer.c) */
    P_M1SRC, P_M1DST, P_M1AMT,                 /* modulation matrix (mod.c): 4 slots of SRC, DST, AMT; */
    P_M2SRC, P_M2DST, P_M2AMT,                 /* new common parameters go just before P_E0 (user presets */
    P_M3SRC, P_M3DST, P_M3AMT,                 /* and projects map by count) */
    P_M4SRC, P_M4DST, P_M4AMT,
    P_FM1_ATK, P_FM1_DEC, P_FM1_SUS, P_FM1_REL, P_FM1_LEVEL,
    P_FM2_ATK, P_FM2_DEC, P_FM2_SUS, P_FM2_REL, P_FM2_LEVEL,
    P_FM3_ATK, P_FM3_DEC, P_FM3_SUS, P_FM3_REL, P_FM3_LEVEL,
    P_FM4_ATK, P_FM4_DEC, P_FM4_SUS, P_FM4_REL, P_FM4_LEVEL,
    P_SOFS, P_POFS,                            /* (Yehnah; the former chord keys) the sequence offset: OFS steps, PIT semitones */
    P_LN0, P_LN1, P_LN2, P_LN3, P_LN4, P_LN5, P_LN6, P_LN7,   /* DRUM lane levels (eng_drum.c, #97): KICK .. BELL */
    P_DTYPE, P_DTONE,                          /* (Yehnah, FUNB) DIST's type and tone (fx.c track_dist) */
    P_MSLEN, P_MSDIV, P_MSSLW,                 /* (Yehnah, FUNB) the modulation sequence (mod.c MS_STEP): its length, */
    P_MS0, P_MS15 = P_MS0 + 15,                /* rate and slew, its 16 levels */
    P_FTYPE,                                   /* (0.5) the filter's type: LP HP BP COMB (eng_analog.c, voice.c) */
    P_FCUT, P_FRES,                            /* .. every other engine's: a filter on the part's sum (voice.c track_filter) */
    P_HPLATE, P_OAUTO, P_OSTR,                 /* (chord-harp fork) SCL > HARP's PLATE (the plate's octave), SCL > OMNI's
                                                * AUTO (the chord under the strings) and STRINGS (their octave): seq.c
                                                * harp_map, eng_omni.c om_auto */
    P_PROG,                                    /* (chord-harp fork) SCL > PROG's progression (harp.c PROGS, seq.c harp_id) */
    P_PHAMT, P_PHRATE, P_PHFB,                 /* (chord-harp fork) FX > PHASER on the part's sum (voice.c track_phaser):
                                                * AMOUNT (0: off), RATE, FDBK; the sound's (a load sets them) */
    /* (chord-harp fork, 2026-10-30: Clipper's voice layer, for its presets; isod89/clipper-fm1 fbd8434. In the slots of
     * the retired SOUND B and BLEND, P_XB P_XAMT, and after them) */
    P_FMODEL,                                  /* FX > FILTER's MODEL: the engines' own low-pass (ANALOG's: dsp.c fmod_*) */
    P_VELF, P_MWDST,                           /* velocity to the cutoff; the mod wheel's target (MD_*): voice.c */
    P_VINT,                                    /* VINTAGE: each voice a little its own (voice.c vintage_pitch) */
    P_SPICE, P_SLOOP,                          /* SPICE / LOOP: how much the arp's notes vary, every LOOP notes (seq.c
                                                * spice_of; Clipper's DICE, which variation: always its first) */
    P_TRIM,                                    /* the sound's level trim, 1/2 dB (a preset's: engine_t.trim; fx.c) */
    P_AMB, P_ADEN, P_AMOT, P_ASPC,             /* (chord-harp fork, 2026-11-05) ARP > AMBIENT: OFF HOLD LATCH, DENSITY,
                                                * MOTION, SPACE (seq.c amb.c); the track's, not the sound's */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_COUNT
};
#define P_LRTRG P_FM1_ATK              /* (0.6.12) LOFI BYTE / FLOAT's RTRG: a note restarts the formula's time (voice.c
                                         * voice_start), in a slot of the retired DIGITAL's (0: OFF, its default) */
#define P_TCHAN P_FM1_DEC              /* (0.6.37) SHIFT > CHANCE: how much of the track's steps it skips, 0 none (100 %,
                                         * every project and preset before it); seq.c trk_chance; DIGITAL's OP1 DEC slot */
#define P_KTRK P_FM3_ATK               /* (chord-harp fork, 2026-10-30; 0.6.45's MORPH slot, retired) KEY>F: the filter's
                                         * key tracking on top of the engine's own, % of 1:1 around C4 (voice.c; 0: the
                                         * engine's alone, every other sound): the Juno-106 bank's VCF KYBD */
#define P_CLEN P_FM3_DEC               /* (0.6.47) SHIFT > C.LEN: the step components as a lane of their own, steps
                                         * 1..C.LEN's read in turn as the track plays (polymetric over the steps); 0 OFF
                                         * (each step its own: every project and preset before it); seq.c comp_lane */
#define P_DXIMG P_FM3_REL              /* (0.6.48) a DRUM track's EDIT > IMAGE: one knob through the kit's pan maps
                                         * (eng_drum.c dx_img_pan; 0: every sound in the middle, as before) */
/* (0.6.81) SCL > CHORD (seq.c chord_build), in DIGITAL's OP4 slots (0, every project and preset before it: OFF,
 * CLOSE, no strum) */
#define P_CHRD P_FM4_ATK               /* OFF; in the scale: TRIAD 7TH ADD9 SUS2 SUS4; any key: MAJ MIN 7 MAJ7 MIN7 POWER */
#define P_CVOI P_FM4_DEC               /* CLOSE, LEAD (each chord the inversion nearest the last), OPEN, +BASS */
#define P_GSET P_FM2_DEC               /* (2026-11-10) SCL > GUITAR's SET: guitchordi's 50 chord styles (harp.c gt_*; was unused) */
#define P_GTUN P_FM1_REL               /* (2026-11-10) SCL > GUITAR > TUNE's TUNING: guitchordi's 14 tunings (harp.c GT_TUN; was unused) */
#define P_GSHP P_FM2_ATK               /* .. its SHAPE 1..6 (0..5) of each chord in that tuning (was unused) */
#define P_CSTR P_FM4_REL               /* STRUM: 0 at once; up to ~63 ms between the notes, low to high */
/* (chord-harp fork) SCL's CHORD, HARP and OMNI tabs each keep their own P_CVOI (VOICING, HARP's SET, OMNI's SET) and the
 * CHORD tab its mode, packed in DIGITAL's last slot (ui.c chord_tab): bits 0..3 the CHORD tab's mode (1..11, else TRIAD), 4..5 its
 * VOICING, 6..9 HARP's SET (its low 4 bits; 13..15 its high 3: the J-6 sets, 2026-10-27), 10..12 OMNI's SET (whatever an
 * older store holds there reads as some valid choice) */
#define P_CTABS P_FM4_LEVEL
/* (0.6.87) SEQ > HUMAN, in DIGITAL's OP1 SUS / LEVEL and OP2 SUS slots (127, every project and preset before
 * it: shown as 0 %, none): how much each note's VELOCITY, TIME (a little late) and GATE vary at random as the sequencer
 * plays them (seq.c hum_amt); stored the other way round, 127 - the amount */
#define P_HVEL P_FM1_SUS
#define P_HTIM P_FM1_LEVEL
#define P_HGAT P_FM2_SUS
#define P_HRQ P_FM2_LEVEL              /* (0.6.88) SEQ > TIMING's REC Q: how much live recording snaps to the grid, 127 (every
                                         * project before it) 100 %: on it; less: the notes keep that much of where they were */
/* (0.7.8) three more in DIGITAL's last slots; SUS and LEVEL slots hold 127 in every project and preset before them,
 * so 127 is each one's "as before": DRIFT and DUST read 127 as 0 % (F_PCTINV, as HUMAN), R.DEPTH 127 as 100 % */
#define P_DRIFT P_FM3_SUS              /* EDIT > MORPH KNOB 2: each voice's pitch (and filter) wandering slowly, its own way */
#define P_WDUST P_FM3_LEVEL            /* WHEEL's EDIT > ROTOR: an old organ's crackle and hiss */
#define P_WRDEP P_FM4_SUS              /* .. the rotor's depth (the Leslie's swirl), 100 % as before */
#define P_ELOOP P_ED_FX                /* (0.4) ENV LOOP: OFF / ON (voice.c env_tick) */
/* (0.6.9) AUTO PAN (fx.c mix_part), in the slots of the old per-track SLICER (one on a bus since 0.6.3, its
 * slots stored, never read but by proj_sl_migrate). Laid so what old stores hold means OFF: DPTH where SLCR was (0, or
 * 1..2 from a project of before 0.6.3: inaudible), RATE where PAT was (1..16: slow), WAVE where RATE was (1: TRI), SYNC
 * where DEPTH was (127: BEAT) */
#define P_APDEP P_SLCR                 /* depth 0..127 (0: off) */
#define P_APRATE P_SLPAT               /* FREE: 0..127 as the LFO's RATE; BEAT: 0..7, 4 BARS .. 1/32 (fx.c AP_SIXT) */
#define P_APWAVE P_SLRATE              /* SIN TRI SAW SQR S&H (N_LWAVE) */
#define P_APSYNC P_SLDEPTH             /* FREE / BEAT (any value but 0: BEAT) */

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_DREL, G_ROUTE, G_INFO,   /* G_DREL (Yehnah): DUCK's release; was G_SYNC, a placeholder ("--", 0..0) */   /* G_ROUTE: MIDI IN, 0 CH1-4 (channels 1..4 -> parts 1..4, 5..16 ignored), 1 SEL (seq.c) */
    G_SLOT, G_DJF, G_LOAD, G_SAVE,  /* G_DJF (0.9.1): GLO > MIXER KNOB 4, the master DJ FILTER (fx.c djf_block), -64 .. 63:
                                 * 0 out of the chain, below a low-pass sweeping down, above a high-pass sweeping up. Was
                                 * G_NAME, a placeholder ("--", 0..0, never set): every project before holds 0 there */
    G_ENGSEL, G_CLIP,           /* no page: the editor switches the engine with a SET of G_ENGSEL. G_CLIP (Yehnah):
                                 * the master clipper; was G_ENGGO, unused (0..0) */
    G_CLRSEQ, G_INITSND,
    G_RTYPE,                    /* REVERB TYPE: 0 ROOM, 1 SPRING (fx.c). Was G_DRCH, the GM drum part's MIDI
                                 * channel (inert since 1.0, never read); projects of formats before FUN7 load it
                                 * as ROOM (project.c proj_rtype_room) */
    G_PUNCH, G_DUCK,            /* (Yehnah) the drum bus's PUNCH and the kick's DUCK of the synths (fx.c, eng_drum.c).
                                 * Were G_DRLVL / G_DRREV, inert since 1.0 (0..0): the GM drum part's level and
                                 * reverb send, read only by the import of a project of before 1.0
                                 * (proj_drums_to_part, which clears them after) */
    G_NSTORE,                   /* the 27 a project's header holds (FUN7 on: their ids kept) */
    G_DHPF = G_NSTORE,          /* (Yehnah, FUNB: after the punch-in lane, project.c) the delay's feedback low cut (0.4:
                                 * in DLY's TONE; stored, unused); 0.6.9: the CHORUS's STEREO (G_CSTER), */
    G_WIDTH,                    /* the buses' stereo width (the delay's right echo later, the chorus's taps apart), */
    G_RMOD, G_RRATE,            /* the reverb's modulation (ROOM: its combs' lengths; SPRING: its wobble) depth, rate, */
    G_RPRE,                     /* its pre-delay */
    G_RFILT, G_RWIDE,           /* (0.3) the reverb's tone (darker / brighter) and width, */
    G_DPIT, G_DSPRY,            /* the delay's grains: their pitch (0: a plain delay) and spray */
    G_STRN,                     /* (0.4) every sequence moved this many steps of its scale (seq.c seq_step) */
    G_SLMODE, G_SLPAT, G_SLRATE, G_SLDEP,   /* (0.6.3) the SLICER, one on a bus (slicer.c): OFF / GATE / STUT, its */
    G_SLT1, G_SLT6 = G_SLT1 + 5, G_SLFX,    /* pattern, step, depth; which tracks go in (and FX: the delay, reverb and */
    G_SLPLEN, G_SLPQ,                       /* chorus returns); the repeats' pitch sequence: its length, CHR / SCL, */
    G_SLP0, G_SLP15 = G_SLP0 + 15,          /* its 16 steps (semitones) */
    G_DMOD,                     /* (0.6.86) the delay's tape drift (WOW and FLUTTER) depth (project.c: its own block, 'DM') */
    G_DTYPE, G_DPING,           /* (0.6.90) the delay's TYPE (ANALOG / DIGITAL) and PING-PONG (project.c: block 'DT') */
    G_PFXLEN, G_PFXRES,         /* (0.6.99) the punch-in lane's length (1..64 steps) and resolution (1/8 1/16 1/32)
                                 * (pfx.c; project.c: block 'PX') */
    G_BLA, G_BLB, G_BLEND, G_BMOVE,   /* (chord-harp fork, 2026-10-29) SAVE > MORPH: TRACK A, TRACK B (0 OFF, 1..6), BLEND
                                 * A .. B, MOVE (what moves it: voice.c bl_*, fx.c bl_block; project.c: block 'BL') */
    G_COUNT
};
#define G_SL_N (G_SLP15 + 1 - G_SLMODE)     /* the SLICER's globals (stored together: project.c, song_chain.c) */
#define G_CSTER G_DHPF                 /* (0.6.9) CHORUS STEREO -64..63: 0 as before (G_DHPF's 0), - narrower .. mono */
#define G_SYNC G_DREL                   /* (the old names, for the formats' importers and their tests) */
#define G_ENGGO G_CLIP
#define G_DRLVL G_PUNCH
#define G_DRREV G_DUCK

/* stored parameters of an older layout -> today's P_* order. A store keeps np = the P_COUNT it was
 * written with; common parameters are only ever added just before P_E0, so the first np - 8 are
 * P_LEVEL.. in order and the last 8 are P_E0..P_E7; the parameters a store does not have take def[]
 * (user presets, projects of formats 1 and 2) */
static void params_by_count(int16_t *out, const int16_t *in, uint32_t np, const int16_t *def)
{
    uint32_t i, nc = np - 8u;
    for (i = 0; i < P_E0; i++)
        out[i] = i < nc ? in[i] : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = in[nc + i];
}

/* engine indices the stores name (engines.c ENGINES[]: append-only) */
#define ENGI_ANALOG 0u           /* (0.5: named for its FILTER page) */
#define ENGI_PHYS 9u
#define ENGI_DRUM 10u
#define ENGI_GRAIN 8u
#define ENGI_SLICE 13u
#define ENGI_SAMPLE 4u
#define ENGI_ANIMAL 14u          /* (0.8.3) eng_animal.c */
#define ENGI_PHASE 2u            /* (0.9.9: named for the power-on parts and the display order, engines.c) */
#define ENGI_LOFI 3u
#define ENGI_VOICE 5u            /* eng_formant.c ENG_FORMANT */
#define ENGI_WHEEL 7u
#define ENGI_NOISE 11u
#define ENGI_JUNO 15u            /* (chord-harp fork) eng_juno.c, from Clipper */
#define ENGI_OMNI 16u            /* (chord-harp fork) eng_omni.c, from FoMni */
#define ENGI_KALIMBA 17u         /* (chord-harp fork) eng_kalimba.c, from FiMba-1 */
#define ENGI_MINI 18u            /* (2026-11-11) eng_fmini.c: FMINI, FMini-1's Minimoog engine (GPL-3.0), in the slot LCD-4 left */
/* FMINI's patch (2026-11-11): the settings E0..E7 do not hold live in the track's P_LN0..P_LN6, packed in bit fields (DRUM's lane levels:
 * no other engine reads them; every other slot of the FM operator block is some feature's parameter under another name: P_CHRD, P_HVEL,
 * P_KTRK, P_DRIFT ..). The list page (EDIT > PATCH) reaches them by the virtual ids VF(k) (params.c page_desc); the engine by fmx_get.
 * k: OSC 1..3 RANGE (LO 32' 16' 8' 4' 2'), WAVE (TRI SHARK SAW SQR PLS-W PLS-N SIN), mixer LEVEL of OSC 1..3 and the NOISE (0 = off),
 * OSC 2 / 3 DETUNE in cents (-700..700), TUNE (-64..63: +-100 cents), MOD MIX (LFO .. noise), NOISE colour (WHITE PINK), MOD>OSC,
 * MOD>FILTER, the LFO's shape (the part's / SQUARE), the output (LP HP) */
enum { FX_R1, FX_R2, FX_R3, FX_W1, FX_W2, FX_W3, FX_L1, FX_L2, FX_L3, FX_LN, FX_D2, FX_D3, FX_TU, FX_MM, FX_PK, FX_MO, FX_MF, FX_SQ,
       FX_HP,
       /* (2026-11-11) the SOUND rows of other engines in the same packed slots (only one engine's are live in a track; the engine's pset
        * clears them and sets the defaults): ANALOG's OSC 2 RANGE / WAVE, PW offset, SYNC; DIGITAL's operator 2..4 LEVEL and DETUNE;
        * FORMANT's F4 and KEY TRK */
       FX_A_RNG, FX_A_WAV, FX_A_PW, FX_A_SYN, FX_D_L2, FX_D_L3, FX_D_L4, FX_D_T2, FX_D_T3, FX_D_T4, FX_F_F4, FX_F_KT, NFMX };
/* the motion ids (parameter locks, automation, the editor's wire): FMINI's patch setting k is the id P_COUNT + k (motion.c mp_get / mp_set) */
#define MOT_NID (P_COUNT + NFMX)
#define MOT_W ((MOT_NID + 31u) / 32u)            /* words of a bit per motion id */
#define VF(k) (0xA0u + (k))              /* a patch setting's id on a page (real parameter ids are < P_COUNT, the menu rows 0xD0..) */
#define IS_VF(id) ((id) >= 0xA0u && (id) < 0xA0u + NFMX)
static const struct { uint8_t slot, shift, bits; int16_t bias; } FMX_BITS[NFMX] = {
    {P_LN0, 0, 3, 0}, {P_LN0, 3, 3, 0}, {P_LN0, 6, 3, 0}, {P_LN0, 9, 3, 0}, {P_LN0, 12, 3, 0}, {P_LN1, 0, 3, 0},
    {P_LN2, 0, 7, 0}, {P_LN2, 7, 7, 0}, {P_LN3, 0, 7, 0}, {P_LN3, 7, 7, 0}, {P_LN4, 0, 11, 700}, {P_LN5, 0, 11, 700},
    {P_LN1, 8, 7, 64}, {P_LN6, 0, 7, 0}, {P_LN1, 3, 1, 0}, {P_LN1, 4, 1, 0}, {P_LN1, 5, 1, 0}, {P_LN1, 6, 1, 0}, {P_LN1, 7, 1, 0},
    {P_LN0, 0, 3, 0}, {P_LN0, 3, 3, 0}, {P_LN1, 0, 7, 64}, {P_LN0, 6, 1, 0},   /* (the other engines' extras: one byte a slot, so a project and a USER
                                                         * preset keep them; only MINI's patch needs the 'P' 'X' block) */
    {P_LN0, 0, 7, 0}, {P_LN1, 0, 7, 0}, {P_LN2, 0, 7, 0}, {P_LN3, 0, 7, 64}, {P_LN4, 0, 7, 64}, {P_LN5, 0, 7, 64},
    {P_LN0, 0, 7, 0}, {P_LN1, 0, 7, 0}};
static inline int32_t fmx_get(const int16_t *p, uint32_t k)
{
    return (((int32_t)p[FMX_BITS[k].slot] >> FMX_BITS[k].shift) & ((1 << FMX_BITS[k].bits) - 1)) - FMX_BITS[k].bias;
}
static inline void fmx_set(int16_t *p, uint32_t k, int32_t v)
{
    int32_t m = (1 << FMX_BITS[k].bits) - 1, w = v + FMX_BITS[k].bias;
    w = w < 0 ? 0 : w > m ? m : w;
    p[FMX_BITS[k].slot] = (int16_t)(((int32_t)p[FMX_BITS[k].slot] & ~(m << FMX_BITS[k].shift)) | (w << FMX_BITS[k].shift));
}
#define ENGI_GUITAR 19u          /* (chord-harp fork) eng_guitar.c: a plucked string (Karplus-Strong), our own */
#define ENGI_ACID 20u            /* (chord-harp fork) eng_acid.c, from Clipper: a TB-303 bass line */
#define ENGI_DRIFT 21u           /* (chord-harp fork) eng_drift.c: wandering partials, ambient, our own */
#define ENGI_PLAITS 22u          /* (chord-harp fork) eng_plaits.c: four Plaits models, from Optimist */
#define ENGI_STRINGS 23u         /* (chord-harp fork) eng_strings.c: Clipper's string machine, via Optimist */
#define ENGI_VPS 24u             /* (chord-harp fork) eng_vps.c: vector phaseshaping, from Clipper via Optimist */
#define ENGI_MODAL 25u           /* (chord-harp fork) eng_modal.c: tines and bars, KALIMBA's resonators, our own */
#define ENGI_WSEQ 26u            /* (chord-harp fork) eng_wseq.c: wave sequencing, from Clipper via Optimist */
#define ENGI_TERRAIN 27u         /* (chord-harp fork, 2026-11-02) eng_terrain.c: Clipper's wave terrain */
#define ENGI_VECTOR 28u          /* (chord-harp fork, 2026-11-02) eng_vector.c: Clipper's vector synthesis */
#define ENGI_HYBRID 29u          /* (chord-harp fork, 2026-11-04) eng_hybrid.c: Clipper's HYBRID voice */
#define ENGI_TRIO 6u             /* (0.4) folded into ANALOG (analog_from_trio) */
/* (Yehnah) the retired engines: PHYS, and the sample engines SAMPLE GRAIN SLICE (their 123 KB of samples and GRAIN's
 * 27 KB of RAM went to DRUM-X and the effects). Their numbers stay reserved (engines.c ENG_GONE), never offered; a sound
 * of theirs that arrives -- a project, a user preset, the editor -- plays as ANALOG's first preset (SAMPLE's old PERC
 * set as DRUM first: drum_from_perc) */
static inline int eng_gone(uint32_t e)
{
    return e == ENGI_PHYS || e == ENGI_SAMPLE || e == ENGI_GRAIN || e == ENGI_SLICE || e == ENGI_TRIO ||
           (!YEHNAH_ANIMAL && e == ENGI_ANIMAL) || (!YEHNAH_NOISE && e == ENGI_NOISE) ||   /* (chord-harp fork: not built) */
           0;
}
/* (0.4) a sound stored before 0.4 -> today's E values, in place: LOFI (3) {.., SWP, VIB, ARP, TONE} -> {.., CUT
 * (its TONE), VIB, BEND 0, MASK 0}; ANALOG (0) KTR -> INT 0 (the key tracking fixed at its old default). Projects
 * (project.c: the kit's marker below 3, or an older format) and user presets (upreset.c: no UP_V04_MARK) */
static void sound_v04(uint32_t engine, int16_t *e)
{
    if (engine == 3u) {
        e[4] = e[7];
        e[6] = e[7] = 0;
    } else if (engine == 0u) {
        e[7] = 0;
    }
}
/* (0.4) TRIO (engine 6) folded into ANALOG: its sound's E values -> ANALOG's, in place, the nearest WAVE (its
 * three saws SAW3, its sync sets SYNC, its ring sets RING, pulses PWM, triangles TRI, noise as NOIS), INT2 as the
 * SYNC / RING ratio (DTN: 16 x (2^(st/12) - 1)), DTN, CUT and RES kept; 1 = it was TRIO (its engine is ANALOG now):
 * projects, user presets, the editor's PRESET and SET (project.c, upreset.c, ui.c). {WAVE INT2 INT3 DTN MODE CUT RES
 * PW} -> {WAVE DTN MIX NOIS CUT RES DRV KTR} */
static int analog_from_trio(uint32_t engine, int16_t *e)
{
    static const uint8_t W[16] = {7, 4, 4, 7, 2, 0, 1, 4, 0, 5, 5, 5, 6, 6, 6, 6};
    static const uint8_t R[25] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12, 14, 16, 18, 20, 22, 25, 27, 30, 34, 37, 41, 45, 49,
                                  54};   /* (16 x (2^(st/12) - 1), st 0..24) */
    uint32_t w = (uint32_t)e[0] & 15u;
    int32_t st = e[1] < 0 ? -e[1] : e[1];
    if (engine != ENGI_TRIO)
        return 0;
    e[0] = W[w];
    e[1] = W[w] >= 5u ? (int16_t)R[st > 24 ? 24 : st] : (int16_t)(e[3] > 0 ? e[3] : 6);
    e[2] = (int16_t)(w == 8u ? 0 : 64);
    e[3] = (int16_t)(w == 7u ? 60 : w == 8u ? 100 : 0);
    e[4] = e[5];                                        /* CUT */
    e[5] = e[6];                                        /* RES */
    e[6] = 0;
    e[7] = 64;
    return 1;
}
/* PHYS MODEL DRUM (MODEL 4, before 1.0) -> the DRUM engine, its E values in place: {MODEL, TUNE, TONE, DECY,
 * SNAP, ACC, KICK, PERC} -> {MRPH 64, TUNE, TONE, DECY, NOIS 64, FM 0, -, DRV 0}. 1 = it was one (its engine is
 * ENGI_DRUM now); projects (project.c) and user presets (upreset.c) */
static int drum_from_phys(uint32_t engine, int16_t *e)
{
    if (engine != ENGI_PHYS || e[0] != 4)
        return 0;
    e[0] = e[4] = 64;
    e[5] = e[6] = e[7] = 0;                             /* (ACC is gone: E5 is FM now, off) */
    return 1;
}

/* SAMPLE SET 4 was PERC, the GM-mapped drum kit (tools/gen_samples.py); retired after 1.0.2: its index stays
 * (SET / GRAIN SRC 4 is an alias of PIANO; 5..7, once USR1..3, clamp to it). A sound that selected it is the DRUM engine with
 * its default kit (eng_drum.c DRUM_PRESETS[0]: the same GM key map, so its patterns still play as drums): its E
 * values become the kit's, the rest of the sound (mix, sends, matrix, ..) stays. 1 = it was one (its engine is
 * ENGI_DRUM now); projects (project.c proj_perc), user presets (upreset.c up_migrate), factory preset 4 and
 * favourites (ui.c, settings_persist.c). Idempotent */
#define SMP_SET_PERC 4u
#define DRUM_KIT_E {64, 64, 70, 64, 64, 0, 0, 0}   /* {MRPH, TUNE, TONE, DECY, NOIS, FM, WARP, DRV} (eng_drum.c) */
static int drum_from_perc(uint32_t engine, int16_t *e)
{
    static const int16_t KIT[8] = DRUM_KIT_E;
    uint32_t i;
    if (engine != ENGI_SAMPLE || e[0] != (int16_t)SMP_SET_PERC)
        return 0;
    for (i = 0; i < 8u; i++)
        e[i] = KIT[i];
    return 1;
}

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON, V_PUNI2, V_PUNI4 };   /* P_VOICE (chord-harp fork: Clipper's poly unison,
                                                 * UNI2 / UNI4: 2 or 4 voices a note, voice.c) */
static inline int voice_poly(int m) { return m == V_POLY || m >= V_PUNI2; }   /* a note per key, several at once */
enum { FT_LP, FT_HP, FT_BP, FT_COMB };          /* P_FTYPE: the part's FILTER (voice.c track_filter) */
enum { FM_SVF = 1, FM_MOOG = 2, FM_MS20, FM_DIODE, FM_SEM, FM_NOTCH, FM_BP, FM_HP, FM_LAST = FM_HP };   /* P_FMODEL (dsp.c) */
enum { MWD_OFF, MWD_VIB, MWD_CUT, MWD_SHP, MWD_WAH, MWD_TREM };   /* P_MWDST (Clipper's MD_*; mod.c has its own MD_) */
typedef struct {
    uint8_t note, vel, gate, active;
    uint8_t stage;               /* env: 0 off, 1 attack, 2 decay/sustain, 3 release, 4 given up (fading) */
    uint8_t kfade;               /* (0.6.74) stage 4: the fade's blocks so far (voice.c env_tick) */
    uint16_t fin_d;              /* (0.6.81) a chord glide's new note fading in (seq.c chord_glide); 0.6.82: as its glide
                                  * goes, the distance it set out from (1/16 st; 0 none) */
    int32_t env;                 /* Q24 */
    int32_t env_out;             /* last control-rate amplitude, Q15 */
    int32_t pitch16, pitch_cur;  /* 1/16 semitone, with glide */
    int32_t gstep;               /* glide TIME mode: 1/16 st per control tick, Q8 (0.6.82: slow glides smooth), 0 = RATE */
    int32_t fine;                /* unison detune: phase increment * (1 + fine / 4096) */
    uint32_t ph[3];
    int32_t s[8];                /* engine state (filters, envs) */
    uint32_t age;
    uint8_t mvel;                /* mod.c: the note-on velocity (before UNISON scaling) and RAND of the note */
    uint8_t gacc;                /* (0.6.82) the glide's fraction of a 1/16 st carried (gstep Q8) */
    int8_t henv;                 /* (0.6.92) HUMAN: this note's DEC and REL moved (steps of the knobs; voice.c env_tick) */
    int16_t mrnd;
    int16_t vdrift, vdtgt;       /* (chord-harp fork: Clipper 0.9's) VINTAGE: the voice's slow pitch drift now, and where it
                                  * goes (Q15; voice.c vintage_pitch) */
    int16_t gfrom;               /* (0.9.8) where the glide set out from (1/16 st; VOICE sings it as an S-curve: voice.c
                                  * glide_sung; in the struct's padding, its size as before) */
} voice_t;

typedef struct {                 /* per-voice control-rate modulation, computed in voice.c */
    uint32_t inc;                /* phase increment of the base pitch */
    int32_t pitch16;
    int32_t amp0, amp1;          /* Q15 ramp over the block */
    int32_t cutoff;              /* 0..127 << 8 */
    int32_t shape;               /* 0..127 << 8 */
    int32_t envq15;              /* env value (for engines that use it as a mod source) */
    int32_t fine;                /* the residual below 1/16 semitone in inc: unison detune, TUNE, bend (1/4096) */
} vmod_t;

typedef struct {
    char name[16];               /* (chord-harp fork, 2026-11-04) in the preset itself: the tables are in the resource
                                  * region (res.c), the names with them */
    int8_t e[8];                 /* P_E0..P_E7 (signed: an interval below the note; every value fits) */
    uint8_t env[4];              /* ATK DEC SUS REL */
    int8_t fenv;                 /* ENV -> FILTER amount (-64..63) */
    uint8_t mono;                /* 1 = MONO (bass / lead), 0 = POLY */
    /* the rest of the patch; each value is stored + 1, 0 = the default */
    uint8_t fx[4];               /* DIST, CHORUS, DELAY, REVERB sends */
    uint8_t pat;                 /* suggested pattern (PATTERNS[pat - 1], 0 = none): only a hint. A sound load
                                  * never touches the steps; SEQ > PATTERNS loads a pattern (ui.c pat_load) */
    int8_t x[8];                 /* (chord-harp fork, Clipper's) more of the patch: up to 4 (track parameter id + 1,
                                  * value) pairs, 0 = none: glide, pitch / LFO modulation, voice mode (engines.c
                                  * preset_extras) */
} preset_t;
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define PAT(n) .pat = (n)
#define XP(...) .x = {__VA_ARGS__}  /* XP(P_GLIDE + 1, 30, P_LD_SHP + 1, 40): parameter id + 1, value */

/* (2026-11-11) a row of the SOUND page (params.c sound_rows, ui_lzlist.c): KNOB 1 picks the row, KNOB 2..4 its (up to) three settings
 * (a parameter id, or VF(k) for a patch setting); name: shown in KNOB 1's card only; s[k]: the
 * setting's 3-letter label, 0 = the parameter's own name */
typedef struct { const char *name; uint8_t id[3]; const char *s[3]; } sound_row_t;

struct track;
typedef struct {                 /* an engine (engines.c ENGINES[]; the eng_*.c files) */
    const char *name;            /* "ANALOG" (PRESETS, the editor) */
    const char *page_title[2];   /* EDIT 1 and EDIT 2 */
    param_desc_t edit[8];        /* P_E0..P_E7 */
    const preset_t *presets;     /* (chord-harp fork) in the resource region: read through ENG_NP */
    uint8_t npresets;
    const char *ptype;           /* (chord-harp fork) each preset's sound type, a letter a preset (ui.c PTYPE); 0 none */
    const int8_t *trim;          /* (chord-harp fork) each preset's level trim (P_TRIM, 1/2 dB; Clipper's tools/level_presets.py), 0 none */
    uint8_t knob[4];             /* HOME: the four parameters on KNOB 1..4 */
    uint8_t poly;                /* voice cap for POLY and UNISON, 0 = NVOICE */
    uint8_t sampled;             /* 1 = plays recorded material (a position, not a phase): voice.c keeps
                                  * no phases over a retrigger, spreads none for UNISON, renders at SUS 0 */
    uint8_t keep;                /* bit k: s[k] is kept when a sounding voice is retriggered (filters) */
    uint8_t oneshot;             /* 1 = hits (DRUM): always POLY, no glide; amp ends its voices (eng_drum.c) */
    void (*note_on)(struct track *t, voice_t *v);
    void (*render)(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
    /* optional (0 = none): the voice amplitude instead of the ADSR curve, once per control tick;
     * gets the ADSR value (Q15, env_tick already ran: it still gates the voice), returns Q15 */
    int32_t (*amp)(struct track *t, voice_t *v, int32_t adsr);
    /* optional: a mode-dependent descriptor of EDIT k (the same range and default as edit[k],
     * another label / value names), 0 = edit[k] */
    const param_desc_t *(*desc)(const struct track *t, uint32_t k);
    /* optional: once per block and part, before its voices (also with no voice sounding) */
    void (*block)(struct track *t);
    /* optional: the note of key k (0 = the lowest) when the engine maps the keys itself, else -1
     * (then the scale keyboard of seq.c kb_map) */
    int32_t (*keys)(const struct track *t, uint32_t k);
    /* 1 = the engine's own envelopes are the voice's amplitude (FM6): voice.c renders it at 1.0 (no ADSR, no
     * velocity; LFO -> AMP, the matrix's AMP and the fades still apply; ENV is 0) and the voice ends when
     * done() says so (once per control tick, before the render), not at the end of the ADSR's release */
    uint8_t ownenv;
    int (*done)(struct track *t, voice_t *v);
    /* optional: after a preset's values are in p (engines.c preset_values), the engine's own: FMINI's patch into its parameters */
    uint32_t (*pset)(int16_t *p, uint32_t pi);   /* returns 1 when the patch set the arp (P_AMODE..P_APROB) */
    /* optional: the SOUND page's rows (0 = four generic rows over edit[]) */
    const sound_row_t *rows;
    uint8_t nrows;
} engine_t;

/* ------------------------------------------------- tracks, the song --- */
enum { ST_NOTE, ST_TIE, ST_REST };
#define SF_ACCENT 1u
#define SF_SLIDE 2u
#define SF_RATCH_SH 3u                   /* RATCH: the hits of a NOTE step - 1 (0..3: x1..x4, seq.c seq_ratchet); */
#define SF_RATCH (3u << SF_RATCH_SH)     /* bits 3..4, so a user preset's pattern (flag 4 = tie) carries it too */
#define SF_MFINE_SH 5u                   /* (0.9.1) MICRO's fine part: the 64ths under its sixteenth (ST_MICRO), bits 5..6 */
#define SF_MFINE (3u << SF_MFINE_SH)
#define NLANE 8                  /* drum lanes of a step (the DRUM engine's: eng_drum.c DRUM_LANE_NOTE) */
typedef struct {                 /* acid-style step: up to 4 notes (POLY), time, accent, slide; drum hits */
    uint8_t note[4];
    uint8_t n;                   /* notes in use, 0 = empty */
    uint8_t time;                /* ST_NOTE / ST_TIE / ST_REST */
    uint8_t flags;               /* SF_ACCENT | SF_SLIDE | SF_RATCH */
    uint8_t vel;
    uint8_t hit;                 /* bit l: lane l hits (its GM note, on any engine): the DRUM grid */
    uint8_t acc;                 /* bit l: that hit is accented (velocity 127) */
    uint8_t probability;         /* 0 = legacy 100%; 1..100 = percent, 101 = silent */
    uint8_t comp, carg;          /* (0.6.39) the step's component (CP_*, 0 none) and its arguments (seq.c) */
    uint8_t comp2, carg2;        /* (0.6.82) a second component (SEQ > COMP 2), the same way */
    uint8_t cwhen;               /* (0.6.82) WHEN each plays: bits 0-1 comp's, 2-3 comp2's (CW_*) */
    uint8_t ctime;               /* (0.6.82) bits 0-2 DIV: RATCH's and ECHO's parts a step/2 .. /8 apart (0: the step
                                  * shared out, as before); bits 3-6 LEN: a part's gate n/16 (0: the track's GATE) */
    uint8_t lm[3];               /* (0.8.5) each drum lane's own way of playing on this step (a DRUM track: its hit and
                                  * its notes on the lane): 3 bits a lane, lane l at bits 3l..3l+2 of the 24, LM_* */
} step_t;
/* (0.8.5) a lane's mode (eng_drum.c, the DRUM-X octaves): its hit ratcheted x2 / x4 / x8 over the step, or struck SHORT,
 * GHOST, FLAM; that lane only. 0: as ever (every step before 0.8.5) */
enum { LM_NONE, LM_X2, LM_X4, LM_X8, LM_SHORT, LM_GHOST, LM_FLAM, LM_N };
static uint32_t step_lmode(const step_t *s, uint32_t l)
{
    uint32_t v = (uint32_t)s->lm[0] | (uint32_t)s->lm[1] << 8 | (uint32_t)s->lm[2] << 16, m = (v >> (3u * (l & 7u))) & 7u;
    return m < LM_N ? m : LM_NONE;
}
static void step_set_lmode(step_t *s, uint32_t l, uint32_t m)
{
    uint32_t v = (uint32_t)s->lm[0] | (uint32_t)s->lm[1] << 8 | (uint32_t)s->lm[2] << 16, sh = 3u * (l & 7u);
    v = (v & ~(7u << sh)) | ((m < LM_N ? m : 0u) << sh);
    s->lm[0] = (uint8_t)v; s->lm[1] = (uint8_t)(v >> 8); s->lm[2] = (uint8_t)(v >> 16);
}
static int step_lm_any(const step_t *s) { return s->lm[0] | s->lm[1] | s->lm[2]; }
static uint32_t lm_hits(uint32_t m) { return m == LM_X2 ? 2u : m == LM_X4 ? 4u : m == LM_X8 ? 8u : 1u; }   /* a step */
static uint32_t lm_touch(uint32_t m) { return m >= LM_SHORT && m <= LM_FLAM ? m - LM_SHORT + 1u : 0u; }   /* TOUCH 1..3 */
/* (0.6.92) EVERY: on which loops of the pattern the step's components play (both of them; seq.c every_on), in cwhen's
 * bits 0-3: ALWAYS, 1:2 (the odd loops), 2:2, SKIP (kept, never), 1:3 2:3 3:3, 1:4 2:4 3:4 4:4, 1:8, FIRST, NOT FIRST.
 * (0.6.82 WHEN: bits 0-1 the first component's ALWAYS / 1:2 / 2:2 / SKIP, the same codes; bits 2-3 the second's, now
 * read as the step's: a project that set WHEN B alone reads another EVERY) */
enum { EV_ALWAYS, EV_1_2, EV_2_2, EV_SKIP, EV_1_3, EV_2_3, EV_3_3, EV_1_4, EV_2_4, EV_3_4, EV_4_4, EV_1_8, EV_FIRST,
       EV_NOT1, EV_N };
static const uint8_t EV_AB[EV_N][2] = {{0, 0}, {1, 2}, {2, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4},
                                       {4, 4}, {1, 8}, {0, 1}, {0, 2}};   /* a of b loops; {0,0} always, {0,3} never,
                                                                          * {0,1} the first, {0,2} not the first */
static const uint8_t EV_ORD[EV_N] = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 3};   /* the knob's order, SKIP last */
#define ST_EVERY(s) ((uint32_t)(s)->cwhen & 15u)
#define ST_EVERY_SET(s, e) ((s)->cwhen = (uint8_t)(((s)->cwhen & 0xF0u) | ((uint32_t)(e) & 15u)))
/* (0.6.88) TIMING: the step's own place off the grid (0 on it); recorded off the grid (REC Q) or set on SEQ > NOTE's
 * MICRO. seq.c: late by a queue, early by ending the step before it that much sooner. (0.9.1) -32 .. +31 64ths of a
 * step (~2 ms a 64th of a 1/16 at 120 BPM; was -8 .. +7 sixteenths): the sixteenth below it in cwhen's bits 4-7 as
 * always (stored with it, project.c; what firmware before 0.9.1 plays), the 64ths above that (0..3) in flags' bits 5-6
 * (SF_MFINE: FUNC's block 'M' 'F'). A step of before: its sixteenths x 4, exactly */
#define MICRO_MIN (-32)
#define MICRO_MAX 31
static inline int32_t st_micro(const step_t *s)
{
    return ((int32_t)(int8_t)s->cwhen >> 4) * 4 + (int32_t)((s->flags & SF_MFINE) >> SF_MFINE_SH);
}
static inline void st_micro_set(step_t *s, int32_t m)   /* m: MICRO_MIN .. MICRO_MAX */
{
    s->cwhen = (uint8_t)((s->cwhen & 15u) | (((uint32_t)m >> 2) & 15u) << 4);
    s->flags = (uint8_t)((s->flags & ~SF_MFINE) | ((uint32_t)m & 3u) << SF_MFINE_SH);
}
#define ST_MICRO(s) st_micro(s)
#define ST_MICRO_SET(s, m) st_micro_set((s), (m))
#define CT_DIV(s) ((uint32_t)(s)->ctime & 7u)
#define CT_LEN(s) (((uint32_t)(s)->ctime >> 3) & 15u)
#define CT_NDIV 6u
static const uint8_t CT_DIVS[CT_NDIV] = {0, 2, 3, 4, 6, 8};
/* (0.6.39) step components, OP-Z style (SEQ > COMP): one a step, on a NOTE step of a melodic track.
 * ARP NOTES / SEQ / SCALE: the step plays an arpeggio, RATE notes in its length, over the TIEs after it; its notes
 * (NOTES: the step's own, SEQ: every note of the track's pattern, both over RANGE octaves; SCALE: the scale up from
 * the step's lowest note, RANGE: 3 5 8 15 degrees) in the ORDER. TRIG: the step plays on some loops of the pattern
 * only (1:2 the first of every two, .., FIRST, NOT FIRST). carg: ARP ORDER bits 0-1, RATE 2-4, RANGE 5-6; TRIG the
 * condition. */
/* (0.6.42) RAND: the notes up / down the scale at random (carg 1..7 degrees); OCT: an octave (carg: +1, -1, +1 every
 * other loop, -1 every other loop, + / - by turns, at random); TONAL: the rest of the loop moved (carg 0..14: -7..+7
 * degrees); BEND: the notes glide over the step (carg 0..24: -12..+12 semitones); ECHO: repeats fading (carg 0..2:
 * x2..x4; 0.6.82: .. x8); PULSE: its notes again on the empty steps after it (carg 0..3: every 1..4); JUMP: then step carg + 1;
 * REV: back from here to step 1 (ping-pong); FILL: only while FX is held; KILL: what the track sounds cut first */
enum { CP_NONE, CP_ARPN, CP_ARPQ, CP_ARPS, CP_TRIG, CP_RAND, CP_OCT, CP_TONAL, CP_BEND, CP_ECHO, CP_PULSE, CP_JUMP,
       CP_REV, CP_FILL, CP_KILL, CP_WALK, CP_CLIMB, CP_TOUCH, CP_CHORD, CP_HOLD, CP_N };
/* (2026-11-10) HOLD: a NOTE step's notes are not ended by its gate or by the steps after it: they ring for carg steps (the length turned
 * past another note, ui_input.c step_edit; 0: until the pattern comes round to step 1), to the next HOLD step or STOP, the steps after it playing over them (seq.c seq_to_hold; on a kit or a ratchet: nothing) */
/* (chord-harp fork) CHORD: the step plays a chord of SCL > CHORD HARP's sets whole from its first note, the chord's
 * root (seq.c step_notes; its other notes over it); carg the chord, set * 12 + its place (0..191), an OMNI chord 192 +
 * its type * 12 + its pitch class (..239, harp.c hc_chord). What a HARP chord
 * key records (seq.c key_rec). Stock Yehnah does not open a project with one (project.c: comp < CP_N) */
/* (0.8.5) TOUCH: on a DRUM track how the step's hits are struck (carg 0..2: SHORT, half the decay; GHOST, short, darker
 * and softer; FLAM, a soft grace hit ~20 ms before the hit; eng_drum.c DXO_*): what a hit played in the octaves below
 * the kit's records (OCT -1 .. -3, MIDI below 35: seq.c dxo_on). Other engines: nothing. ECHO's carg bit 7, EVEN
 * (0.8.5): its repeats all at the step's level, a ratchet past RATCH's x4 (OCT +3 on a kit records x8 so) */
#define CP_ECHO_EVEN 0x80u
/* (0.6.98) WALK and CLIMB (after the OP-Z's and OP-XY's evolving components): the step's notes move one degree of the
 * scale (WALK) or one octave (CLIMB) every RATE loops of the pattern, over a RANGE of moves, then round again: UP,
 * DOWN, UP-DN (there and back) or RND (a random place a move, the same every time round). carg: bits 0-3 RATE
 * (CP_WRATES: 1..10, 12, 16 loops a move), bits 4-5 MODE, bits 6-7 RANGE (WALK: 3 5 7 12 degrees; CLIMB: 1 2 3 4
 * octaves). Both stack with each other and with the rest (TONAL, OCT, RAND, an ARP): four notes, two of these, and
 * the pattern comes back to itself after RATE x RANGE loops, different every time round */
#define CP_WRATE(a) ((uint32_t)(a) & 15u)
#define CP_WMODE(a) (((uint32_t)(a) >> 4) & 3u)
#define CP_WRNG(a) (((uint32_t)(a) >> 6) & 3u)
#define CP_WARG(r, m, g) ((uint8_t)(((r) & 15u) | ((m) & 3u) << 4 | ((g) & 3u) << 6))
#define CP_NWRATE 12u
static const uint8_t CP_WRATES[CP_NWRATE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 16};
static const uint8_t CP_WDEGS[4] = {3, 5, 7, 12};
/* the range of carg each component's KNOB 3 turns (ARP: KNOB 3 / 4 their own; WALK CLIMB: theirs) */
static const uint8_t CP_AMAX[CP_N] = {0, 0, 0, 0, 11, 7, 5, 14, 24, 6, 3, 63, 0, 0, 0, 0, 0, 2, 239, 0};
static const uint8_t CP_ADEF[CP_N] = {0, 0, 0, 0, 0, 2, 0, 9, 7, 1, 0, 0, 0, 0, 0, 0x40, 0x00, 0, 0, 0};
static const uint8_t CP_AMIN[CP_N] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define CP_ORD(a) ((uint32_t)(a) & 3u)            /* UP DOWN UP-DN RND */
/* (0.6.62) JUMP: its step (bits 0-5) and TIMES (bits 6-7); REV: TIMES (bits 0-1). TIMES: ALWAYS, x1 (once a pass of
 * the pattern, then it plays on), x2, 1:2 (once, every other pass). 0, ALWAYS: as a JUMP / REV of before (seq.c fl_ok) */
#define CP_JTO(a) ((uint32_t)(a) & 63u)
#define CP_TIMES(c, a) ((c) == CP_JUMP ? ((uint32_t)(a) >> 6) & 3u : (uint32_t)(a) & 3u)
#define CP_RATE(a) (((uint32_t)(a) >> 2) & 7u)    /* 0..4: x2 x3 x4 x6 x8 */
#define CP_RNG(a) (((uint32_t)(a) >> 5) & 3u)     /* 0..3: 1..4 octaves (SCALE: 3 5 8 15 degrees) */
#define CP_ARG(o, r, g) ((uint8_t)(((o) & 3u) | ((r) & 7u) << 2 | ((g) & 3u) << 5))
#define CP_NRATE 8u                                 /* (0.6.92: x1, a note a step over the TIEs, as index 5; 2026-11-10: 6 and 7 slower than
                                                     * a step: a note every 2 / 4 steps over the TIEs (seq.c: arp_c), 1/8 and 1/4 at DIV 1/16) */
#define CP_NTRIG 12u
static const uint8_t CP_RATES[CP_NRATE] = {2, 3, 4, 6, 8, 1, 1, 1};   /* the parts a step plays */
static const uint8_t CP_SLOW[CP_NRATE] = {0, 0, 0, 0, 0, 0, 2, 4};    /* the steps a note lasts, when slower than a step */
static const uint8_t CP_RATE_ORD[CP_NRATE] = {7, 6, 5, 0, 1, 2, 3, 4};   /* the knob's order: 1/4 1/8 x1 x2 x3 x4 x6 x8 */
static const uint8_t CP_DEGS[4] = {3, 5, 8, 15};
static const uint8_t CP_TRIGS[CP_NTRIG][2] = {{1, 2}, {2, 2}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {4, 4},
                                              {1, 8}, {0, 1}, {0, 2}};   /* (a of b; {0, 1} FIRST, {0, 2} NOT FIRST) */
static int cp_isarp(uint32_t c) { return c >= CP_ARPN && c <= CP_ARPS; }
typedef struct {                 /* a step as formats 1..4 (projects to FUN4) stored it: no hits */
    uint8_t note[4];
    uint8_t n, time, flags, vel;
} step8_t;

/* Probability keeps zero-initialized and legacy patterns at 100%. */
static uint32_t step_chance(const step_t *s) { return !s->probability ? 100u : s->probability <= 100u ? s->probability : 0u; }
static void step_set_chance(step_t *s, uint32_t chance) { s->probability = (uint8_t)(chance >= 100u ? 0u : chance ? chance : 101u); }
/* RATCH: a NOTE step plays its notes and hits this many times (1..4), in equal parts of the step; every older
 * pattern holds 0 there, x1 */
static uint32_t step_ratchet(const step_t *s) { return ((s->flags & SF_RATCH) >> SF_RATCH_SH) + 1u; }
static void step_set_ratchet(step_t *s, uint32_t hits)
{
    s->flags = (uint8_t)((s->flags & ~SF_RATCH) | ((hits < 1u ? 0u : hits > 4u ? 3u : hits - 1u) << SF_RATCH_SH));
}
#define MOTION_MAX 64u
/* Four tracks x64 steps fit one byte (0.6, six tracks: the track's bit 2 is value's bit 12 flipped against its
 * sign, MOTION_TRK; values stay within -64..127, so an old record reads as track 0..3). Values retain their signed parameter range.
 * param: the P_* id in bits 0..9 (MOTION_IDS: room for 1023 track parameters; (chord-harp fork) one byte with the id in
 * bits 0..6 until FUND, project.c motion_ev4), bit 15 (MOTION_LOCK, 1.1) a parameter lock: the value sounds on that
 * step only and goes back after it (motion.c motion_step); without it an automation event, the value holds until
 * another one changes it; bits 10..14 0. One record per (track, step, id), of either kind */
#define MOTION_LOCK 0x8000u
#define MOTION_IDS 0x3FFu
#define MOTION_ID(e) ((uint32_t)(e)->param & MOTION_IDS)
#define MOTION_HI(e) ((uint32_t)(((e)->value >> 12) ^ ((e)->value >> 15)) & 1u)
#define MOTION_TRK(e) ((uint32_t)(e)->place >> 6 | MOTION_HI(e) << 2)
#define MOTION_STEP(e) ((uint32_t)(e)->place & 63u)
#define MOTION_PLACE(e) (MOTION_TRK(e) << 6 | MOTION_STEP(e))   /* track << 6 | step */
#define MOTION_VAL(e) ((int16_t)((e)->value ^ (int32_t)(MOTION_HI(e) << 12)))
/* (0.9.1) MOT_DJF: a record whose id is past every P_* is the master DJ FILTER's (G_DJF), automation only (no lock):
 * recorded on the selected track (GLO > MIXER KNOB 4), it sets song.g[G_DJF] at its step and the value holds as any
 * event's; a stop or the loop's restart puts back the knob's (motion.c). Stored in FUNC's block 'M' 'G' (project.c).
 * (chord-harp fork) the last id since FUND; 127 in the one-byte records before it and on the editor's wire (editor.c) */
#define MOT_DJF MOTION_IDS
#define MOT_DJF_V4 127u
_Static_assert(P_COUNT < MOT_DJF, "MOT_DJF past every track parameter");
typedef struct { uint8_t place, rsv; uint16_t param; int16_t value; } motion_event_t;   /* rsv: 0 */
typedef struct { uint8_t count, on, rsv[2]; motion_event_t event[MOTION_MAX]; } motion_store_t;
static void motion_at(motion_event_t *e, uint32_t place, int16_t v)   /* place: track << 6 | step; v: the value */
{
    e->place = (uint8_t)(place & 255u);
    e->rsv = 0;
    e->value = (int16_t)(v ^ (int32_t)(((place >> 8) & 1u) << 12));
}
_Static_assert(sizeof(motion_store_t) == 4u + MOTION_MAX * 6u, "motion layout (RAM: project.c stores it byte by byte)");
#define CHAIN_ROWS 16u
typedef struct { uint8_t slot, bars; } chain_row_t;   /* a song row: section A..D (a project slot), 1..CHAIN_BARS bars */
#define CHAIN_BARS 64u
typedef struct {
    uint8_t count, rsv[3];
    chain_row_t row[CHAIN_ROWS];
} chain_config_t;

typedef struct track {
    int16_t p[P_COUNT];
    uint8_t engine, preset;      /* engine: what the audio ISR renders */
    uint8_t eng_req;             /* engine the UI asked for (the ISR switches at a block start) */
    uint8_t user;                /* user preset slot + 1 the sound came from (UI), 0 = none */
    voice_t v[NVOICE];
    /* LFO */
    uint32_t lfo_ph;
    int32_t lfo_val;             /* Q15 */
    int32_t lfo_fade;            /* Q15 ramp after note-on */
    uint32_t lfo_rnd;
    /* keyboard / arp input: held notes in press order */
    uint8_t held[16];
    uint8_t nheld;
    uint8_t arp_phys;            /* keys physically held for the arp */
    uint8_t latched;             /* HOLD: keep notes after release */
    /* arp runtime */
    uint32_t arp_pos;            /* q8 samples into the current arp step */
    uint32_t arp_idx;
    uint8_t arp_note;            /* sounding arp note, 0 = none */
    uint8_t arp_walk;            /* WALK: the place in the note list it stands on */
    uint16_t tm_reg;             /* (chord-harp fork: Clipper 0.9's) the arp's TURNG: its shift register */
    int16_t tm_seed;             /* .. the DICE it was seeded with (+ 1; 0: not yet) */
    uint8_t arp_ch[3];           /* CHORD: the other notes sounding with arp_note, 0 = none */
    uint32_t arp_off;            /* q8 sample time of its note-off */
    /* (strum-on-hold) the STR* ARP modes: a stroke's notes after the first, as_i of as_n sounding, as_due samples to the
     * next, as_gap between; arp_hd: the next step late by this many samples (HUMAN) */
    uint8_t as_n, as_i, as_nn[18], as_vv[18];
    uint32_t as_due, as_gap, arp_hd;
    /* sequencer */
    step_t step[NSTEP];
    uint32_t seq_pos;            /* q8 samples into the current step */
    uint16_t seq_idx;
    uint8_t seq_notes[4 + NLANE];   /* sounding seq notes (the step's notes, then its hits) */
    uint8_t seq_n;
    uint8_t hold_n, hold_left, hold_notes[8];
    uint8_t hold_hits, hold_k, hold_vel, hold_fade;   /* (HOLD with a RATCH / ECHO: its parts over the whole length: seq.c seq_hold_parts) */
    uint32_t hold_part, hold_due;   /* (2026-11-10) CP_HOLD: the notes ringing over the steps (seq.c seq_to_hold) */
    uint8_t seq_hold;            /* last step slides: keep the notes until the next step */
    uint8_t slide_glide;         /* next legato note glides (slide) */
    uint8_t rat_left;            /* RATCH: repeats of the playing step still to come (seq.c seq_ratchet) */
    uint8_t arp_from;            /* (0.6.39) an ARP component playing: its step + 1 (0: none), over the TIEs after it */
    uint8_t arp_k;                      /* .. the arpeggio's next note (its position in the ORDER) */
    uint8_t arp_c;                      /* (2026-11-10) the steps since it began (a rate slower than a step) */
    uint16_t loops;              /* (0.6.39) the pattern's loops since PLAY (TRIG components) */
    int8_t tonal;                /* (0.6.42) TONAL: the rest of the loop this many degrees up / down */
    uint8_t pulse_from, pulse_k; /* .. PULSE: its step + 1 (0: none), the empty steps since */
    uint8_t jump_to, rev;        /* .. JUMP: the next step + 1 (0: the next one); REV: backwards to step 1 */
    uint16_t mo_cur, mo_prev;    /* (0.6.50) MORPH as the engines play it: Q8, slewed a block at a time (voice.c
                                  * track_render) and ramped across the block from mo_prev (no step: no click) */
    uint8_t cpos, cv_on;         /* (0.6.47) C.LEN: the component lane's next step; the step playing is cvs */
    step_t cvs, cva, cvp;        /* .. the step playing with the lane's component; the ARP's and PULSE's (CV_IDX) */
    uint32_t seq_off;
    uint8_t seq_active;          /* any step programmed */
    uint8_t rskip_idx;           /* live recording put notes into the step about to play: */
    uint8_t rskip_n, rskip[NLANE];   /* do not trigger them again there (they sound already) */
    /* live recording of held notes (seq.c rec_hold): the steps they are held into become TIEs */
    uint8_t rh_n, rh_note[4];    /* recorded notes still held, 0 = none */
    uint8_t rh_start;            /* the step they were recorded into */
    uint8_t rh_ties;             /* TIE steps written after it */
    uint8_t rh_last;             /* the last of them; rh_bak: what it held (an early release puts it back) */
    step_t rh_bak;
    /* mono */
    uint8_t mono_stack[8];
    uint8_t nmono;
    uint8_t mono_note;           /* note the MONO / LEGATO / UNISON voice(s) play, 0 = none */
    uint8_t rr;                  /* POLY ROTATE: next voice to try */
    /* mix runtime */
    int32_t peak;
    int32_t dist_hp, dist_lp1, dist_lp2;   /* DIST insert state (fx.c) */
    int32_t dist_dc, dist_hold;  /* (Yehnah) RECT's DC; CRUSH's held sample */
    uint8_t dist_hc;             /* .. CRUSH's hold count */
    int32_t dist_cab_l, dist_cab_b;   /* (0.7.8) AMP's cabinet: its state-variable filter (low, band) */
    int32_t ms_v;                /* (Yehnah) the modulation sequence's level now (mod.c MS_STEP, Q15, slewed) */
    uint8_t tail;                /* blocks to mix after the last voice (the DIST tail) */
    int16_t armp, aholdp;        /* P_AMODE / P_AHOLD as last seen by the ISR */
    /* engine switch (voice.c engine_block): the old engine's voices fade out, then it switches */
    uint8_t xf_on, xf;           /* fading; blocks of the fade still to render */
    uint8_t xf_len, xf_dip;      /* (0.6.41) the fade's length; a dip (an engine's list value changed: the voices stay) */
    uint8_t xin, xin_len;        /* (0.6.41) fading in after it: blocks still to come, of xin_len */
    int16_t pe_old[8];           /* P_E0..P_E7 of the sounding engine: the fade renders with these */
    uint8_t xp_n, xp_note[4], xp_vel[4];   /* note-ons during the fade, played on the new engine */
    /* modulation matrix (mod.c): MIDI performance controllers of the track's channel, 0..127 */
    uint8_t mw, at;              /* CC1 mod wheel, channel aftertouch */
    uint8_t ex_off;              /* 127 - CC11 expression (0: full, the MIDI default) */
    uint8_t m_vel, m_key, m_vi;  /* the latest note-on: velocity, note, voice index (per-block destinations) */
    int16_t m_rnd;               /* .. its RAND */
    int32_t m_env;               /* the amp envelope of voice m_vi, last block (Q15) */
    /* the punch-in MIDI effects (pfx.c), the ISR's: what acts on this track this block, never saved */
    int8_t trn;                  /* ARP TRNS (seq.c): the semitones the keys set (from C4), on the steps' notes */
    uint8_t pfx;                 /* PFX_* */
    int16_t pfx_pit;             /* OCT- / OCT+: 1/16 semitones on every voice */
    int16_t pfx_rpit;            /* RANDOM: the next note's offset (1/16 semitones), drawn by trk_note_on */
    int8_t dpit;                 /* (0.6.82) the sequencer's next drum hits: their tune (semitones; a step's OCT, RAND,
                                  * TONAL, ARP SCALE on a kit) */
    int16_t dbend;               /* .. BEND: where the hit glides to over dbt blocks (1/16 semitones; eng_drum.c) */
    uint16_t dbt;
    uint8_t ss_n, ss_i;          /* (0.6.84) STRUM on the sequencer's chords: the step's notes still to come in, in */
    uint8_t ss_nn[4 + NLANE], ss_vv[4 + NLANE];  /* order low to high (seq.c seq_step, seq_tick), ss_due samples to the next */
    uint8_t dtch, ss_tch;        /* (0.8.5) a step's TOUCH (1..3, 0 none) on the sequencer's next drum hits; .. on the
                                  * queued ones (HUMAN TIME: seq_tick) */
    uint8_t lr_on;               /* (0.8.5) the playing step has a lane ratcheting (seq.c seq_lane_rat) */
    uint32_t ss_due, ss_gap;     /* (.. and the gap between them; 0.6.88: 32 bits, a long TIMING) */
    uint16_t ext;                /* (0.6.88) TIMING: samples the playing step began before its place (it is early) */
    int8_t mic;                  /* .. the playing step's own TIMING (ST_MICRO), late: seq_step queues its notes */
    int8_t hum_env;              /* (0.6.92) HUMAN: the next notes' DEC / REL move (seq_step sets it, voice_start takes it) */
    uint8_t pfx_half;            /* 1/2 TEMPO: the odd sample carried; pfx_rs: let go, back to the real place */
    uint8_t pfx_rs;
    uint16_t pfx_idx0;           /* .. where it was pressed (step, samples into it) and the samples since */
    uint32_t pfx_pos0, pfx_el;
    uint8_t pfx_ln[4], pfx_lv[4], pfx_lnn;   /* the last notes struck together (a step's), STUTTER / ARP repeat them */
    uint32_t pfx_lblk;           /* .. the block they were struck in */
    uint8_t pfx_sn[4], pfx_snn;  /* the notes a repeat holds now */
    uint8_t pfx_si;              /* ARP: the next of them */
    uint32_t pfx_sc;             /* samples since the last repeat */
} track_t;
enum { PFX_RND = 1, PFX_HALF = 2, PFX_REP = 4, PFX_DEC = 8, PFX_ATK = 16 };   /* (track_t.pfx) */

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing, seq_mode;
    uint8_t grid;                /* 1: the keys are the DRUM grid (ui.c grid_on): only the lane keys play (seq.c);
                                  * 2: NAME (ui_name.c): no key plays */
    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t sel;                 /* selected track 0..NTRK-1: keys, pages, editor */
    int8_t octave;
    uint32_t tick;               /* sub-blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
} song_t;

static track_t trk[NTRK];        /* the instrument: four parts */
static song_t song;
#define TSEL (&trk[song.sel])    /* the selected track */

/* SWING of a track's step clock: the track's own plus the global one, at most 100 (#31). The sequencer
 * (seq.c step_samples) and the SLICER (slicer.c sl_enter) both time steps with it. At 100 the even
 * steps are 1.4 x the straight length and the odd ones 0.6 x. */
#define SWING_MAX 100
static inline int32_t track_swing(const track_t *t)
{
    int32_t sw = t->p[P_SSWING] + song.g[G_SWING];
    return sw < 0 ? 0 : sw > SWING_MAX ? SWING_MAX : sw;
}
/* the length of step idx of a straight length `base`, swung: even steps longer, odd ones shorter */
static inline uint32_t swing_step_len(const track_t *t, uint32_t base, uint32_t idx)
{
    int32_t sw = track_swing(t) * (int32_t)base / 250;
    return base + (uint32_t)((idx & 1u) ? -sw : sw);
}

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */
/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));
