/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) HOME's lens (ui_lens.c): its panels in the lens's panel (240 x 140 at y 84). Included by ui_graph.c after
 * ui_being.c. (chord-harp fork) TRACKS draws the road (graph_tracks); MACRO (was ROUTES; 2026-11-02: ENGINE gone)
 * M1..M4 down the left with their routes drawn to their destinations ("T1 CUT+40"). (2026-10-26) Yehnah's beings are gone: the ECO
 * swim (0.8.8, its faces 0.9.7) and the beings drawn where an engine had no picture. eco_step still runs each track's
 * level, time and swim (the pictures and the triangles read the levels).
 * Nothing in the audio path: the parts' peaks and the scope the UI already reads. */
#define ECO_N NTRK
static const int16_t ECO_PX[ECO_N] = {44, 120, 196, 64, 146, 206}, ECO_PY[ECO_N] = {34, 28, 40, 84, 78, 86};   /* (where they
                                                         * start, as 0.8.4's HOME) */
static struct {
    int32_t en[ECO_N], pen[ECO_N];                      /* each track's level now (0..256, slewed), the frame before */
    int32_t sx[ECO_N], sy[ECO_N], vx[ECO_N], vy[ECO_N];   /* the swim: where it is in the panel and its speed, Q8 px */
    int32_t r[ECO_N];                                   /* its radius now (Q4 px: swollen, pulsing) */
    uint32_t tm[ECO_N];                                 /* each being's time (it runs with its sound) */
    uint32_t f0;
    uint8_t init;
} eco;
/* (0.9.7, variant A) each being's face and colours, worked out once a drawn frame by eco_step: its three isotherms'
 * colours by its level (c0 outer .. c2), its last hit's time (fm1_ms, low 16 bits), its face's EF_* (fl); init: the hits'
 * times set. (Sized as the 0.9.6 critters were, in their place: the RAM's layout and so the sound code's addressing as
 * they were; EH_N the beings' room, ECO_N used.) (chord-harp fork, 2026-10-26) No face is drawn any more; kept, and still
 * written, so the RAM's layout stays as it was */
#define EH_N 36u
static struct {
    uint16_t c0[EH_N], c1[EH_N], c2[EH_N], ms[EH_N];
    uint8_t fl[EH_N];
    uint8_t init;
} eco_fc;
enum { EF_SHUT = 1, EF_SLEEP = 2, EF_UP = 4 };          /* (0.9.7) the face's: eyes shut (a hit), asleep, looking up */
static int eco_off(uint32_t c) { return trk[c].p[P_MUTE] || c >= trk_live; }   /* (muted, or asleep: TRACKS 4) */
/* (0.5, as 0.8.4's HOME: graph_ecosys's life) the beings once a drawn frame (dt frames since the last): each
 * track's level (the part's peak, slewed: a hit at once, a quick fall), its time, its size (it pulses, and swells up to
 * ~2.2 x; the selected one larger), its swim over the whole panel (wandering, bouncing off the walls, a hit kicking it
 * on up to ~7 x). Still with ANIM OFF */
/* (0.8.9) the beings' water: the panel (COLORS 232 x 140, BOTTOM 240 x 148), HUD's HOME the whole screen; its floor, the
 * numbers' line under it (HUD: the numbers only in the middle, the floor near the screen's foot) */
static int32_t eco_h(void) { return (int32_t)graph_h(); }
static int32_t eco_bot(void) { return eco_h() - 18; }
static uint32_t eco_step(void)
{
    uint32_t c, k, dt = ui.frame - eco.f0;
    int32_t bot = eco_bot();                            /* (the panel's floor, the number's line under it: 0.8.4's 104 of
                                                         * 122, the lens's 140 its 122) */
    if (!eco.init) {
        eco.init = 1;
        for (c = 0; c < ECO_N; c++) {
            eco.sx[c] = ECO_PX[c] << 8; eco.sy[c] = ECO_PY[c] << 8;
            eco.vx[c] = (c & 1u) ? 90 : -110; eco.vy[c] = (c & 2u) ? 70 : -60;
        }
    }
    if (dt > 8u) dt = 8u;
    eco.f0 = ui.frame;
    for (c = 0; c < ECO_N; c++) {
        track_t *t = &trk[c];
        int32_t v = t->peak * 256 / 7000, r = (c == song.sel ? 17 : 12) * 16, rad, sp;
        if (v > 256) v = 256;
        eco.pen[c] = eco.en[c];
        if (dt) {                                       /* (once a frame: a redraw in the same frame draws the same) */
            t->peak = 0;
            eco.en[c] = v > eco.en[c] ? v : eco.en[c] + (v - eco.en[c]) / 4;
            if (eco.en[c] - eco.pen[c] > 48)            /* (0.9.7) a hit: the face's eyes shut for 0.1 s */
                eco_fc.ms[c] = (uint16_t)fm1_ms;
        }
        if (!eco_fc.init || (uint16_t)((uint16_t)fm1_ms - eco_fc.ms[c]) > 1000u)   /* (none yet, or long ago: kept so, the
                                                         * 16 bits never wrapping round to a hit) */
            eco_fc.ms[c] = (uint16_t)(fm1_ms - 1000u);
        {   /* (0.9.7) its isotherms' colours: cyan / blue quiet .. red, orange, yellow loud; the inner ones warmer */
            int32_t h = 23 + eco.en[c] * 118 / 256, st = 38 + eco.en[c] * 13 / 256;
            eco_fc.c0[c] = vk_heat(h);
            eco_fc.c1[c] = vk_heat(h + st);
            eco_fc.c2[c] = vk_heat(h + 2 * st);
        }
        eco_fc.fl[c] = (uint8_t)((mut_flash[c] ? EF_UP : 0u) |
                                 ((uint16_t)((uint16_t)fm1_ms - eco_fc.ms[c]) < 100u && eco.en[c] > 77 ? EF_SHUT : 0u));
        if (eco.en[c] > 2) eco.tm[c] += dt * (uint32_t)(200 + eco.en[c] * 5);
        else eco.tm[c] += dt * 60u;                     /* (silent: it still breathes, slowly) */
        r = r * (256 + eco.en[c] * 5 / 4 + (bg_sin(eco.tm[c] / 3u) >> 11)) / 256;
        eco.r[c] = r;
        rad = r >> 4;
        sp = 256 + eco.en[c] * 6;
        for (k = 0; k < dt; k++) {                      /* swimming in the panel, bouncing off its walls, wandering */
            eco.vx[c] += bg_sin(ui.frame * 211u + c * 16000u + k * 977u) >> 12;
            eco.vy[c] += bg_sin(ui.frame * 157u + c * 23000u + k * 613u) >> 12;
            eco.vx[c] = clamp(eco.vx[c], -140, 140);
            eco.vy[c] = clamp(eco.vy[c], -110, 110);
            eco.sx[c] += eco.vx[c] * sp >> 8;
            eco.sy[c] += eco.vy[c] * sp >> 8;
            if (eco.sx[c] < (rad + 8) << 8) { eco.sx[c] = (rad + 8) << 8; eco.vx[c] = -eco.vx[c]; }
            if (eco.sx[c] > (232 - rad) << 8) { eco.sx[c] = (232 - rad) << 8; eco.vx[c] = -eco.vx[c]; }
            if (eco.sy[c] < (rad + 4) << 8) { eco.sy[c] = (rad + 4) << 8; eco.vy[c] = -eco.vy[c]; }
            if (eco.sy[c] > (bot - rad) << 8) { eco.sy[c] = (bot - rad) << 8; eco.vy[c] = -eco.vy[c]; }
        }
    }
    eco_fc.init = 1;
    return dt;
}

/* (0.8.5) a gauge, as the mock-up's: an arc of 270 degrees (from 7:30, clockwise) round (cx, cy) px, radius r, w px
 * thick, the value f (0..1000) in vc over the track tc; antialiased lines (og_line), 24 a turn */
static void lz_arc(int32_t cx, int32_t cy, int32_t r, int32_t w, int32_t f, uint16_t tc, uint16_t vc)
{
    int32_t k, i;
    for (k = 0; k < w; k++) {
        int32_t rr = (r - k) * 16, px = 0, py = 0;
        for (i = 0; i <= 18; i++) {                     /* (18 segments over 270 degrees) */
            uint32_t a = 0x6000u + (uint32_t)i * 0xC000u / 18u;   /* (from 135 degrees, y down) */
            int32_t x = cx * 16 + (og_cos(a) * rr >> 15), y = cy * 16 + (og_sin(a) * rr >> 15);
            if (i)
                og_line(px, py, x, y, i * 1000 <= f * 18 ? vc : tc);
            px = x;
            py = y;
        }
    }
}

static void graph_home_being(void)
{
    (void)eco_step();
    (void)bg_energy();
    {                                                   /* MACRO: the macros and their routes (the user's,
                                                         * 2026-10-28: no picture; 2026-11-02: ENGINE gone) */
        uint32_t m, i, j, row = 0;
        int32_t ry[12];
        for (m = 0; m < 4u; m++) {
            int32_t y = 16 + (int32_t)m * 34, n = 0;
            char mn[3] = {'M', (char)('1' + m), 0};
            uint16_t col = ui.hot_t && ui.hot_col == m ? T_ACCENT : T_THEME;
            for (i = 0; i < NTRK; i++)
                for (j = 0; j < 4u && row < 12u; j++) {
                    const int16_t *p = &trk[i].p[P_M1SRC + 3u * j];
                    char e[20] = {'T', (char)('1' + i), ' ', 0};
                    int32_t ty;
                    if (p[0] != (int16_t)(MS_M1 + m) || !p[1] || !p[2] || n >= 3)
                        continue;
                    ty = 6 + (int32_t)row * 11;
                    ry[row++] = ty;
                    n++;
                    str_cpy(e + 3, mod_dst_name(&trk[i], p[1]), sizeof e - 3);
                    str_cpy(e + str_len(e), p[2] > 0 ? "+" : "", sizeof e - str_len(e));
                    fmt_int(e + str_len(e), p[2] * 100 / 64);
                    og_quad_a(34 * 16, y * 16, 120 * 16, y * 16, 168 * 16, (ty + 4) * 16, heat_col(70 + macro_v[m] * 150 / 127),
                              14u, 200);
                    cv_clip_text(171, ty, &AF_X, e, T_MID, T_SURF, 66);
                }
            og_node(22 * 16, y * 16, 12, T_RAISE);
            lz_arc(22, y, 10, 2, macro_v[m] * 1000 / 127, T_LINE, col);
            GFX_HOOK_ALIGN(12, y - 5, 32, y + 5, AL_HV, "ROUTES macro name in its knob");
            cv_text_in(12, y - 4, 20, &AF_X, mn, T_TEXT, T_RAISE);
            if (!n)
                cv_text(40, y - 4 - AF_X_CAP_Y + 1, &AF_X, "NO ROUTE", T_DIM);
        }
        (void)ry;
    }
}
/* (chord-harp fork, 2026-10-28, the user's design tools/robots/engine_robots.html; was the tiles, 2026-10-25) TRACKS and
 * MACRO: the selected track's robot dancing under a street lamp at a desert rest stop at night, its 16 steps along the
 * foot of the panel (ui_robot.c rb_home). HOME_TRIANGLES=1 builds the triangles instead (kept in reserve) */
#ifndef HOME_TRIANGLES
#define HOME_TRIANGLES 0
#endif
#if !HOME_TRIANGLES
static void graph_tracks(void)
{
    uint32_t c = song.sel % NTRK;
    (void)eco_step();                                   /* (each track's level: eco.en) */
    rd_lv = eco.en;                                     /* (the road's visualiser) */
    rb_home(eco_off(c) ? 0 : eco.en[c]);
}
#else
/* (chord-harp fork) TRACKS, in ECO's place (mockups/home_triangles.html, v3): the panel cut as a zigzag into one triangle
 * a track (TRACKS 4 / 6: all the same size; their bases alternate top and bottom), each in its track's colour (the four
 * knobs', THEME, the accent paled), its outline a pen's line (the selected track's bolder, a muted or asleep one dim).
 * Round each, inset to an even margin (towards its incentre), the 16 steps of the page the track is playing: a faint
 * ring an empty step, a dot a step with notes, the playhead ringed, a step sounding flashing bright and swelling. No
 * text: the track's number is the header's chip, its sound the header's centre, its length and page the footer's */
static struct {
    uint8_t fl[NTRK][16];                               /* each step's flash (256 the hit, falling a frame) */
    uint8_t last[NTRK];                                 /* the step that flashed last */
    uint32_t f0;
} trs;
static uint16_t tr_col(uint32_t t)
{
    return t < 4u ? ux.k[t] : t == 4u ? T_THEME : ux_mix(T_ACCENT, T_TEXT, 35);
}
/* a pen's line (the mock-up's): a little wobble across it, a fainter second pass beside it; Q0 px floats, al 0..256 */
static void tr_pen(float x0, float y0, float x1, float y1, uint16_t c, uint32_t seed, int32_t al)
{
    float dx = x1 - x0, dy = y1 - y0, l = fm_sqrtf(dx * dx + dy * dy), nx = -dy / l, ny = dx / l;
    uint32_t n = (uint32_t)(l * (1.0f / 6.0f)) + 2u, i, pass;
    for (pass = 0; pass < 2u; pass++) {
        int32_t px = 0, py = 0;
        for (i = 0; i <= n; i++) {
            float u = (float)i / (float)n, w = el_sin(u * 7.1f + (float)seed * 3.7f + (float)pass) * 0.9f +
                                                 el_sin(u * 17.0f + (float)seed) * 0.35f;
            int32_t x = (int32_t)((x0 + dx * u + nx * w + (pass ? 0.6f : 0.0f)) * 16.0f);
            int32_t y = (int32_t)((y0 + dy * u + ny * w + (pass ? 0.4f : 0.0f)) * 16.0f);
            if (i)
                og_line_a(px, py, x, y, c, pass ? al * 45 / 100 : al);
            px = x;
            py = y;
        }
    }
}
/* the triangles of n tracks in x0..x1, y0..y1 (the mock-up's split): v[k] its three corners (x, y) */
static uint32_t tr_split(uint32_t n, float x0, float y0, float x1, float y1, float (*v)[6])
{
    uint32_t kt = n / 2u, kb = n - kt, i = 0, j = 0, k = 0;
    float w = x1 - x0;
    while (i < kt || j < kb) {
        float ti = x0 + w * (float)i / (float)kt, ti1 = x0 + w * (float)(i + 1u) / (float)kt;
        float bj = x0 + w * (float)j / (float)kb, bj1 = x0 + w * (float)(j + 1u) / (float)kb;
        if (j < kb && (i >= kt || bj1 <= ti1 + 0.01f)) {
            v[k][0] = ti, v[k][1] = y0, v[k][2] = bj, v[k][3] = y1, v[k][4] = bj1, v[k][5] = y1;
            j++;
        } else {
            v[k][0] = ti, v[k][1] = y0, v[k][2] = bj, v[k][3] = y1, v[k][4] = ti1, v[k][5] = y0;
            i++;
        }
        k++;
    }
    return k;
}
static void graph_tracks(void)
{
    float v[NTRK][6];
    uint32_t n = trk_live < NTRK ? trk_live : NTRK, k, i, dt = ui.frame - trs.f0;
    int32_t h = (int32_t)graph_h();
    if (dt > 8u) dt = 8u;
    trs.f0 = ui.frame;
    if (n > 1u)
        (void)tr_split(n, 6.0f, 4.0f, 234.0f, (float)(h - 4), v);
    for (k = 0; k < n; k++) {
        const track_t *t = &trk[k];
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), pos = song.playing ? seq_src(t, t->seq_idx) : 0u;
        uint32_t base = pos / 16u * 16u, on = k == song.sel % NTRK, off = eco_off(k), nc = n > 1u ? 3u : 4u, s;
        uint16_t c = off ? T_DIM : tr_col(k);
        float p[8], q[8], seg[4], cx = 0.0f, cy = 0.0f, d, in = n > 1u ? 0.8f : 0.86f;
        if (n > 1u)
            for (i = 0; i < 6u; i++) p[i] = v[k][i];
        else {                                          /* one track: the whole panel */
            p[0] = 6.0f, p[1] = 4.0f, p[2] = 234.0f, p[3] = 4.0f, p[4] = 234.0f, p[5] = (float)(h - 4), p[6] = 6.0f, p[7] = (float)(h - 4);
        }
        for (i = 0; i < nc; i++)                        /* the outline (the selected: bolder) */
            tr_pen(p[2u * i], p[2u * i + 1u], p[(2u * i + 2u) % (2u * nc)], p[(2u * i + 3u) % (2u * nc)], c, k * 3u + i,
                   on ? 256 : 200);
        if (on)
            for (i = 0; i < nc; i++)
                tr_pen(p[2u * i] + 0.7f, p[2u * i + 1u] + 0.5f, p[(2u * i + 2u) % (2u * nc)] + 0.7f,
                       p[(2u * i + 3u) % (2u * nc)] + 0.5f, c, k * 3u + i + 11u, 220);
        if (nc == 3u) {                                 /* the incentre: an even margin */
            float la = fm_sqrtf((p[2] - p[4]) * (p[2] - p[4]) + (p[3] - p[5]) * (p[3] - p[5]));
            float lb = fm_sqrtf((p[0] - p[4]) * (p[0] - p[4]) + (p[1] - p[5]) * (p[1] - p[5]));
            float lc = fm_sqrtf((p[0] - p[2]) * (p[0] - p[2]) + (p[1] - p[3]) * (p[1] - p[3]));
            cx = (la * p[0] + lb * p[2] + lc * p[4]) / (la + lb + lc);
            cy = (la * p[1] + lb * p[3] + lc * p[5]) / (la + lb + lc);
        } else {
            cx = 120.0f;
            cy = (float)h * 0.5f;
        }
        for (i = 0; i < nc; i++) {                      /* the dots' path: the shape drawn in towards its centre, out into
                                                         * its corners */
            q[2u * i] = cx + (p[2u * i] - cx) * in;
            q[2u * i + 1u] = cy + (p[2u * i + 1u] - cy) * in;
        }
        for (i = 0; i < nc; i++) {
            float ex = q[(2u * i + 2u) % (2u * nc)] - q[2u * i], ey = q[(2u * i + 3u) % (2u * nc)] - q[2u * i + 1u];
            seg[i] = fm_sqrtf(ex * ex + ey * ey);
        }
        if (song.playing && pos != trs.last[k] && step_on(&seq_steps(t)[pos]))
            trs.fl[k][pos % 16u] = 255;                 /* a step sounding: its flash */
        trs.last[k] = (uint8_t)(song.playing ? pos : 0xFFu);
        for (s = 0; s < 16u; s++) {                     /* always 16: the track's own steps bolder, the rest faint */
            uint32_t si = base + s, g, fl = trs.fl[k][s];
            int32_t px, py;
            d = (float)s * (float)nc / 16.0f;           /* each side the same share of the 16 (every shape turns its
                                                         * corners on the same steps: 6 / 5 / 5, the rectangle 4 a side),
                                                         * a little hand-placed wobble along it */
            g = (uint32_t)d;
            d = (d - (float)g) * seg[g] + (s ? el_sin((float)(k * 16u + s) * 2.3f) * 1.2f : 0.0f);
            px = (int32_t)((q[2u * g] + (q[(2u * g + 2u) % (2u * nc)] - q[2u * g]) * d / seg[g]) * 16.0f);
            py = (int32_t)((q[2u * g + 1u] + (q[(2u * g + 3u) % (2u * nc)] - q[2u * g + 1u]) * d / seg[g]) * 16.0f);
            if (si >= len)                              /* past the track's length: a faint ring */
                eco_ell(px, py, 24, 24, ux_mix(T_SURF, c, 25));
            else if (step_on(&seq_steps(t)[si]))
                og_node(px, py, 2 + (int32_t)(fl * 2u / 256u), off ? T_DIM : ux_mix(c, T_TEXT, (int32_t)(fl * 75u / 256u)));
            else {                                      /* an empty step of it: a bold ring */
                eco_ell(px, py, 34, 34, ux_mix(T_SURF, c, 80));
                eco_ell(px + 6, py + 5, 34, 34, ux_mix(T_SURF, c, 50));
            }
            if (song.playing && si == pos)
                eco_ell(px, py, 76, 76, T_TEXT);
            if (dt)
                trs.fl[k][s] = (uint8_t)(fl * 220u >> 8);   /* (falling ~0.86 a frame) */
        }
    }
}
#endif
static void graph_home_lz(void)
{
    if (home_level() == 1u)
        graph_home_being();
    else
        graph_tracks();                                 /* (chord-harp fork: TRACKS, the beings gone) */
}
