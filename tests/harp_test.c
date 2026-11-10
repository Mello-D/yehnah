/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp) SCL > CHORD HARP's chord sets (firmware/src/harp.c): every chord of every set in every ROOT whole,
 * low to high, no pitch class twice, the slash bass lowest; the plate's tones the chord's; the readings the owner
 * confirmed. HARP_MD=file writes every chord (ROOT C) as a Markdown table for the owner to check */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../firmware/src/harp.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("harp: FAIL " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static const char *note_name(uint32_t n, char *b)
{
    sprintf(b, "%s%d", HARP_PC[n % 12u], (int)(n / 12u) - 1);   /* C4 = 60 */
    return b;
}

/* set, chord (1..12), ROOT C: its name and pitch classes low to high (the bass first) */
static const struct { uint32_t set, ch; const char *name; int8_t pc[HARP_NMAX + 1]; } KNOWN[] = {
    {7, 12, "G7(b9,b13)", {7, 11, 2, 5, 8, 3, -1}},      /* G B D F Ab Eb */
    {15, 3, "Cm11/G", {7, 0, 3, 10, 2, 5, -1}},          /* bass G + Cm11 without its G */
    {13, 7, "Gaddb9", {7, 11, 2, 8, -1}},                /* owner: G B D Ab */
    {10, 10, "Cm#5", {0, 3, 8, -1}},                     /* owner: C Eb G# */
    {15, 7, "Ebm7", {3, 6, 10, 1, -1}},                  /* owner: Ebmi7 */
    {15, 9, "Am11", {9, 0, 4, 7, 11, 2, -1}},            /* owner: am11 */
    {9, 2, "G+/B", {11, 7, 3, -1}},                      /* B, G D#: the bass's B not again above */
    {15, 12, "Cm9Maj7", {0, 3, 7, 11, 2, -1}},
    {14, 4, "C7(#9,b13)", {0, 4, 7, 10, 3, 8, -1}},
    {1, 2, "C/B", {11, 0, 4, 7, -1}},
    {7, 7, "FMaj7add13", {5, 9, 0, 4, 2, -1}},
    {14, 6, "EbMaj7/F", {5, 3, 7, 10, 2, -1}},
};

static void check_all(void)
{
    uint32_t s, i, k, r;
    uint8_t nt[HARP_NMAX], pl[HARP_NPLATE];
    for (s = 0; s < HARP_NSET; s++)
        for (i = 0; i < HARP_NCH; i++)
            for (r = 0; r < 12u; r++) {
                const harp_ch_t *c = harp_get(s, i);
                uint32_t n = harp_chord(s, i, (int32_t)r, nt), seen = 0, lo = HARP_LO + (harp_get(s, 0)->root + r) % 12u;
                if (lo > HARP_LO + 6u) lo -= 12u;                   /* chord 1's root G2..F#3, */
                lo += (c->root - harp_get(s, 0)->root + 12u) % 12u;   /* the others in the octave up from it */
                CHECK(n >= 3u && n <= HARP_NMAX, "set %u chord %u root %u: %u notes", s, i + 1u, r, n);
                for (k = 0; k < n; k++) {
                    CHECK(!k || nt[k] > nt[k - 1u], "set %u chord %u root %u: not low to high", s, i + 1u, r);
                    CHECK(!((seen >> (nt[k] % 12u)) & 1u), "set %u chord %u root %u: a pitch class twice", s, i + 1u, r);
                    seen |= 1u << (nt[k] % 12u);
                    CHECK(nt[k] >= 24u && nt[k] <= 96u, "set %u chord %u root %u: note %u", s, i + 1u, r, nt[k]);
                }
                CHECK((seen >> ((c->root + r) % 12u)) & 1u, "set %u chord %u root %u: no root", s, i + 1u, r);
                if (c->bass < 12u)
                    CHECK(nt[0] % 12u == (c->bass + r) % 12u && nt[0] < lo, "set %u chord %u root %u: bass", s, i + 1u, r);
                else
                    CHECK(nt[0] == lo, "set %u chord %u root %u: root position", s, i + 1u, r);
                harp_plate(s, i, (int32_t)r, pl);
                CHECK(pl[0] > nt[n - 1u], "set %u chord %u root %u: plate not above the chord (a note shared)", s, i + 1u, r);
                for (k = nt[n - 1u] + 1u; k < pl[0]; k++)       /* .. its first tone above the chord's top: as low as can be */
                    CHECK(!((seen >> (k % 12u)) & 1u) || (c->bass < 12u && k % 12u == (c->bass + r) % 12u),
                          "set %u chord %u root %u: plate higher than it needs", s, i + 1u, r);
                for (k = 0; k < HARP_NPLATE; k++) {
                    CHECK(!k || pl[k] > pl[k - 1u], "set %u chord %u root %u: plate not low to high", s, i + 1u, r);
                    CHECK((seen >> (pl[k] % 12u)) & 1u, "set %u chord %u root %u: plate note %u not the chord's", s, i + 1u,
                          r, pl[k]);
                }
            }
}

static void check_known(void)
{
    uint32_t k, j, n;
    uint8_t nt[HARP_NMAX];
    char nm[16];
    for (k = 0; k < sizeof KNOWN / sizeof KNOWN[0]; k++) {
        harp_name(KNOWN[k].set, KNOWN[k].ch - 1u, 0, nm);
        CHECK(!strcmp(nm, KNOWN[k].name), "%s read as %s", KNOWN[k].name, nm);
        n = harp_chord(KNOWN[k].set, KNOWN[k].ch - 1u, 0, nt);
        for (j = 0; KNOWN[k].pc[j] >= 0; j++)
            CHECK(j < n && nt[j] % 12u == (uint32_t)KNOWN[k].pc[j], "%s: tone %u", KNOWN[k].name, j + 1u);
        CHECK(n == j, "%s: %u notes, not %u", KNOWN[k].name, n, j);
    }
    harp_name(0, 0, 2, nm);                              /* ROOT D: MAJ1's C is D, its Em F#m */
    CHECK(!strcmp(nm, "D"), "ROOT D: %s", nm);
    harp_name(0, 1, 2, nm);
    CHECK(!strcmp(nm, "F#m"), "ROOT D: %s", nm);
}

static void write_md(const char *path)
{
    static const char *const KEY[HARP_NCH] = {"F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E"};
    FILE *f = fopen(path, "w");
    uint32_t s, i, k, n;
    uint8_t nt[HARP_NMAX], pl[HARP_NPLATE];
    char nm[16], b[8];
    if (!f) { printf("harp: cannot write %s\n", path); fails++; return; }
    fprintf(f, "# Chord-harp sets, ROOT C\n\nChord keys from the lowest F. Notes: C4 = middle C. "
               "Plate: the 9 white keys above the chord keys, low to high.\n");
    for (s = 0; s < HARP_NSET; s++) {
        fprintf(f, "\n## %s\n\n| # | Key | Chord | Notes (chord key) | Plate (9 white keys) |\n|---|---|---|---|---|\n",
                HARP_SET_NAME[s]);
        for (i = 0; i < HARP_NCH; i++) {
            harp_name(s, i, 0, nm);
            n = harp_chord(s, i, 0, nt);
            fprintf(f, "| %u | %s | %s | ", i + 1u, KEY[i], nm);
            for (k = 0; k < n; k++) fprintf(f, "%s%s", k ? " " : "", note_name(nt[k], b));
            harp_plate(s, i, 0, pl);
            fprintf(f, " | ");
            for (k = 0; k < HARP_NPLATE; k++) fprintf(f, "%s%s", k ? " " : "", note_name(pl[k], b));
            fprintf(f, " |\n");
        }
    }
    fclose(f);
    printf("harp: every chord -> %s\n", path);
}

/* (2026-11-10) GUITAR's tunings and shapes: all 14 tunings x 12 ROOTs x 6 shapes of each style's chords keep the chord's
 * pitch classes, six strings at most, in the fret bounds, low to high; standard SHAPE 0 stays the table's; timed */
static void check_tunings(void)
{
    uint32_t tn, sp, st, k, rt, i, nchk = 0, worst = 0;
    double tmax = 0;
    for (tn = 0; tn < GT_NTUN; tn++)
        for (rt = 0; rt < 12u; rt += (tn ? 5u : 1u))
            for (st = 0; st < GT_NSET; st += 7u)
                for (sp = 0; sp < GT_NSHP; sp++)
                    for (k = 0; k < 11u; k++) {
                        uint8_t nt[8];
                        uint32_t set = HARP_GT0 + st + GT_NSET * (sp + GT_NSHP * tn), c0 = GT_CH[st][k], pm = 0, got = 0, n;
                        uint32_t cr = (c0 / 21u + rt) % 12u, want = 0;
                        const uint8_t *tbl = GT_NOTES[c0 % 21u];
                        struct timespec a, b;
                        clock_gettime(CLOCK_MONOTONIC, &a);
                        n = gt_chord(set, k, (int32_t)rt, nt);
                        clock_gettime(CLOCK_MONOTONIC, &b);
                        if ((b.tv_sec - a.tv_sec) * 1e6 + (b.tv_nsec - a.tv_nsec) / 1e3 > tmax)
                            tmax = (b.tv_sec - a.tv_sec) * 1e6 + (b.tv_nsec - a.tv_nsec) / 1e3;
                        for (i = 0; i < 6u; i++)
                            if (tbl[i]) want |= 1u << ((tbl[i] + cr) % 12u);
                        for (i = 0; i < n; i++) {
                            got |= 1u << (nt[i] % 12u);
                            CHECK(i == 0 || nt[i] > nt[i - 1u], "tuning %u shape %u: not rising", tn, sp);
                        }
                        (void)pm;
                        if (tn || sp)
                            CHECK(n >= 3u && n <= 6u && got == want, "tuning %u shape %u style %u key %u ROOT %u: %u notes pcs %x want %x",
                                  tn, sp, st, k, rt, n, got, want);
                        nchk++;
                        worst = n < worst ? worst : n;
                    }
    { uint8_t a[8], b[8]; /* standard SHAPE 0 stays the table's voicing */
      CHECK(gt_chord(HARP_GT0, 0, 0, a) == 5u && a[0] == 48, "standard shape 0 changed");
      CHECK(gt_chord(HARP_GT0 + GT_NSET * 3u, 0, 0, b) >= 3u, "shape 3"); }
    printf("harp: %u guitar shapes checked, slowest search %.0f us on this host\n", nchk, tmax);
}

int main(void)
{
    check_tunings();
    const char *md = getenv("HARP_MD");
    check_all();
    check_known();
    if (md)
        write_md(md);
    printf(fails ? "harp: %d FAILED\n" : "harp: 16 sets x 12 chords x 12 ROOTs, the owner's readings: all ok\n", fails);
    return fails != 0;
}
