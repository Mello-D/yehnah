/* CPU test song generator (2026-11-09). Builds four song sections as FUND project records on the host and writes
 * them (3840 bytes each) plus the song's chain row (36 bytes); tools/measure/inject_song.py puts them into a backup file.
 * Build: cc -O1 -w -fpermissive -DYEHNAH_ANIMAL=1 -DYEHNAH_NOISE=1 -Ibuild/gen -Ifirmware/src -Itests tools/measure/gen_cpu_song.c -o gen_cpu_song -lm
 * Run:   gen_cpu_song OUTDIR   (writes sec_A.bin .. sec_D.bin, chain.bin) ;  gen_cpu_song list  (the engines' preset names)
 * The song (round 3): four 8-bar sections, staggered chords with long releases: A six engines every 8th step, B reference
 * (six engines every 4th), C four engines (JUNO FM6 PLAITS DRIFT), D JUNO on all six parts, looping. Earlier rounds'
 * sections: tools/measure/cpu_song_stagger_v1, cpu_song_round2. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
#include "../firmware/src/project.c"

static int cmp_u64(const void *x, const void *y) { return (*(const uint64_t *)x > *(const uint64_t *)y) - (*(const uint64_t *)x < *(const uint64_t *)y); }
static const uint8_t CH[4][4] = {{48, 51, 55, 58}, {44, 48, 51, 55}, {51, 55, 58, 62}, {46, 50, 53, 57}};

/* track k: engine e, preset pi (-1: by name), a chord every `every` steps (0: none), octave shift oct, sends, release */
static uint32_t stagger;                            /* B: part k starts its chords this many steps late (0: all together) */
static void part(uint32_t k, uint32_t e, const char *pname, uint32_t every, int oct, int send, int rel)
{
    track_t *t = &trk[k];
    uint32_t i, pi = 0, found = 0;
    for (i = 0; i < ENGINES[e]->npresets; i++)
        if (!strcmp(ENGINES[e]->presets[i].name, pname)) { pi = i; found = 1; }
    if (!found) { fprintf(stderr, "no preset %s on engine %u\n", pname, e); exit(1); }
    host_preset(t, e, pi);
    t->p[P_VOICE] = V_POLY;
    t->p[P_SLEN] = NSTEP;
    if (send >= 0) t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = (int16_t)send;
    if (rel >= 0) t->p[P_REL] = (int16_t)rel;
    for (i = stagger; every && i < NSTEP; i += every) {
        uint8_t n[4];
        uint32_t j;
        for (j = 0; j < 4u; j++) n[j] = (uint8_t)(CH[(i / 16u) % 4u][j] + oct);
        put_step(t, i, getenv("NNOTES") ? (uint32_t)atoi(getenv("NNOTES")) : 4u, n, ST_NOTE, 0);   /* NNOTES: a test, fewer notes a chord */
    }
}
static void drums(uint32_t k)                       /* DRUM: kick, snare, hats, a hit on every step */
{
    track_t *t = &trk[k];
    uint32_t i;
    host_preset(t, ENGI_DRUM, 0);
    t->p[P_SLEN] = NSTEP;
    for (i = 0; i < NSTEP; i++) {
        uint8_t n = i % 8u == 0 ? 36 : i % 8u == 4 ? 38 : i % 4u == 2 ? 46 : 42;
        put_step(t, i, 1, &n, ST_NOTE, i % 8u == 0 ? SF_ACCENT : 0);
    }
}
static void one_note(uint32_t k, uint32_t e, const char *pname)
{
    part(k, e, pname, 0, 0, -1, -1);
    uint8_t n = 36;
    for (uint32_t i = 0; i < NSTEP; i += 16u) put_step(&trk[k], i, 1, &n, ST_NOTE, 0);
}

/* B's six heavy parts, staggered; mask: bit k = part k sounds (0x3F all); send: the chorus / delay / reverb sends */
static void heavy(uint32_t mask, int send, uint32_t every, int juno6)
{
    static const struct { uint32_t e; const char *n; int oct; uint32_t st; } H[6] = {
        {ENGI_JUNO, "JUNO STRINGS", 0, 0}, {ENGI_HYBRID, "P5 STRINGS", 12, 1}, {ENGI_FM6, "STRINGS", 0, 2},
        {ENGI_PLAITS, "ADD PAD", 12, 3}, {ENGI_VOICE, "CHOIR AAH", 12, 2}, {ENGI_DRIFT, "DRIFT PAD", 0, 1}};
    uint32_t k;
    for (k = 0; k < 6u; k++)
        if (mask >> k & 1u) {
            stagger = H[k].st;                      /* staggered: the chords start on steps 0 1 2 3 (never all six together) */
            part(k, juno6 ? ENGI_JUNO : H[k].e, juno6 ? "JUNO STRINGS" : H[k].n, every, H[k].oct, send, 110);
        }
    stagger = 0;
}

/* (2026-11-09, round 3, on the FULL_VOICES 8 test firmware) A = B's six engines, a chord every 8th step (fewer steals),
 * B = the reference, C = JUNO + FM6 + DRIFT + PLAITS (4 engines), D = JUNO on all six parts (one engine, six parts).
 * Round 2 (no sends / two trios) is in tools/measure/cpu_song_round2, round 1 in cpu_song_stagger_v1. */
static void build(int s)
{
    uint32_t k;
    host_tracks_init();
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 0;
    song.g[G_BPM] = 140;
    switch (s) {
    case 0: heavy(0x3F, 90, 8, 0); break;           /* A: six engines, every 8th step */
    case 1: heavy(0x3F, 90, 4, 0); break;           /* B: the reference */
    case 2: heavy(0x2D, 90, 4, 0); break;           /* C: JUNO, FM6, PLAITS, DRIFT (parts 0, 2, 3, 5) */
    default: heavy(0x3F, 90, 4, 1); break;          /* D: JUNO x6 */
    }
}

/* project.c's project_capture (it needs the whole UI), from the host state: the globals, the six tracks, the chain, the
 * factory DRUM-X kit, each track's FM6 patch; no motion, no punch-in lane */
static void capture(project_t *p)
{
    uint32_t i, j;
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    for (i = 0; i < G_COUNT; i++) p->g[i] = song.g[i];
    p->sel = 0;
    p->parts = NPART;
    p->phys = PROJ_PHYS;
    p->chain = chain_config;
    proj_fm6_init(p);
    for (i = 0; i < NTRK; i++) {
        for (j = 0; j < P_COUNT; j++) p->t[i].p[j] = trk[i].p[j];
        p->t[i].engine = trk[i].eng_req;
        p->t[i].preset = trk[i].preset;
        memcpy(p->t[i].step, trk[i].step, sizeof trk[i].step);
        fm6_pack(fm6_patch[i], p->fm6[i]);
    }
    p->sum = proj_sum(p);
}

int main(int argc, char **argv)
{
    uint32_t e, k;
    if (argc > 1 && !strcmp(argv[1], "list")) {
        for (e = 0; e < NENGINES; e++) {
            printf("%u:", e);
            for (k = 0; k < ENGINES[e]->npresets; k++) printf(" %u=%s", k, ENGINES[e]->presets[k].name);
            printf("\n");
        }
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "run")) {      /* run S [SECS]: section S (0..3) played by the sequencer on the host:
                                                     * voices at once, render time per block (relative; the host is not the device) */
        int s = atoi(argv[2]);
        uint32_t secs = argc > 3 ? (uint32_t)atoi(argv[3]) : 16u, nblk = secs * FS / CTL, kk, bm = 0, hist[33] = {0};
        uint64_t tot = 0, mx = 0, *dur = calloc(nblk, sizeof *dur);
        int32_t o[2 * CTL];
        build(s);
        transport_req = 1;
        for (kk = 0; kk < nblk; kk++) {
            uint64_t t0 = now_ns(), d;
            mix_block(o, CTL);
            d = now_ns() - t0;
            dur[kk] = d;
            if (kk >= nblk / 4u) { tot += d; if (d > mx) mx = d; }      /* the first quarter settles */
            if (busy_now() > bm) bm = busy_now();
            hist[busy_now() > 32u ? 32u : busy_now()]++;
        }
        {   /* the spread of the block times (host noise adds evenly): p50 / p90 / p99 / p99.9, us */
            uint32_t n = nblk - nblk / 4u;
            uint64_t *v = dur + nblk / 4u, t;
            qsort(v, n, sizeof *v, cmp_u64);
            printf("  block us: p50 %.1f  p90 %.1f  p99 %.1f  p99.9 %.1f\n", v[n / 2] / 1e3, v[n * 9 / 10] / 1e3, v[n * 99 / 100] / 1e3, v[n * 999 / 1000] / 1e3);
        }
        printf("section %c: max voices %u, avg %.1f us / block, worst %.1f us (block = %.0f us)  voice_kills %u\n", 'A' + s, bm,
               (double)tot / (nblk - nblk / 4u) / 1000.0, mx / 1000.0, 1e6 * CTL / FS, (unsigned)voice_kills);
        return 0;
    }
    if (argc > 4 && !strcmp(argv[1], "clicks")) {   /* clicks S K SECS [PRESET]: part K of section S alone (the others' LEVEL 0); counts the sample
                                                     * jumps: |first difference| over 8x the mean of the 64 before it and over 40 (a click) */
        int s = atoi(argv[2]);
        uint32_t kq = (uint32_t)atoi(argv[3]), nblk = (uint32_t)atoi(argv[4]) * FS / CTL, kk, j, n = 0, spikes = 0;
        int32_t o[2 * CTL], prev = 0;
        double ma = 0, mx = 0, rms = 0;
        build(s);
        if (argc > 5) {                                 /* PRESET: another preset name on that part's engine */
            uint32_t e = trk[kq].eng_req, pi;
            for (pi = 0; pi < ENGINES[e]->npresets; pi++)
                if (!strcmp(ENGINES[e]->presets[pi].name, argv[5])) {
                    uint32_t keep_send = trk[kq].p[P_REV], st[NSTEP * sizeof(step_t)];
                    host_preset(&trk[kq], e, pi);
                    trk[kq].p[P_VOICE] = V_POLY; trk[kq].p[P_SLEN] = NSTEP; trk[kq].p[P_CHOR] = trk[kq].p[P_DLY] = trk[kq].p[P_REV] = (int16_t)keep_send; trk[kq].p[P_REL] = 110;
                }
        }
        for (j = 0; j < NTRK; j++) if (j != kq) trk[j].p[P_LEVEL] = 0;
        transport_req = 1;
        for (kk = 0; kk < nblk; kk++) {
            mix_block(o, CTL);
            if (kk < FS / CTL) continue;                 /* the first second settles */
            for (j = 0; j < CTL; j++) {
                int32_t x = o[2 * j], d = x - prev;
                double ad = d < 0 ? -d : d;
                if (n > 64 && ad > 40.0 && ad > 8.0 * ma) spikes++;
                ma += (ad - ma) / 64.0;
                prev = x; n++;
                rms += (double)x * x; if (ad > mx) mx = ad;
            }
        }
        printf("(kills %u) ", (unsigned)voice_kills);
        printf("section %c part %u %s: %u jumps in %.0f s, largest step %.0f, rms %.0f\n", 'A' + s, kq + 1,
               argc > 5 ? argv[5] : ENGINES[trk[kq].eng_req]->presets[trk[kq].preset].name, spikes, (double)n / FS, mx, sqrt(rms / n));
        return 0;
    }
    if (argc < 2) { fprintf(stderr, "usage: gen_cpu_song OUTDIR | list\n"); return 1; }
    {
        static const uint8_t BARS[4] = {8, 8, 8, 8};
        static const char *const NAME[4] = {"B EVERY 8", "CPU TEST B", "4 ENGINES", "JUNO X6"};
        static project_store_t st; static project_t pj;
        char path[512];
        int s;
        chain_defaults(&chain_config);
        chain_config.count = 4;
        for (s = 0; s < 4; s++) { chain_config.row[s].slot = (uint8_t)s; chain_config.row[s].bars = BARS[s]; }
        SONG_END(&chain_config) = SONG_FOREVER;
        for (s = 0; s < 4; s++) {
            FILE *f;
            build(s);
            capture(&pj);
            memset(pj.name, 0, sizeof pj.name);
            memcpy(pj.name, NAME[s], strlen(NAME[s]));
            pj.sum = proj_sum(&pj);
            memset(&st, 0, sizeof st);
            if (!proj_pack(&st, &pj)) { fprintf(stderr, "section %d: proj_pack failed (full %d)\n", s, proj_full); return 1; }
            if (!proj_import(&pj, &st, sizeof st)) { fprintf(stderr, "section %d: re-import failed\n", s); return 1; }
            snprintf(path, sizeof path, "%s/sec_%c.bin", argv[1], 'A' + s);
            f = fopen(path, "wb"); fwrite(st.raw, 1, sizeof st.raw, f); fclose(f);
            printf("section %c: packed ok, %u bytes\n", 'A' + s, (unsigned)sizeof st.raw);
        }
        snprintf(path, sizeof path, "%s/chain.bin", argv[1]);
        { FILE *f = fopen(path, "wb"); fwrite(&chain_config, 1, sizeof chain_config, f); fclose(f); }
        printf("chain: %u bytes\n", (unsigned)sizeof chain_config);
    }
    return 0;
}
