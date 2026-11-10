/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp) SCL > CHORD HARP's chords: 16 sets of 12 (MAJ1..MAJ8 in C, MIN1..MIN8 in C minor), chord 1 on the
 * lowest key (F) up to chord 12 on key 11 (E), the 9 white keys above them the last chord's tones (seq.c).
 * Each chord is exact and whole: a root, a shape (its intervals, up to 6 notes) and a slash chord's bass. They move
 * with the track's ROOT. VOICING: close, root position; chord 1's root G2..F#3 (around C3), every other chord's root
 * in the octave up from it (running up the keys never jumps an octave down); a slash chord's bass the first one below it, its own tone left out above (no pitch class twice). The plate: the chord's tones up from its root,
 * from the first one above the chord's top note (C4 for C: as low as it can be without a note the chord sounds),
 * round again an octave up. Standalone (no libc): params.c includes it (SCL > CHORD's SET names), seq.c plays it */

#define HARP_NSET 16u
#define HARP_NCH 12u                    /* chords a set: keys 0..11 */
#define HARP_NMAX 6u                    /* notes a chord at most (bass included) */
#define HARP_NPLATE 9u                  /* the plate's keys: the white keys from key 12 */
#define HARP_LO 48                      /* C3: chord 1's root in HARP_LO - 5 .. HARP_LO + 6 (G2..F#3) */

/* the shapes: intervals over the root, low to high, -1 after the last; extensions above the octave (9 = 14) */
enum { HQ_MAJ, HQ_MIN, HQ_SUS4, HQ_SUS2, HQ_ADD9, HQ_MADD9, HQ_6, HQ_M6, HQ_MAJ7, HQ_M7, HQ_7, HQ_7SUS4, HQ_AUG,
       HQ_MS5, HQ_DIM, HQ_DIM7, HQ_M7B5, HQ_MAJ9, HQ_M9, HQ_9, HQ_9S5, HQ_MAJ7A13, HQ_M11, HQ_11, HQ_7B9B13, HQ_7S9B13,
       HQ_7S11, HQ_7S9, HQ_7B9, HQ_ADDB9, HQ_M69, HQ_MMAJ9, HQ_MAJ7S5, HQ_N };
static const int8_t HQ_IV[HQ_N][HARP_NMAX] = {
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 5, 7, -1}, {0, 2, 7, -1},                    /* MAJ MIN SUS4 SUS2 */
    {0, 4, 7, 14, -1}, {0, 3, 7, 14, -1}, {0, 4, 7, 9, -1}, {0, 3, 7, 9, -1},      /* ADD9 MADD9 6 M6 */
    {0, 4, 7, 11, -1}, {0, 3, 7, 10, -1}, {0, 4, 7, 10, -1}, {0, 5, 7, 10, -1},    /* MAJ7 M7 7 7SUS4 */
    {0, 4, 8, -1}, {0, 3, 8, -1}, {0, 3, 6, -1}, {0, 3, 6, 9, -1},                 /* AUG M#5 DIM DIM7 */
    {0, 3, 6, 10, -1}, {0, 4, 7, 11, 14, -1}, {0, 3, 7, 10, 14, -1},               /* M7B5 MAJ9 M9 */
    {0, 4, 7, 10, 14, -1}, {0, 4, 8, 10, 14, -1}, {0, 4, 7, 11, 21, -1},           /* 9 9#5 MAJ7ADD13 */
    {0, 3, 7, 10, 14, 17}, {0, 4, 7, 10, 14, 17},                                  /* M11 11 */
    {0, 4, 7, 10, 13, 20}, {0, 4, 7, 10, 15, 20},                                  /* 7(B9,B13) 7(#9,B13) */
    {0, 4, 7, 10, 18, -1}, {0, 4, 7, 10, 15, -1}, {0, 4, 7, 10, 13, -1},           /* 7#11 7#9 7B9 */
    {0, 4, 7, 13, -1}, {0, 3, 7, 9, 14, -1}, {0, 3, 7, 11, 14, -1}, {0, 4, 8, 11, -1}};   /* ADDB9 M6/9 M9MAJ7 MAJ7#5 */
static const char *const HQ_NAME[HQ_N] = {
    "", "m", "sus4", "sus2", "add9", "madd9", "6", "m6", "Maj7", "m7", "7", "7sus4", "+", "m#5", "dim", "dim7",
    "m7b5", "Maj9", "m9", "9", "9#5", "Maj7add13", "m11", "11", "7(b9,b13)", "7(#9,b13)", "7#11", "7#9", "7b9",
    "addb9", "m6/9", "m9Maj7", "Maj7#5"};
static const char *const HARP_PC[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const HARP_SET_NAME[HARP_NSET + 1] = {"MAJ1", "MAJ2", "MAJ3", "MAJ4", "MAJ5", "MAJ6", "MAJ7", "MAJ8",
                                                         "MIN1", "MIN2", "MIN3", "MIN4", "MIN5", "MIN6", "MIN7", "MIN8", 0};

typedef struct { uint8_t root, q, bass; } harp_ch_t;   /* root, bass: pitch classes over C (ROOT 0); HB_NONE no bass */
#define HB_NONE 15u
enum { HN_C, HN_CS, HN_D, HN_EB, HN_E, HN_F, HN_FS, HN_G, HN_AB, HN_A, HN_BB, HN_B };
#define HCH(r, q) {HN_##r, HQ_##q, HB_NONE}
#define HSL(r, q, b) {HN_##r, HQ_##q, HN_##b}
static const harp_ch_t HARP_SETS[HARP_NSET][HARP_NCH] = {
    {HCH(C, MAJ), HCH(E, MIN), HCH(F, MAJ), HCH(G, MAJ), HCH(A, MIN), HCH(E, SUS4), HCH(G, ADD9), HCH(D, MIN),   /* MAJ1 */
     HCH(F, ADD9), HCH(F, 6), HCH(G, SUS4), HCH(G, MAJ)},
    {HCH(C, MAJ), HSL(C, MAJ, B), HSL(C, MAJ, A), HSL(C, MAJ, G), HCH(F, ADD9), HCH(F, MIN), HCH(C, SUS4),        /* MAJ2 */
     HCH(A, M7), HCH(E, MIN), HCH(F, MAJ), HCH(G, SUS4), HCH(G, 6)},
    {HCH(C, MAJ), HCH(A, MIN), HCH(F, MAJ7), HCH(G, SUS4), HCH(A, MADD9), HCH(F, 6), HCH(C, SUS2), HCH(G, MAJ),  /* MAJ3 */
     HCH(D, SUS2), HCH(BB, ADD9), HCH(G, SUS4), HCH(F, ADD9)},
    {HCH(C, MAJ), HCH(G, MAJ), HCH(A, MIN), HCH(F, 6), HCH(F, MAJ), HSL(A, MIN, E), HCH(D, M7), HSL(A, MIN, C),  /* MAJ4 */
     HCH(G, ADD9), HCH(A, M7), HSL(G, MAJ, B), HSL(D, MIN, G)},
    {HCH(C, MAJ), HCH(D, MAJ), HCH(F, MAJ), HCH(C, MAJ), HCH(AB, MAJ), HCH(EB, MAJ), HCH(BB, MAJ), HCH(F, MAJ),  /* MAJ5 */
     HCH(G, 7SUS4), HCH(BB, ADD9), HCH(F, 6), HCH(C, ADD9)},
    {HCH(C, MAJ), HCH(G, MAJ), HCH(D, MIN), HCH(A, MIN), HCH(F, MAJ), HSL(G, MAJ, F), HSL(C, MAJ, E), HCH(A, M7), /* MAJ6 */
     HCH(D, M7), HCH(E, M7), HSL(C, MAJ, F), HCH(G, SUS4)},
    {HCH(C, MAJ), HCH(G, MAJ), HCH(A, MIN), HCH(E, MIN), HCH(F, MAJ), HSL(C, MAJ, E), HCH(D, MIN), HSL(D, MIN, C), /* MAJ7 */
     HSL(G, 7, B), HCH(G, MAJ), HSL(F, MAJ, A), HSL(G, 7, B)},
    {HCH(C, MAJ9), HCH(CS, DIM), HCH(D, M9), HCH(EB, DIM7), HCH(E, M9), HCH(C, 9S5), HCH(F, MAJ7A13), HCH(BB, 9), /* MAJ8 */
     HCH(E, M7), HCH(A, 9), HCH(D, M11), HCH(G, 7B9B13)},
    {HCH(C, MIN), HSL(C, MIN, EB), HCH(F, MIN), HCH(G, MAJ), HCH(AB, MAJ7), HCH(EB, MAJ), HCH(G, MIN), HCH(BB, MAJ), /* MIN1 */
     HCH(F, MAJ), HSL(F, MIN, AB), HSL(C, MIN, G), HCH(G, MAJ)},
    {HCH(C, MIN), HSL(G, AUG, B), HSL(C, MIN, BB), HSL(C, MIN, A), HCH(AB, MAJ7), HCH(EB, MAJ7), HCH(F, MIN),     /* MIN2 */
     HCH(BB, 7), HCH(C, MIN), HCH(BB, ADD9), HCH(AB, ADD9), HCH(G, 7SUS4)},
    {HCH(C, MIN), HCH(AB, MAJ), HCH(EB, MAJ), HCH(BB, MAJ), HCH(F, MAJ), HCH(F, MIN), HSL(C, MIN, G), HCH(G, SUS4), /* MIN3 */
     HCH(C, MIN), HCH(C, MS5), HCH(C, M6), HCH(C, M7)},
    {HCH(C, MIN), HCH(EB, MAJ), HCH(BB, MAJ), HCH(F, MAJ), HCH(AB, MAJ), HCH(AB, MAJ7), HCH(AB, M7), HCH(EB, MAJ7), /* MIN4 */
     HCH(D, SUS4), HCH(D, MAJ), HSL(F, MIN, G), HCH(G, AUG)},
    {HCH(C, MIN), HCH(BB, MAJ), HCH(AB, 6), HCH(BB, ADD9), HCH(C, MIN), HCH(D, M7), HSL(C, MIN, EB), HCH(F, MIN), /* MIN5 */
     HCH(G, MAJ), HCH(AB, MAJ7), HSL(C, MIN, G), HCH(C, SUS4)},
    {HCH(C, MIN), HCH(F, MIN), HCH(BB, MAJ), HCH(EB, MAJ), HCH(AB, MAJ), HCH(D, M7B5), HCH(G, ADDB9), HCH(G, MAJ), /* MIN6 */
     HSL(AB, MAJ, G), HCH(G, 7), HSL(C, MIN, G), HCH(G, 7B9)},
    {HCH(C, M9), HCH(AB, 9), HCH(C, M11), HCH(C, 7S9B13), HCH(F, M9), HSL(EB, MAJ7, F), HCH(C, 11), HCH(A, M11),  /* MIN7 */
     HCH(AB, 7S11), HCH(G, 7S9), HCH(C, MADD9), HCH(G, 7B9B13)},
    {HCH(C, M69), HCH(D, M7B5), HSL(C, M11, G), HCH(C, M9), HCH(F, M9), HCH(AB, M7), HCH(EB, M7), HCH(BB, M7B5), /* MIN8 */
     HCH(A, M11), HCH(AB, MAJ7S5), HCH(G, 7B9B13), HCH(C, MMAJ9)}};
#undef HCH
#undef HSL

/* (2026-10-27) the J-6's 100 sets after them (SET 16..115, harp_j6.h, by genre): each chord as the J-6 voices it on C,
 * moved with ROOT (C# .. F# up, G .. B down); no chord id (seq.c records their notes). A set's genre (the HARP page's
 * SET label) in J6_GNAME: 0 HARP's own sets, then the J-6's genres */
#include "res_decl.h"                   /* (chord-harp fork) YEHNAH_RES: the J-6 sets, PROGS in the resource region (harp_test.c builds this alone) */
#include "harp_j6.h"
#define HARP_NALL (HARP_NSET + J6_NSET)
#include "harp_gtr.h"
/* (2026-11-10) SCL > GUITAR's 50 sets (harp_gtr.h: guitchordi's chord styles, GT_GNAME the families): in this file's set
 * numbers HARP_GT0 + n, the voicing of a guitar in standard tuning, in the style's key (C = ROOT C), moved with ROOT;
 * no chord id either (seq.c records the notes). GUITAR's own parameter, P_GSET, holds n */
#define HARP_GT0 HARP_NALL
static uint32_t gt_genre_set(uint32_t g)    /* family g's first style (0 .. GT_NSET - 1) */
{
    uint32_t i;
    for (i = 0; i < GT_NSET; i++)
        if (GT_GEN[i] >= g)
            return i;
    return 0;
}
static uint32_t harp_genre(uint32_t set) { return set < HARP_NSET ? 0u : J6_GEN[(set - HARP_NSET) % J6_NSET]; }
static uint32_t harp_genre_set(uint32_t g)   /* genre g's first set */
{
    uint32_t i;
    for (i = 0; g && i < J6_NSET; i++)
        if (J6_GEN[i] >= g)
            return HARP_NSET + i;
    return 0;
}
/* a guitar set's chord: six strings low to high (the same note twice once), moved with ROOT (G .. B down) */
static uint32_t gt_chord(uint32_t set, uint32_t idx, int32_t root, uint8_t *out)
{
    const uint8_t *nt = GT_NOTES[GT_CH[(set - HARP_GT0) % GT_NSET][idx % HARP_NCH % 11u]];
    int32_t sh = (root % 12 + 12) % 12;
    uint32_t i, n = 0;
    if (sh > 6)
        sh -= 12;
    for (i = 0; i < 6u; i++)
        if (nt[i] && (!n || out[n - 1u] != (uint8_t)(nt[i] + sh)))
            out[n++] = (uint8_t)(nt[i] + sh);
    return n;
}
static void gt_name(uint32_t set, uint32_t idx, int32_t root, char *buf)
{
    uint32_t c = GT_CH[(set - HARP_GT0) % GT_NSET][idx % HARP_NCH % 11u], n = 0;
    const char *s;
    for (s = HARP_PC[(c / 21u + (uint32_t)(root % 12 + 12)) % 12u]; *s; s++) buf[n++] = *s;
    for (s = GT_QSUF[c % 21u]; *s && n < 15u; s++) buf[n++] = *s;
    buf[n] = 0;
}
static uint32_t j6_ty(uint32_t set, uint32_t idx) { return J6_TY[J6_CH[(set - HARP_NSET) % J6_NSET][idx % HARP_NCH] & 511u]; }
static int32_t j6_lo(uint32_t set, uint32_t idx, int32_t root)   /* the chord's bass */
{
    int32_t sh = (root % 12 + 12) % 12;
    return HARP_LO + (int32_t)(idx % HARP_NCH) + (J6_CH[(set - HARP_NSET) % J6_NSET][idx % HARP_NCH] >> 9) - 24 +
           (sh > 6 ? sh - 12 : sh);
}
static uint32_t j6_chord(uint32_t set, uint32_t idx, int32_t root, uint8_t *out)
{
    uint32_t ty = j6_ty(set, idx), n = 1, i;
    out[0] = (uint8_t)j6_lo(set, idx, root);
    for (i = 0; i < 3u && (ty >> 6u * i & 63u); i++)
        out[n++] = (uint8_t)(out[0] + (ty >> 6u * i & 63u));
    return n;
}
static void j6_name(uint32_t set, uint32_t idx, int32_t root, char *buf)
{
    uint32_t ty = j6_ty(set, idx), lo = (uint32_t)j6_lo(set, idx, root), n = 0, i;
    const char *s;
    for (s = HARP_PC[(lo + (ty >> 25 & 15u)) % 12u]; *s; s++) buf[n++] = *s;
    for (s = J6_SUF, i = ty >> 18 & 127u; i; s++)       /* (the suffix: the i-th of J6_SUF's) */
        i -= !*s;
    for (; *s && n < 15u; s++) buf[n++] = *s;
    if (ty >> 29 & 1u) {
        if (n < 15u) buf[n++] = '/';
        for (s = HARP_PC[lo % 12u]; *s && n < 15u; s++) buf[n++] = *s;
    }
    buf[n] = 0;
}

static const harp_ch_t *harp_get(uint32_t set, uint32_t idx) { return &HARP_SETS[set % HARP_NSET][idx % HARP_NCH]; }
/* chord idx's root note: chord 1's G2..F#3, the others in the octave up from it (a J-6 chord's: its bass) */
static int32_t harp_lo(uint32_t set, uint32_t idx, int32_t root)
{
    const harp_ch_t *c1 = harp_get(set, 0);
    if (set >= HARP_GT0) {
        uint8_t nt[6];
        gt_chord(set, idx, root, nt);
        return nt[0];
    }
    if (set >= HARP_NSET)
        return j6_lo(set, idx, root);
    int32_t r1 = HARP_LO + (c1->root + root % 12 + 12) % 12;
    if (r1 > HARP_LO + 6)
        r1 -= 12;
    return r1 + (harp_get(set, idx)->root - c1->root + 12) % 12;
}

/* the chord's notes, low to high (bass first), into out (HARP_NMAX); root: the track's ROOT (0 C .. 11 B) */
static uint32_t harp_chord(uint32_t set, uint32_t idx, int32_t root, uint8_t *out)
{
    const harp_ch_t *c = harp_get(set, idx);
    int32_t lo = harp_lo(set, idx, root), b = c->bass < 12u ? (c->bass + root % 12 + 12) % 12 : -1;
    uint32_t i, n = 0;
    if (set >= HARP_GT0)
        return gt_chord(set, idx, root, out);
    if (set >= HARP_NSET)
        return j6_chord(set, idx, root, out);
    if (b >= 0) {
        int32_t d = (lo - b + 120) % 12;
        out[n++] = (uint8_t)(lo - (d ? d : 12));   /* the first one below the root */
    }
    for (i = 0; i < HARP_NMAX && HQ_IV[c->q][i] >= 0 && n < HARP_NMAX; i++)
        if ((lo + HQ_IV[c->q][i]) % 12 != b)
            out[n++] = (uint8_t)(lo + HQ_IV[c->q][i]);
    return n;
}

/* the plate's 9 notes for the chord, low to high: its tones (the bass left out) from the first one above the chord's
 * top note on (as low as it goes without a note the chord sounds), round again an octave up */
static void harp_plate(uint32_t set, uint32_t idx, int32_t root, uint8_t *out)
{
    const harp_ch_t *c = harp_get(set, idx);
    int32_t lo = harp_lo(set, idx, root), x;
    uint8_t nt[HARP_NMAX];
    uint32_t i, n = harp_chord(set, idx, root, nt), pcs = 0;
    for (i = 0; set >= HARP_NSET && i < n; i++)         /* (a J-6 chord's: every note's) */
        pcs |= 1u << nt[i] % 12u;
    for (i = 0; set < HARP_NSET && i < HARP_NMAX && HQ_IV[c->q][i] >= 0; i++)   /* the tones' places in the octave */
        pcs |= 1u << (uint32_t)((lo + HQ_IV[c->q][i]) % 12);
    for (i = 0, x = nt[n - 1u] + 1; i < HARP_NPLATE; x++)
        if ((pcs >> (uint32_t)(x % 12)) & 1u)
            out[i++] = (uint8_t)x;
}

/* the chord's name as the screen shows it, e.g. "Cm11/G" (buf: 16 bytes) */
static void harp_name(uint32_t set, uint32_t idx, int32_t root, char *buf)
{
    const harp_ch_t *c = harp_get(set, idx);
    const char *s;
    uint32_t n = 0;
    if (set >= HARP_GT0) {
        gt_name(set, idx, root, buf);
        return;
    }
    if (set >= HARP_NSET) {
        j6_name(set, idx, root, buf);
        return;
    }
    for (s = HARP_PC[(c->root + root % 12 + 12) % 12]; *s; s++) buf[n++] = *s;
    for (s = HQ_NAME[c->q]; *s && n < 15u; s++) buf[n++] = *s;
    if (c->bass < 12u) {
        if (n < 15u) buf[n++] = '/';
        for (s = HARP_PC[(c->bass + root % 12 + 12) % 12]; *s && n < 15u; s++) buf[n++] = *s;
    }
    buf[n] = 0;
}

/* (chord-harp) SCL > CHORD OMNI's chords, after FoMni (charlesvestal/fm1-fomni, GPL-3.0) and the OM-84 it follows:
 * (chord-harp fork) laid out as HARP's: the lowest 12 keys chord buttons, the 9 white keys above them the strum plate,
 * the top F# LATCH, the other black keys over the plate HARP's bends (seq.c). SET picks the buttons: FOMNI, FoMni's own
 * (F C G, Dm Am Em, G7 E7 D7, Bb, A7: the primary chords, their relative minors, the dominants into them; and C7, the
 * twelfth: the dominant into F), or the circle of fifths from ROOT up and round, C G D A E B F# Db Ab Eb Bb F, every
 * button MAJ, MIN, 7 or M7. Triads, as the OM's: a 7th drops its fifth. Voiced as FoMni's: the three tones folded into
 * F#3..F4 (the OM's chord register), the root below them in C2..B2 (its bass); each string a tone of the chord folded
 * into its own octave window, root, third, fifth, from F#3 up (FoMni's 16 strings from F#2, here its middle 9).
 * A chord's id (a recorded step's CHORD, seq.c) carries its pitch class whole, ROOT in it: HARP_NID + type * 12 + pc */
#define OMNI_NSET 5u
#define OMNI_NPAD 12u                   /* the chord keys, as HARP's */
#define OMNI_NSTR 9u                    /* the plate's keys, as HARP's */
_Static_assert(OMNI_NPAD == HARP_NCH && OMNI_NSTR == HARP_NPLATE, "OMNI's keys: HARP's");
#define HARP_NID (HARP_NSET * HARP_NCH) /* the chord ids: HARP's 0..191, OMNI's 192..239, PROG's 240..255 */
#define PROG_NID (HARP_NID + 4u * 12u)
#define CHORD_NID 256u
enum { OQ_MAJ, OQ_MIN, OQ_7, OQ_M7 };
static const int8_t OQ_IV[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 4, 10}, {0, 3, 10}};
static const char *const OMNI_SET_NAME[OMNI_NSET + 1] = {"FOMNI", "MAJ", "MIN", "7", "M7", 0};
static const uint8_t OMNI_FPC[OMNI_NPAD] = {HN_F, HN_C, HN_G, HN_D, HN_A, HN_E, HN_G, HN_E, HN_D, HN_BB, HN_A, HN_C};
static const uint8_t OMNI_FQ[OMNI_NPAD] = {OQ_MAJ, OQ_MAJ, OQ_MAJ, OQ_MIN, OQ_MIN, OQ_MIN, OQ_7, OQ_7, OQ_7, OQ_MAJ, OQ_7, OQ_7};

/* (chord-harp fork) SCL > PROG's chords: 16 qualities (of HQ_*), each one chord id, PROG_NID + its PQ, rooted on C3: a
 * step holds the root as its first note and plays the quality from it (seq.c step_expand), named from it (hc_name) */
enum { PQ_MAJ, PQ_MIN, PQ_7, PQ_MAJ7, PQ_M7, PQ_DIM, PQ_M7B5, PQ_SUS4, PQ_SUS2, PQ_7SUS4, PQ_6, PQ_M6, PQ_9, PQ_ADD9,
       PQ_AUG, PQ_DIM7, PQ_N };
static const uint8_t PQ_HQ[PQ_N] = {HQ_MAJ, HQ_MIN, HQ_7, HQ_MAJ7, HQ_M7, HQ_DIM, HQ_M7B5, HQ_SUS4, HQ_SUS2, HQ_7SUS4,
                                    HQ_6, HQ_M6, HQ_9, HQ_ADD9, HQ_AUG, HQ_DIM7};
_Static_assert(PROG_NID + PQ_N == CHORD_NID, "PROG's chord ids: the byte's last 16");
/* the progressions: a name, then each chord as its degree over ROOT (semitones, 0..11) and its PQ: PC(deg, q) */
#define PROG_NCH 12u                    /* chords a progression at most: the chord keys */
typedef struct { const char *name; uint8_t n; uint8_t ch[PROG_NCH]; } prog_t;
#define PC(d, q) (uint8_t)((d) | PQ_##q << 4)
static const prog_t PROGS[] YEHNAH_RES = {   /* (chord-harp fork: the resource region, res.c) */
    {"I V vi IV", 4, {PC(0, MAJ), PC(7, MAJ), PC(9, MIN), PC(5, MAJ)}},                      /* pop */
    {"vi IV I V", 4, {PC(9, MIN), PC(5, MAJ), PC(0, MAJ), PC(7, MAJ)}},
    {"I vi IV V", 4, {PC(0, MAJ), PC(9, MIN), PC(5, MAJ), PC(7, MAJ)}},                      /* the 50s */
    {"I IV vi V", 4, {PC(0, MAJ), PC(5, MAJ), PC(9, MIN), PC(7, MAJ)}},
    {"I IV V IV", 4, {PC(0, MAJ), PC(5, MAJ), PC(7, MAJ), PC(5, MAJ)}},                      /* rock */
    {"CANON", 8, {PC(0, MAJ), PC(7, MAJ), PC(9, MIN), PC(4, MIN), PC(5, MAJ), PC(0, MAJ), PC(5, MAJ), PC(7, MAJ)}},
    {"ROYAL ROAD", 4, {PC(5, MAJ7), PC(7, 7), PC(4, M7), PC(9, MIN)}},                      /* IVM7 V7 iii7 vi */
    {"I bVII IV", 4, {PC(0, MAJ), PC(10, MAJ), PC(5, MAJ), PC(0, MAJ)}},                     /* mixolydian */
    {"I bVI bVII", 4, {PC(0, MAJ), PC(8, MAJ), PC(10, MAJ), PC(0, MAJ)}},                    /* the hero's cadence */
    {"I III IV iv", 4, {PC(0, MAJ), PC(4, MAJ), PC(5, MAJ), PC(5, MIN)}},
    {"I I7 IV iv", 4, {PC(0, MAJ), PC(0, 7), PC(5, MAJ), PC(5, MIN)}},
    {"ii V I", 3, {PC(2, M7), PC(7, 7), PC(0, MAJ7)}},                                     /* jazz */
    {"I vi ii V", 4, {PC(0, MAJ7), PC(9, M7), PC(2, M7), PC(7, 7)}},
    {"iii VI ii V", 4, {PC(4, M7), PC(9, 7), PC(2, M7), PC(7, 7)}},
    {"ii V i", 3, {PC(2, M7B5), PC(7, 7), PC(0, M6)}},
    {"RHYTHM", 4, {PC(4, 7), PC(9, 7), PC(2, 7), PC(7, 7)}},                                /* III7 VI7 II7 V7 */
    {"12 BAR", 12, {PC(0, 7), PC(0, 7), PC(0, 7), PC(0, 7), PC(5, 7), PC(5, 7), PC(0, 7), PC(0, 7), PC(7, 7), PC(5, 7),
                    PC(0, 7), PC(7, 7)}},                                                   /* blues */
    {"QUICK 12", 12, {PC(0, 7), PC(5, 7), PC(0, 7), PC(0, 7), PC(5, 7), PC(5, 7), PC(0, 7), PC(0, 7), PC(7, 7),
                      PC(5, 7), PC(0, 7), PC(7, 7)}},
    {"8 BAR", 8, {PC(0, 7), PC(7, 7), PC(5, 7), PC(5, 7), PC(0, 7), PC(7, 7), PC(0, 7), PC(7, 7)}},
    {"MINOR BLUES", 12, {PC(0, M7), PC(0, M7), PC(0, M7), PC(0, M7), PC(5, M7), PC(5, M7), PC(0, M7), PC(0, M7),
                         PC(8, 7), PC(7, 7), PC(0, M7), PC(7, 7)}},
    {"i VI III VII", 4, {PC(0, MIN), PC(8, MAJ), PC(3, MAJ), PC(10, MAJ)}},                 /* minor */
    {"ANDALUSIAN", 4, {PC(0, MIN), PC(10, MAJ), PC(8, MAJ), PC(7, MAJ)}},
    {"i iv v i", 4, {PC(0, MIN), PC(5, MIN), PC(7, MIN), PC(0, MIN)}},
    {"i VI VII i", 4, {PC(0, MIN), PC(8, MAJ), PC(10, MAJ), PC(0, MIN)}},
    {"i iv VII III", 4, {PC(0, MIN), PC(5, MIN), PC(10, MAJ), PC(3, MAJ)}},                 /* the circle */
    {"i III VII iv", 4, {PC(0, MIN), PC(3, MAJ), PC(10, MAJ), PC(5, MIN)}},
    {"PHRYGIAN", 4, {PC(0, MIN), PC(1, MAJ), PC(0, MIN), PC(1, MAJ)}},                      /* modal */
    {"DORIAN", 4, {PC(0, M7), PC(5, MAJ), PC(0, M7), PC(5, 7)}},
    {"LYDIAN", 4, {PC(0, MAJ7), PC(2, MAJ), PC(0, MAJ7), PC(2, MAJ)}},
    {"GOSPEL", 8, {PC(0, MAJ), PC(0, 7), PC(5, MAJ), PC(6, DIM7), PC(0, MAJ), PC(9, M7), PC(2, M7), PC(7, 9)}},
    {"DEEP HOUSE", 4, {PC(0, M7), PC(5, M7), PC(3, MAJ7), PC(10, ADD9)}},                   /* dance */
    {"SUS FLOW", 4, {PC(0, SUS2), PC(5, MAJ7), PC(9, M7), PC(7, SUS4)}},
};
#undef PC
#define PROG_N ((uint32_t)(sizeof PROGS / sizeof PROGS[0]))
static const char *const PROG_NAME[] = {
    "I V vi IV", "vi IV I V", "I vi IV V", "I IV vi V", "I IV V IV", "CANON", "ROYAL ROAD", "I bVII IV", "I bVI bVII",
    "I III IV iv", "I I7 IV iv", "ii V I", "I vi ii V", "iii VI ii V", "ii V i", "RHYTHM", "12 BAR", "QUICK 12", "8 BAR",
    "MINOR BLUES", "i VI III VII", "ANDALUSIAN", "i iv v i", "i VI VII i", "i iv VII III", "i III VII iv", "PHRYGIAN",
    "DORIAN", "LYDIAN", "GOSPEL", "DEEP HOUSE", "SUS FLOW", 0};
_Static_assert(sizeof PROG_NAME / sizeof PROG_NAME[0] == PROG_N + 1u, "PROG's names: one a progression (the knob's list)");
/* chord key k of progression p (k past its chords: round again an octave up, three octaves, then from the bottom again)
 * under ROOT root: its chord id and its root note (*rn: the key's root in octave 3 and the chord's degree above it, so a
 * round's chords rise within one octave from the key: C, then G3, A3, F3 for I V vi IV in C; + 12 a round. Chord-harp
 * fork, 2026-10-26: was each nearest C3, G2..F#3, the V under the I) */
static uint32_t prog_chord(uint32_t p, uint32_t k, int32_t root, int32_t *rn)
{
    if (!RES_OK()) {                                    /* (chord-harp fork) the progressions in the resource region (res.c):
                                                         * missing, each key the major chord on ROOT, an octave a round */
        *rn = HARP_LO + (root % 12 + 12) % 12 + 12 * (int32_t)(k % 3u);
        return PROG_NID + PQ_MAJ;
    }
    const prog_t *g = &PROGS[p % PROG_N];
    uint32_t c = g->ch[k % g->n], deg = c & 15u;
    int32_t r = HARP_LO + (root % 12 + 12) % 12 + (int32_t)(deg % 12u);
    *rn = r + 12 * (int32_t)(k / g->n % 3u);
    return PROG_NID + (c >> 4);
}

/* button p's chord id under SET set and ROOT root */
static uint32_t omni_id(uint32_t set, uint32_t p, int32_t root)
{
    uint32_t pc = set ? 7u * (p % OMNI_NPAD) % 12u : OMNI_FPC[p % OMNI_NPAD];   /* (up the fifths from C: the root) */
    uint32_t q = set ? (set - 1u) % 4u : OMNI_FQ[p % OMNI_NPAD];
    return HARP_NID + q * 12u + (pc + (uint32_t)(root % 12 + 12)) % 12u;
}
static int32_t om_fold(int32_t n, int32_t lo) { return lo + ((n - lo) % 12 + 12) % 12; }

/* chord id's root note (a recorded step's first note): HARP's (root: the track's ROOT), OMNI's bass (root unused) */
static int32_t hc_lo(uint32_t id, int32_t root)
{
    return id < HARP_NID ? harp_lo(id / HARP_NCH, id % HARP_NCH, root) :
           id >= PROG_NID ? HARP_LO : 36 + (int32_t)((id - HARP_NID) % 12u);   /* (PROG's: on C3, shifted by its step) */
}
/* chord id's notes, low to high (HARP_NMAX) */
static uint32_t hc_chord(uint32_t id, int32_t root, uint8_t *out)
{
    uint32_t q = (id - HARP_NID) / 12u % 4u, i, j, n = 1;
    int32_t pc = (int32_t)((id - HARP_NID) % 12u);
    if (id < HARP_NID)
        return harp_chord(id / HARP_NCH, id % HARP_NCH, root, out);
    if (id >= PROG_NID) {                               /* (PROG's) the quality on C3, root position */
        const int8_t *iv = HQ_IV[PQ_HQ[(id - PROG_NID) % PQ_N]];
        for (n = 0; n < HARP_NMAX && iv[n] >= 0; n++)
            out[n] = (uint8_t)(HARP_LO + iv[n]);
        return n;
    }
    out[0] = (uint8_t)(36 + pc);
    for (i = 0; i < 3u; i++) {                          /* (insertion: the folded tones low to high) */
        uint8_t x = (uint8_t)om_fold(pc + OQ_IV[q][i], 54);
        for (j = n++; j > 1u && out[j - 1u] > x; j--)
            out[j] = out[j - 1u];
        out[j] = x;
    }
    return n;
}
/* the plate's notes for chord id (out: HARP_NPLATE) */
static void hc_plate(uint32_t id, int32_t root, uint8_t *out)
{
    uint32_t q = (id - HARP_NID) / 12u % 4u, i;
    if (id < HARP_NID) {
        harp_plate(id / HARP_NCH, id % HARP_NCH, root, out);
        return;
    }
    if (id >= PROG_NID) {                               /* (PROG's) as HARP's: its tones up from above its top note */
        const int8_t *iv = HQ_IV[PQ_HQ[(id - PROG_NID) % PQ_N]];
        uint32_t pcs = 0, n = 0;
        int32_t x;
        for (; n < HARP_NMAX && iv[n] >= 0; n++)
            pcs |= 1u << (uint32_t)((HARP_LO + iv[n]) % 12);
        for (i = 0, x = HARP_LO + iv[n - 1u] + 1; i < HARP_NPLATE; x++)
            if ((pcs >> (uint32_t)(x % 12)) & 1u)
                out[i++] = (uint8_t)x;
        return;
    }
    for (i = 0; i < OMNI_NSTR; i++)
        out[i] = (uint8_t)om_fold((int32_t)((id - HARP_NID) % 12u) + OQ_IV[q][i % 3u], 54 + 12 * (int32_t)(i / 3u));
}
/* chord id's name (buf: 16 bytes) */
static void hc_name(uint32_t id, int32_t root, char *buf)
{
    static const char *const OQ_NAME[4] = {"", "m", "7", "m7"};
    const char *s;
    uint32_t n = 0;
    if (id < HARP_NID) {
        harp_name(id / HARP_NCH, id % HARP_NCH, root, buf);
        return;
    }
    if (id >= PROG_NID) {                               /* (PROG's) its root: root (the step's, from C) */
        for (s = HARP_PC[(root % 12 + 12) % 12]; *s; s++) buf[n++] = *s;
        for (s = HQ_NAME[PQ_HQ[(id - PROG_NID) % PQ_N]]; *s && n < 15u; s++) buf[n++] = *s;
        buf[n] = 0;
        return;
    }
    for (s = HARP_PC[(id - HARP_NID) % 12u]; *s; s++) buf[n++] = *s;
    for (s = OQ_NAME[(id - HARP_NID) / 12u % 4u]; *s; s++) buf[n++] = *s;
    buf[n] = 0;
}
