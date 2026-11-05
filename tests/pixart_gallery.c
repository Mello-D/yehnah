/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork, 2026-10-28) The engines' robots (ui_robot.c) as the device draws them, for the user to look at, and
 * the port checked against their design page: every engine's EDIT 1 with SCOPE OFF (the robot on the whole panel), HOME
 * (the rest stop, each engine the selected track's, the song playing, a beat each) and each robot in the page's four
 * reference states (tools/robots/export_parts.mjs: OUTDIR/ref, compared by tools/robots/check_port.py). Built on
 * tests/ui_render.c (its stubs, scenes and write_ppm), its main left out:
 *   cc -O1 -w '-DYEHNAH_VERSION="dev"' -DYEHNAH_ANIMAL=1 -DYEHNAH_NOISE=1 -Ibuild/gen -Ifirmware/src -Itests \
 *      -o build/pg/pixart_gallery tests/pixart_gallery.c -lm
 *   pixart_gallery OUTDIR ROAD          -> HOME's road (road_dump)
 *   pixart_gallery OUTDIR [PALETTE ..]   -> OUTDIR/ppm/<PALETTE>_full_<NN>_<ENGINE>.ppm, <PALETTE>_home_<NN>_<ENGINE>.ppm,
 *                                           OUTDIR/ref/<ROBOT>_<STATE>.ppm (30 x 34, black where nothing),
 *                                           OUTDIR/index.tsv (file, palette, kind, engine); default Yehnah PAPER */
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"

static int pal_by_name(const char *n)
{
    uint32_t p;
    for (p = 0; p < NPALETTES; p++)
        if (!strcmp(UI_PALETTES[p].name, n)) return (int)p;
    return -1;
}
static void slug(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; *s && i + 1 < n; s++) d[i++] = (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ? *s : '-';
    d[i] = 0;
}

/* the page's reference states (export_parts.mjs STATES) */
static void ref_dump(const char *out)
{
    static const rb_in_t ST[4] = {{0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0}, {1234.0f, 1.0f, 1.0f, 2.1f, 7, 0, 3, 0},
                                  {2600.0f, 0.45f, 0.6f, 4.4f, 4, 1, 7, 0}, {5021.0f, 0.2f, 0.3f, 5.5f, 9, 0, 12, 1}};
    uint32_t r, k, x, y;
    char path[600];
    for (r = 0; r < RB_NROBOTS; r++)
        for (k = 0; k < 4u; k++) {
            FILE *f;
            snprintf(path, sizeof path, "%s/ref/%u_%u.ppm", out, r, k);
            if (!(f = fopen(path, "wb"))) { fprintf(stderr, "cannot write %s\n", path); return; }
            rb_build(r, &ST[k]);
            fprintf(f, "P6\n%d %d\n255\n", RB_W, RB_H);
            for (y = 0; y < RB_H; y++)
                for (x = 0; x < RB_W; x++) {
                    uint32_t v = rb_px[y][x], c = v ? rb_col(&RB_ROBOTS[r], v) : 0u;
                    uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
                    fwrite(b, 1, 3, f);
                }
            fclose(f);
        }
}
/* (2026-10-29) HOME's road (ui_road.c): OUTDIR/ppm/ROAD_<row>_<col>.ppm. Rows 0..4 the loops (a day is 5/4 of one, so
 * (2026-11-03: the cyberpunk street) rows 0, 1 dusk, night, columns 0..2 the street's thirds; row 2 the turns (dusk to
 * night, back); row 5 the pivot (the song playing, the beats whose move has a foot up) and the walk (stopped) */
static void road_frame(uint32_t e, uint32_t way, int play, uint32_t beat, const char *out, uint32_t r, uint32_t c)
{
    char nm[32];
    state(); pal(0);
    load_depth++;
    set_engine_of(&trk[0], e);
    apply_preset_to(&trk[0], 0);
    trk[0].engine = trk[0].eng_req;
    load_depth--;
    song.sel = 0;
    vx_run(40);
    if (play) { trk_note_on(&trk[0], 57, 110); trk[0].peak = 7000; vx_run(30); }
    else { song.playing = 0; fm1_ms += 3000u; }
    rbs.beat = beat, vxs.nn[VX_K(&trk[0])] = beat;   /* (the moves: one a note) */
    rd.pos = way << 8, rd.ms = fm1_ms;
    draw(S_HOME);
    snprintf(nm, sizeof nm, "%u_%u", r, c);
    write_ppm(out, "ROAD", nm);
}
/* (2026-10-29) SAVE > MORPH's track blend on HOME: T1 (engine ea) and T2 (eb), BLEND x: the two robots, the bigger
 * share in front */
static void road_blend(uint32_t ea, uint32_t eb, int32_t x, int play, const char *out, uint32_t c)
{
    char nm[32];
    uint32_t k;
    state(); pal(0);
    load_depth++;
    set_engine_of(&trk[0], ea); apply_preset_to(&trk[0], 0); trk[0].engine = trk[0].eng_req;
    set_engine_of(&trk[1], eb); apply_preset_to(&trk[1], 0); trk[1].engine = trk[1].eng_req;
    load_depth--;
    song.sel = 0;
    song.g[G_BLA] = 1; song.g[G_BLB] = 2; song.g[G_BLEND] = (int16_t)x; song.g[G_BMOVE] = 0;
    vx_run(40);
    if (play) { trk_note_on(&trk[0], 57, 110); trk[0].peak = trk[1].peak = 7000; vx_run(30); }
    else { song.playing = 0; fm1_ms += 3000u; }
    rbs.beat = 3;
    rd.pos = 150u << 8, rd.ms = fm1_ms;
    draw(S_HOME);
    snprintf(nm, sizeof nm, "7_%u", c);
    write_ppm(out, "ROAD", nm);
    for (k = G_BLA; k <= G_BMOVE; k++) song.g[k] = GP[k].def;
    vx_run(1);
}
static void road_dump(const char *out)
{
    uint32_t m, c, b, n = 0;
    for (m = 0; m < 2u; m++)
        for (c = 0; c < 3u; c++)
            road_frame(0, m * RD_LOOP + c * 120u, 1, 3, out, m, c);
    road_frame(0, 324u, 1, 3, out, 2, 0);
    road_frame(0, 684u, 1, 3, out, 2, 1);
    for (b = 0; b < 400u && n < 6u; b++) {             /* (rb_in's pick: seed track 0, robot ANALOG 0) */
        uint32_t i = rb_hsh(b * 31u) % RB_NPOSES;
        if (i == rb_hsh((b - 1u) * 31u) % RB_NPOSES) i = (i + 5u) % RB_NPOSES;
        if (RB_POSE[i].lift) road_frame(0, 120u, 1, b, out, 5, n++);
    }
    for (c = 0; c < 4u; c++)
        road_frame(0, 240u + c * 2u, 0, 0, out, 6, c);
    road_blend(ENGI_ANALOG, ENGI_JUNO, 30, 1, out, 0);   /* (A in front) */
    road_blend(ENGI_ANALOG, ENGI_JUNO, 100, 1, out, 1);  /* (B in front) */
    road_blend(ENGI_PLAITS, ENGI_ANALOG, 64, 0, out, 2);
    road_blend(ENGI_ANALOG, ENGI_DRUM, 90, 1, out, 3);
    state(); pal(0);                                    /* (2026-11-03) the splash: the name half written, whole */
    spl_anim = 1;
    splash_scene(SPL_WRITE / 2u); write_ppm(out, "ROAD", "8_0");
    splash_scene(SPL_WRITE + SPL_HOLD - 1u); write_ppm(out, "ROAD", "8_1");
    spl_anim = 0;
}
int main(int argc, char **argv)
{
    static const char *const DEF[] = {"Yehnah", "PAPER"};
    const char *out = argc > 1 ? argv[1] : "build/pixart";
    const char *const *pn = argc > 2 ? (const char *const *)argv + 2 : DEF;
    if (argc > 2 && !strcmp(argv[2], "ROAD")) {
        road_dump(out);                               /* (OUTDIR/ppm made first) */
        return 0;
    }
    uint32_t np = argc > 2 ? (uint32_t)(argc - 2) : 2u, i, e, show[NENGINES], ns = 0;
    char path[600];
    FILE *ix;
    snprintf(path, sizeof path, "%s/report.txt", out);
    rep = fopen(path, "w");                               /* (ui_render's lint writes here; not read) */
    snprintf(path, sizeof path, "%s/index.tsv", out);
    ix = fopen(path, "w");
    if (!rep || !ix) { fprintf(stderr, "cannot write in %s (and %s/ppm, %s/ref)\n", out, out, out); return 1; }
    for (e = 0; e < NENGINES; e++)
        if (eng_ok(e) && ENGINES[e]->npresets) show[ns++] = e;
    for (i = 0; i < np; i++) {
        int p = pal_by_name(pn[i]);
        uint32_t g;
        if (p < 0) { fprintf(stderr, "no palette %s\n", pn[i]); continue; }
        for (g = 0; g < ns; g++) {
            char nm[64], sl[32];
            e = show[g];
            slug(sl, ENGINES[e]->name, sizeof sl);
            state(); pal((uint32_t)p);                    /* EDIT 1: the whole panel */
            v6_scene(e, ENGINES[e]->presets[0].name, "EDIT 1", 0, 57, 300, 3 + g);
            draw(S_V6_ANALOG_FULL);
            snprintf(nm, sizeof nm, "full_%02u_%s", e, sl);
            write_ppm(out, pn[i], nm);
            fprintf(ix, "%s_%s.ppm\t%s\tfull\t%s\n", pn[i], nm, pn[i], ENGINES[e]->name);
            state(); pal((uint32_t)p);                    /* HOME: its robot the selected track's, dancing */
            load_depth++;
            set_engine_of(&trk[0], e);
            apply_preset_to(&trk[0], 0);
            trk[0].engine = trk[0].eng_req;
            load_depth--;
            song.sel = 0;
            vx_run(40);
            trk_note_on(&trk[0], 57, 110); trk[0].peak = 7000;
            vx_run(30);
            rbs.beat = g * 3u, vxs.nn[VX_K(&trk[0])] = g * 3u;
            song.octave = 2;
            draw(S_HOME);
            snprintf(nm, sizeof nm, "home_%02u_%s", e, sl);
            write_ppm(out, pn[i], nm);
            fprintf(ix, "%s_%s.ppm\t%s\thome\t%s\n", pn[i], nm, pn[i], ENGINES[e]->name);
        }
    }
    fclose(ix);
    ref_dump(out);
    printf("pixart_gallery: %u engines, %u palette(s) in %s/ppm, %u robots in %s/ref\n", ns, np, out, RB_NROBOTS, out);
    return 0;
}
