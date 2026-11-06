/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Yehnah (YEHNAH TONIC): Yehnah's organic line art, drawn by code (docs/TONIC-UI.md). */
/* Everything is drawn into the graph canvas (gfx.c cv_px) with integer maths: no stored artwork. Positions in
 * 1/16 px (Q4) so curves move smoothly between pixels; lines antialiased (two pixels across, blended over what
 * is there). Angles are 16-bit turns (0x10000 = 360 degrees), through dsp.c's sine table.
 *   og_line, og_quad_a   an antialiased line (Q4 ends); a Bezier curve in short lines
 *   og_node     a dot with a soft rim
 *   og_col      the five Yehnah colours: cream, coral, teal, mustard, mint (on another palette: blends of its own)
 * (chord-harp fork, 2026-11-03) What is left of Yehnah's anatomical plates (the paths, rings, dashes, labels and
 * stipple went with the pages that drew them). */
enum { OG_CREAM, OG_CORAL, OG_TEAL, OG_MUSTARD, OG_MINT };

static int og_neutral(uint16_t c) { return (c >> 11) == (c & 31u) && ((c >> 5) & 63u) >> 1 == (c >> 11); }
static uint16_t og_col(uint32_t k)
{
    switch (k) {
    case OG_CORAL: return T_ACCENT;
    case OG_TEAL: return T_THEME;
    case OG_MUSTARD: return ux_mix(T_ACCENT, 0xFE60u, 55);   /* (toward yellow) */
    case OG_MINT: return ux_mix(T_THEME, T_TEXT, 45);
    default: return T_TEXT;
    }
}
/* c over the canvas pixel at (x, y) with coverage a (0..256) */
static inline void og_plot(int32_t x, int32_t y, uint16_t c, int32_t a)
{
    uint16_t *p, o;
    uint32_t r, g, b;
    y += cv_oy;
    if (a <= 0 || (uint32_t)x >= cv_w || (uint32_t)y >= cv_h)
        return;
    p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 256) {
        *p = swap16(c);
        return;
    }
    o = swap16(*p);
    r = (o >> 11) + ((((int32_t)(c >> 11) - (int32_t)(o >> 11)) * a) >> 8);
    if (og_neutral(o) && og_neutral(c)) {                /* grey over grey stays grey */
        *p = swap16((uint16_t)(r << 11 | r << 6 | r));
        return;
    }
    g = ((o >> 5) & 63u) + ((((int32_t)((c >> 5) & 63u) - (int32_t)((o >> 5) & 63u)) * a) >> 8);
    b = (o & 31u) + ((((int32_t)(c & 31u) - (int32_t)(o & 31u)) * a) >> 8);
    *p = swap16((uint16_t)(r << 11 | g << 5 | b));
}
/* an antialiased line between Q4 points, at strength a (0..256) */
static void og_line_a(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t a)
{
    int32_t dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, t, g, p, e, f;
    if (adx >= ady) {                                    /* along x: y in Q8, two pixels per column */
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
        g = adx ? ((y1 - y0) << 8) / (x1 - x0) : 0;      /* dy / dx, Q8 */
        e = x1 >> 4;
        for (p = x0 >> 4; p <= e && p - (x0 >> 4) < 480; p++) {
            int32_t y = (y0 << 4) + g * (p - (x0 >> 4)) - 128;   /* (pixel centres) */
            f = y & 255;
            og_plot(p, y >> 8, c, ((256 - f) * a) >> 8);
            og_plot(p, (y >> 8) + 1, c, (f * a) >> 8);
        }
    } else {
        if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
        g = ((x1 - x0) << 8) / (y1 - y0);
        e = y1 >> 4;
        for (p = y0 >> 4; p <= e && p - (y0 >> 4) < 480; p++) {
            int32_t x = (x0 << 4) + g * (p - (y0 >> 4)) - 128;
            f = x & 255;
            og_plot(x >> 8, p, c, ((256 - f) * a) >> 8);
            og_plot((x >> 8) + 1, p, c, (f * a) >> 8);
        }
    }
}
static void og_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { og_line_a(x0, y0, x1, y1, c, 256); }

/* a quadratic Bezier (Q4 points) in n short lines */
static void og_quad_a(int32_t x0, int32_t y0, int32_t cx, int32_t cy, int32_t x1, int32_t y1, uint16_t c, uint32_t n,
                      int32_t a)
{
    int32_t px = x0, py = y0;
    uint32_t i;
    for (i = 1; i <= n; i++) {
        int32_t t = (int32_t)((i << 8) / n), u = 256 - t;   /* Q8 */
        int32_t x = (u * u * x0 + 2 * u * t * cx + t * t * x1) >> 16, y = (u * u * y0 + 2 * u * t * cy + t * t * y1) >> 16;
        og_line_a(px, py, x, y, c, a);
        px = x;
        py = y;
    }
}

/* cos / sin of a 16-bit angle, Q15 */
static int32_t og_cos(uint32_t ang) { return sine_i((ang + 0x4000u) << 16); }
static int32_t og_sin(uint32_t ang) { return sine_i(ang << 16); }


/* A shape: n cubic segments, a start point then three points a segment, x y as int8 (units, y down), closed by its
 * last point = its start. og_path draws it morphed m (0 A .. 256 B), each unit u / 16 px (Q4 per unit: u), about
 * the anchor (ax, ay) (units) scaled by k (Q8: 256 the shape itself) and mirrored (mx -1) at (cx, cy) (Q4) */
typedef struct {
    int16_t cx, cy, u, mx;                               /* where, units -> Q4, -1 mirrors */
    int16_t ax, ay, k;                                   /* scaled about (ax, ay) by k / 256 */
} og_xf_t;

/* a filled dot of radius r px (pixel centre (cx, cy) in Q4) with an antialiased rim */
static void og_node(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x0 = cx >> 4, y0 = cy >> 4, i, j;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++) {
            int32_t d = i * i + j * j - r * r;          /* inside: d <= 0; the rim: 0 .. 2r */
            if (d <= -r)
                og_plot(x0 + i, y0 + j, c, 256);
            else if (d < r)
                og_plot(x0 + i, y0 + j, c, 128 - d * 128 / r);
        }
}

/* a shape from assets/ui-shapes (tools/gen_ui_shapes.py: build/gen/ui_shapes.h) */
typedef struct {
    uint8_t nseg, mirror;
    const int8_t *a, *b;
} og_shape_t;
#include "ui_shapes.h"
#define og_mix ux_mix

