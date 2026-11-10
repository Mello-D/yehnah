/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Parameter descriptors, formatting and the page table. */
#include "harp.c"                                       /* (chord-harp) CHORD HARP's sets (seq.c), PROG's progressions */
static const char *const N_LWAVE[] = {"SIN", "TRI", "SAW", "SQR", "S&H"};
/* (0.6.51) the LFO's RATE below 0: in time with the tempo, -1 the slowest (next to the slowest free rate) */
static const char *const N_LSYNC[12] = {"4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8D", "1/4T", "1/8", "1/8T", "1/16",
                                        "1/16T", "1/32"};
static const char *const N_AMODE[] = {"OFF", "UP", "DN", "UPDN", "RND", "ORD", "REPEAT",   /* (append-only: seq.c AM_*) */
                                       "DNUP", "UP+8", "CONV", "DIVG", "PINKY", "THUMB", "WALK", "CHORD",
                                       "TRNS", "SKIP", "HOLD", "TURNG",
                                       "STRDN", "STRUP", "STRALT", "BLOCK", "BS+CH", "SHUF", "HALF", "2/3", "FAST"};   /* (strum-on-hold:
                                                         * strokes of the held chord; seq.c arp_stroke) */         /* (Yehnah: seq.c AM_TRNS; chord-harp fork: SKIP
                                                                         * HOLD) */
/* the ARP's RATE: N_DIV's ten and the dotted 1/4D 1/8D 1/16D (10..12; seq.c arp_step: 3/2 of the division 0..2) */
static const char *const N_ADIV[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR", "1/4D", "1/8D", "1/16D"};
static const char *const N_AMB[] = {"OFF", "HOLD", "LATCH"};   /* (chord-harp fork) ARP > AMBIENT (seq.c amb.c) */
static const char *const N_DIV[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR"};
/* the PATTERN's DIV: N_DIV's ten and a 1/64 (index 10; the delay's TIME, which has free times from 10, never takes it) */
static const char *const N_PDIV[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR", "1/64"};
/* (0.4) the delay's TIME: N_DIV's ten, then free times past the last division (5 ms .. 1.48 s, exponential;
 * fx.c delay_samples, DTIME_MS); (0.8.4) appended up to the longest the line holds at a quarter rate (the values
 * before them mean what they did) */
#define DT_FREE 10
/* (0.8.4) the delay's line (fx.c dly_buf): DLY_LEN samples of int16, the one constant of its size. 32768 (64 KB): the full
 * rate to 0.74 s, half to 1.49 s, a quarter to 2.97 s; 65536 (128 KB): 1.49 / 2.97 / 5.94 s */
#ifndef DLY_LEN
#define DLY_LEN 32768u
#endif
static const char *const N_DTIME[] = {"1/4", "1/8", "1/16", "1/32", "8T", "16T", "1/2", "1/1", "2BAR", "4BAR", "5ms", "6ms", "7ms", "8ms", "9ms", "10ms", "11ms", "12ms", "13ms", "15ms", "17ms", "19ms", "21ms", "24ms", "27ms", "31ms", "35ms", "39ms", "44ms", "50ms", "56ms", "64ms", "72ms", "81ms", "91ms", "103ms", "116ms", "131ms", "148ms", "167ms", "189ms", "213ms", "241ms", "272ms", "307ms", "346ms", "391ms", "441ms", "498ms", "562ms", "634ms", "716ms", "808ms", "912ms", "1.03s", "1.16s", "1.31s", "1.48s",
                                      /* (0.8.4) past the line at the full rate (fx.c dly_lo): half rate to 2.97 s, then a quarter */
                                      "1.67s", "1.89s", "2.13s", "2.40s", "2.71s"
#if DLY_LEN >= 65536u
                                      , "3.06s", "3.45s", "3.90s", "4.40s", "4.97s", "5.61s", "5.90s"
#else
                                      , "2.95s"
#endif
                                      };
static const uint16_t DTIME_MS[NELEM(N_DTIME) - DT_FREE] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 17, 19, 21, 24, 27, 31, 35, 39, 44, 50, 56, 64, 72, 81, 91, 103, 116, 131, 148, 167, 189, 213, 241, 272, 307, 346, 391, 441, 498, 562, 634, 716, 808, 912, 1029, 1162, 1311, 1480,
                                      1670, 1885, 2128, 2402, 2711
#if DLY_LEN >= 65536u
                                      , 3060, 3454, 3899, 4400, 4967, 5606, 5900
#else
                                      , 2950
#endif
                                      };
static const char *const N_SCALE[] = {"CHR", "MAJ", "MIN", "DOR", "MIX", "PEN", "MPEN", "HARM",
                                    "PHRY", "LYD", "LOC", "MEL", "BLUES", "WHOLE", "DIMHW", "DIMWH"};
static const char *const N_ONOFF[] = {"OFF", "ON"};
static const char *const N_DLYTYPE[] = {"ANALOG", "DIGITAL", 0};   /* (0.6.90) DLY 3 > TYPE */
static const char *const N_PFXRES[] = {"1/8", "1/16", "1/32"};     /* (0.6.99) FX > PUNCH: the lane's step */
static const char *const N_BLTRK[] = {"OFF", "T1", "T2", "T3", "T4", "T5", "T6"};   /* (chord-harp fork) MORPH's tracks */
static const char *const N_BMOVE[] = {"OFF", "VEL", "WHEEL", "LFO 1", "LFO 2", "LFO 4", "LFO 8", "LFO 16"};   /* .. MOVE:
                                                         * the last note's velocity, the mod wheel, a sweep over 1..16 bars */
/* seq.c kb_map; 1 = SNAP (stored projects: the former ON); 3 = SEQ: SNAP, and the sequencer's notes snap too as
 * they play (seq.c seq_step; the steps keep what was written). Append-only: older projects hold 0..2 */
static const char *const N_QUANT[] = {"OFF", "SNAP", "WHITE", "SEQ"};
static const char *const N_DTYPE[] = {"SOFT", "HARD", "FOLD", "CRUSH", "RECT", "AMP"};   /* (Yehnah) fx.c DT_*; 0.7.8 AMP */
static const char *const N_FTYPE[] = {"LP", "HP", "BP", "COMB"};   /* (0.5) P_FTYPE: eng_analog.c FT_* */
static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI", "UNI2", "UNI4"};   /* V_POLY .. V_PUNI4 */
static const char *const N_GLMODE[] = {"RATE", "TIME"};
static const char *const N_PRIO[] = {"LAST", "LOW", "HIGH"};
static const char *const N_ALLOC[] = {"ROT", "REUSE"};
static const char *const N_ORDER[] = {"NOTE", "PLAY"};
static const char *const N_CLOCK[] = {"INT", "USB", "TRS"};
static const char *const N_MIDI_INPUT[] = {"USB", "TRS"};
static const char *const N_ROUTE[] = {"CH1-6", "SEL"};   /* MIDI IN (seq.c): 1..6 -> parts, 7..16 ignored / all -> selected */
static const char *const N_NOTE[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const N_DASH[] = {"--"};
static const char *const N_RTYPE[] = {"ROOM", "SPRING", "SHIMR", "RESO", "CLOUD"};   /* (0.5: + 3) */   /* G_RTYPE: the reverb bus's model (fx.c) */
static const char *const N_GO[] = {"--", "GO"};
static const char *const N_SLCR[] = {"OFF", "GATE", "STUT"};             /* SL_OFF .. SL_STUT (slicer.c) */
static const char *const N_SLDIV[] = {"1/8", "1/16", "1/32", "8T", "16T", "32T"};   /* SL_DEN */
static const char *const N_SLPQ[] = {"CHR", "SCL"};                      /* (0.6.3) the SLICER's pitch steps */
static const char *const N_APSYNC[] = {"FREE", "BEAT"};                  /* (0.6.9) AUTO PAN's RATE: Hz / the tempo */
static const char *const N_APDIV[] = {"4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};   /* .. BEAT (fx.c AP_SIXT) */
/* modulation matrix (mod.c): sources, destinations (E1..E8 = P_E0..P_E7: shown with the engine's labels) */
static const char *const N_MSRC[] = {"OFF", "LFO", "ENV", "VEL", "KEY", "RAND", "MODW", "AT", "EXPR",
                                     "M1", "M2", "M3", "M4", "STEP", "NOISE"};   /* (0.7.8) NOISE */   /* (Yehnah: the macros, LFO held; the modulation sequence) */
static const char *const N_MDST[] = {"OFF", "PITCH", "CUT", "SHP", "AMP", "PAN", "DIST", "CHO", "DLY", "REV", "RATE",
                                     "VIB", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8",
                                     "CLIP", "PNCH",   /* (Yehnah: the master, FX > MASTER) */
                                     "D.TIM", "D.FB", "D.TON", "D.MIX", "R.SIZ", "R.DMP", "C.RAT", "C.DEP"};   /* (0.7.0) the FX buses'
                                                         * globals (DELAY TIME FDBK TONE MIX, REVERB SIZE DAMP, CHORUS RATE
                                                         * DEPTH): mod.c macro_master, fx.c GFX() */
static const char *const N_ENGNAME[] = {"ANALOG", "DIGITAL", "PHASE", "LOFI", "-", "VOICE", "-", "WHEEL", "-", "-",
                                             "DRUM", "NOISE", "FM6", "-", "ANIMAL", "JUNO", "OMNI", "KALIMBA", "FMINI",
                                             "GUITAR", "ACID", "DRIFT", "PLAITS", "STRINGS", "VPS",
                                             "MODAL", "WSEQ", "TERRAIN", "VECTOR", "HYBRID"};   /* (Yehnah:
                                             * SAMPLE 4, GRAIN 8, SLICE 13 retired; 0.8.3 ANIMAL; chord-harp fork 15 JUNO,
                                             * 16 OMNI, 17 KALIMBA, 18 LCD-4, 19 GUITAR, 20 ACID, 21 DRIFT, 22 PLAITS,
                                             * 23 STRINGS, 24 VPS, 25 MODAL, 26 WSEQ, 27 TERRAIN, 28 VECTOR, 29 HYBRID) */

static const char *const N_FMODEL[] = {"", "SVF", "MOOG", "MS20", "DIODE", "SEM", "NOTCH", "BP", "HP"};   /* P_FMODEL: FM_* (from 1: names[v]) */
static const char *const N_MODDST[] = {"OFF", "VIB", "CUT", "SHP", "WAH", "TREM"};   /* P_MWDST: MWD_OFF .. MWD_TREM */
#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}

static const char *const N_POCT[] = {"-1", "0", "+1"};   /* (chord-harp fork) PLATE, STRINGS: the octave */
static const param_desc_t TP[P_COUNT] = {
    [P_LEVEL] = PD("LEVEL", F_DB, 0, 127, 104),
    [P_ATK] = PD("ATK", F_TIME, 0, 127, 10),
    [P_DEC] = PD("DEC", F_TIME, 0, 127, 70),
    [P_SUS] = PD("SUS", F_PCT, 0, 127, 90),
    [P_REL] = PD("REL", F_TIME, 0, 127, 60),
    [P_ED_FLT] = PD("FILTER", F_BIPCT, -64, 63, 0),
    [P_ED_PIT] = PD("PITCH", F_BIPCT, -64, 63, 0),
    [P_ED_SHP] = PD("SHAPE", F_BIPCT, -64, 63, 0),
    [P_ED_FX] = PE("LOOP", N_ONOFF, 0),          /* (0.4, P_ELOOP) ON: held, the ENV starts again at the sustain */
    [P_LRATE] = PD("RATE", F_LFOHZ, -12, 127, 60),  /* (0.6.51) below 0 in time: 4BAR .. 1/32 (N_LSYNC) */
    [P_LWAVE] = PE("WAVE", N_LWAVE, 0),
    [P_LPHASE] = PD("PHASE", F_INT, -1, 127, 0),   /* (0.6.51) -1: FREE, never restarted by a note */
    [P_LFADE] = PD("FADE", F_TIME, 0, 127, 0),
    [P_LD_PIT] = PD("PITCH", F_BIPCT, -64, 63, 0),
    [P_LD_FLT] = PD("FILTER", F_BIPCT, -64, 63, 0),
    [P_LD_SHP] = PD("SHAPE", F_BIPCT, -64, 63, 0),
    [P_LD_AMP] = PD("AMP", F_PCT, 0, 127, 0),
    [P_AMODE] = PE("MODE", N_AMODE, 0),
    [P_ARATE] = PE("RATE", N_ADIV, 2),
    [P_AOCT] = PD("OCT", F_INT, 1, 4, 1),
    [P_AGATE] = PD("GATE", F_PCT, 1, 127, 64),
    [P_ASWING] = PD("SWING", F_PCT, 0, 100, 0),
    [P_APROB] = PD("PROB", F_PCT, 0, 127, 127),
    [P_AHOLD] = PE("HOLD", N_ONOFF, 0),
    [P_AORDER] = PE("ORDER", N_ORDER, 0),
    [P_ROOT] = PD("ROOT", F_NOTE, 0, 11, 0),
    [P_SCALE] = PE("SCALE", N_SCALE, 0),
    [P_QUANT] = PE("QUANT", N_QUANT, 0),
    [P_TRANS] = PD("TRANS", F_SEMI, -24, 24, 0),
    [P_SLEN] = PD("LEN", F_STEPS, 1, NSTEP, 16),
    [P_SDIV] = PE("DIV", N_PDIV, 2),
    [P_SSWING] = PD("SWING", F_PCT, 0, 100, 0),
    [P_SGATE] = PD("GATE", F_PCT, 1, 127, 64),
    [P_DIST] = PD("DIST", F_PCT, 0, 127, 0),
    [P_CHOR] = PD("CHORUS", F_PCT, 0, 127, 0),
    [P_DLY] = PD("DLY", F_PCT, 0, 127, 0),
    [P_REV] = PD("REV", F_PCT, 0, 127, 0),
    [P_VOICE] = PE("VOICE", N_VOICE, 0),
    [P_GLIDE] = PD("GLIDE", F_TIME, 0, 127, 0),
    [P_GLMODE] = PE("MODE", N_GLMODE, 0),
    [P_PRIO] = PE("PRIO", N_PRIO, 0),
    [P_ALLOC] = PE("ALLOC", N_ALLOC, 0),
    [P_DETUNE] = PD("DETUNE", F_INT, 0, 127, 40),
    [P_PAN] = PD("PAN", F_BIPCT, -64, 63, 0),
    [P_MUTE] = PE("MUTE", N_ONOFF, 0),
    [P_APDEP] = PD("DEPTH", F_PCT, 0, 127, 0),       /* (0.6.9) AUTO PAN (core.h: the old SLICER's slots) */
    [P_APRATE] = PD("RATE", F_LFOHZ, 0, 127, 2),    /* (BEAT: 0..7, page_desc AP_BEAT) */
    [P_APWAVE] = PE("WAVE", N_LWAVE, 1),
    [P_APSYNC] = PE("SYNC", N_APSYNC, 1),
#define MSLOT(k) [P_M##k##SRC] = PE("SRC" #k, N_MSRC, 0), [P_M##k##DST] = PE("DEST" #k, N_MDST, 0), \
                 [P_M##k##AMT] = PD("AMT" #k, F_BIPCT, -64, 63, 0)
    MSLOT(1), MSLOT(2), MSLOT(3), MSLOT(4),
#undef MSLOT
/* the OP ENV / OP LEVEL values of DIGITAL (ids 61..80): with YEHNAH_FM4 its operator envelopes; without it
 * inert, on no page and in no editor layout, read only when a DIGITAL sound converts (fm4_convert.c). Their labels
 * stay: the editor's library files key parameters by label ("ATK#2" ..), so an old file's values still find them */
#define FMOP(k) [P_FM##k##_ATK] = PD("ATK", F_TIME, 0, 127, 0), \
                [P_FM##k##_DEC] = PD("DEC", F_TIME, 0, 127, 0), \
                [P_FM##k##_SUS] = PD("SUS", F_PCT, 0, 127, 127), \
                [P_FM##k##_REL] = PD("REL", F_TIME, 0, 127, 0), \
                [P_FM##k##_LEVEL] = PD("LEVEL", F_PCT, 0, 127, 127)
    FMOP(1), FMOP(2), FMOP(3), FMOP(4),
#undef FMOP
    [P_SOFS] = PD("ROTATE", F_INT, -32, 32, 0),    /* the sequence played OFS steps later (seq.c seq_src; 0.6.92: was OFFSET) */
    [P_POFS] = PD("PITCH", F_SEMI, -24, 24, 0),   /* its notes PIT semitones up (not on a kit) */
/* the DRUM engine's lane levels (eng_drum.c, #97): 100 % = the kit as designed (the sound before 1.1) */
#define LANE(k, l) [P_LN0 + (k)] = PD(l, F_PCT, 0, 127, 127)
    LANE(0, "KICK"), LANE(1, "SNARE"), LANE(2, "CLAP"), LANE(3, "HATCL"),
    LANE(4, "HATOP"), LANE(5, "TOM"), LANE(6, "RIM"), LANE(7, "BELL"),
#undef LANE
    [P_DTYPE] = PE("TYPE", N_DTYPE, 0),         /* (Yehnah) DIST: SOFT (as before) HARD FOLD CRUSH RECT AMP */
    [P_DTONE] = PD("TONE", F_BIPCT, -64, 63, 0),   /* .. its tone: darker / brighter */
    [P_MSLEN] = PD("LEN", F_STEPS, 1, 16, 16),  /* (Yehnah) the modulation sequence (mod.c MS_STEP) */
    [P_MSDIV] = PE("DIV", N_DIV, 2),
    [P_MSSLW] = PD("SLEW", F_PCT, 0, 127, 0),
#define MSTEP(k) [P_MS0 + (k)] = PD("LEVEL", F_PCT, 0, 127, 0)
    MSTEP(0), MSTEP(1), MSTEP(2), MSTEP(3), MSTEP(4), MSTEP(5), MSTEP(6), MSTEP(7),
    MSTEP(8), MSTEP(9), MSTEP(10), MSTEP(11), MSTEP(12), MSTEP(13), MSTEP(14), MSTEP(15),
#undef MSTEP
    [P_FTYPE] = PE("TYPE", N_FTYPE, 0),         /* (0.5) the filter: LP (as before) HP BP COMB */
    [P_FCUT] = PD("CUT", F_CUTOFF, 0, 127, 127),  /* .. the other engines' (on the part): open, */
    [P_FRES] = PD("RES", F_PCT, 0, 127, 0),       /* no resonance: out of the chain (its drive: DIST, the part's) */
    [P_HPLATE] = PE("PLATE", N_POCT, 1),          /* (chord-harp fork) HARP's plate an octave down / up */
    [P_OAUTO] = PD("AUTO", F_PCT, 0, 127, 48),    /* .. OMNI AUTO's chord voice (48: 0.3, as it was fixed) */
    [P_OSTR] = PE("PLATE", N_POCT, 1),            /* .. OMNI's strings an octave down / up (2026-10-27: PLATE, as HARP's) */
    [P_PROG] = {"PROG", F_ENUM, 0, (int16_t)(PROG_N - 1u), 0, PROG_NAME, 0},   /* .. SCL > PROG's progression (harp.c
                                                   * PROGS; PROG_NAME ends in a 0: not PE's) */
    [P_PHAMT] = PD("AMOUNT", F_PCT, 0, 127, 0),   /* (chord-harp fork) FX > PHASER: the wet half of the mix, 0 off */
    [P_PHRATE] = PD("RATE", F_LFOHZ, 0, 100, 40),   /* .. its sweep, as the LFO's RATE (40: ~0.4 Hz; 100: ~9.7 Hz) */
    [P_PHFB] = PD("FDBK", F_PCT, 0, 127, 48),     /* .. the last stage back into the first: resonance */
    /* (chord-harp fork, 2026-10-30) Clipper's voice layer (fbd8434 params.c), for its presets */
    [P_FMODEL] = {"MODL", F_ENUM, FM_SVF, FM_LAST, FM_SVF, N_FMODEL, 0},
    [P_VELF] = PD("VEL>F", F_BIPCT, -64, 63, 0),
    [P_MWDST] = PE("MW>", N_MODDST, MWD_OFF),      /* (Clipper's: VIB; ours OFF, the wheel is MORPH's WHEEL's too) */
    [P_VINT] = PD("VINTG", F_PCT, 0, 127, 0),
    [P_SPICE] = PD("SPICE", F_PCT, 0, 127, 0),
    [P_SLOOP] = PD("LOOP", F_INT, 1, 16, 16),
    [P_TRIM] = PD("TRIM", F_INT, -64, 100, 0),
    [P_AMB] = PE("AMBNT", N_AMB, 0),               /* (chord-harp fork) ARP > AMBIENT (amb.c) */
    [P_ADEN] = PD("DENS", F_PCT, 0, 127, 40),
    [P_AMOT] = PD("MOTN", F_PCT, 0, 127, 30),
    [P_ASPC] = PD("SPACE", F_PCT, 0, 127, 64),
};

static const param_desc_t GP[G_COUNT] = {
    [G_BPM] = PD("BPM", F_BPM, 40, 240, 120),
    [G_SWING] = PD("SWING", F_PCT, 0, 100, 0),
    [G_CLOCK] = PE("CLOCK", N_CLOCK, 0),
    [G_TUNE] = PD("TUNE", F_INT, -50, 50, 0),
    [G_DTIME] = PE("TIME", N_DTIME, 1),                 /* (0.4: past the divisions, free times) */
    [G_DFDBK] = PD("FEEDBK", F_PCT, 0, 120, 60),
    [G_DCOLOR] = PD("TONE", F_PCT, 0, 127, 64),        /* (0.4, was COLR + HPF) the repeats: below 64 darker, above
                                                        * thinner (64 open) */
    [G_DMIX] = PD("MIX", F_PCT, 0, 127, 90),
    [G_RSIZE] = PD("SIZE", F_PCT, 0, 127, 90),
    [G_RDAMP] = PD("DAMP", F_PCT, 0, 127, 60),
    [G_CRATE] = PD("RATE", F_LFOHZ, 0, 127, 40),
    [G_CDEPTH] = PD("DEPTH", F_PCT, 0, 127, 60),
    [G_MIDI] = PE("MIDI", N_MIDI_INPUT, 0),
    [G_DREL] = PD("REL", F_PCT, 0, 100, 30),     /* DUCK's release: 40 .. 600 ms (fx.c duck_block) */
    [G_ROUTE] = PE("ROUTE", N_ROUTE, 0),          /* (was "--": stored 0 = CH1-4, as MIDI IN always was) */
    [G_INFO] = PD("CPU", F_INT, 0, 0, 0),
    [G_SLOT] = PD("SLOT", F_INT, 1, 4, 1),
    [G_DJF] = PD("DJ FILT", F_BIPCT, -64, 63, 0),   /* (0.9.1) the master DJ FILTER: 0 off, - low-pass, + high-pass (fx.c) */
    [G_LOAD] = PE("LOAD", N_GO, 0),
    [G_SAVE] = PE("SAVE", N_GO, 0),
    [G_ENGSEL] = PE("ENG", N_ENGNAME, 0),
    [G_CLIP] = PD("CLIP", F_PCT, 0, 100, 0),     /* the master clipper: drive x1 .. x4 into the soft clip */
    [G_CLRSEQ] = PE("CLRSQ", N_GO, 0),
    [G_INITSND] = PE("INIT", N_GO, 0),
    /* the reverb's model on the REVERB page: the id of the old GM drum channel (G_DRCH, inert since 1.0) */
    [G_RTYPE] = PE("TYPE", N_RTYPE, 0),
    /* (Yehnah) the drum bus and the kick's ducking (fx.c, eng_drum.c); the ids of the GM drum part's level and
     * reverb send, inert since 1.0 */
    [G_PUNCH] = PD("PUNCH", F_PCT, 0, 100, 0),    /* DRUM fattened: up to +5 dB the first 4 ms, the body +3 dB (0.5) */
    [G_DUCK] = PD("DUCK", F_PCT, 0, 100, 0),     /* the kick ducks the other parts: up to -18 dB */
    /* (Yehnah, FUNB) the buses: not in a project's header (project.c: after the punch-in lane) */
    [G_CSTER] = PD("STEREO", F_BIPCT, -64, 63, 0), /* (0.6.9) the CHORUS's stereo: 0 as before, -64 mono, 63 ~2x (was G_DHPF) */
    [G_WIDTH] = PD("WIDE", F_PCT, 0, 127, 0),    /* the delay's right echo up to 1/32 later, the chorus's taps apart */
    [G_DTYPE] = {"TYPE", F_ENUM, 0, 1, 0, N_DLYTYPE, 0},   /* (0.6.90) ANALOG (saturating, resonant, past 100 % grows) / DIGITAL */
    [G_DPING] = {"PING", F_ENUM, 0, 1, 0, N_ONOFF, 0},   /* (0.6.90) PING-PONG: the repeats left, right, left .. */
    [G_PFXLEN] = PD("LEN", F_INT, 1, 64, 64),    /* (0.6.99) the punch-in lane: its steps and their size */
    [G_PFXRES] = {"RES", F_ENUM, 0, 2, 1, N_PFXRES, 0},
    [G_BLA] = {"TRACK A", F_ENUM, 0, NTRK, 0, N_BLTRK, 0},   /* (chord-harp fork, 2026-10-29) SAVE > MORPH: two tracks */
    [G_BLB] = {"TRACK B", F_ENUM, 0, NTRK, 0, N_BLTRK, 0},   /* playing one part (voice.c bl_*) */
    [G_BLEND] = PD("BLEND", F_PCT, 0, 127, 64),           /* .. A only (0) .. B only (127), equal power */
    [G_BMOVE] = {"MOVE", F_ENUM, 0, 7, 0, N_BMOVE, 0},    /* .. moved from A up to BLEND (fx.c bl_block) */
    [G_DMOD] = PD("MOD", F_PCT, 0, 127, 0),      /* (0.6.89: 0, the repeats exact copies, by default) */     /* (0.6.86) the delay's drift: WOW + FLUTTER (fx.c dly_wow_adv) */
    [G_RMOD] = PD("MOD", F_PCT, 0, 127, 0),      /* the reverb's modulation depth (ROOM: the combs, SPRING: its wobble) */
    [G_RRATE] = PD("RATE", F_LFOHZ, 0, 127, 30), /* .. its rate */
    [G_RPRE] = PD("PRE", F_INT, -24, 100, 0),    /* the reverb's pre-delay, ms (0.6.12: SHIMMER / CLOUD's AIR, -24..24, below 0) */
    [G_RFILT] = PD("FILT", F_BIPCT, -64, 63, 0), /* (0.3) the reverb's input: darker (low-pass) / brighter (low cut) */
    [G_RWIDE] = PD("WIDE", F_PCT, 0, 127, 0),    /* .. its width: its combs (ROOM) / taps (SPRING) apart, left and right */
    [G_DPIT] = PD("PITCH", F_SEMI, -12, 12, 0),  /* the GRAIN delay: each repeat through two grains this far up / down */
    [G_DSPRY] = PD("SPRAY", F_PCT, 0, 127, 0),    /* .. their spray: each grain from a random place (0: plain delay) */
    [G_STRN] = PD("ALL TR", F_INT, -14, 14, 0),  /* (0.4) every sequence this many steps up / down its scale (0.6.45:
                                                  * named ALL TR, was STRN: nobody knew what moved their notes) */
    [G_SLMODE] = PE("MODE", N_SLCR, 0),         /* (0.6.3) the SLICER bus (slicer.c) */
    [G_SLPAT] = PD("PAT", F_INT, 1, 16, 1),
    [G_SLRATE] = PE("RATE", N_SLDIV, 1),
    [G_SLDEP] = PD("DEPTH", F_PCT, 0, 127, 127),
    [G_SLT1] = PE("T1", N_ONOFF, 1), [G_SLT1 + 1] = PE("T2", N_ONOFF, 1), [G_SLT1 + 2] = PE("T3", N_ONOFF, 1),
    [G_SLT1 + 3] = PE("T4", N_ONOFF, 1), [G_SLT1 + 4] = PE("T5", N_ONOFF, 1), [G_SLT6] = PE("T6", N_ONOFF, 1),
    [G_SLFX] = PE("FX", N_ONOFF, 1),            /* the delay, reverb and chorus returns: on, the tails cut too */
    [G_SLPLEN] = PD("LEN", F_INT, 1, 16, 16),
    [G_SLPQ] = PE("QUANT", N_SLPQ, 1),            /* the pitch steps: every semitone / the scale's */
    [G_SLP0] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 1] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 2] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 3] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 4] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 5] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 6] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 7] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 8] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 9] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 10] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 11] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 12] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP0 + 13] = PD("PITCH", F_SEMI, -24, 24, 0),
    [G_SLP0 + 14] = PD("PITCH", F_SEMI, -24, 24, 0), [G_SLP15] = PD("PITCH", F_SEMI, -24, 24, 0),
};

static const param_desc_t LOFI_RTRG = {"RTRG", F_ONOFF, 0, 1, 0, 0, 0};   /* (0.6.12) P_LRTRG on a LOFI track */
static const param_desc_t TRK_CHANCE = {"CHANCE", F_PCTINV, 0, 127, 0, 0, 0};   /* (0.6.37) P_TCHAN: 0 = 100 % */
static const param_desc_t KTRK_DESC = {"KEY>F", F_INT, -64, 63, 0, 0, "%"};   /* (chord-harp fork) P_KTRK */
static const char *const N_CLEN[] = {"OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15",
                                     "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
                                     "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43",
                                     "44", "45", "46", "47", "48", "49", "50", "51", "52", "53", "54", "55", "56", "57",
                                     "58", "59", "60", "61", "62", "63", "64", 0};
static const param_desc_t CLEN_DESC = {"C.LEN", F_ENUM, 0, 64, 0, N_CLEN, 0};   /* (0.6.47) P_CLEN */
static const param_desc_t DXIMG_DESC = {"IMAGE", F_INT, 0, 127, 0, 0, 0};       /* (0.6.48) P_DXIMG */
static const char *const N_CHRD[] = {"OFF", "TRIAD", "7TH", "ADD9", "SUS2", "SUS4", "MAJ", "MIN", "7", "MAJ7", "MIN7",
                                     "POWER", "HARP", "OMNI", "PROG", "GUITAR", 0};
static const char *const N_CVOI[] = {"CLOSE", "LEAD", "OPEN", "+BASS", 0};
static const param_desc_t CHORD_DESC[3] = {              /* (0.6.81) SCL > CHORD: P_CHRD P_CVOI P_CSTR */
    {"CHORD", F_ENUM, 0, 15, 0, N_CHRD, 0}, {"VOICING", F_ENUM, 0, 3, 0, N_CVOI, 0}, {"STRUM", F_PCT, 0, 127, 0, 0, 0}};
static const param_desc_t CHORD_TAB_DESC = {"CHORD", F_ENUM, 1, 11, 1, N_CHRD, 0};   /* (chord-harp fork) the CHORD tab's
                                                         * knob: HARP and OMNI are tabs of their own (ui.c chord_tab) */
static const param_desc_t HARP_SET_DESC0 = {"SET", F_ENUM, 0, HARP_NSET - 1, 0, HARP_SET_NAME, 0};   /* (chord-harp fork) HARP's
                                                         * own only: the J-6 sets in the resource region (res.c), missing */
static const param_desc_t HARP_SET_DESC = {"SET", F_ENUM, 0, HARP_NALL - 1, 0, HARP_SET_NAME, 0};   /* HARP: P_CVOI (the
                                                         * J-6's: their place in their genre, param_format) */
static const param_desc_t GTR_SET_DESC = {"SET", F_INT, 0, GT_NSET - 1, 0, 0, 0};   /* GUITAR: P_GSET (the style's place in its family, param_format) */
static const char *const N_GTUN[] = {"STD", "DROP D", "DBL DD", "OPEN D", "OPEN G", "OPEN E", "OPEN A", "DADGAD", "OPN Dm", "OPN Gm",
                                     "OPEN C", "OPN D6", "-1/2", "-1"};   /* GUITAR > TUNE (harp.c GT_TUN's order) */
static const char *const N_GSHP[] = {"1", "2", "3", "4", "5", "6"};
static const param_desc_t GTR_TUN_DESC = PE("TUNING", N_GTUN, 0), GTR_SHP_DESC = PE("SHAPE", N_GSHP, 0);   /* GUITAR > TUNE: P_GTUN, P_GSHP */
static const param_desc_t OMNI_SET_DESC = {"SET", F_ENUM, 0, OMNI_NSET - 1, 0, OMNI_SET_NAME, 0};   /* OMNI: P_CVOI */
static const param_desc_t HUM_DESC[3] = {                /* (0.6.87) SEQ > HUMAN: P_HVEL P_HTIM P_HGAT (127 = 0 %) */
    {"HUMAN", F_PCTINV, 0, 127, 127, 0, 0}, {"TIME", F_PCTINV, 0, 127, 127, 0, 0}, {"GATE", F_PCTINV, 0, 127, 127, 0, 0}};   /* (0.6.92:
                                                         * one amount, P_HVEL; TIME and GATE stay stored, 127) */
static const param_desc_t RECQ_DESC = {"REC Q", F_PCT, 0, 127, 127, 0, 0};   /* (0.6.88) P_HRQ: 100 % on the grid */
static const param_desc_t DRIFT_DESC = {"DRIFT", F_PCTINV, 0, 127, 127, 0, 0};   /* (0.7.8) P_DRIFT: 127 = 0 % */
static const param_desc_t WROT_DESC[2] = {               /* (0.7.8) WHEEL's ROTOR: P_WRDEP (127 = 100 %, as before), P_WDUST */
    {"DEPTH", F_PCT, 0, 127, 127, 0, 0}, {"DUST", F_PCTINV, 0, 127, 127, 0, 0}};
/* (2026-11-11) FMINI's patch values (core.h FX_*, VF): the descriptors by FMX_ID's order, found by id */
static const char *const N_FRNG[] = {"LO", "32'", "16'", "8'", "4'", "2'"};
static const char *const N_FCLR[] = {"WHITE", "PINK"}, *const N_FLFO[] = {"LFO", "SQUARE"}, *const N_FOUT[] = {"LOWPASS", "HIGHPASS"};
static const char *const N_FWAV[] = {"TRI", "SHARK", "SAW", "SQR", "PLS W", "PLS N", "SIN"};
static const param_desc_t FMX_DESC[NFMX] = {
    PE("RANGE", N_FRNG, 3), PE("RANGE", N_FRNG, 3), PE("RANGE", N_FRNG, 3),
    PE("WAVE", N_FWAV, 2), PE("WAVE", N_FWAV, 2), PE("WAVE", N_FWAV, 2),
    PD("LEVEL", F_PCT, 0, 127, 0), PD("LEVEL", F_PCT, 0, 127, 0), PD("LEVEL", F_PCT, 0, 127, 0), PD("NOISE", F_PCT, 0, 127, 0),
    {"DET", F_INT, -700, 700, 0, 0, "ct"}, {"DET", F_INT, -700, 700, 0, 0, "ct"},
    {"TUNE", F_INT, -64, 63, 0, 0, 0}, PD("MIX", F_PCT, 0, 127, 0),
    PE("COLOR", N_FCLR, 0), PE("MOD>OSC", N_ONOFF, 0), PE("MOD>FLT", N_ONOFF, 0), PE("LFO", N_FLFO, 0), PE("OUTPUT", N_FOUT, 0),
};
static int16_t fmx_ui;           /* a patch setting on a page: its value (page_desc), written back by ui_input.c edit_param */
static uint8_t fmx_ui_k;
static const param_desc_t *track_desc(const track_t *t, uint32_t id)
{
    if (IS_VF(id))                                    /* (2026-11-11) FMINI's patch settings (core.h VF) */
        return &FMX_DESC[id - 0xA0u];
    if (id == MOT_DJF)                                /* (0.9.1) a DJ FILTER record (motion.c): the global's */
        return &GP[G_DJF];
    if (id >= P_E0 && id <= P_E7) {                   /* the engine asked for (t->engine follows after a fade) */
        const engine_t *e = ENGINES[eng_idx(t->eng_req)];
        const param_desc_t *d = e->desc ? e->desc(t, id - P_E0) : 0;   /* a mode-dependent label / names */
        return d ? d : &e->edit[id - P_E0];
    }
    if (id == P_LRTRG && ENGINES[eng_idx(t->eng_req)] == &ENG_LOFI)   /* (0.6.12) LOFI's RTRG in DIGITAL's slot */
        return &LOFI_RTRG;
    if (id == P_TCHAN && !(YEHNAH_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL))   /* (0.6.37) SHIFT's CHANCE */
        return &TRK_CHANCE;
    if (!(YEHNAH_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL))   /* (0.6.43) MUTATE's four */
        switch (id) {
        case P_KTRK: return &KTRK_DESC;
        case P_CLEN: return &CLEN_DESC;
        case P_DXIMG: return &DXIMG_DESC;
        case P_CHRD: return &CHORD_DESC[0];
        case P_CVOI: return t->p[P_CHRD] == 12 ? (RES_OK() ? &HARP_SET_DESC : &HARP_SET_DESC0) : t->p[P_CHRD] == 13 ? &OMNI_SET_DESC : &CHORD_DESC[1];   /* (HARP,
                                                         * OMNI: the set) */
        case P_CSTR: return &CHORD_DESC[2];
        case P_GSET: return &GTR_SET_DESC;
        case P_GTUN: return &GTR_TUN_DESC;
        case P_GSHP: return &GTR_SHP_DESC;
        case P_HVEL: return &HUM_DESC[0];
        case P_HTIM: return &HUM_DESC[1];
        case P_HGAT: return &HUM_DESC[2];
        case P_HRQ: return &RECQ_DESC;
        case P_DRIFT: return &DRIFT_DESC;
        case P_WRDEP: return &WROT_DESC[0];
        case P_WDUST: return &WROT_DESC[1];
        default: break;
        }
    return &TP[id];
}

/* the descriptor of parameter id for engine e without the engine's desc hook (the device display
 * only; range and default are the same): what the editor protocol, user presets and projects use */
static const param_desc_t *param_desc_of(uint32_t e, uint32_t id)
{
    return id >= P_E0 ? &ENGINES[e]->edit[id - P_E0] : &TP[id];
}

/* a retired F_ENUM value kept as an alias (the sample engines' SET / SRC aliases left with them in Yehnah): none now.
 * -> the value v stands for */
static int32_t enum_orig(const param_desc_t *d, int32_t v)
{
    (void)d;
    return v;
}

/* a stored value as the parameter takes it (projects, user presets, motion): inside d's range. FM6's SLOT is never
 * stored as a bank slot (0.6.11, from Felucca 1.4.1): 9..40 (B1..B32) is 8, OWN, and the patch comes with what is
 * loaded */
static int32_t param_fit(const param_desc_t *d, int32_t v)
{
    v = clamp(v, d->min, d->max);
    return d->names == N_FM6_PATCH && v > (int32_t)FM6_OWN ? (int32_t)FM6_OWN : v;
}

/* v is a value a knob passes over: an alias, or one of FM6's bank slots without a voice */
static int enum_skip(const param_desc_t *d, int32_t v)
{
    return enum_orig(d, v) != v || (d->names == N_FM6_PATCH && !fm6_slot_ok(v));
}
/* a knob moved an F_ENUM from `from` to v: past any value it skips in that direction; with none left to the end, the
 * last one it does not skip on the way from `from` (else `from`) */
static int32_t enum_step(const param_desc_t *d, int32_t from, int32_t v)
{
    int32_t dir = v > from ? 1 : -1, w = v;
    while (w != from && enum_skip(d, w))
        w = w + dir > d->max || w + dir < d->min ? from : w + dir;
    while (w == from && v != from && enum_skip(d, v))
        v -= dir;
    return w == from ? v : w;
}

/* #48: the note divisions in the order of their length, longest first (triplets between their neighbours), on the
 * knobs and the gauges; the stored values (N_DIV, N_SLDIV indices: projects, presets, the editor protocol) stay.
 * -> the shown order of d's values (index: position, entry: value), 0 = the values' own order */
static const uint8_t DIV_ORDER[10] = {9, 8, 7, 6, 0, 1, 4, 2, 5, 3};   /* 4BAR 2BAR 1/1 1/2 1/4 1/8 8T 1/16 16T 1/32 */
static const uint8_t PDIV_ORDER[11] = {9, 8, 7, 6, 0, 1, 4, 2, 5, 3, 10};   /* ... 1/32, 1/64 */
static const uint8_t ADIV_ORDER[13] = {9, 8, 7, 6, 10, 0, 11, 1, 12, 4, 2, 5, 3};   /* ... 1/2 1/4D 1/4 1/8D 1/8 1/16D 8T 1/16 16T 1/32 */
static const uint8_t SLDIV_ORDER[6] = {0, 3, 1, 4, 2, 5};              /* 1/8 8T 1/16 16T 1/32 32T */
static const uint8_t RWAVE_ORDER[7] = {5, 6, 0, 1, 2, 3, 4};   /* (0.4) LOFI's WAVE: BYTE first (0.5.1: FLOAT
                                                         * next), then PLS .. WRAM */
static const uint8_t DTIME_ORDER[NELEM(N_DTIME)] = {9, 8, 7, 6, 0, 1, 4, 2, 5, 3, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
                                                 58, 59, 60, 61, 62
#if DLY_LEN >= 65536u
                                                 , 63, 64, 65, 66, 67, 68, 69
#else
                                                 , 63
#endif
                                                 };   /* the divisions as DIV_ORDER,
                                                         * then the free times, short to long */
static const uint8_t *enum_order(const param_desc_t *d)
{
    return d->names == N_DIV ? DIV_ORDER : d->names == N_PDIV ? PDIV_ORDER : d->names == N_ADIV ? ADIV_ORDER : d->names == N_SLDIV ? SLDIV_ORDER : d->names == N_DTIME ? DTIME_ORDER :
           d->names == N_RWAVE ? RWAVE_ORDER : 0;
}
static int32_t enum_rank(const param_desc_t *d, int32_t v)   /* v's place in the shown order (+ min): the gauges */
{
    const uint8_t *o = enum_order(d);
    int32_t r;
    if (d->fmt == F_PCTINV) return d->max + d->min - v;   /* (the gauge full at 0) */
    for (r = 0; o && r < d->max - d->min; r++)
        if (o[r] == v - d->min)
            break;
    return o ? r + d->min : v;
}
/* (2026-11-03, the user's) a knob with a grid (a pick) wraps as the page row does: the last, then the first */
static int32_t wrap_add(int32_t v, int32_t s, int32_t n) { return n > 0 ? ((v + s) % n + n) % n : 0; }
/* d shown as a pick (ui_draw.c gauge: an enum or note of 2..32 values, a count of 2..16): its count, else 0 */
static int32_t param_pick(const param_desc_t *d)
{
    int32_t n = d->max - d->min + 1;
    return ((d->fmt == F_ENUM || d->fmt == F_NOTE || d->fmt == F_ONOFF) && n >= 2 && n <= 32) ||
           ((d->fmt == F_INT || d->fmt == F_STEPS) && n >= 2 && n <= 16) ? n : 0;
}
/* a knob turned `steps` (signed) from v: clamped (a pick: wrapped), over the shown order, past aliases (enum_step) */
static int32_t param_turn(const param_desc_t *d, int32_t v, int32_t steps)
{
    const uint8_t *o = enum_order(d);
    int32_t n = param_pick(d), w, k;
    if (d->fmt == F_PCTINV) steps = -steps;         /* (right: more chance, less skipped) */
    if (n && steps) {
        w = wrap_add(enum_rank(d, v) - d->min, steps, n);
        if (o)
            return o[w] + d->min;
        for (k = 0, w += d->min; k < n && enum_skip(d, w); k++)   /* (past what the knob skips, onwards) */
            w = wrap_add(w - d->min, steps > 0 ? 1 : -1, n) + d->min;
        return k < n ? w : v;
    }
    if (o)
        return o[clamp(enum_rank(d, v) - d->min + steps, 0, d->max - d->min)] + d->min;
    return enum_step(d, v, clamp(v + steps, d->min, d->max));
}

static uint8_t fmt_named;        /* the last param_format was a name (F_ENUM, even "4"): ui_draw.c's digits do not roll it */

/* value string (<= 5 chars; a name <= 7, chord-harp fork) and unit for a parameter value */
static void param_format(const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    *unit = "";
    fmt_named = d->fmt == F_ENUM;
    if (d == &HARP_SET_DESC && v >= (int32_t)HARP_NSET) {   /* (2026-10-27) a J-6 set: "3" "/10", its place in its genre */
        static char u[5] = "/";
        uint32_t g = harp_genre((uint32_t)v), f = harp_genre_set(g), n = f;
        while (n < HARP_NALL && harp_genre(n) == g) n++;
        fmt_int(val, v - (int32_t)f + 1);
        fmt_int(u + 1, (int32_t)(n - f));
        *unit = u;
        return;
    }
    if (d == &GTR_SET_DESC) {                           /* (2026-11-10) a guitar style: "3" "/9", its place in its family */
        static char u[5] = "/";
        uint32_t g = GT_GEN[(uint32_t)clamp(v, 0, (int32_t)GT_NSET - 1)], f = gt_genre_set(g), n = f;
        while (n < GT_NSET && GT_GEN[n] == g) n++;
        fmt_int(val, v - (int32_t)f + 1);
        fmt_int(u + 1, (int32_t)(n - f));
        *unit = u;
        return;
    }
    if (d == &TP[P_AGATE] && v >= 127) {                /* (2026-11-10) the strum modes' FREE gate (seq.c arp_free) */
        str_cpy(val, "FREE", 6);
        fmt_named = 1;
        return;
    }
    if (d == &TP[P_LPHASE] && v < 0) {                  /* (0.6.51) the LFO's PHASE FREE */
        str_cpy(val, "FREE", 6);
        fmt_named = 1;
        return;
    }
    switch (d->fmt) {
    case F_PCT:                                     /* a 0..100 range (SWG) is its value, others a share of 127 */
        fmt_int(val, d->max == 100 || d->max == 120 ? v : (v * 100 + 63) / 127);   /* (0.6.86: DLY FEEDBK 0..120 %, 100 the
                                                                              * loop's gain 1) */
        *unit = "%";
        break;
    case F_PCTINV:
        fmt_int(val, ((d->max - v) * 100 + 63) / 127);
        *unit = "%";
        break;
    case F_BIPCT:
        fmt_int(val, v * 100 / 64);
        if (v > 0) {
            char t[8];
            fmt_int(t, v * 100 / 64);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "%";
        break;
    case F_TIME: {
        uint32_t ms10 = TIME_MS_X10[v & 127];
        if (ms10 < 100u) {
            fmt_fix(val, (int32_t)ms10, 1);
            *unit = "ms";
        } else if (ms10 < 10000u) {
            fmt_int(val, (int32_t)((ms10 + 5u) / 10u));
            *unit = "ms";
        } else {
            fmt_fix(val, (int32_t)(ms10 / 100u), 2);
            if (ms10 >= 100000u)
                fmt_fix(val, (int32_t)(ms10 / 1000u), 1);
            *unit = "s";
        }
        break;
    }
    case F_LFOHZ: {
        uint32_t h = LFO_HZ_X100[v & 127];
        if (v < 0) {                                    /* (0.6.51) the LFO in time */
            str_cpy(val, N_LSYNC[(uint32_t)(-v - 1) % 12u], 6);
            break;
        }
        if (h < 1000u)
            fmt_fix(val, (int32_t)h, 2);
        else
            fmt_fix(val, (int32_t)(h / 10u), 1);
        *unit = "Hz";
        break;
    }
    case F_CUTOFF: {
        uint32_t h = CUTOFF_HZ[v & 127];
        if (h < 1000u) {
            fmt_int(val, (int32_t)h);
            *unit = "Hz";
        } else {
            fmt_fix(val, (int32_t)(h / 100u), 1);
            *unit = "kHz";
        }
        break;
    }
    case F_DB:
        if (v <= 0) {
            str_cpy(val, "OFF", 6);
        } else {
            fmt_fix(val, LEVEL_DB_X10[v], 1);
            *unit = "dB";
        }
        break;
    case F_SEMI:
        fmt_int(val, v);
        if (v > 0) {
            char t[8];
            fmt_int(t, v);
            val[0] = '+';
            str_cpy(val + 1, t, 6);
        }
        *unit = "st";
        break;
    case F_ENUM:                                      /* (chord-harp fork: up to 7 letters, was 5: "RHODES", "MARIMBA";
                                                         * a card draws a long one smaller, ui_draw.c draw_column) */
        str_cpy(val, d->names[v < d->min ? d->min : v > d->max ? d->max : v], 8);
        if (d->unit)
            *unit = d->unit;
        break;
    case F_BPM:
        fmt_int(val, v);
        *unit = "BPM";
        break;
    case F_NOTE:
        str_cpy(val, N_NOTE[v % 12], 6);
        break;
    case F_ONOFF:
        str_cpy(val, N_ONOFF[v ? 1 : 0], 6);
        break;
    case F_STEPS:
        fmt_int(val, v);
        *unit = "STEP";
        break;
    default:
        if (d->names) {                               /* F_INT with a 0-terminated name list: the range */
            uint32_t k = 0;                           /* split evenly over the names (engine desc hooks) */
            while (d->names[k])
                k++;
            str_cpy(val, d->names[(uint32_t)(clamp(v, d->min, d->max) - d->min) * k / (uint32_t)(d->max - d->min + 1)], 6);
        } else {
            fmt_int(val, v);
        }
        if (d->unit)
            *unit = d->unit;
        break;
    }
}

/* ------------------------------------------------------------ pages --- */
enum { FAM_HOME, FAM_ENV, FAM_LFO, FAM_FX, FAM_SCL, FAM_EDIT, FAM_GLO, FAM_SAVE, FAM_ARP, FAM_SEQ, FAM_TRK,
       FAM_COUNT };
enum { SC_TRACK, SC_GLOBAL, SC_ENGINE, SC_STEP, SC_TRK };   /* SC_TRK: the TRACKS page (ui_input.c tracks_edit) */
enum { GR_NONE, GR_ADSR, GR_LFO, GR_STEPS, GR_ARP, GR_SCALE, GR_FX, GR_ROLL, GR_BROWSE, GR_SLOTS, GR_USER, GR_TRK,
       GR_SLCR, GR_MOD, GR_SONG, GR_TOOLS, GR_ERASE, GR_SEDIT, GR_CHANCE, GR_MOTION, GR_DXSND, GR_MSEQ, GR_SLPIT,
       GR_FMOP, GR_FMOP2, GR_FMENV,
       GR_COMP, GR_MORPH, GR_DXIMG, GR_SHAPE, GR_PUNCH, GR_BUSES, GR_STAGE,
       GR_PROG, GR_BLEND, GR_SETS, GR_PLIST };   /* (GR_STAGE: LENTE's ENV > STAGE; GR_PUNCH: FX > PUNCH;
                                                         * GR_COMP: no page uses it now, SEQ > SPICE draws GR_ROLL; GR_FMOP*: FM6's operator pages, ui_fm6op.c) */

typedef struct {
    const char *title;
    uint8_t fam, scope, graph;
    uint8_t id[4];               /* param ids; 0xFF = empty slot */
} page_t;

/* (0.6.18) GLO > SYSTEM's TRKS: how many tracks play, the setting of MENU > AUDIO > TRACKS (ui.c ui_rec_prefs bit 7,
 * kept with the settings, not in the song). An id past the globals (no song.g of its own): trks_ui mirrors the setting
 * (ui.c rp_apply), a turn writes it back (ui_input.c knob_turn) */
#define G_TRKS 0xFEu
static const char *const N_TRKS[] = {"1", "2", "3", "4", "5", "6"};   /* (chord-harp fork: 1..6, was 4 / 6) */
static const param_desc_t TRKS_DESC = PE("TRACKS", N_TRKS, 3);
static int16_t trks_ui;
/* (0.6.30) .. and LOAD, next to it (ui.c ui_load, voice.c sys_load): the heavy parts lightened or gone while they
 * sound, ECO the lightest. load_ui mirrors it as trks_ui does */
#define G_LOADP 0xFDu
/* (0.7.6) a track page's slot holding the global ALL TR (G_STRN): SCL's KNOB 4, where TRANS was (page_desc) */
/* the MENU's rows (menu_items.c: their names, values, the editor's MENU_DESC). (2026-11-02, the redesign step 5) GLO's
 * pages name them: a knob MROW(MI_*) is that row, drawn and turned as the MENU does (ui_draw.c, ui_input.c); page_desc
 * gives none */
enum { MI_COLOR, MI_LEDS, MI_SCROFF, MI_FILL, MI_HOLD, MI_ACCEL, MI_LATCH, MI_SCLLED, MI_MIDIOUT,   /* (2026-11-02: SKIN STYLE
       * ANIM BPM LOCK SCOPE BIG VIEW gone) */
       MI_LOWCUT, MI_USB,
       MI_CLICK, MI_CLKLVL, MI_COUNTIN, MI_TRACKS, MI_SERIAL, MI_RESTORE, MI_LOAD, MI_PANEL, MI_INIT, MI_ABOUT, MI_COUNT };   /* (1.1: the metronome's rows in
                                                                                             * AUDIO; 1.2: RESTORE LAST in SYSTEM, SCALE LEDS
                                                                                             * in CONTROL) */
#define ID_MENU 0xD0u
#define MROW(r) (ID_MENU + (r))
#define IS_MROW(id) ((id) >= ID_MENU && (id) < ID_MENU + MI_COUNT)
#define ID_ALLTR 0xFCu
static const char *const N_LOAD[] = {"ECO", "LITE", "FULL"};
static const param_desc_t LOAD_DESC = PE("LOAD", N_LOAD, 2);
static int16_t load_ui;
static uint8_t rev_rt0 = 0xFFu;                         /* (the reverb model ui_input_frame last saw) */
#if FELUCCA_FXSCENE
/* (chord-harp fork, 2026-11-02) FX scenes: the reverb and delay are one bus each, so a sound can't have its own; a
 * scene is a whole setting of both (ui_input.c FX_SCENE), loaded at once. GLO > FX's SCENE loads one as it turns
 * (scene_ui: the last loaded, "--" none); a user preset may carry one (SAVE > USER FX, ufx_ui: what SAVE stores,
 * upreset.c UP_FX_AT), loaded with the sound. Not in the song: the bus's values themselves are */
#define G_SCENEP 0xFBu
#define G_UFXP 0xFAu
static const char *const N_SCENE[] = {"--", "ROOM", "HALL", "SPRING", "SHIMR", "CLOUD", "RESO", "TAPE", "PING", "GRAIN"};
#define N_SCENES 9u
static const param_desc_t SCENE_DESC = PE("SCENE", N_SCENE, 0);
static const param_desc_t UFX_DESC = PE("FX", N_SCENE, 0);
static int16_t scene_ui, ufx_ui;
/* (chord-harp fork, 2026-11-02) the FX scenes (N_SCENE): the reverb's TYPE SIZE DAMP PRE MOD RATE FILT WIDE,
 * the delay's TIME FEEDBK TONE MIX PITCH SPRAY MOD TYPE PING. A scene sets the whole bus at once (every track hears it) */
static const uint8_t FX_SCENE_ID[17] = {G_RTYPE, G_RSIZE, G_RDAMP, G_RPRE, G_RMOD, G_RRATE, G_RFILT, G_RWIDE,
                                        G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX, G_DPIT, G_DSPRY, G_DMOD, G_DTYPE, G_DPING};
static const int8_t FX_SCENE[N_SCENES][17] = {
    {0,  90, 60,  0,  0, 30,   0,   0,   1, 60, 64,  90,  0,  0,  0, 0, 0},   /* ROOM: the defaults */
    {0, 115, 50, 20, 30, 30, -10,  80,   0, 45, 50,  70,  0,  0, 10, 0, 1},   /* HALL: long, wide, a soft echo */
    {1,  80, 40,  0, 40, 40,  10,  40,   1, 25, 60,  60,  0,  0,  0, 0, 0},   /* SPRING: the tank, a short echo */
    {2, 115, 40,  0, 90, 30, -10,  60,   0, 55, 40,  60,  0,  0, 20, 0, 1},   /* SHIMR: octaves rising */
    {4, 110, 50,  0, 40, 60,   0, 100,   0, 50, 45,  70,  0, 40, 10, 0, 1},   /* CLOUD: a granular wash */
    {3, 100,  3,  0, 40, 30,   0,  40,   1, 40, 64,  60,  0,  0,  0, 0, 0},   /* RESO: rings in the key */
    {0,  60, 70,  0,  0, 30,   0,   0,   1, 70, 40, 100,  0,  0, 60, 0, 0},   /* TAPE: a small room, a wobbly echo */
    {0,  80, 60,  0,  0, 30,   0,  40,   1, 55, 64,  90,  0,  0,  0, 1, 1},   /* PING: clean echoes left and right */
    {0, 100, 55,  0, 20, 30,   0,  60,   0, 65, 50,  90,  7, 60, 10, 0, 1},   /* GRAIN: each repeat a fifth up, scattered */
};
static void fx_scene(uint32_t n)                        /* 1..N_SCENES (0: none, the bus as it is) */
{
    uint32_t i;
    if (!n || n > N_SCENES)
        return;
    for (i = 0; i < 17u; i++)
        song.g[FX_SCENE_ID[i]] = (int16_t)clamp(FX_SCENE[n - 1u][i], GP[FX_SCENE_ID[i]].min, GP[FX_SCENE_ID[i]].max);
    rev_rt0 = (uint8_t)song.g[G_RTYPE];                 /* (the scene's PRE / AIR, DAMP as set: not the model change's) */
    scene_ui = ufx_ui = (int16_t)n;
}
#endif /* FELUCCA_FXSCENE */
static const page_t PAGES[] = {
    {"ENV", FAM_ENV, SC_TRACK, GR_ADSR, {P_ATK, P_DEC, P_SUS, P_REL}},
    {"ENV DEST", FAM_ENV, SC_TRACK, GR_NONE, {P_ED_FLT, P_ED_PIT, P_ED_SHP, P_ELOOP}},   /* 0.4: LOOP */
    {"LFO", FAM_LFO, SC_TRACK, GR_LFO, {P_LRATE, P_LWAVE, P_LPHASE, P_LFADE}},
    {"LFO DEST", FAM_LFO, SC_TRACK, GR_NONE, {P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP}},
    {"MOD", FAM_LFO, SC_TRACK, GR_MOD, {0xFF, P_M1SRC, P_M1DST, P_M1AMT}},   /* KNOB 1: the slot (mod_ui_slot) */
    {"MSEQ", FAM_LFO, SC_TRACK, GR_MSEQ, {0xFF, P_MS0, P_MSLEN, P_MSSLW}},   /* Yehnah: KNOB 1 the step (ms_ui_step) */
    {"MSEQ 2", FAM_LFO, SC_TRACK, GR_NONE, {P_MSDIV, 0xFF, 0xFF, 0xFF}},
    {"FX", FAM_FX, SC_TRACK, GR_FX, {P_DIST, P_CHOR, P_DLY, P_REV}},
    {"MASTER", FAM_FX, SC_GLOBAL, GR_NONE, {G_CLIP, G_PUNCH, G_DUCK, G_DREL}},   /* Yehnah: clipper, drum bus, ducking;
                                                         * (0.7.6) FX's second page, at hand (was after SL PITCH) */
    {"DIST", FAM_FX, SC_TRACK, GR_NONE, {P_DIST, P_DTYPE, P_DTONE, 0xFF}},   /* Yehnah: the drive's type and tone */
    {"AUTO PAN", FAM_FX, SC_TRACK, GR_NONE, {P_APDEP, P_APRATE, P_APSYNC, P_APWAVE}},   /* (0.6.9) the track's pan swung */
    {"PHASER", FAM_FX, SC_TRACK, GR_NONE, {P_PHAMT, P_PHRATE, P_PHFB, 0xFF}},   /* (chord-harp fork) on the part's sum */
    {"BUSES", FAM_FX, SC_GLOBAL, GR_BUSES, {G_DMIX, G_RSIZE, G_CDEPTH, G_SLDEP}},   /* (0.8.5) FX's lens: the buses' main values */
#if FELUCCA_FXSCENE
    {"SCENE", FAM_FX, SC_GLOBAL, GR_NONE, {G_SCENEP, 0xFF, 0xFF, 0xFF}},   /* (chord-harp fork) the buses' FX scenes */
#endif
    {"DLY", FAM_FX, SC_GLOBAL, GR_NONE, {G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX}},
    {"DLY 2", FAM_FX, SC_GLOBAL, GR_NONE, {G_WIDTH, G_DPIT, G_DSPRY, G_DMOD}},
    {"DLY 3", FAM_FX, SC_GLOBAL, GR_NONE, {G_DTYPE, G_DPING, 0xFF, 0xFF}},   /* (0.6.90) ANALOG / DIGITAL, PING-PONG */   /* Yehnah: width, the grains (0.4: the low cut
                                                                                 * in DLY's TONE; G_DHPF stored, unused) */
    {"REVERB", FAM_FX, SC_GLOBAL, GR_NONE, {G_RTYPE, G_RSIZE, G_RDAMP, G_RPRE}},   /* TYPE: ROOM / SPRING */
    {"REVERB 2", FAM_FX, SC_GLOBAL, GR_NONE, {G_RMOD, G_RRATE, G_RFILT, G_RWIDE}},   /* Yehnah: modulation, tone, width */
    {"CHORUS", FAM_FX, SC_GLOBAL, GR_NONE, {G_CRATE, G_CDEPTH, G_CSTER, 0xFF}},   /* (0.6.9: STEREO) */
    /* (0.6.81) the SLICER after the buses, before MASTER: DELAY, REVERB and CHORUS nearer at hand */
    {"SLICER", FAM_FX, SC_GLOBAL, GR_SLCR, {G_SLMODE, G_SLPAT, G_SLRATE, G_SLDEP}},   /* (0.6.3) one, on a bus */
    {"SL BUS", FAM_FX, SC_GLOBAL, GR_SLCR, {G_SLT1, G_SLT1 + 1, G_SLT1 + 2, G_SLT1 + 3}},   /* what goes in */
    {"SL BUS 2", FAM_FX, SC_GLOBAL, GR_SLCR, {G_SLT1 + 4, G_SLT6, G_SLFX, 0xFF}},
    {"SL PITCH", FAM_FX, SC_GLOBAL, GR_SLPIT, {0xFF, G_SLP0, G_SLPLEN, G_SLPQ}},   /* KNOB 1 the step (sl_ui_step) */
    {"PUNCH", FAM_FX, SC_GLOBAL, GR_PUNCH, {G_PFXLEN, G_PFXRES, 0xFF, 0xFF}},   /* (0.6.99) the punch-in lane: LEN RES, REC, CLEAR */
    {"SCL", FAM_SCL, SC_TRACK, GR_SCALE, {P_ROOT, P_SCALE, P_QUANT, ID_ALLTR}},   /* (Yehnah: ROOT, SCALE every melodic
                                                         * track's; 0.7.6: ALL TR, the global, where TRANS was: SCL 2 gone;
                                                         * chord-harp fork: QUANT on SCL held too, ui_layer.c LY_SCL) */
    {"CHORD", FAM_SCL, SC_TRACK, GR_NONE, {P_ROOT, P_CHRD, P_CVOI, P_HPLATE}},   /* (0.6.81) a key plays a chord (seq.c);
                                                         * (chord-harp fork) ROOT on KNOB 1 too, as SCL's: chords and
                                                         * root from one page; 2026-10-27: KNOB 4 PLATE (HARP's), STRUM
                                                         * on SCL held */
    {"EDIT 1", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E0, P_E1, P_E2, P_E3}},
    {"EDIT 2", FAM_EDIT, SC_ENGINE, GR_NONE, {P_E4, P_E5, P_E6, P_E7}},
    {"DRIFT", FAM_EDIT, SC_TRACK, GR_NONE, {P_DRIFT, P_VINT, 0xFF, 0xFF}},   /* (0.7.8) DRIFT: the voices wander;
                                                         * (chord-harp fork, 2026-10-30) VINTAGE, Clipper's (was MORPH,
                                                         * ANALOG PHASE LOFI's waves: gone with their engines) */
    {"MORPH", FAM_SAVE, SC_GLOBAL, GR_BLEND, {G_BLA, G_BLB, G_BLEND, G_BMOVE}},   /* (chord-harp fork, 2026-10-29) two
                                                         * tracks, one part: TRACK A, TRACK B, BLEND, MOVE (voice.c bl_*).
                                                         * SAVE's SOUND, between PRESETS and USER */
    /* (0.6.54) DRUM: the whole kit first (KIT = EDIT 1, KIT 2 = EDIT 2, KIT 3, X-MOD, FILTER), then a sound
     * (SOUND 1..4, KNOB 1 the sound on each), (0.6.58) then the volumes (LEVELS, LEVELS 2); (0.7.8) STEREO after X-MOD */
    {"KIT 3", FAM_EDIT, SC_TRACK, GR_DXIMG, {0xFF, 0xFF, 0xFF, P_DXIMG}},  /* the kit's BP, FEEDBK, HANDS and IMAGE (2026-11-10: STEREO
                                                         * merged in; on an LCD-4 MACHINE only IMAGE works) */
    {"X-MOD", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},     /* (0.5.1) the kit's LFO on MORPH, RAND */
    {"OPERATOR", FAM_EDIT, SC_TRACK, GR_FMOP, {0xFF, 0xFF, 0xFF, 0xFF}},    /* FM6 only: KNOB 1 the operator, then its */
    {"OP ENV", FAM_EDIT, SC_TRACK, GR_FMENV, {0xFF, 0xFF, 0xFF, 0xFF}},     /* bytes of the track's patch (ui_fm6op.c): */
    {"OPERATOR 2", FAM_EDIT, SC_TRACK, GR_FMOP2, {0xFF, 0xFF, 0xFF, 0xFF}}, /* CRS FINE LEVEL / STG RATE LEVEL / MODE DTUN VEL */
    {"FILTER", FAM_EDIT, SC_TRACK, GR_NONE, {P_FMODEL, P_E4, P_E5, P_VELF}},   /* ANALOG: (Clipper's) MODL CUT RES VEL>F */
    {"FILTER", FAM_EDIT, SC_TRACK, GR_NONE, {P_FTYPE, P_FCUT, P_FRES, P_DIST}},   /* .. every other engine's (on the part;
                                                         * DRV: the part's DIST, after it) */
    {"SOUND", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},   /* DRUM: a sound of the kit (ui_dx.c) */
    {"SOUND 2", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"SOUND 3", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.4) MODE, the pitch modulation, DRIVE */
    {"SOUND 4", FAM_EDIT, SC_TRACK, GR_DXSND, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.54) LAYER, FEEDBK, SEND */
    {"LEVELS", FAM_EDIT, SC_TRACK, GR_NONE, {P_LN0, P_LN1, P_LN2, P_LN3}},   /* DRUM only: the sounds' levels (was LANES) */
    {"LEVELS 2", FAM_EDIT, SC_TRACK, GR_NONE, {P_LN4, P_LN5, P_LN6, P_LN7}},
    {"VOICE", FAM_EDIT, SC_TRACK, GR_NONE, {P_VOICE, P_GLIDE, P_GLMODE, P_PRIO}},
    /* (2026-11-10, topic C) GLO as four lists (SETS below): KNOB 1 scrolls the rows top to bottom, KNOB 2 sets the row's value (an
     * action row, KNOB 2 right, asks YES / NO); the page's id[] stay empty, cur_page() hands out a copy with the row's id in slot 1 */
    {"MAIN", FAM_GLO, SC_GLOBAL, GR_SETS, {0xFF, 0xFF, 0xFF, 0xFF}},       /* BPM SWING TRACKS USB CLICK: the everyday ones */
    {"DEVICE", FAM_GLO, SC_GLOBAL, GR_SETS, {0xFF, 0xFF, 0xFF, 0xFF}},     /* the screen and the LEDs */
    {"CONTROL", FAM_GLO, SC_GLOBAL, GR_SETS, {0xFF, 0xFF, 0xFF, 0xFF}},    /* the knobs, MIDI, the speaker */
    {"SYSTEM", FAM_GLO, SC_GLOBAL, GR_SETS, {0xFF, 0xFF, 0xFF, 0xFF}},     /* load, CPU, serial, restore, calibration, init, about */
    {"PRESETS", FAM_SAVE, SC_GLOBAL, GR_BROWSE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* browser: PRESETS knob / KNOB 1 */
    {"PROJECT", FAM_SAVE, SC_GLOBAL, GR_SLOTS, {0xFF, G_SLOT, G_LOAD, G_SAVE}},   /* (0.6.12) PROJECT 1-8, SECT A-D */
    {"ARRANGE", FAM_SAVE, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.12) the project's song (SEQ > SONG) */
    {"SONG EDIT", FAM_SAVE, SC_GLOBAL, GR_SEDIT, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (2026-11-06) K1 ROW, K2 BAR, K3 TRIM START, K4 TRIM END; the
                                                         * clear / erase / new tools are the SAVE held layer's (ui_layer.c) */
    {"ARP", FAM_ARP, SC_TRACK, GR_ARP, {P_AMODE, P_ARATE, P_AOCT, P_AGATE}},
    {"ARP 2", FAM_ARP, SC_TRACK, GR_NONE, {P_ASWING, P_APROB, P_AHOLD, P_AORDER}},
    {"ARP 3", FAM_ARP, SC_TRACK, GR_NONE, {P_SPICE, P_SLOOP, 0xFF, 0xFF}},   /* (2026-11-02, NAV-LAYOUT.md) Clipper's SPICE,
                                                         * its LOOP (the TURNG mode's random melody, its repeat) */
#if FELUCCA_AMBIENT
    {"AMBIENT", FAM_ARP, SC_TRACK, GR_NONE, {P_AMB, P_ADEN, P_AMOT, P_ASPC}},   /* (chord-harp fork, 2026-11-05) amb.c */
#endif
    /* (0.6.90) SEQ: STEP (the piano roll) and the pages that edit it, one after another: NOTE, TIMING, STRETCH, COMP,
     * COMP 2, WHEN; then the track's: PATTERN, SHIFT, HUMAN, PLAYHEADS, AUTOMATION, AUTO LIST, MUTATE; PHRASES, SONG and
     * CHANCE out of the cycle (ui.c page_visible) */
    /* (0.6.92) eleven pages, was fifteen: the piano roll, the pattern, the roll's edits (NOTE: FOCUS PITCH MOVE MICRO,
     * TIMING folded in), STRETCH (on the turn), the step's two components (EVERY on them: WHEN and TRIG's loops in
     * one), TRACK (SHIFT's PITCH and CHANCE, HUMAN as one amount, REC Q; C.LEN retired from the pages), then the rest */
    {"STEP", FAM_SEQ, SC_STEP, GR_ROLL, {0, 1, 2, 3}},
    {"PATTERN", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},
    {"NOTE", FAM_SEQ, SC_STEP, GR_ROLL, {4, 5, 6, 9}},   /* (0.6.81) the piano roll's edits: FOCUS PITCH MOVE; (0.6.92) MICRO */
    {"STRETCH", FAM_SEQ, SC_TRACK, GR_SHAPE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (0.6.85) STRETCH REPEAT NUDGE (0.6.92: on the turn) ROTATE */
    {"SPICE", FAM_SEQ, SC_STEP, GR_ROLL, {7, 8, 9, 10}},   /* (2026-11-10) COMP and COMP 2 as one page on the NOTE roll: K1 NOTE, K2 TYPE,
                                                         * K3 its value, K4 which of the step's two components (ui_input.c flav_edit) */
    /* (0.8.2) the pages above edit the piano roll and keep it in the panel (ui_graph.c roll_view); TRACK and the ones
     * after are the track's own, with their own graphs. (0.8.3) LENTE (ui.c lens_*) groups them by level, the table
     * as it was: PATTERN (PATTERN STRETCH PLAYHEADS TRACK MUTATE), BAR (STEP NOTE AUTOMATION AUTO LIST), STEP (NOTE
     * COMP COMP 2); SELECT zooms, a SEQ tap the level's next set */
    {"TRACK", FAM_SEQ, SC_TRACK, GR_STEPS, {P_POFS, P_TCHAN, P_HVEL, P_HRQ}},   /* (0.6.92) PITCH CHANCE HUMAN REC Q */
    {"MIXER", FAM_TRK, SC_TRK, GR_TRK, {0, 1, 2, 3}},   /* GLO button; LEVEL PAN REV, (0.9.1) the DJ FILTER (was MUTE) */
    {"SONG", FAM_SEQ, SC_GLOBAL, GR_SONG, {0xFF, 0xFF, 0xFF, 0xFF}},
    {"CHANCE", FAM_SEQ, SC_STEP, GR_CHANCE, {0xFF, 0xFF, 0xFF, 0xFF}},   /* a step's CHANCE RATCH; 0.6.37: hidden */
    {"AUTOMATION", FAM_SEQ, SC_TRACK, GR_MOTION, {0xFF, 0xFF, 0xFF, 0xFF}},
    /* (0.8.5) LENTE everywhere (ui_lens.c): pages the mock-up added, appended (the table's order is no one's index) */
    {"VOICE 2", FAM_EDIT, SC_TRACK, GR_NONE, {P_ALLOC, P_DETUNE, P_KTRK, P_MWDST}},   /* EDIT > TONE: ALLOC, UNISON's
                                                         * DETUNE (in TP since Felucca, on no page until then);
                                                         * (2026-11-02, NAV-LAYOUT.md) KEY>F, MW> (on no page before) */
    {"STAGE", FAM_ENV, SC_TRACK, GR_STAGE, {0xFF, 0xFF, P_ELOOP, 0xFF}},   /* ENV's detail: STAGE (ATK DEC SUS REL), its
                                                         * value, LOOP (ui_input.c stage_knob) */
    {"SENDS", FAM_TRK, SC_TRACK, GR_TRK, {P_DIST, P_CHOR, P_DLY, P_REV}},   /* GLO > MIX: the track's sends (as FX > SENDS),
                                                         * the six tracks' as bars (ui_graph.c track_col6) */
    {"BAR", FAM_SEQ, SC_TRACK, GR_STEPS, {0xFF, P_SLEN, 0xFF, 0xFF}},   /* (0.9.2) SEQ > BAR: the pattern's bars, KNOB 1
                                                         * the bar (the cursor at its place in it; was PRESETS on PATTERN's
                                                         * level), KNOB 2 LEN (ui_input.c edit_param) */
    {"HARP", FAM_SCL, SC_TRACK, GR_NONE, {P_ROOT, 0xFF, P_CVOI, P_HPLATE}},   /* (chord-harp fork) SCL's HARP and OMNI
                                                         * tabs: going to one turns its mode on (ui.c chord_tab); HARP's
                                                         * KNOB 2 GENRE (SET to the genre's first), KNOB 3 SET within
                                                         * it (2026-10-27, ui_input.c edit_param; STRUM on SCL held) */
    {"OMNI", FAM_SCL, SC_TRACK, GR_NONE, {P_ROOT, P_CVOI, P_OAUTO, P_OSTR}},
    {"GUITAR", FAM_SCL, SC_TRACK, GR_NONE, {P_ROOT, 0xFF, P_GSET, P_HPLATE}},   /* (2026-11-10) SCL > GUITAR: guitchordi's chord
                                                         * styles on HARP's keys; KNOB 2 GENRE, KNOB 3 the style within it */
    {"TUNE", FAM_SCL, SC_TRACK, GR_NONE, {P_GTUN, P_GSHP, 0xFF, P_HPLATE}},   /* (2026-11-10) SCL > TUNE: GUITAR's tuning
                                                         * (14) and the shape (1..6) of each chord in it; a tab of GUITAR's mode */
    {"PROG", FAM_SCL, SC_TRACK, GR_PROG, {P_ROOT, P_PROG, 0xFF, P_HPLATE}},   /* (chord-harp fork) SCL > PROG: the chord
                                                         * keys a progression's chords (STRUM: HARP's); KNOB 3 RATE, KNOB 4
                                                         * PLATE (HARP's); F#4 WRITE, OCT+ writing it on the track
                                                         * (2026-10-27, ui_input.c prog_write) */
    {"PATCH", FAM_EDIT, SC_TRACK, GR_PLIST, {0xFF, 0xFF, 0xFF, 0xFF}},   /* (2026-11-11) FMINI's whole patch as a list: KNOB 1 the row,
                                                         * KNOB 2..4 its settings (plist_page); on FMINI only (page_visible) */
};
#define BAR_PAGE(pg) ((pg)->graph == GR_STEPS && (pg)->id[0] == 0xFFu)
#define GTR_PAGE(pg) ((pg)->id[2] == P_GSET)
#define GTU_PAGE(pg) ((pg)->id[0] == P_GTUN)          /* (2026-11-10) SCL > TUNE */          /* (2026-11-10) SCL > GUITAR: KNOB 2 GENRE, KNOB 3 STYLE */
#define HARP_PAGE(pg) ((pg)->id[3] == P_HPLATE && (pg)->id[1] == 0xFFu && !GTR_PAGE(pg))   /* (chord-harp fork) SCL > HARP: KNOB 2 GENRE
                                                         * (CHORD has PLATE too) */
#define NPAGES (sizeof(PAGES) / sizeof(PAGES[0]))
/* (2026-11-11) EDIT > PATCH on FMINI: every setting of the patch in one list. KNOB 1 picks the row, KNOB 2..4 are its (up to) three
 * settings; the page cur_page() hands out is a copy with the row's ids in slots 1..3. The picture (ui_lzlist.c) is the list */
typedef struct { const char *name; uint8_t id[3]; const char *s[3]; } plist_row_t;
static const plist_row_t PLIST[] = {
    {"OSC 1", {VF(FX_R1), VF(FX_W1), VF(FX_L1)}, {"RNG", "WAV", "LVL"}},
    {"OSC 2", {VF(FX_R2), VF(FX_W2), VF(FX_L2)}, {"RNG", "WAV", "LVL"}},
    {"OSC 3", {VF(FX_R3), VF(FX_W3), VF(FX_L3)}, {"RNG", "WAV", "LVL"}},
    {"TUNING", {VF(FX_D2), VF(FX_D3), VF(FX_TU)}, {"DT2", "DT3", "TUN"}},
    {"NOISE", {VF(FX_LN), VF(FX_PK), 0xFF}, {"LVL", "CLR", ""}},
    {"FILTER", {P_E0, P_E1, P_E7}, {"CUT", "EMP", "KBD"}},
    {"CONTOUR", {P_E2, P_E3, P_E4}, {"AMT", "ATK", "DEC"}},
    {"SUSTAIN", {P_E5, VF(FX_HP), 0xFF}, {"SUS", "OUT", ""}},
    {"MOD", {P_E6, VF(FX_MM), VF(FX_SQ)}, {"AMT", "MIX", "LFO"}},
    {"MOD TO", {VF(FX_MO), VF(FX_MF), 0xFF}, {"OSC", "FLT", ""}},
};
#define PLIST_N (sizeof(PLIST) / sizeof(PLIST[0]))
static uint8_t plist_cur;
static int32_t plist_val(const track_t *t, uint32_t id) { return IS_VF(id) ? fmx_get(t->p, id - 0xA0u) : t->p[id]; }
static const page_t *plist_page(const page_t *p)
{
    static page_t sh;
    uint32_t k;
    sh = *p;
    if (plist_cur >= PLIST_N)
        plist_cur = (uint8_t)(PLIST_N - 1u);
    for (k = 0; k < 3u; k++)
        sh.id[1 + k] = PLIST[plist_cur].id[k];
    return &sh;
}
static uint8_t mod_ui_slot;      /* the MOD page: the matrix slot (0..3) KNOB 2..4 edit */
static uint8_t ms_ui_step;       /* (Yehnah) the MSEQ page: the step (0..15) KNOB 2 edits */
static const char *const N_RFOLW[] = {"OFF", "T1", "T2", "T3", "T4", "T5", "T6"};
static const param_desc_t RESO_PITCH = PD("PITCH", F_INT, 0, 14, 0);   /* (0.6.5) RESO: the chord's steps up the scale (G_RPRE) */
static const param_desc_t RESO_FOLW = PE("FOLLOW", N_RFOLW, 0);          /* .. the track whose notes it follows (G_RDAMP) */
static const param_desc_t REV_PRE = PD("PRE", F_INT, 0, 100, 0);       /* ROOM / SPRING: the pre-delay, ms (G_RPRE) */
static const param_desc_t REV_AIR = PD("AIR", F_SEMI, -24, 24, 0);    /* (0.6.12) SHIMMER / CLOUD: pitch and tone, down dark,
                                                                       * up airy (G_RPRE; fx.c rev_air) */
static const param_desc_t AP_BEAT = PE("RATE", N_APDIV, 2);          /* (0.6.9) AUTO PAN's RATE on the tempo (SYNC BEAT) */
static uint8_t sl_ui_step;       /* (0.6.3) the SL PITCH page: the step KNOB 2 edits */

/* (2026-11-10, topic C) the rows of GLO's four lists (a page's place among GR_SETS pages = its list); 0xFF ends a list */
#define SETS_MAX 8u
static const uint8_t SETS[4][SETS_MAX] = {
    {G_BPM, G_SWING, G_TRKS, MROW(MI_USB), MROW(MI_CLICK), MROW(MI_CLKLVL), MROW(MI_COUNTIN), 0xFF},
    {MROW(MI_COLOR), MROW(MI_SCROFF), MROW(MI_FILL), MROW(MI_LEDS), MROW(MI_SCLLED), 0xFF, 0xFF, 0xFF},
    {MROW(MI_HOLD), MROW(MI_ACCEL), MROW(MI_LATCH), G_CLOCK, G_ROUTE, MROW(MI_MIDIOUT), MROW(MI_LOWCUT), G_TUNE},
    {G_LOADP, G_INFO, MROW(MI_SERIAL), MROW(MI_RESTORE), MROW(MI_PANEL), MROW(MI_INIT), MROW(MI_ABOUT), 0xFF}};
static uint8_t sets_cur[4];      /* each list's row KNOB 1 is on */
static uint32_t sets_of(const page_t *p)              /* the list a GR_SETS page shows */
{
    uint32_t k, s = 0;
    for (k = 0; &PAGES[k] != p; k++)
        s += PAGES[k].graph == GR_SETS;
    return s;
}
static uint32_t sets_n(uint32_t s)
{
    uint32_t n = 0;
    while (n < SETS_MAX && SETS[s][n] != 0xFFu)
        n++;
    return n;
}
/* the page cur_page() hands out for a GR_SETS page: itself with the row KNOB 1 is on in slot 1 (KNOB 2's) */
static const page_t *sets_page(const page_t *p)
{
    static page_t sh;
    uint32_t s = sets_of(p) & 3u, n = sets_n(s);
    sh = *p;
    if (sets_cur[s] >= n)
        sets_cur[s] = (uint8_t)(n ? n - 1u : 0u);
    sh.id[1] = SETS[s][sets_cur[s]];
    return &sh;
}

static const param_desc_t *page_desc(const page_t *pg, uint32_t slot, int16_t **valp)
{
    uint32_t id = pg->id[slot];
    if (id == 0xFFu || IS_MROW(id)) {                 /* (a MENU row: ui_draw.c, ui_input.c) */
        *valp = 0;
        return 0;
    }
    if (IS_VF(id)) {                                  /* (2026-11-11) FMINI's patch setting: a copy, written back on a turn */
        fmx_ui_k = (uint8_t)(id - 0xA0u);
        fmx_ui = (int16_t)fmx_get(TSEL->p, fmx_ui_k);
        *valp = &fmx_ui;
        return &FMX_DESC[fmx_ui_k];
    }
    if (id == ID_ALLTR) {                             /* (0.7.6) SCL's KNOB 4: the global ALL TR on the track page */
        *valp = &song.g[G_STRN];
        return &GP[G_STRN];
    }
    if (id == G_TRKS) {                               /* (0.6.18) not a song global: the device's setting */
        *valp = &trks_ui;
        return &TRKS_DESC;
    }
    if (id == G_LOADP) {                              /* (0.6.30) the same */
        *valp = &load_ui;
        return &LOAD_DESC;
    }
#if FELUCCA_FXSCENE
    if (id == G_SCENEP || id == G_UFXP) {             /* (chord-harp fork) FX scenes: not song globals */
        *valp = id == G_SCENEP ? &scene_ui : &ufx_ui;
        return id == G_SCENEP ? &SCENE_DESC : &UFX_DESC;
    }
#endif
    if (pg->scope == SC_STEP || pg->scope == SC_TRK) {
        *valp = 0;
        return 0;
    }
    if (pg->scope == SC_GLOBAL) {
        if (pg->graph == GR_SLPIT && id == G_SLP0)    /* the pitch of the step shown */
            id += sl_ui_step & 15u;
        *valp = &song.g[id];
        if (song.g[G_RTYPE] == 3 && (id == G_RPRE || id == G_RDAMP))   /* (0.6.5) RESO: PITCH, FOLW in their slots */
            return id == G_RPRE ? &RESO_PITCH : &RESO_FOLW;
        if (id == G_RPRE)                             /* (0.6.12) SHIMMER / CLOUD: AIR; ROOM / SPRING: PRE */
            return song.g[G_RTYPE] == 2 || song.g[G_RTYPE] == 4 ? &REV_AIR : &REV_PRE;
        return &GP[id];
    }
    if (pg->graph == GR_MOD)                          /* SRC DST AMT of the slot shown */
        id += 3u * mod_ui_slot;
    if (pg->graph == GR_MSEQ && id == P_MS0)          /* the level of the step shown */
        id += ms_ui_step & 15u;
    *valp = &TSEL->p[id];
    if (id == P_APRATE && TSEL->p[P_APSYNC])         /* (0.6.9) AUTO PAN on the tempo: a division */
        return &AP_BEAT;
    if (id == P_CHRD)                                 /* (chord-harp fork) the CHORD tab: not HARP / OMNI (their own tabs) */
        return &CHORD_TAB_DESC;
    if (id == P_PROG) {                               /* (chord-harp fork) SCL > PROG: its number on the card (the name in the
                                                       * breadcrumb, ui_draw.c foot_crumbs) */
        static const param_desc_t PROG_NUM = {"PROG", F_INT, 0, (int16_t)(PROG_N - 1u), 0, 0, 0};
        return &PROG_NUM;
    }
    return track_desc(TSEL, id);
}
