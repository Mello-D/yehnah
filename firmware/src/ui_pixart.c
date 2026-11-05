/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork, 2026-10-25) Pixel art on an 80 x 46 grid of art pixels, scaled to the rectangle it is given in whole
 * pixels, centred and clipped to it; filled shapes in the palette's colours, moving
 * with the sound (el_in: its level, its newest note, its time). (2026-10-28) The engines' pictures are robots now
 * (ui_robot.c); kept here: FM6's OPERATOR and OP ENV pages. (2026-10-29) DRUM-X's
 * kit gone: its EDIT pages show its robot.
 * Included by ui_engline.c (its el_in, el_hash, vxs). */
#define PX_W 80
#define PX_H 46
#define PX_NONE (-1)
static struct { int32_t x0, y0, ps, dv, cx0, cy0, cx1, cy1, lx, live; } PX;   /* (dv > 1: smaller than the grid, an art
                                                         * pixel ps / dv screen px) */
static void px_begin(const int32_t *A)
{
    float s = (float)A[2] / (float)PX_W < (float)A[3] / (float)PX_H ? (float)A[2] / (float)PX_W : (float)A[3] / (float)PX_H;
    PX.ps = (int32_t)(s + 0.2f) < 1 ? 1 : (int32_t)(s + 0.2f);   /* (up to ~10 % of it trimmed: HOME's ENGINE 2 x, 146 px) */
    PX.dv = s < 0.8f ? (int32_t)(1.0f / s + 0.99f) : 1;   /* (smaller than 80 x 46: 1 / dv, whole) */
    PX.x0 = A[0] + (A[2] - PX_W * PX.ps / PX.dv) / 2;
    PX.y0 = A[1] + (A[3] - PX_H * PX.ps / PX.dv) / 2;
    PX.cx0 = A[0], PX.cy0 = A[1], PX.cx1 = A[0] + A[2], PX.cy1 = A[1] + A[3];
}
/* (chord-harp fork) the art pixels x0..x1, y0..y1 as large as they fit the rectangle A (whole pixels, at most 3 x),
 * centred in it (FM6's OPERATOR pages: their own grid, as wide as the algorithm) */
static void px_fit(const int32_t *A, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t w = x1 - x0 + 1, h = y1 - y0 + 1, s = A[2] / w < A[3] / h ? A[2] / w : A[3] / h;
    PX.ps = s < 1 ? 1 : s > 3 ? 3 : s;
    PX.dv = 1;
    PX.x0 = A[0] + (A[2] - w * PX.ps) / 2 - x0 * PX.ps;
    PX.y0 = A[1] + (A[3] - h * PX.ps) / 2 - y0 * PX.ps;
    PX.cx0 = A[0], PX.cy0 = A[1], PX.cx1 = A[0] + A[2], PX.cy1 = A[1] + A[3];
}
/* art pixels x0..x1, y0..y1 (inclusive) filled */
static void px_fill(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c)
{
    int32_t sx0 = PX.x0 + x0 * PX.ps / PX.dv, sy0 = PX.y0 + y0 * PX.ps / PX.dv, sx1 = PX.x0 + (x1 + 1) * PX.ps / PX.dv,
            sy1 = PX.y0 + (y1 + 1) * PX.ps / PX.dv;
    if (c < 0 || x1 < x0 || y1 < y0)
        return;
    if (sx1 == sx0) sx1++;                              /* (a thumbnail: a thin part still a pixel) */
    if (sy1 == sy0) sy1++;
    if ((uint16_t)c == T_RAISE)                         /* (never a card's colour: a dark wood or screen nudged off it,
                                                         * along the gray axis) */
        c = (c >> 11) < 31 ? c + 0x0841 : c - 0x0841;
    sx0 = sx0 < PX.cx0 ? PX.cx0 : sx0;
    sy0 = sy0 < PX.cy0 ? PX.cy0 : sy0;
    sx1 = sx1 > PX.cx1 ? PX.cx1 : sx1;
    sy1 = sy1 > PX.cy1 ? PX.cy1 : sy1;
    if (sx1 > sx0 && sy1 > sy0)
        cv_rect(sx0, sy0, sx1 - sx0, sy1 - sy0, (uint16_t)c);
}
static void px_pt(int32_t x, int32_t y, int32_t c) { px_fill(x, y, x, y, c); }
static void px_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, n = 0;
    for (; n < 200; n++) {                              /* (Bresenham) */
        int32_t e2 = 2 * err;
        px_pt(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void px_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t fill, int32_t out)
{
    px_fill(x0, y0, x1, y1, fill);
    if (out >= 0) {
        px_fill(x0, y0, x1, y0, out); px_fill(x0, y1, x1, y1, out);
        px_fill(x0, y0, x0, y1, out); px_fill(x1, y0, x1, y1, out);
    }
}
/* the colours: the palette's */
#define C_TX ((int32_t)T_TEXT)
#define C_DM ((int32_t)ux_mix(T_SURF, T_TEXT, 28))
#define C_TH ((int32_t)T_THEME)
#define C_THD ((int32_t)ux_mix(T_SURF, T_THEME, 50))
#define C_AC ((int32_t)T_ACCENT)
#define C_ACD ((int32_t)ux_mix(T_SURF, T_ACCENT, 55))
#define C_K(i) ((int32_t)ux.k[i])
typedef struct { el_in_t in; uint32_t fr; int on; float e; } px_in_t;
static px_in_t px_in(const track_t *t, uint32_t k, int32_t en_home, const int32_t *A)
{
    px_in_t r;
    r.in = el_in(t, k, en_home);
    r.on = r.in.en > 0.02f;
    r.e = r.in.e;
    r.fr = (uint32_t)(r.in.ms * (1.0f / 160.0f));     /* (the mock-ups' frames: 160 ms) */
    PX.live = r.on;
    px_begin(A);
    return r;
}
/* (chord-harp fork, 2026-10-26) FM6's OPERATOR pages in pixel art (were line drawings, el_fm6): the algorithm as six chips
 * on the art grid, each where its cell puts it (ui_graph.c FM6_CELL: its column, its row above the output), its number
 * on it; the carriers in the theme, the modulators in the second key colour, the one picked (sel < 6) in the accent and
 * ringed; a chip glows with its level now (vx_fm_ops: its level x its envelope); the wires down from each modulator to
 * what it modulates, a pulse running down them while the sound plays, the carriers down to the output's bar; the
 * feedback operator a small loop at its corner */
static VX_CODE void px_fm6ops(const track_t *t, const int32_t *A, uint32_t sel)
{
    px_in_t p = px_in(t, 4, -1, A);
    vxfm_t o;
    int32_t x[6], y[6], hi = 0, lo = 99, xl = 99, xr = 0;
    uint32_t k, j;
    vx_fm_ops(t, &o);
    for (k = 0; k < 6u; k++)
        hi = (int32_t)(FM6_CELL[o.alg][k] & 7u) > hi ? (int32_t)(FM6_CELL[o.alg][k] & 7u) : hi;
    for (k = 0; k < 6u; k++) {
        x[k] = 40 + ((int32_t)(FM6_CELL[o.alg][k] & 7u) * 2 - hi) * 6;
        y[k] = 33 - (int32_t)(FM6_CELL[o.alg][k] >> 4) * 10;   /* (rows 10 apart: a wire 3 px long at least) */
        lo = y[k] < lo ? y[k] : lo;
        xl = x[k] < xl ? x[k] : xl;
        xr = x[k] > xr ? x[k] : xr;
    }
    px_fit(A, xl - 8, lo - 6, xr + 8, 43);              /* (as large as the algorithm fits) */
    px_fill(xl - 8, 42, xr + 8, 43, C_TX);              /* the output */
    for (k = 0; k < 6u; k++) {
        for (j = 0; j < 6u; j++)                         /* the wires: j modulates k */
            if (o.m[k] >> j & 1u) {
                int32_t d = (int32_t)((p.in.ms * 0.0017f + (float)j * 0.3f) * 64.0f) & 63;
                px_line(x[j], y[j] + 4, x[k], y[k] - 4, C_DM);
                if (p.on)                                /* (while it sounds: the signal running down) */
                    px_fill(x[j] + (x[k] - x[j]) * d / 64, y[j] + 4 + (y[k] - y[j] - 8) * d / 64,
                            x[j] + (x[k] - x[j]) * d / 64, y[j] + 4 + (y[k] - y[j] - 8) * d / 64, C_AC);
            }
        if (o.car >> k & 1u)                             /* a carrier down to the output */
            px_line(x[k], y[k] + 4, x[k], 41, C_DM);
    }
    if (o.fbo < 6u) {                                    /* the feedback loop */
        k = o.fbo;
        px_rect(x[k] + 5, y[k] - 6, x[k] + 8, y[k] - 2, PX_NONE, C_K(2));
        px_pt(x[k] + 4, y[k] - 2, C_K(2));
    }
    for (k = 0; k < 6u; k++) {
        int32_t c = k == sel ? C_AC : (o.car >> k & 1u) ? C_TH : C_K(2);
        char n[2] = {(char)('1' + k), 0};
        px_rect(x[k] - 4, y[k] - 3, x[k] + 4, y[k] + 3, (int32_t)ux_mix(T_SURF, (uint16_t)c, 12 + o.amp[k] * 60 / 256), c);
        if (k == sel)
            px_rect(x[k] - 5, y[k] - 4, x[k] + 5, y[k] + 4, PX_NONE, C_AC);
        if (PX.ps >= 2)
            cv_text(PX.x0 + x[k] * PX.ps + PX.ps / 2 - 2, PX.y0 + y[k] * PX.ps + PX.ps / 2 - 3 - AF_X_CAP_Y, &AF_X, n,
                    k == sel ? T_TEXT : T_MID);
        VX_FM6_HOOK(k, (PX.x0 + x[k] * PX.ps + PX.ps / 2) * 16, (PX.y0 + y[k] * PX.ps + PX.ps / 2) * 16,
                    (4 * PX.ps + PX.ps / 2) * 16);   /* (ui_render.c's lint: the chip as a circle, its half height and a ring) */
    }
}
/* .. OP ENV: the operator picked's envelope as the patch has it (from L4 to L1 at R1, L2 at R2, L3 at R3, held while the
 * key is down, back to L4 at R4; a stage as wide as its rate is slow), filled under it in columns, the stage KNOB 3 / 4
 * edit in the accent, its corner marked; where the operator is now (its level), a square beating while it sounds */
static VX_CODE void px_fm6env(const track_t *t, const int32_t *A, const uint8_t *op, uint32_t ko, uint32_t sg)
{
    px_in_t p = px_in(t, 4, -1, A);
    vxfm_t o;
    int32_t x[6], y[6], w[6], tot = 0, k, xx;
    vx_fm_ops(t, &o);
#define PFY(l) (44 - (int32_t)(l) * 38 / 99)
#define PFW(r) (4 + ((99 - (int32_t)(r)) * (99 - (int32_t)(r))) * 44 / 9801)
    for (k = 0; k < 3; k++)
        w[k] = PFW(op[FP_R1 + k]);
    w[3] = 10;                                           /* the hold */
    w[4] = PFW(op[FP_R1 + 3]);
    for (k = 0; k < 5; k++)
        tot += w[k];
    px_fit(A, 0, 0, 113, 47);                           /* (the panel's width: 2 x) */
    x[0] = 4;
    y[0] = PFY(op[FP_L1 + 3]);
    for (k = 0; k < 5; k++) {
        x[k + 1] = x[k] + w[k] * 106 / tot;
        y[k + 1] = k < 3 ? PFY(op[FP_L1 + k]) : k == 3 ? y[k] : y[0];
    }
    x[5] = 110;
    for (k = 0; k < 5; k++) {                            /* filled under, a column an art pixel */
        uint32_t st = k < 3 ? (uint32_t)k : k == 4 ? 3u : 9u;
        for (xx = x[k]; xx < x[k + 1]; xx++) {
            int32_t yy = y[k] + (y[k + 1] - y[k]) * (xx - x[k]) / (x[k + 1] - x[k] ? x[k + 1] - x[k] : 1);
            px_fill(xx, yy + 1, xx, 44, st == sg ? C_ACD : C_THD);
        }
    }
    px_fill(2, 45, 111, 45, C_DM);                       /* the floor */
    for (k = 0; k < 5; k++) {
        uint32_t st = k < 3 ? (uint32_t)k : k == 4 ? 3u : 9u;
        px_line(x[k], y[k], x[k + 1], y[k + 1], st == sg ? C_AC : st == 9u ? C_DM : C_TH);
    }
    for (k = 1; k < 6; k++)                              /* the corners; the stage edited's larger */
        if ((uint32_t)(k - 1) == (sg < 3u ? sg : 4u))
            px_rect(x[k] - 1, y[k] - 1, x[k] + 1, y[k] + 1, C_AC, C_TX);
        else
            px_pt(x[k], y[k], C_TX);
    if (p.on) {                                          /* where the operator is now */
        uint32_t s = o.st[ko % 6u];
        int32_t i = s < 4u ? (int32_t)(s & 3u) + 1 : 5, py = PFY(o.env[ko % 6u] * 99 / 256), r = 1 + (int32_t)(p.fr & 1u);
        px_rect(x[i] - r, py - r, x[i] + r, py + r, PX_NONE, C_K(1));
        px_pt(x[i], py, C_K(1));
    }
#undef PFY
#undef PFW
}
