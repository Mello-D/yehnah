/* SPDX-License-Identifier: GPL-3.0-only
 * Yehnah (0.6.26; 0.6.32 darker; 0.6.34 a generator of pieces, not of conditions): HOME + SELECT's DICE ALL. Included
 * by ui_input.c (main loop only). (chord-harp fork) What is left of it: the genres and each role's figure, written by
 * HOME > IDEAS > TRACK (ideas.c), and SAVE > TOOLS' CPU TEST song; the piece's sounds and the whole song dice are gone.
 *
 * Three choices make a piece, then guard rails keep it musical inside the chaos:
 *   GENRE   (dstyle_t) its tempo, scales, progressions, drum family (or none) and kits, bass figure and register, how
 *           its chords move, how its lead and its arp speak, which engines it likes for each role, its space (reverb,
 *           delay, how wet). (0.6.80) The wave's: POST PUNK, SYNTH POP, MINIMAL SYNTH, MAN MACHINE (Joy Division,
 *           New Order, Suicide, Kraftwerk; two throws in five). The night's (DARK MINIMAL, DUB TECHNO, IDM, DARK IDM,
 *           DARK ELECTRO, ACID NIGHT, BROKEN, HYPNOTIC, DEEP BREAKS) and SOUNDTRACK (enveloping pads, no drums or a
 *           heartbeat), HARD TECHNO, NEO CLASSICAL (Reich's pulses and phases, Glass's additive arps, Part's
 *           tintinnabuli).
 *   ENERGY  0 SPACE (two or three things sounding, long tails: minimalism, so the timbres are cared for), 1 MID,
 *           2 FRANTIC (everything, denser, ratchets); inside the genre's range.
 *   ORCHESTRATION  which roles play: the genre's order of importance cut by the energy; the drums may not play at all;
 *           a role left out keeps its sound and an empty sequence (play it, record it).
 * The guard rails: one harmony anchored on the tonic in every register; the chords four notes (0.6.80: or three, a
 * power chord or a triad), voiced in a middle register and led to the nearest voicing; the bass deep (from E1; the
 * wave's hook from E2); the arps in the middle octaves (C3..C5); the lead under G5 and coherent: a rhythmic motif of a
 * few notes, its contour in steps and repeated notes (a leap filled back by a step), its strong beats on chord tones
 * three times in five, repeated over four bars (A, A moved to the next chord, A, A' its last note moved or left out:
 * 0.6.80, it ended on a cadence held on the tonic, a nursery tune's); high notes softer, a part that sits high quieter; space between notes. A throw in five gets a TWIST, one
 * experiment (two loops phasing, a comb, the grain delay an octave down, a polymetric lead, a drone): surprises with
 * refinement. Sounds: a preset whose name fits the role and is not a bright one, its envelope shaped for the role and
 * the energy, filtered by role with an envelope on the filter, a TYPE that changes, an LFO of its own shape; the kit
 * (0.6.35: back to 0.6.33's, it sounded better) the DRUM-X kit diced around the factory one, through a soft low-pass. No automation and no
 * modulation matrix beyond the macros' routes. UNDO (REC + D4) brings back what was there */

enum { RL_DRUM, RL_BASS, RL_PAD, RL_LEAD, RL_ARP, RL_TEX, RL_N };
enum { DS_MOTORIK, DS_ELECTRO, DS_BREAK, DS_BROKEN, DS_MINIMAL, DS_DEEP, DS_HARD, DS_PUNK, DS_INDIE, DS_SPARSE,
       DS_POSTPUNK, DS_RBOX, DS_NEWORD, DS_KLING, DS_EBM, DS_DARK, DS_IDM, DS_NONE };   /* (0.6.80: the wave's four before
                                                         * DS_IDM; 0.6.81: EBM's, darkwave's) */
enum { KT_808, KT_HARD, KT_CLICK, KT_INDUS, KT_SOFT, KT_GLITCH, KT_ELECTRO };
enum { BS_OCT8, BS_PUMP, BS_ACID, BS_DRONE, BS_ROLL, BS_BLIP, BS_SUB, BS_PUNK, BS_INDIE, BS_RUMBLE, BS_IDM,
       BS_HOOK, BS_SUICIDE, BS_SEQ, BS_EBM, BS_DARK };   /* (0.6.80) a bass that sings high, a rockabilly ostinato, a
                                                         * sequencer; (0.6.81) EBM's galloping sixteenths, darkwave's
                                                         * low root hammered in eighths */
enum { PS_HOLD, PS_STAB, PS_PULSE, PS_SPARSE, PS_DUB, PS_SWELL, PS_DRONE, PS_POWER };   /* (0.6.80: an organ's drone, a
                                                         * guitar's power chords) */
enum { VC_7, VC_9, VC_QUART, VC_SUS, VC_ADD9, VC_MIX, VC_POWER, VC_TRIAD };   /* the chords' voicings: four notes; (0.6.80)
                                                         * the root, its fifth and its octave; a plain triad */
enum { LM_MOTIF, LM_OSTINATO, LM_SPARSE, LM_TINTI, LM_MECH };   /* how the lead speaks (0.6.80: a machine's motif) */
enum { AR_UPDOWN, AR_ALBERTI, AR_GLASS, AR_PULSE, AR_WALK, AR_SEQ };   /* how the arp speaks (0.6.80: a sequencer) */
#define EG_A 0u                                         /* ANALOG */
#define EG_PH 2u                                        /* PHASE */
#define EG_LO 3u                                        /* LOFI */
#define EG_VO 5u                                        /* VOICE */
#define EG_NO 11u                                       /* NOISE */
#define EG_FM ENGI_FM6
#define SC_MAJ 1u
#define SC_MIN 2u
#define SC_DOR 3u
#define SC_MPEN 6u
#define SC_HARM 7u
#define SC_PHRY 8u
#define SC_MEL 11u
#define SC_MIX 4u
#define SC_LYD 9u

typedef struct {
    const char *name;
    uint8_t bpm_lo, bpm_hi, swing;
    uint8_t scale[4];
    int8_t prog[3][4];                                  /* scale degrees, a chord each (127: the progression ends) */
    uint8_t drums, drum_pct, kit[2];                    /* the family (DS_NONE: never), how often it plays, two kits */
    uint8_t bass, pads, voicing, lead, arp;
    uint8_t e_lo, e_hi;                                 /* its energies */
    uint8_t prio[5];                                    /* the melodic roles, the most important first */
    uint8_t odd;                                        /* percent: loops of odd lengths against each other */
    uint8_t e_bass[3], e_pad[3], e_lead[3], e_arp[3], e_tex[3];
    uint8_t rtype[2], dtime[2];                         /* reverb types (no SHIMMER), delay times (N_DTIME) */
    uint8_t bars, wet;                                  /* chords every 1, 2 or 4 bars; 0..100: how wet the space */
    uint8_t plain;                                      /* (0.6.81) a band's sound: open filters (air, cymbals), plain
                                                         * chords (rarely the rich ones), hats on the offbeat eighths */
} dstyle_t;

#define X 127
#define P_DANCE {RL_BASS, RL_PAD, RL_LEAD, RL_ARP, RL_TEX}
#define P_ARPS {RL_BASS, RL_ARP, RL_PAD, RL_LEAD, RL_TEX}
#define P_PADS {RL_PAD, RL_TEX, RL_LEAD, RL_BASS, RL_ARP}
#define P_SONG {RL_BASS, RL_LEAD, RL_PAD, RL_ARP, RL_TEX}
#define P_CLASS {RL_ARP, RL_PAD, RL_LEAD, RL_TEX, RL_BASS}
static const dstyle_t DSTYLE[] YEHNAH_RES = {   /* (chord-harp fork: the resource region, res.c) */
    {"IDM", 86, 140, 10, {SC_PHRY, SC_HARM, SC_MIN, SC_DOR}, {{0, 5, 1, X}, {0, 3, 1, 5}, {0, 1, 5, 4}},
     DS_IDM, 100, {KT_GLITCH, KT_CLICK}, BS_IDM, PS_SPARSE, VC_MIX, LM_MOTIF, AR_WALK, 0, 2, P_ARPS, 60,
     {EG_FM, EG_A, EG_PH}, {EG_FM, EG_PH, EG_LO}, {EG_PH, EG_LO, EG_FM}, {EG_FM, EG_LO, EG_PH}, {EG_NO, EG_LO, EG_PH},
     {4, 3}, {4, 5}, 1, 60},
    {"DARK IDM", 78, 112, 14, {SC_PHRY, SC_HARM, SC_PHRY, SC_MIN}, {{0, X, X, X}, {0, 1, X, X}, {0, 5, 1, X}},
     DS_IDM, 90, {KT_GLITCH, KT_INDUS}, BS_DRONE, PS_SPARSE, VC_QUART, LM_SPARSE, AR_WALK, 0, 1, P_PADS, 60,
     {EG_FM, EG_PH, EG_A}, {EG_PH, EG_FM, EG_VO}, {EG_PH, EG_VO, EG_FM}, {EG_FM, EG_PH, EG_LO}, {EG_NO, EG_VO, EG_LO},
     {4, 3}, {5, 4}, 2, 75},
    {"DARK MINIMAL", 122, 128, 4, {SC_MIN, SC_PHRY, SC_DOR, SC_MIN}, {{0, X, X, X}, {0, 5, X, X}, {0, 1, X, X}},
     DS_MINIMAL, 100, {KT_CLICK, KT_808}, BS_ROLL, PS_DUB, VC_9, LM_OSTINATO, AR_PULSE, 0, 1, P_DANCE, 40,
     {EG_A, EG_FM, EG_A}, {EG_PH, EG_A, EG_FM}, {EG_A, EG_PH, EG_FM}, {EG_FM, EG_A, EG_PH}, {EG_NO, EG_VO, EG_PH},
     {0, 4}, {1, 4}, 2, 60},
    {"DUB TECHNO", 116, 124, 6, {SC_MIN, SC_DOR, SC_MIN, SC_MPEN}, {{0, X, X, X}, {0, 3, X, X}, {0, 5, 3, X}},
     DS_DEEP, 100, {KT_SOFT, KT_808}, BS_SUB, PS_DUB, VC_9, LM_SPARSE, AR_PULSE, 0, 1, P_PADS, 0,
     {EG_A, EG_FM, EG_A}, {EG_A, EG_PH, EG_FM}, {EG_PH, EG_A, EG_VO}, {EG_FM, EG_A, EG_PH}, {EG_NO, EG_VO, EG_A},
     {4, 0}, {0, 4}, 2, 85},
    {"DARK ELECTRO", 126, 134, 0, {SC_PHRY, SC_MIN, SC_HARM, SC_PHRY}, {{0, 0, 1, 0}, {0, 5, 1, X}, {0, X, X, X}},
     DS_ELECTRO, 100, {KT_ELECTRO, KT_808}, BS_PUMP, PS_STAB, VC_SUS, LM_OSTINATO, AR_UPDOWN, 1, 2, P_DANCE, 0,
     {EG_A, EG_FM, EG_PH}, {EG_A, EG_PH, EG_FM}, {EG_A, EG_PH, EG_VO}, {EG_A, EG_LO, EG_FM}, {EG_NO, EG_VO, EG_A},
     {1, 0}, {4, 1}, 1, 35},
    {"ACID NIGHT", 124, 132, 4, {SC_PHRY, SC_HARM, SC_MIN, SC_DOR}, {{0, X, X, X}, {0, 1, X, X}, {0, 5, X, X}},
     DS_MINIMAL, 95, {KT_808, KT_HARD}, BS_ACID, PS_SPARSE, VC_QUART, LM_SPARSE, AR_PULSE, 1, 2, P_DANCE, 0,
     {EG_A, EG_A, EG_PH}, {EG_PH, EG_A, EG_FM}, {EG_A, EG_PH, EG_VO}, {EG_FM, EG_A, EG_PH}, {EG_NO, EG_VO, EG_PH},
     {0, 4}, {4, 1}, 2, 45},
    /* (0.6.80) the wave: Joy Division, Suicide, New Order, Kraftwerk. Few chords (one, two), the bass carrying the
     * tune high, machines' drums, power chords and triads instead of lush ones, a lead that repeats; their degrees
     * major or minor in every scale they take (iv, VII: never dorian's diminished sixth) */
    /* (0.6.81, measured on Joy Division and Bauhaus: the kick on every beat, the hat on the offbeat eighths, the bass
     * repeating each note two to four eighths, one chord or two over four bars, major too (Love Will Tear Us Apart:
     * A, D), bright) */
    {"POST PUNK", 118, 150, 0, {SC_MIN, SC_MAJ, SC_PHRY, SC_MIN}, {{0, X, X, X}, {0, 3, X, X}, {0, 5, X, X}},
     DS_POSTPUNK, 100, {KT_SOFT, KT_808}, BS_HOOK, PS_POWER, VC_POWER, LM_SPARSE, AR_UPDOWN, 1, 2,
     {RL_BASS, RL_LEAD, RL_PAD, RL_TEX, RL_ARP}, 0,
     {EG_A, EG_A, EG_PH}, {EG_A, EG_PH, EG_FM}, {EG_A, EG_FM, EG_PH}, {EG_A, EG_PH, EG_FM}, {EG_NO, EG_A, EG_VO},
     {0, 1}, {1, 0}, 4, 55, 1},
    {"BROKEN", 80, 100, 14, {SC_DOR, SC_MIN, SC_MEL, SC_DOR}, {{0, 3, X, X}, {0, X, X, X}, {0, 6, 3, X}},
     DS_BROKEN, 90, {KT_SOFT, KT_GLITCH}, BS_DRONE, PS_SPARSE, VC_QUART, LM_MOTIF, AR_WALK, 0, 1, P_SONG, 60,
     {EG_FM, EG_VO, EG_PH}, {EG_PH, EG_VO, EG_FM}, {EG_VO, EG_FM, EG_PH}, {EG_FM, EG_PH, EG_LO}, {EG_VO, EG_NO, EG_PH},
     {4, 3}, {0, 4}, 2, 65},
    {"HYPNOTIC", 124, 130, 2, {SC_PHRY, SC_MIN, SC_DOR, SC_HARM}, {{0, X, X, X}, {0, X, X, X}, {0, 1, X, X}},
     DS_DEEP, 100, {KT_CLICK, KT_808}, BS_BLIP, PS_DUB, VC_SUS, LM_OSTINATO, AR_PULSE, 1, 2, P_ARPS, 50,
     {EG_A, EG_FM, EG_PH}, {EG_PH, EG_A, EG_FM}, {EG_A, EG_PH, EG_LO}, {EG_A, EG_FM, EG_PH}, {EG_NO, EG_PH, EG_VO},
     {0, 4}, {5, 1}, 2, 55},
    {"DEEP BREAKS", 108, 122, 8, {SC_MIN, SC_HARM, SC_DOR, SC_PHRY}, {{0, 5, X, X}, {0, 6, 5, X}, {0, 3, 1, X}},
     DS_BREAK, 100, {KT_ELECTRO, KT_SOFT}, BS_ACID, PS_STAB, VC_9, LM_MOTIF, AR_UPDOWN, 1, 2, P_DANCE, 0,
     {EG_A, EG_PH, EG_A}, {EG_PH, EG_A, EG_FM}, {EG_A, EG_PH, EG_VO}, {EG_A, EG_FM, EG_PH}, {EG_NO, EG_VO, EG_PH},
     {1, 4}, {4, 1}, 1, 40},
    {"SOUNDTRACK", 64, 92, 0, {SC_MIN, SC_DOR, SC_MEL, SC_MPEN}, {{0, 5, 3, 6}, {0, 3, X, X}, {0, 5, 2, 6}},
     DS_SPARSE, 35, {KT_SOFT, KT_808}, BS_SUB, PS_SWELL, VC_ADD9, LM_SPARSE, AR_ALBERTI, 0, 1, P_PADS, 30,
     {EG_A, EG_FM, EG_PH}, {EG_A, EG_PH, EG_VO}, {EG_FM, EG_PH, EG_VO}, {EG_FM, EG_A, EG_PH}, {EG_NO, EG_VO, EG_PH},
     {4, 0}, {0, 4}, 2, 90},
    {"HARD TECHNO", 138, 150, 0, {SC_PHRY, SC_MIN, SC_HARM, SC_PHRY}, {{0, X, X, X}, {0, 1, X, X}, {0, X, X, X}},
     DS_HARD, 100, {KT_HARD, KT_INDUS}, BS_RUMBLE, PS_STAB, VC_SUS, LM_OSTINATO, AR_PULSE, 1, 2, P_DANCE, 0,
     {EG_A, EG_PH, EG_A}, {EG_PH, EG_A, EG_FM}, {EG_A, EG_PH, EG_LO}, {EG_A, EG_PH, EG_FM}, {EG_NO, EG_NO, EG_PH},
     {1, 0}, {4, 2}, 2, 30},
    {"SYNTH POP", 118, 130, 0, {SC_MIN, SC_DOR, SC_MIN, SC_PHRY}, {{0, 6, X, X}, {0, 3, X, X}, {3, 6, 0, 0}},
     DS_NEWORD, 100, {KT_ELECTRO, KT_808}, BS_HOOK, PS_STAB, VC_TRIAD, LM_MOTIF, AR_SEQ, 1, 2,
     {RL_BASS, RL_ARP, RL_LEAD, RL_PAD, RL_TEX}, 0,
     {EG_A, EG_FM, EG_A}, {EG_A, EG_PH, EG_FM}, {EG_A, EG_FM, EG_PH}, {EG_FM, EG_A, EG_PH}, {EG_NO, EG_VO, EG_A},
     {0, 4}, {1, 2}, 1, 40},
    {"MINIMAL SYNTH", 104, 132, 0, {SC_MIN, SC_DOR, SC_MIN, SC_PHRY}, {{0, X, X, X}, {0, X, X, X}, {0, 3, X, X}},
     DS_RBOX, 100, {KT_808, KT_SOFT}, BS_SUICIDE, PS_DRONE, VC_POWER, LM_SPARSE, AR_PULSE, 0, 1,
     {RL_BASS, RL_PAD, RL_LEAD, RL_TEX, RL_ARP}, 0,
     {EG_A, EG_LO, EG_A}, {EG_A, EG_LO, EG_PH}, {EG_A, EG_VO, EG_A}, {EG_A, EG_LO, EG_PH}, {EG_NO, EG_LO, EG_A},
     {1, 0}, {2, 1}, 2, 50},
    {"NEO CLASSICAL", 70, 112, 0, {SC_MIN, SC_DOR, SC_MAJ, SC_MEL}, {{0, 5, 3, 4}, {0, 3, 4, 0}, {0, 2, 5, 4}},
     DS_NONE, 0, {KT_SOFT, KT_SOFT}, BS_SUB, PS_HOLD, VC_ADD9, LM_TINTI, AR_GLASS, 0, 1, P_CLASS, 50,
     {EG_FM, EG_PH, EG_A}, {EG_PH, EG_A, EG_VO}, {EG_FM, EG_PH, EG_VO}, {EG_FM, EG_PH, EG_A}, {EG_VO, EG_PH, EG_NO},
     {0, 4}, {0, 1}, 2, 70},
    {"MAN MACHINE", 100, 124, 0, {SC_MIN, SC_MAJ, SC_DOR, SC_MIN}, {{0, X, X, X}, {0, 3, X, X}, {0, 3, 0, 4}},
     DS_KLING, 100, {KT_ELECTRO, KT_CLICK}, BS_SEQ, PS_HOLD, VC_TRIAD, LM_MECH, AR_SEQ, 1, 2,
     {RL_BASS, RL_LEAD, RL_ARP, RL_PAD, RL_TEX}, 0,
     {EG_A, EG_PH, EG_FM}, {EG_A, EG_PH, EG_FM}, {EG_A, EG_PH, EG_FM}, {EG_PH, EG_A, EG_FM}, {EG_VO, EG_NO, EG_PH},
     {0, 0}, {1, 0}, 2, 15},
    /* (0.6.81) EBM (DAF, Nitzer Ebb, Front 242: a galloping sequenced bass, a hard machine, a shout), GOTHIC (Bauhaus,
     * Sisters of Mercy, the Cure: the bass singing, chorused chords, a big room), BREAKBEAT (the break and an acid
     * bass), IMPRESSION (Debussy, Ravel: lydian, ninths moving in parallel, arps like water, no beat or a breath) */
    {"EBM", 112, 126, 0, {SC_PHRY, SC_MIN, SC_PHRY, SC_HARM}, {{0, X, X, X}, {0, X, X, X}, {0, 3, X, X}},
     DS_EBM, 100, {KT_HARD, KT_ELECTRO}, BS_EBM, PS_STAB, VC_POWER, LM_SPARSE, AR_SEQ, 1, 2,
     {RL_BASS, RL_LEAD, RL_ARP, RL_PAD, RL_TEX}, 0,
     {EG_A, EG_PH, EG_FM}, {EG_A, EG_PH, EG_FM}, {EG_VO, EG_A, EG_PH}, {EG_A, EG_PH, EG_LO}, {EG_NO, EG_VO, EG_LO},
     {0, 1}, {1, 2}, 2, 25},
    {"GOTHIC", 98, 140, 0, {SC_MIN, SC_HARM, SC_PHRY, SC_MIN}, {{0, X, X, X}, {0, X, X, X}, {0, 5, X, X}},
     DS_POSTPUNK, 100, {KT_SOFT, KT_808}, BS_HOOK, PS_SWELL, VC_TRIAD, LM_SPARSE, AR_UPDOWN, 1, 2,
     {RL_BASS, RL_PAD, RL_LEAD, RL_ARP, RL_TEX}, 0,
     {EG_A, EG_PH, EG_A}, {EG_A, EG_FM, EG_PH}, {EG_A, EG_FM, EG_PH}, {EG_PH, EG_A, EG_FM}, {EG_NO, EG_VO, EG_A},
     {1, 0}, {1, 4}, 4, 75, 1},
    {"BREAKBEAT", 126, 140, 6, {SC_MIN, SC_DOR, SC_PHRY, SC_MIN}, {{0, X, X, X}, {0, 3, X, X}, {0, 6, X, X}},
     DS_BREAK, 100, {KT_ELECTRO, KT_HARD}, BS_ACID, PS_STAB, VC_9, LM_OSTINATO, AR_UPDOWN, 1, 2, P_DANCE, 0,
     {EG_A, EG_PH, EG_A}, {EG_PH, EG_A, EG_FM}, {EG_A, EG_PH, EG_VO}, {EG_A, EG_FM, EG_PH}, {EG_NO, EG_VO, EG_PH},
     {1, 0}, {2, 4}, 1, 40},
    {"IMPRESSION", 60, 92, 0, {SC_LYD, SC_MAJ, SC_LYD, SC_MPEN}, {{0, 1, X, X}, {0, 3, X, X}, {0, 5, 1, X}},
     DS_SPARSE, 30, {KT_SOFT, KT_SOFT}, BS_SUB, PS_SWELL, VC_9, LM_TINTI, AR_UPDOWN, 0, 1,
     {RL_PAD, RL_ARP, RL_LEAD, RL_TEX, RL_BASS}, 50,
     {EG_FM, EG_PH, EG_A}, {EG_FM, EG_PH, EG_A}, {EG_FM, EG_PH, EG_VO}, {EG_FM, EG_PH, EG_A}, {EG_VO, EG_PH, EG_NO},
     {4, 0}, {0, 4}, 2, 90},
    /* (0.6.81) DARKWAVE (Boy Harsher, measured: four on the floor, the snare on 2 and 4, the hat on the offbeat, a low
     * synth bass hammering its root in eighths, minor, one chord or two, a lot of room) */
    {"DARKWAVE", 124, 136, 0, {SC_MIN, SC_PHRY, SC_DOR, SC_MIN}, {{0, X, X, X}, {0, 3, X, X}, {0, 6, X, X}},
     DS_DARK, 100, {KT_808, KT_ELECTRO}, BS_DARK, PS_HOLD, VC_TRIAD, LM_SPARSE, AR_PULSE, 1, 2,
     {RL_BASS, RL_PAD, RL_LEAD, RL_TEX, RL_ARP}, 0,
     {EG_A, EG_A, EG_PH}, {EG_A, EG_PH, EG_A}, {EG_A, EG_PH, EG_FM}, {EG_A, EG_PH, EG_FM}, {EG_NO, EG_VO, EG_A},
     {0, 1}, {1, 2}, 4, 50, 1},
};
#undef X
#define NDSTYLE (sizeof DSTYLE / sizeof DSTYLE[0])
#define DSTYLE_IDM 2u
#define DSTYLE_DARK 10u                                 /* (the first ten: the night's) */
/* (0.6.80) the wave's four (POST PUNK, SYNTH POP, MINIMAL SYNTH, MAN MACHINE; 0.6.81: and EBM, GOTHIC, BREAKBEAT,
 * IMPRESSION): nearly one throw in two */
static const uint8_t DSTYLE_WAVE[9] = {6, 12, 13, 15, 16, 17, 18, 19, 20};   /* (0.6.81: EBM, GOTHIC, BREAKBEAT,
                                                         * IMPRESSION, DARKWAVE) */

static int32_t rr(int32_t lo, int32_t hi) { return lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }
static int chance(uint32_t pct) { return rng() % 100u < pct; }
static uint32_t dstyle_pick(void)
{
    return chance(45) ? DSTYLE_WAVE[rng() % 9u] : chance(25) ? rng() % DSTYLE_IDM : chance(80) ? rng() % DSTYLE_DARK :
           DSTYLE_DARK + rng() % (NDSTYLE - DSTYLE_DARK);
}

/* the piece: the harmony (the progression, how long each chord lasts, the voicing, the last chord voiced: the next is
 * led to it), the energy, the twist */
enum { TW_NONE, TW_PHASE, TW_COMB, TW_GRAIN, TW_POLY, TW_DRONE };
static struct {
    const int8_t *prog;
    uint32_t n, steps, voicing, nlast, energy, twist, plain;
    uint32_t arp;                                       /* (0.6.81) how the arp speaks this throw (the genre's, or
                                                         * ambient: up and down in eighths over a rich chord) */
    int32_t last[4];
} dh;
static int32_t chord_at(uint32_t step) { return dh.prog[(step / dh.steps) % dh.n]; }
/* the tonic at or above lo: every part's degree 0 (0.6.32: the chords and the lead had the fifth and the third) */
static int32_t dtonic(const track_t *t, int32_t lo) { return lo + ((t->p[P_ROOT] - lo) % 12 + 12) % 12; }

static void dstep_note(step_t *s, const int32_t *n, uint32_t cnt, uint32_t vel, uint32_t flags)
{
    uint32_t j;
    int32_t top = 0;
    s->n = (uint8_t)cnt;
    for (j = 0; j < 4u; j++) {
        s->note[j] = (uint8_t)(j < cnt ? clamp(n[j], 0, 127) : 0);
        if (j < cnt && n[j] > top) top = n[j];
    }
    s->time = ST_NOTE;
    s->flags = (uint8_t)(flags | (s->flags & SF_MFINE));   /* (0.9.1: MICRO's 64ths stay with its sixteenth) */
    if (top > 74) vel -= (uint32_t)((top - 74) * 2 < 30 ? (top - 74) * 2 : 30);   /* (high notes measured: softer) */
    s->vel = (uint8_t)clamp((int32_t)vel + rr(-6, 6), 40, 127);   /* (a human's hand: never twice the same) */
}
static void dstep_tie(step_t *s) { s->n = 0; s->time = ST_TIE; s->flags &= SF_MFINE; s->vel = 0; }
/* holds the note of step i over the next k steps, up to the next note; round past the loop's end (the sequencer plays
 * TIEs over the wrap: the last note holds over the loop's top) */
static void dhold(track_t *t, uint32_t i, uint32_t k, uint32_t len)
{
    uint32_t j;
    for (j = 1; j <= k && j < len; j++) {
        if (t->step[(i + j) % len].time == ST_NOTE)
            break;
        dstep_tie(&t->step[(i + j) % len]);
    }
}
/* an euclidean rhythm: k hits spread over n steps, turned by rot */
static int euclid(uint32_t i, uint32_t k, uint32_t n, uint32_t rot)
{
    if (!n || !k) return 0;
    i = (i + rot) % n;
    return (i * k) % n < k;
}

/* ---- DRUMS (lanes: 0 BD, 1 SD, 2 CP, 3 CH, 4 OH, 5 TM, 6 RS, 7 CB): a family's bar as strings ('x' a hit, 'a'
 * accented, 'g' a ghost: three times in four), a fill over the loop's last beat (0.6.35: as 0.6.33 wrote them, the
 * energy leaves the drums alone: they sounded better) */
/* (0.9.7, flash) the text tools/gen_drum_maps.py reads (not compiled): build/gen/dice_bars.h holds each lane once, 2 bits
 * a step (DB_LANE), the table as lane numbers (DRUM_BAR_I: db_char) */
#ifdef GEN_DRUM_MAPS_TEXT
static const char *const DRUM_BAR[][8] = {
    [DS_MOTORIK] = {"x...x...x...x...", "....x.......x...", "", "x.x.x.x.x.x.x.x.", "", "", "..x.....x.x.....", ""},
    [DS_ELECTRO] = {"a......x..x.....", "....a.......a...", "....x.......x..x", "xgxgxgxgxgxgxgxg", "", "..............x.", "", "......x........."},
    [DS_BREAK]   = {"a.........x.....", "....a..g.g..a..g", "", "x.x.x.x.x.x.x.x.", "......x.......x.", "", "", ""},
    [DS_BROKEN]  = {"x..x....x.x.....", "....a..g.g..a...", "", "x.xgx.x.x.xgx.x.", ".......x........", "", "", ""},
    [DS_MINIMAL] = {"a...x...a...x...", "", "....g.......g...", "g.x...xgg.x...x.", "..x...x...x...x.", "", "...x..g...x..x..", "......g........."},
    [DS_DEEP]    = {"a...x...a...x...", "", "............g...", "..x...x...x...x.", "..g...x...g...x.", "", ".g.....x.g....x.", ""},
    [DS_HARD]    = {"a...a...a...a...", "", "....x.......x...", "xgxgxgxgxgxgxgxg", "..x...x...x...x.", "", "..g..g....g..g..", "......g.......g."},
    [DS_PUNK]    = {"a.....x.a.x.....", "....a.......a...", "", "x.x.x.x.x.x.x.x.", "", "", "", ""},
    [DS_INDIE]   = {"a......xa.x.....", "....a.......a..g", "", "x.xgx.xgx.xgx.xg", "..............x.", "", "", ""},
    [DS_SPARSE]  = {"x...............", "", "", "", "", "", "......g.....x...", "..........g....."},
    /* (0.6.80) the wave's: Stephen Morris's tight kit (sixteenths on the hat, the floor tom answering), a cheap rhythm
     * box (Suicide), Blue Monday's four on the floor and its handclap, Kraftwerk's dry machine and its metal */
    [DS_POSTPUNK] = {"a...x...a...x...", "....a.......a...", "", "x.a.x.a.x.a.x.a.", "", "", "", ""},   /* (0.6.81:
                                                         * measured, the kick on every beat, the hat's offbeats) */
    [DS_RBOX]     = {"x.......x.x.....", "....x.......x...", "", "xgxgxgxgxgxgxgxg", "", "", "", ""},
    [DS_NEWORD]   = {"a...a...a...a...", "", "....a.......a...", "xgxgxgxgxgxgxgxg", "..x.......x.....", "", "", ""},
    [DS_KLING]    = {"x.....x.x.......", "....x.......x...", "", "x.x.x.x.x.x.x.x.", "", "", "..........x...x.", "x..x..x...x.x..."},
    [DS_EBM]      = {"a...a...a...a...", "....a.......a...", "....x.......x...", "..x...x...x...x.", "", "", "", ""},   /* (0.6.81) */
    [DS_DARK]     = {"a...a...a...a...", "....a.......a...", "....x.......x...", "..a...a...a...a.", "", "", "", ""},
};
static const char *const DRUM_BAR_KICK[1] = {"a.......a.x....."};   /* (0.6.81: the post punk kick's other bar) */
#endif
#include "dice_bars.h"
static char db_char(uint32_t k, uint32_t p)          /* lane k's step p: '.' 'x' 'a' 'g' */
{
    return ".xag"[(DB_LANE[k][(p >> 2) & 3u] >> (6u - 2u * (p & 3u))) & 3u];
}
static void dice_drums(track_t *t, const dstyle_t *st, uint32_t len)
{
    uint32_t i, l;
    if (st->drums == DS_IDM) {                          /* euclids against each other, ratchets, chance (0.6.33's) */
        uint32_t kb = (uint32_t)rr(3, 6), ks = (uint32_t)rr(2, 4), kh = (uint32_t)rr(7, 13), rot = (uint32_t)rr(0, 7);
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            uint32_t h = 0, b = i / 16u;                /* (each bar its own: the hats denser or sparser, the kick turned) */
            if (euclid(i, kb, 16, rot + (b == 3u ? 2u : 0u))) h |= 1u << 0;
            if (euclid(i, ks, 16, rot + 4u) && !(h & 1u)) h |= 1u << (chance(50) ? 1 : 6);
            if (euclid(i, kh + (b & 1u) * 2u - (b == 2u ? 3u : 0u), 16, b)) h |= 1u << (chance(20) ? 4 : 3);
            if (chance(8)) h |= 1u << (5u + rng() % 3u);
            if (!h) continue;
            s->time = ST_NOTE; s->hit = (uint8_t)h; s->acc = (uint8_t)(i % 4u == 0u ? h & 1u : 0u);
            s->vel = (uint8_t)(h & 1u ? 104 : rr(70, 100));
            if ((h & (1u << 3)) && chance(25)) step_set_ratchet(s, (uint32_t)rr(2, 4));
            if (!(h & 1u) && chance(30)) step_set_chance(s, (uint32_t)rr(45, 85));
        }
        return;
    }
    /* (0.6.81) the post punk kick now and then on 1 and 3 and an eighth before 3 (Boy Harsher's Pain); darkwave's
     * snare now and then a sixteenth before the bar (step 15); the hat's open one on the last offbeat */
    uint32_t kick = st->drums == DS_POSTPUNK && chance(30) ? DRUM_BAR_KICK_I : 0u;   /* (dice_bars.h) */
    uint32_t snr15 = st->drums == DS_DARK && chance(50), oh = (st->drums == DS_POSTPUNK || st->drums == DS_DARK) && chance(40);
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        uint32_t h = 0, a = 0, ghost = 0, p = i % 16u;
        uint32_t fill = len > 16u && i >= len - 4u && st->drums != DS_DEEP && st->drums != DS_SPARSE &&
                        st->drums != DS_KLING && st->drums != DS_RBOX && st->drums != DS_DARK;   /* (machines: no fills) */
        for (l = 0; l < NLANE; l++) {
            uint32_t b = l == 0u && kick ? kick : DRUM_BAR_I[st->drums][l];
            char c = b ? db_char(b, p) : '.';
            if (c == 'x' || c == 'a' || (c == 'g' && rng() % 4u)) h |= 1u << l;
            if (c == 'a') a |= 1u << l;
            if (c == 'g') ghost = 1;
        }
        if (snr15 && p == 15u && (i / 16u) % 2u == 1u) h |= 1u << 1;
        if (oh && p == 14u && (i / 16u) % 2u == 1u) h = (h & ~(1u << 3)) | (1u << 4);
        if (fill) {
            if (st->drums == DS_NEWORD) h |= 1u;        /* (Blue Monday's sixteenth kicks) */
            else if (chance(50)) h |= 1u << (chance(50) ? 1 : 5);
            h &= ~(1u << 4);
        }
        if (i >= 16u && !fill && chance(8)) h ^= 1u << (chance(50) ? 6 : 7);   /* bar 2: a percussion moved */
        if (!h) continue;
        s->time = ST_NOTE; s->hit = (uint8_t)h; s->acc = (uint8_t)(a & h);   /* (0.6.90: an accent only on a hit: one
                                                         * moved away or swapped left its accent, which a save dropped) */
        s->vel = (uint8_t)(st->drums == DS_KLING ? (a ? 112 : 100) : ghost && !a ? rr(62, 82) : rr(96, 110));   /* (a
                                                         * machine: the same every time) */
        if (fill && chance(40)) step_set_ratchet(s, 2);
        if ((st->drums == DS_ELECTRO || st->drums == DS_MINIMAL || st->drums == DS_HARD) && (h & (1u << 3)) && chance(10))
            step_set_ratchet(s, 2);
        if (!(h & 1u) && (st->drums == DS_MINIMAL || st->drums == DS_DEEP || st->drums == DS_SPARSE) && chance(15))
            step_set_chance(s, (uint32_t)rr(50, 85));
    }
}
/* ---- the chords: degree d's four notes from the tonic t0 in the voicing, then led (each note an octave up or down)
 * to the nearest of the last chord inside [t0 - 2, t0 + 17] (C3..F4 or so): no leaps, a middle register */
static uint32_t dchord(const track_t *t, int32_t t0, int32_t d, uint32_t vc, int32_t *n)
{
    static const int8_t V[6][4] = {{0, 2, 4, 6}, {0, 2, 6, 8}, {0, 3, 6, 9}, {0, 3, 4, 8}, {0, 2, 4, 8}, {0, 2, 4, 0}};   /*
                                     * 7th, 9th (no 5th), fourths, sus4 add9, add9; (0.6.80) a triad */
    uint32_t j, i, cnt = 4;
    if (vc == VC_MIX) vc = rng() % 5u;
    if (vc == VC_POWER) {                               /* (0.6.80) the root, its fifth, its octave: parallel, a
                                                         * guitar's (the fifth always perfect, the scale or not) */
        n[0] = scale_steps(t, t0, d);
        n[1] = n[0] + 7;
        n[2] = n[0] + 12;
        cnt = 3;
    } else {
        if (vc == VC_TRIAD) { vc = 5; cnt = 3; }
        for (j = 0; j < cnt; j++) n[j] = scale_steps(t, t0, d + V[vc][j]);
    }
    for (j = 0; j < cnt; j++) {
        while (n[j] < t0 - 2) n[j] += 12;
        while (n[j] > t0 + 15) n[j] -= 12;
    }
    if (dh.nlast) {                                     /* led: each note the octave nearest the last chord's middle */
        int32_t mid = 0;
        for (i = 0; i < dh.nlast; i++) mid += dh.last[i];
        mid /= (int32_t)dh.nlast;
        for (j = 0; j < cnt; j++) {
            if (n[j] - mid > 7 && n[j] - 12 >= t0 - 5) n[j] -= 12;
            else if (mid - n[j] > 7 && n[j] + 12 <= t0 + 17) n[j] += 12;
        }
    }
    for (i = 1; i < cnt; i++)                           /* (low to high) */
        for (j = i; j > 0 && n[j] < n[j - 1]; j--) { int32_t x = n[j]; n[j] = n[j - 1]; n[j - 1] = x; }
    for (j = 1; j < cnt; j++)                           /* (no unisons: a clash an octave up) */
        if (n[j] == n[j - 1]) n[j] += 12;
    for (j = 0; j < cnt; j++) dh.last[j] = n[j];
    dh.nlast = cnt;
    return cnt;
}

/* ---- the bass: deep (from E1: its register starts low), the figure the genre's, denser with the energy */
static void dice_bass(track_t *t, const dstyle_t *st, uint32_t len)
{
    int32_t base = dtonic(t, 28), n[1];
    uint32_t i, e = dh.energy;
    static const uint8_t PUMP[16] = {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0};
    uint8_t acid[16], roll[16], ind[16];
    int8_t hook[8];
    {   /* (0.6.80) Peter Hook's line: eight eighths a bar, high, a tune of its own around the chord's root, the bar's
         * last two moved on the fourth bar. (0.6.81, measured: a real one repeats the note before it in half to three
         * quarters of its eighths: each note now hammered two to four eighths (one now and then), then a step, a
         * third now and then; it started on the root) */
        int32_t d = 0, j, mv, run = 0;
        for (j = 0; j < 8; j++) {
            if (!run) {
                if (j) {
                    mv = chance(70) ? (chance(50) ? 1 : -1) : (chance(50) ? 2 : -2);
                    if (d + mv > 5 || d + mv < -2) mv = -mv;
                    d += mv;
                }
                run = chance(15) ? 1 : rr(2, 4);
            }
            hook[j] = (int8_t)d;
            run--;
        }
    }
    for (i = 0; i < 16u; i++) {                         /* (one bar's figure, kept: a riff, not noise) */
        acid[i] = (uint8_t)(i % 4u == 0u ? 1u : chance(30 + e * 10u) ? 1u + rng() % 4u : 0u);
        roll[i] = (uint8_t)(i % 4u == 2u ? 1u : i % 4u == 3u ? chance(20 + e * 15u) : i % 4u == 1u ? chance(e * 8u) : 0u);
        ind[i] = (uint8_t)(i == 0u || i == 10u || (i == 6u && chance(50)) || (i == 14u && chance(40)));
    }
    if (st->bass != BS_DRONE && st->bass != BS_SUB && base < 33) base += 12;   /* (a figure: from A1; a drone: E1) */
    if (st->bass == BS_HOOK) base = dtonic(t, 40);     /* (0.6.80: the hook an octave up, from E2: it sings) */
    if (st->bass == BS_DARK) base = dtonic(t, 31);     /* (0.6.81: darkwave's, low: from G1) */
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        int32_t d = chord_at(i), on = 0, oct = 0, step = 0, vel = 96;
        uint32_t fl = 0, p = i % 16u, hold = 0;
        switch (st->bass) {
        case BS_OCT8: on = i % 2u == 0u && (e || i % 4u == 0u); oct = (i / 2u) & 1u; break;
        case BS_PUMP: on = PUMP[p]; oct = p == 11u; fl = p == 0u ? SF_ACCENT : 0; break;
        case BS_ACID: on = acid[p] != 0u; step = acid[p] == 2u ? 1 : acid[p] == 3u ? 4 : acid[p] == 4u ? -1 : 0;
                      oct = acid[p] == 3u && chance(50); fl = (p % 4u == 0u ? SF_ACCENT : 0) | (chance(25) ? SF_SLIDE : 0); break;
        case BS_DRONE: on = i % (dh.steps < 16u ? dh.steps : 16u) == 0u; hold = 15; break;
        case BS_ROLL: on = roll[p]; vel = p % 4u == 2u ? 100 : 80; break;
        case BS_SUB:
            on = p == 0u || (e && p == 10u) || (e == 2u && p == 6u);
            hold = p == 0u ? (e ? 5 : 13) : 2;
            if (p == 10u && chance(40)) step = 4;
            break;
        case BS_BLIP: on = (p % 4u == 2u && chance(70)) || p == 0u; vel = 90; break;
        case BS_PUNK: on = i % 2u == 0u; step = p >= 12u && chance(40) ? 4 : 0; fl = p % 8u == 0u ? SF_ACCENT : 0; break;
        case BS_INDIE: on = ind[p]; step = p == 14u ? (chord_at(i + 2u) - d) - 1 : p == 10u && chance(40) ? 4 : 0;
                       hold = p == 0u ? 3 : 1; break;
        case BS_RUMBLE: on = p % 4u != 0u && (p % 4u == 2u || chance(30 + e * 20u)); vel = p % 4u == 2u ? 96 : 76; break;
        case BS_HOOK:                                   /* (0.6.80) the tune on the eighths, legato */
            on = p % 2u == 0u;
            step = hook[p / 2u] + ((i / 16u) % 4u == 3u && p >= 12u ? (p == 12u ? 1 : -1) : 0);
            vel = p % 8u == 0u ? 100 : 88;
            hold = 1;
            break;
        case BS_SUICIDE: {                              /* (0.6.80) Martin Rev's rockabilly ostinato: root, root,
                                                         * fifth, root .. the seventh, the fifth; an octave every
                                                         * other bar's end */
            static const int8_t SU[8] = {0, 0, 4, 0, 0, 0, 6, 4};
            on = p % 2u == 0u;
            step = SU[p / 2u];
            oct = (i / 16u) % 2u == 1u && p == 14u;
            fl = p % 8u == 0u ? SF_ACCENT : 0;
            vel = 92;
            break;
        }
        case BS_EBM: {                                  /* (0.6.81) EBM's gallop: sixteenths, root, root, octave, root;
                                                         * the off-beat ones softer */
            static const int8_t EB[4] = {0, 0, 12, 0};
            n[0] = scale_steps(t, base, d) + EB[p % 4u];
            dstep_note(s, n, 1, p % 2u ? 82u : 96u, 0);
            continue;
        }
        case BS_DARK:                                   /* (0.6.81) the root hammered in eighths, the downbeats harder;
                                                         * the octave at the end of every second bar, the fifth before
                                                         * it now and then */
            on = p % 2u == 0u;
            oct = (i / 16u) % 2u == 1u && p == 14u;
            step = (i / 16u) % 4u == 3u && p == 12u && chance(50) ? 4 : 0;
            vel = p % 4u == 0u ? 100 : 86;
            break;
        case BS_SEQ: {                                  /* (0.6.80) Kraftwerk's sequencer: sixteenths, root, octave,
                                                         * fifth, octave; the same strength every step */
            static const int8_t SQ[4] = {0, 12, 7, 12};   /* (semitones: the fifth always perfect) */
            on = 1;
            n[0] = scale_steps(t, base, d) + SQ[p % 4u];
            dstep_note(s, n, 1, 90, 0);
            continue;
        }
        default: on = chance(25 + e * 10u); step = (int32_t)(rng() % 3u) * 2 - 2; oct = chance(10); break;   /* BS_IDM */
        }
        if (!on) continue;
        n[0] = scale_steps(t, base, d + step) + (oct ? 12 : 0);
        dstep_note(s, n, 1, (uint32_t)vel + (fl & SF_ACCENT ? 16u : 0u), fl);
        if (hold) dhold(t, i, hold, len);
        if (st->bass == BS_IDM && chance(12)) step_set_ratchet(s, 2);
    }
}

/* ---- the chords: always four notes; held, swelling, stabbed, pulsed, dubbed or sparse */
static void dice_pads(track_t *t, const dstyle_t *st, uint32_t len)
{
    int32_t t0 = dtonic(t, 50), n[4];
    uint32_t i, c, last_d = 0xFFFFu, e = dh.energy;
    uint8_t dub[16];
    {   /* (PS_DUB: one bar's stab figure, kept: the first offbeat and one to three of the others) */
        static const uint8_t AT[6] = {6, 10, 14, 7, 11, 13};
        uint32_t m = (uint32_t)rr(1, 2) + (e == 2u), j;
        for (i = 0; i < 16u; i++) dub[i] = (uint8_t)(i == 2u);
        for (j = 0; j < m; j++) dub[AT[rng() % 6u]] = 1;
    }
    dh.nlast = 0;
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        uint32_t pp = i % dh.steps, p = i % 16u;
        int on = 0;
        switch (st->pads) {
        case PS_HOLD: case PS_SWELL: on = pp == 0u; break;
        case PS_STAB: on = (p % 4u == 2u && chance(35 + e * 15u)) || (pp == 0u && chance(40)); break;
        case PS_PULSE: on = i % (e == 2u ? 2u : 4u) == 0u && !(p == 12u && chance(50)); break;
        case PS_DUB: on = dub[p]; break;
        case PS_DRONE: on = i == 0u; break;             /* (0.6.80) one chord held under everything: an organ */
        case PS_POWER: on = pp == 0u || (p == 8u && (i / 16u) % 2u == 1u && chance(60)); break;   /* (0.6.80) a guitar's
                                                         * power chords: the chord's first beat, now and then a push */
        default: on = pp == 0u && (i == 0u || chance(65)); break;   /* PS_SPARSE: some chords (the first), the rest silence */
        }
        if (!on) continue;
        if ((uint32_t)chord_at(i) != last_d || st->pads == PS_HOLD || st->pads == PS_SWELL || st->pads == PS_SPARSE ||
            st->pads == PS_DRONE) {
            c = dchord(t, t0, chord_at(i), dh.voicing, n);
            last_d = (uint32_t)chord_at(i);
        } else {                                        /* (the same chord: the same voicing, stab after stab) */
            for (c = 0; c < dh.nlast; c++) n[c] = dh.last[c];
        }
        dstep_note(s, n, c, st->pads == PS_DUB ? (p == 2u ? 90u : 78u) : st->pads == PS_SWELL || st->pads == PS_DRONE ? 74u : 80u, 0);
        if (st->pads == PS_HOLD || st->pads == PS_SWELL || st->pads == PS_SPARSE) dhold(t, i, dh.steps - 1u, len);
        if (st->pads == PS_DRONE) dhold(t, i, len - 1u, len);
        if (st->pads == PS_POWER) dhold(t, i, p == 8u ? 5u : 7u, len);
        if (st->pads == PS_PULSE && chance(40)) dhold(t, i, 1, len);
    }
}

/* (0.6.37) a lead note: the scale's degree d from base, folded down under G5 whatever the path (a cell, a line, a
 * cadence: the motif's fold alone let a FRANTIC cell reach C#6) */
static int32_t dlead_note(const track_t *t, int32_t base, int32_t d)
{
    int32_t n = scale_steps(t, base, d);
    while (n > 79) n -= 12;
    return n;
}
/* ---- the lead (around the tonic at G3..: never past G5). LM_MOTIF: a rhythmic motif of 2..5 notes (more with the
 * energy), its contour in steps (a leap filled back by a step the other way), its strong steps on chord tones;
 * the phrase A, A moved to the next chord (a sequence), A (its end varied), a cadence (its first half, then the tonic
 * held). LM_OSTINATO: a cell of 2..4 notes on an euclidean rhythm, turning. LM_SPARSE: a few long chord tones
 * stepping. LM_TINTI: Part's M voice, a scale line in long notes, rising or falling around the tonic */
static int32_t dnear_ct(int32_t deg, int32_t chord)    /* the chord tone (chord + 0, 2, 4) nearest a degree */
{
    int32_t best = chord, k;
    for (k = -7; k <= 11; k += 2) {
        int32_t c = chord + k;
        if (((c - chord) % 7 + 7) % 7 > 4) continue;    /* (0, 2, 4 of the chord: its triad) */
        if ((deg - c < 0 ? c - deg : deg - c) < (deg - best < 0 ? best - deg : deg - best)) best = c;
    }
    return best;
}
static void dice_lead(track_t *t, const dstyle_t *st, uint32_t len)
{
    int32_t base = dtonic(t, 55), n[1];
    uint32_t i, e = dh.energy;
    if (st->lead == LM_OSTINATO) {
        static const int8_t CELL[6][4] = {{0, 0, 1, 0}, {0, 2, 1, 0}, {4, 2, 0, 1}, {0, -1, 0, 2}, {0, 4, 3, 1}, {2, 1, 0, -2}};
        const int8_t *cl = CELL[rng() % 6u];
        uint32_t k = (uint32_t)rr(3, 5) + e, rot = (uint32_t)rr(0, 3), nc = (uint32_t)rr(2, 4), j = 0;
        for (i = 0; i < len; i++) {
            uint32_t b = i / 16u;
            if (!euclid(i % 16u, k, 16, rot)) continue;
            if (b % 4u == 3u && chance(50)) continue;  /* (the fourth bar: a breath) */
            n[0] = dlead_note(t, base, cl[j % nc] + (b % 4u == 2u && j % nc == nc - 1u ? 1 : 0));
            dstep_note(&t->step[i], n, 1, j % nc == 0u ? 98u : 82u, chance(15) ? SF_SLIDE : 0);
            j++;
        }
        return;
    }
    if (st->lead == LM_MECH) {                          /* (0.6.80) a machine's tune (Kraftwerk): a figure of chord
                                                         * tones and their octave on an euclidean rhythm, the same in
                                                         * every bar (moved with the chord), the same strength, short */
        static const int8_t MT[4][8] = {{0, 2, 4, 2, 0, 4, 7, 4}, {4, 4, 2, 0, 2, 2, 0, -1}, {0, 0, 4, 0, 5, 4, 2, 0},
                                        {7, 4, 2, 4, 0, 2, 4, 0}};
        const int8_t *mt = MT[rng() % 4u];
        uint32_t k = (uint32_t)rr(4, 6) + e, rot = (uint32_t)rr(0, 3), idx, q;
        for (i = 0; i < len; i++) {
            if (!euclid(i % 16u, k, 16, rot)) continue;
            for (q = 0, idx = 0; q < i % 16u; q++) idx += euclid(q, k, 16, rot);   /* (its place in the bar's figure) */
            n[0] = dlead_note(t, base, mt[idx % 8u] + chord_at(i - i % 16u));
            dstep_note(&t->step[i], n, 1, 92, 0);
        }
        return;
    }
    if (st->lead == LM_SPARSE || st->lead == LM_TINTI) {   /* long notes: 1 or 2 a bar (TINTI: a scale line) */
        int32_t d = st->lead == LM_TINTI ? rr(2, 5) : 2, dir = chance(50) ? -1 : 1;
        for (i = 0; i < len; i += 8u) {
            uint32_t b = i / 16u;
            if (st->lead == LM_SPARSE && ((i % 16u) || (b % 2u == 1u && chance(40)))) continue;
            if (st->lead == LM_SPARSE) d = dnear_ct(d + rr(-1, 1), chord_at(i));
            else { d += dir; if (d < -2 || d > 7) { dir = -dir; d += 2 * dir; } }
            n[0] = dlead_note(t, base, d);
            dstep_note(&t->step[i], n, 1, 84, 0);
            dhold(t, i, st->lead == LM_TINTI ? 6 : 12, len);
        }
        return;
    }
    {   /* LM_MOTIF (0.6.80: repeated, not a cadence: A A A A' over and over, as the wave's tunes go round; its notes
         * on the strong steps a chord tone three times in five (the others a tension: a ninth, an eleventh, kept so in
         * every repeat), repeated notes allowed in its contour; it never lands on the tonic to end) */
        uint8_t at[6], snap[6];
        int8_t deg[6];
        uint32_t k = (uint32_t)rr(2, 3) + e, j, rot = (uint32_t)rr(0, 5), bars = len / 16u ? len / 16u : 1u;
        int32_t d = (int32_t)(rng() % 3u) * 2, prev = 0;
        if (k > 5u) k = 5u;
        for (i = 0, j = 0; i < 16u && j < k; i++)       /* the motif's rhythm: k onsets, euclidean, turned */
            if (euclid(i, k, 16, rot)) at[j++] = (uint8_t)i;
        k = j;
        for (j = 0; j < k; j++) {                       /* its contour: steps, repeated notes; a leap, then a step back */
            int32_t mv = chance(25) ? 0 : chance(70) ? (chance(50) ? 1 : -1) : (chance(50) ? rr(2, 3) : -rr(2, 3));
            if (j && (prev >= 2 || prev <= -2)) mv = prev > 0 ? -1 : 1;   /* (the gap filled) */
            if (j) d = clamp(d + mv, -3, 7);
            deg[j] = (int8_t)d;
            snap[j] = (uint8_t)chance(60);
            prev = j ? mv : 0;
        }
        for (i = 0; i < bars; i++) {
            uint32_t ph = i % 4u, b0 = i * 16u;
            int32_t ch = chord_at(b0), shift = ph == 1u ? chord_at(b0) - chord_at(b0 >= 16u ? b0 - 16u : b0) : 0;
            if (ph == 2u && chance(30)) continue;       /* (now and then a bar of rest: space) */
            for (j = 0; j < k; j++) {
                int32_t dd = deg[j] + shift;
                uint32_t pos = b0 + at[j];
                if (ph == 3u && j == k - 1u) {          /* (A': its last note moved a step, or left out) */
                    if (chance(40)) continue;
                    dd += chance(50) ? 1 : -1;
                }
                if (at[j] % 4u == 0u && snap[j]) dd = dnear_ct(dd, ch);
                if (pos >= len) break;
                n[0] = dlead_note(t, base, dd);
                dstep_note(&t->step[pos], n, 1, at[j] % 4u == 0u ? 96u : 82u, chance(10) ? SF_SLIDE : 0);
                {   /* held to the next onset, a step of air before it (more air with less energy) */
                    uint32_t nxt = j + 1u < k ? at[j + 1u] : 16u + at[0], gap = nxt - at[j];
                    if (gap > 2u + (2u - e)) dhold(t, pos, gap - 2u - (2u - e), len);
                }
            }
        }
    }
}

/* ---- the arp, in the middle octaves (C3..C5) over the chord's four notes. AR_UPDOWN: up and down with holes;
 * AR_ALBERTI: low, high, middle, high; AR_GLASS: Glass's additive groups (3, then 3+2, ...) in a steady pulse;
 * AR_PULSE: Reich's repeated tone with accents; AR_WALK: a random walk over the chord (the IDM's) */
static void dice_arp(track_t *t, const dstyle_t *st, uint32_t len)
{
    int32_t t0 = dtonic(t, 52), n[1], c[4];
    uint32_t i, e = dh.energy, rate = e == 2u || dh.arp == AR_GLASS ? 1u : 2u, k = (uint32_t)rr(5, 9) + e * 2u, w = 0;
    static const uint8_t ALB[4] = {0, 3, 1, 3};
    if (dh.arp == AR_GLASS || dh.arp == AR_PULSE || dh.arp == AR_SEQ) k = 16;   /* (a steady pulse: no holes) */
    if (dh.arp == AR_SEQ) rate = 1;                    /* (0.6.80: the sequencer's sixteenths) */
    dh.nlast = 0;
    for (i = 0; i < len; i++) {
        uint32_t idx = i / rate, j;
        if (i % rate) continue;
        if (!euclid(idx % 16u, k, 16, 0) && chance(80)) continue;   /* (holes in the figure: space) */
        if (i % dh.steps == 0u || !dh.nlast) dchord(t, t0, chord_at(i), dh.voicing, c);   /* (0.6.80: from the
                                                         * tonic; t0 + 2 snapped to the scale's next degree, so the arp
                                                         * played the chord a step above the harmony, against the bass) */
        else for (j = 0; j < dh.nlast; j++) c[j] = dh.last[j];
        switch (dh.arp) {
        case AR_ALBERTI: j = ALB[idx % 4u]; break;
        case AR_GLASS: { uint32_t g = (idx / 8u) % 2u ? 5u : 3u; j = (idx % g) % 4u; break; }
        case AR_PULSE: j = idx % 8u == 7u ? 2u : 0u; break;
        case AR_WALK: w = (w + (chance(50) ? 1u : 3u)) % 4u; j = w; break;
        case AR_SEQ: { static const uint8_t SQ[4] = {0, 3, 1, 3}; j = SQ[idx % 4u]; break; }   /* (0.6.80: low, top,
                                                         * middle, top; the top its last note) */
        default: j = (idx % 6u) < 4u ? idx % 6u : 6u - idx % 6u; break;   /* up 0 1 2 3, down 2 1 */
        }
        j = j >= dh.nlast ? dh.nlast - 1u : j;          /* (0.6.80: three-note chords) */
        n[0] = c[j];
        while (n[0] > 72) n[0] -= 12;                   /* (the middle octaves) */
        while (n[0] < 48) n[0] += 12;
        dstep_note(&t->step[i], n, 1, dh.arp == AR_SEQ ? 84u : idx % 4u == 0u ? 92u : dh.arp == AR_PULSE ? 70u : 78u, 0);
        if (st->drums == DS_IDM && chance(10)) step_set_ratchet(&t->step[i], 2);
    }
}

/* a word in a sound's name (gen.c) */
static int dname_has(const char *s, const char *w)
{
    uint32_t i, j;
    for (i = 0; s[i]; i++) {
        for (j = 0; w[j] && s[i + j] == w[j]; j++) {}
        if (!w[j]) return 1;
    }
    return 0;
}
/* a part that sits high plays quieter: its level down with its average note above E4 (and its highest) */
static void dlevel(track_t *t, uint32_t len)
{
    uint32_t i, j, cnt = 0;
    int32_t sum = 0, top = 0, m;
    for (i = 0; i < len; i++)
        if (t->step[i].time == ST_NOTE)
            for (j = 0; j < t->step[i].n; j++) { sum += t->step[i].note[j]; cnt++; if (t->step[i].note[j] > top) top = t->step[i].note[j]; }
    if (!cnt) return;
    m = sum / (int32_t)cnt;
    if (m > 64) t->p[P_LEVEL] = (int16_t)clamp(t->p[P_LEVEL] - (m - 64) * 2, 50, 127);
    if (top > 79) t->p[P_LEVEL] = (int16_t)clamp(t->p[P_LEVEL] - (top - 79), 50, 127);
}


/* (0.6.81) a track's loop, a step that starts a note, the note sounding at a step (gen.c reads them too) */
static uint32_t dlen(const track_t *t) { uint32_t l = (uint32_t)t->p[P_SLEN]; return l >= 1u && l <= NSTEP ? l : 16u; }
static int dstarts(const track_t *t, uint32_t i) { const step_t *s = &t->step[i % dlen(t)]; return s->time == ST_NOTE && s->n; }
static int32_t dsounding(const track_t *t, uint32_t i)  /* the note sounding at step i of t's loop (held), -1 none */
{
    uint32_t len = dlen(t), j;
    for (j = 0; j < len; j++) {
        const step_t *s = &t->step[(i + len - j) % len];
        if (s->time == ST_NOTE) return s->n ? s->note[0] : -1;
        if (s->time != ST_TIE) return -1;
    }
    return -1;
}
/* (0.6.83) SAVE > TOOLS 6 CPU TEST: the heaviest song the instrument should play cleanly, to measure on the device
 * (USB console status: audio_max_us, audio_late, voices_shed) and to listen for clicks. Every live track: chords of four
 * on every step at 160 BPM, gliding into each other (GLIDE, POLY), two step components on each (ARP SEQ / ECHO x8 and
 * RAND / OCT, DIV, LEN); the last live track the DRUM kit, every lane on every step, OCT / ARP on its steps; every FX
 * on (DIST, CHORUS, DLY with PITCH, the CLOUD reverb). UNDO (REC + D4) brings the music back */
__attribute__((unused)) static void cpu_test_song(void)   /* (no longer on SAVE > TOOLS; tests/cpu_vg.c) */
{
    uint32_t k, i, n = trk_live;
    if (song.playing) transport_req = 2;
    song.g[G_BPM] = 160;
    song.g[G_RTYPE] = 4;                                /* (CLOUD: the heaviest reverb) */
    song.g[G_DPIT] = 12;                                /* (the delay's pitch shifter on) */
    for (k = 0; k < n; k++) {
        track_t *t = &trk[k];
        int drum = k + 1u == n;
        set_engine_of(t, drum ? ENGI_DRUM : k == 1u ? ENGI_FM6 : k == 2u ? 2u /* PHASE */ : ENGI_ANALOG);
        apply_preset_to(t, 0);
        load_begin(t, LOAD_PAT);
        track_defaults_steps(t);
        t->p[P_MUTE] = 0;
        t->p[P_VOICE] = V_POLY; t->p[P_SLEN] = 32; t->p[P_SGATE] = 120;
        t->p[P_GLIDE] = drum ? 0 : 90;
        t->p[P_DIST] = 30; t->p[P_CHOR] = 50; t->p[P_DLY] = 60; t->p[P_REV] = 70;
        for (i = 0; i < 32u; i++) {
            step_t *s = &t->step[i];
            memset(s, 0, sizeof *s);
            s->time = ST_NOTE; s->vel = 100;
            if (drum) {
                s->hit = 0xFF;
                s->comp = i % 4u ? CP_OCT : CP_ARPN;
                s->carg = s->comp == CP_OCT ? 4u : CP_ARG(0u, 2u, 0u);
            } else {
                uint32_t r = 48u + (i * 5u) % 12u;
                s->n = 4; s->note[0] = (uint8_t)r; s->note[1] = (uint8_t)(r + 4u); s->note[2] = (uint8_t)(r + 7u);
                s->note[3] = (uint8_t)(r + 11u);
                s->comp = i % 3u == 0u ? CP_ARPQ : CP_ECHO;
                s->carg = s->comp == CP_ARPQ ? CP_ARG(3u, 4u, 3u) : 6u;
                s->comp2 = i % 2u ? CP_RAND : CP_OCT; s->carg2 = i % 2u ? 7u : 5u;
                s->ctime = 5u | (6u << 3);
            }
        }
        load_end(t);
    }
}
static uint64_t tl_used(const track_t *t, uint32_t len);   /* (ui_tools.c: IDEAS > TRACK's locks) */
static void tl_unlock(track_t *t, uint64_t was, uint32_t len);
