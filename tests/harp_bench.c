/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp) HARP's cost and voices, a report (phase 6): on each engine HARP plays on (not DRUM's kit), its first
 * preset on T1 alone (the other parts muted, no sends), MIN8's G7(b9,b13) (six notes, key 11) held, then three plate
 * keys over it, under LOAD ECO / LITE / FULL. Counted: the voices that sound (gated) against the shared budget
 * (voice.c voice_cap) and how many of the chord's six are still there with the plate on; timed (host ns per sample,
 * for comparison between rows only: the device's own is GLO > SYSTEM's CPU) silence, one note, the chord, the chord
 * with the plate. Fails if a chord alone sounds fewer notes than the part's cap allows (voice.c trk_nvoice: the
 * engine's poly, FM6 2 under LITE / ECO, 4 a part under ECO) or the voices pass it. HARP_REPORT=file writes the table
 * as Markdown */
#include <assert.h>
#include <time.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t ob[2 * CTL];
static double ns_per_sample(uint32_t blocks)        /* the audio path's time a sample, these blocks */
{
    struct timespec a, b;
    uint32_t i;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < blocks; i++) {
        events_block(CTL);
        mix_block(ob, CTL);
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    return ((double)(b.tv_sec - a.tv_sec) * 1e9 + (double)(b.tv_nsec - a.tv_nsec)) / (double)(blocks * CTL);
}
static void keys(uint32_t m)
{
    fm1_in.notes = m;
    keyboard_block();
}
static uint32_t gated(const track_t *t, const uint8_t *nt, uint32_t n, uint32_t *of)   /* voices gated; *of: of nt */
{
    uint32_t i, k, c = 0;
    *of = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate) {
            c++;
            for (k = 0; k < n && nt[k] != t->v[i].note; k++)
                ;
            *of += k < n;
        }
    return c;
}

int main(void)
{
    static const char *const LD[3] = {"FULL", "LITE", "ECO"};   /* (voice.c LOAD_FULL LOAD_LITE LOAD_ECO) */
    track_t *t = &trk[0];
    uint8_t chord[HARP_NMAX];
    uint32_t n, i, e, ld, k, bad = 0, plate = 1u << 12 | 1u << 14 | 1u << 16, of;
    const char *path = getenv("HARP_REPORT");
    FILE *md = path ? fopen(path, "w") : 0;
    host_tracks_init();
    kb_prev = 0;
    for (k = 1; k < NTRK; k++)
        trk[k].p[P_MUTE] = 1;
    n = harp_chord(15, 10, 0, chord);
    assert(n == 6u);
    if (md)
        fprintf(md, "| LOAD | engine (preset) | part cap of budget | chord: voices (of 6) | + 3 plate: voices, chord's left | "
                    "ns/sample: silence / 1 note / chord / chord + plate |\n|---|---|---|---|---|---|\n");
    for (ld = 0; ld < 3u; ld++) {
        sys_load = (uint8_t)ld;
        for (i = 0; i < NENG_SHOWN; i++) {
            uint32_t vc, vp, cof, cap;
            double s0, s1, sc, sp;
            e = eng_vis(i);
            if (ENGINES[e]->oneshot || (ENGINES[e]->keys && ENGINES[e]->keys(t, 0) >= 0))
                continue;                               /* (DRUM: the keys are the kit's, no HARP) */
            host_preset(t, e, 0);
            t->engine = t->eng_req;
            t->p[P_VOICE] = V_POLY;
            t->p[P_CHRD] = CD_HARP;
            t->p[P_CVOI] = 15;
            t->p[P_ROOT] = 0;
            t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
            keys(0);
            ns_per_sample(64);                          /* (what sounded before: gone) */
            s0 = ns_per_sample(400);
            keys(1u << 12);                             /* one note: a plate key alone */
            s1 = ns_per_sample(400);
            keys(0);
            ns_per_sample(64);
            keys(1u << 10);                             /* the chord (key 11) */
            ns_per_sample(16);
            vc = gated(t, chord, n, &cof);
            sc = ns_per_sample(400);
            keys(1u << 10 | plate);                     /* and three plate keys over it */
            ns_per_sample(16);
            vp = gated(t, chord, n, &of);
            sp = ns_per_sample(400);
            keys(0);
            ns_per_sample(64);
            cap = trk_nvoice(t) < voice_cap() ? trk_nvoice(t) : voice_cap();   /* (the part's own cap, the budget) */
            if (vc > cap || vp > cap || cof != (n < cap ? n : cap)) {
                printf("harp bench: %s %s: chord %u of %u, with the plate %u voices (cap %u)\n", LD[ld],
                       ENGINES[e]->name, cof, n, vp, cap);
                bad++;
            }
            printf("harp bench: %-4s %-7s %-12s cap %u of %2u: chord %u/6, +plate %u voices (chord %u left); ns/sample "
                   "%.0f / %.0f / %.0f / %.0f\n", LD[ld], ENGINES[e]->name, ENGINES[e]->presets[t->preset].name,
                   trk_nvoice(t), voice_cap(), cof, vp, of, s0, s1, sc, sp);
            if (md)
                fprintf(md, "| %s | %s (%s) | %u of %u | %u | %u, %u | %.0f / %.0f / %.0f / %.0f |\n", LD[ld],
                        ENGINES[e]->name, ENGINES[e]->presets[t->preset].name, trk_nvoice(t), voice_cap(), cof, vp, of,
                        s0, s1, sc, sp);
        }
    }
    if (md)
        fclose(md);
    if (bad) {
        printf("harp bench: %u FAILED\n", bad);
        return 1;
    }
    puts("harp bench: voices within the caps, every chord as whole as its part's cap allows: ok");
    return 0;
}
