/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) EDIT's lens panels (ui_lens.c), as the approved mock-up's; included by ui_graph.c. The canvas is the
 * lens's panel (240 x 140, x 4..236).
 *   SOUND   (EDIT 1 / EDIT 2 of a synth) the engine's picture (chord-harp fork: pixel art, alive with the sound) on the
 *           whole panel and nothing else (2026-11-10: the page name and the sound's name gone). DRUM's KIT keeps its colony
 *   FILTER  (2026-11-10, Yehnah's look: blocks, no words) the response of TYPE at CUT with RES (LP HP BP COMB), CUT a dotted
 *           column with a ball on it, DRIVE a row of blocks
 *   VOICE   the track's voices one lane each as full as its envelope now, GLIDE's curve under them (blocks, no words)
 * The others (MORPH, RETRIG, ROTOR, the kit's, the operators') keep their panels. */
#define ED_CH 17                                        /* a chip's height (two lines of AF_X) */
