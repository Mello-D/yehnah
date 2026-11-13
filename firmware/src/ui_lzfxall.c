/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (2026-11-13) FX ALL's panel (ui_fxall.c): a header per effect, under it a row per old page, each row four knob cells (the cells of
 * HOME > TRACKS: name, the pixels or a grid for a pick, the value). The focused row is framed, the others dimmer; the focused effect's
 * header is taller and carries a hand-drawn sketch of the effect (tr_pen's line) at its right. A row's cells are what draw_cards() puts in
 * kn_buf for that row's page (fxa_cur set to it for the call), so every special case of the old pages shows as it did. The panel is
 * taller than the canvas: two bands, as GLO's lists. Included by ui_draw.c after draw_columns. */
#define FXA_CW 54                                       /* a cell, the pitch 56: four across in x 6 .. 228 */
#define FXA_NAME_H 13
#define FXA_FILL_H 10
#define FXA_RH 39                                       /* a row: the name, the pixels, the value (+ 2 apart) */
#define FXA_HH 16                                       /* a section's header: every one the same height (the focused one's filled) */
#define FXA_PH 34                                       /* a section's picture (a sketch): a strip after its last row */
/* a pen's line (HOME's triangles: tr_pen): a little wobble across it, a fainter second pass beside it; px floats, al 0..256 */
static void fxa_pen(float x0, float y0, float x1, float y1, uint16_t c, uint32_t seed, int32_t al)
{
    float dx = x1 - x0, dy = y1 - y0, l = fm_sqrtf(dx * dx + dy * dy), nx, ny;
    uint32_t n, i, pass;
    if (l < 0.5f)
        return;
    nx = -dy / l;
    ny = dx / l;
    n = (uint32_t)(l * (1.0f / 6.0f)) + 2u;
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
/* the sketches: a pen's line each (fxa_pen), drawn from the effect's own settings, x y w h the box, k the effect (FXS kind) */
static int32_t fxs_g(uint32_t id) { return song.g[id]; }
static int32_t fxs_tv(uint32_t id) { return TSEL->p[id]; }
static uint32_t fxs_h(uint32_t i) { uint32_t h = (i + 1u) * 2654435761u; h ^= h >> 15; return h * 2246822519u >> 16; }   /* (a fixed scatter) */
static void fxs_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t sd)
{
    fxa_pen((float)x, (float)y, (float)(x + w), (float)y, c, sd, 256);
    fxa_pen((float)(x + w), (float)y, (float)(x + w), (float)(y + h), c, sd + 1u, 256);
    fxa_pen((float)(x + w), (float)(y + h), (float)x, (float)(y + h), c, sd + 2u, 256);
    fxa_pen((float)x, (float)(y + h), (float)x, (float)y, c, sd + 3u, 256);
}
static void fxs_line(float x0, float y0, float x1, float y1, uint16_t c, uint32_t sd, int32_t al) { fxa_pen(x0, y0, x1, y1, c, sd, al); }
static float fxs_wv(uint32_t wave, float u)             /* a wave at u cycles, -1 .. 1 (SIN TRI SAW SQR S&H) */
{
    float f = u - (float)(int32_t)u;
    if (f < 0)
        f += 1.0f;
    switch (wave) {
    case 0: return el_sin(f * 6.2832f);
    case 1: return f < 0.5f ? 4.0f * f - 1.0f : 3.0f - 4.0f * f;
    case 2: return 2.0f * f - 1.0f;
    case 3: return f < 0.5f ? 1.0f : -1.0f;
    default: return (float)(int32_t)(fxs_h((uint32_t)(u * 4.0f)) & 255u) / 127.5f - 1.0f;
    }
}
/* a curve y(u), u 0 .. 1 over n points: cyc cycles of wave, amp 0 .. 1 of half the height, phase in cycles */
static void fxs_cyc(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t wave, float cyc, float ph, float amp, uint16_t c, uint32_t sd)
{
    float px = (float)x, py = (float)y + (float)h * 0.5f;
    int32_t i;
    for (i = 0; i <= 28; i++) {
        float u = (float)i / 28.0f, nx = (float)x + u * (float)w, ny = (float)y + (float)h * 0.5f - fxs_wv(wave, u * cyc + ph) * amp * (float)h * 0.45f;
        if (i)
            fxa_pen(px, py, nx, ny, c, sd + (uint32_t)i, 256);
        px = nx;
        py = ny;
    }
}
static float fxs_xfer(uint32_t type, float v)           /* a drive's transfer curve (SOFT HARD FOLD CRUSH RECT AMP FUZZ), -1 .. 1 */
{
    float a = v < 0 ? -v : v, s = v < 0 ? -1.0f : 1.0f;
    switch (type) {
    case 1: return v > 1 ? 1.0f : v < -1 ? -1.0f : v;
    case 2: return el_sin(v * 1.5708f);
    case 3: { float q = (float)(int32_t)(v * 4.0f + (v < 0 ? -0.5f : 0.5f)) / 4.0f; return q > 1 ? 1.0f : q < -1 ? -1.0f : q; }
    case 4: return a > 1 ? 1.0f : a;
    case 5: return v > 0 ? v / (1.0f + v) : v / (1.0f + 0.4f * a);
    case 6: return s * (a / (0.6f + a));
    default: return v / (1.0f + a);
    }
}
static void fxs_curve(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t type, float gain, uint16_t c, uint16_t d, uint32_t sd)
{
    float px = (float)x, py = (float)(y + h);
    int32_t i;
    fxs_line((float)x, (float)(y + h), (float)(x + w), (float)y, d, sd + 90u, 130);      /* (the dry: straight through) */
    for (i = 0; i <= 24; i++) {
        float u = (float)i / 24.0f, v = (u * 2.0f - 1.0f) * gain / 1.0f, o = fxs_xfer(type, v);
        float nx = (float)x + u * (float)w, ny = (float)y + (float)h * 0.5f - o * (float)h * 0.5f;
        if (i)
            fxa_pen(px, py, nx, ny, c, sd + (uint32_t)i, 256);
        px = nx;
        py = ny;
    }
}
static float fxs_dec(float u, float rate) { return 1.0f / (1.0f + rate * u + rate * rate * u * u * 0.5f); }   /* (an exponential's fall) */
static void fxa_sketch(uint32_t k, int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint16_t a = T_ACCENT, b = T_THEME;
    int32_t i;
    switch (k) {
    case 0:                                             /* SENDS: this track's DIST CHORUS DLY REV */
        for (i = 0; i < 4; i++) {
            static const uint8_t ID[4] = {P_DIST, P_CHOR, P_DLY, P_REV};
            int32_t hh = 3 + (h - 3) * fxs_tv(ID[i]) / 127;
            fxs_box(x + i * w / 4 + 3, y + h - hh, w / 4 - 8, hh, i & 1 ? b : a, (uint32_t)i * 5u);
        }
        break;
    case 1: {                                           /* DIST: the transfer curve: its drive bends it, its type shapes it */
        float gain = 1.0f + (float)fxs_tv(P_DIST) * (7.0f / 127.0f);
        fxs_curve(x, y, w, h, (uint32_t)fxs_tv(P_DTYPE), gain, a, b, 1u);
        break;
    }
    case 2: {                                           /* AUTO PAN: the swing, its depth, speed and shape (BEAT: 1 .. 4 cycles) */
        float cyc = fxs_tv(P_APSYNC) ? 1.0f + (float)(fxs_tv(P_APRATE) & 7) * 0.5f : 1.0f + (float)fxs_tv(P_APRATE) * (5.0f / 127.0f);
        fxs_cyc(x, y, w, h, (uint32_t)fxs_tv(P_APWAVE), cyc, 0.0f, 0.06f + 0.94f * (float)fxs_tv(P_APDEP) / 127.0f, a, 10u);
        fxs_line((float)x, (float)(y + h / 2), (float)(x + w), (float)(y + h / 2), b, 9u, 140);
        break;
    }
    case 3: {                                           /* PHASER: the notches, deeper with AMOUNT, narrower with FDBK, more with RATE */
        float amt = (float)fxs_tv(P_PHAMT) / 127.0f, cyc = 1.0f + (float)fxs_tv(P_PHRATE) * 0.04f, px = (float)x, py = (float)y + 2.0f;
        int32_t p = 1 + fxs_tv(P_PHFB) * 3 / 127, j;
        for (i = 0; i <= 30; i++) {
            float u = (float)i / 30.0f, n = el_sin(u * cyc * 3.1416f), nx = (float)x + u * (float)w, ny, q = 1.0f;
            n = n < 0 ? -n : n;
            for (j = 0; j < p; j++)
                q *= n;
            ny = (float)y + 2.0f + (float)(h - 4) * amt * (1.0f - q);
            if (i)
                fxa_pen(px, py, nx, ny, a, 20u + (uint32_t)i, 256);
            px = nx;
            py = ny;
        }
        fxs_line((float)x, (float)(y + h), (float)(x + w), (float)(y + h), b, 19u, 256);
        break;
    }
    case 4:                                             /* SCENE: three scenes */
        for (i = 0; i < 3; i++)
            fxs_box(x + i * w / 3 + 3, y + 2, w / 3 - 8, h - 4, i == 1 ? a : b, (uint32_t)i * 5u);
        break;
    case 5: {                                           /* DELAY: TIME the spacing, FEEDBK the decay, MIX the height, PING the sides, PITCH the climb, SPRAY the scatter, MOD the wobble */
        int32_t tr = RATIO(&GP[G_DTIME], enum_rank(&GP[G_DTIME], fxs_g(G_DTIME))), fb = clamp(fxs_g(G_DFDBK), 0, 127), mix = fxs_g(G_DMIX);
        int32_t sb = w * (500 + tr) / 10000, av = 1000, pit = fxs_g(G_DPIT), spr = fxs_g(G_DSPRY), wid = fxs_g(G_WIDTH), ping = fxs_g(G_DPING);
        sb = sb < 9 ? 9 : sb;
        for (i = 0; i < 9 && av > 45; i++) {
            int32_t bx = 3 + i * sb, bh = (h - 2) * av / 1000 * (30 + 70 * mix / 127) / 100, jx = ((int32_t)(fxs_h((uint32_t)i) % 9u) - 4) * spr / 127, dy = pit * i * h / 120;
            if (bx + 7 > w)
                break;
            bh = bh < 3 ? 3 : bh;
            dy = clamp(dy, -(h - bh), h - bh);
            fxs_box(x + bx + jx, y + h - bh - dy, 6, bh, i ? b : a, (uint32_t)i * 5u);
            if (ping && (i & 1))                        /* (the other side: a foot mark) */
                fxs_line((float)(x + bx + jx), (float)(y + h + 1), (float)(x + bx + jx + 6), (float)(y + h + 1), a, 70u + (uint32_t)i, 256);
            if (wid > 4 && i)                           /* (WIDE: the right channel's tap, later) */
                fxs_line((float)(x + bx + jx + 3 + wid * sb / 127), (float)(y + h), (float)(x + bx + jx + 3 + wid * sb / 127), (float)(y + h - bh / 2 - dy), b, 90u + (uint32_t)i, 150);
            av = av * fb / 127;
        }
        fxs_cyc(x, y + h - 3, w, 6, 0, 6.0f, 0.0f, (float)fxs_g(G_DMOD) / 127.0f, b, 50u);   /* (MOD: the line under them wobbles) */
        break;
    }
    case 6: {                                           /* REVERB: SIZE the tail's length, DAMP how soon its highs go, PRE the gap, the model (SPRING ripples, REVRS runs backward) */
        int32_t ty = fxs_g(G_RTYPE), pre = ty == 2 || ty == 4 || ty == 5 ? 0 : clamp(fxs_g(G_RPRE), 0, 100), dmp = ty == 3 ? 64 : fxs_g(G_RDAMP);
        float dec = 7.0f - 5.5f * (float)fxs_g(G_RSIZE) / 127.0f, dec2 = dec * (1.0f + 3.0f * (float)dmp / 127.0f), mod = (float)fxs_g(G_RMOD) / 127.0f;
        int32_t x0 = pre * w / 400, pass;
        for (pass = 0; pass < 2; pass++) {
            float px = (float)(x + x0), py = (float)(y + h), r = pass ? dec2 : dec;
            for (i = 0; i <= 24; i++) {
                float u = (float)i / 24.0f, e = fxs_dec(ty == 5 ? 1.0f - u : u, r);
                float nx = (float)(x + x0) + u * (float)(w - x0), ny;
                if (ty == 1)
                    e *= 0.72f + 0.28f * el_sin(u * 60.0f);
                if (pass == 0)
                    e *= 1.0f + mod * 0.12f * el_sin(u * 31.0f);
                ny = (float)(y + h) - (float)(h - 3) * (e > 1.0f ? 1.0f : e < 0 ? 0.0f : e) * (pass ? 0.8f : 1.0f);
                if (i)
                    fxa_pen(px, py, nx, ny, pass ? b : a, (pass ? 120u : 30u) + (uint32_t)i, 256);
                px = nx;
                py = ny;
            }
        }
        fxs_line((float)x, (float)(y + h), (float)(x + w), (float)(y + h), b, 99u, 150);
        break;
    }
    case 7: {                                           /* CHORUS: RATE the wave's speed, DEPTH its size, STEREO the offset of the two sides */
        float cyc = 1.0f + (float)fxs_g(G_CRATE) * (5.0f / 127.0f), amp = 0.1f + 0.9f * (float)fxs_g(G_CDEPTH) / 127.0f;
        float off = (float)(fxs_g(G_CSTER) + 64) / 127.0f * 0.5f;     /* (-64: mono, the same wave; 63: half a cycle apart) */
        fxs_cyc(x, y, w, h, 0, cyc, 0.0f, amp, a, 40u);
        fxs_cyc(x, y, w, h, 0, cyc, off, amp, b, 80u);
        break;
    }
    case 8: {                                           /* SLICER: its pattern's gates; DEPTH how shut the shut ones are; STUT shown as halves */
        uint32_t pat = sl_pattern(), mode = (uint32_t)fxs_g(G_SLMODE);
        int32_t cb = w / 16, x0 = (w - 16 * cb) / 2, shut = 3 + (h - 6) * (127 - fxs_g(G_SLDEP)) / 127;
        for (i = 0; i < 16; i++) {
            int on = mode && ((pat >> i) & 1u);
            if (!mode)
                fxs_line((float)(x + x0 + i * cb), (float)(y + 2), (float)(x + x0 + (i + 1) * cb - 2), (float)(y + 2), b, (uint32_t)i, 256);
            else if (on)
                fxs_box(x + x0 + i * cb, y + 1, cb - 2, h - 2, a, (uint32_t)i * 5u);
            else
                fxs_box(x + x0 + i * cb, y + h - shut, cb - 2, shut - 1, b, (uint32_t)i * 5u);
            if (on && mode == 2u)                       /* (STUT: a split across the gate) */
                fxs_line((float)(x + x0 + i * cb), (float)(y + h / 2), (float)(x + x0 + (i + 1) * cb - 2), (float)(y + h / 2), a, 60u + (uint32_t)i, 256);
        }
        break;
    }
    case 9: {                                           /* SL BUS: the tracks and the FX going in (on: pink, off: cyan, dim) into the slicer */
        int32_t sw = (w * 62 / 100) / 7;
        for (i = 0; i < 7; i++) {
            uint32_t id = i < 5 ? G_SLT1 + (uint32_t)i : i == 5 ? G_SLT6 : G_SLFX;
            int on = fxs_g(id) != 0;
            fxs_box(x + i * sw, y, sw - 2, h * 40 / 100, on ? a : b, (uint32_t)i * 5u);
            fxs_line((float)(x + i * sw + sw / 2), (float)(y + h * 40 / 100), (float)(x + w * 72 / 100), (float)(y + h / 2), on ? a : b, 90u + (uint32_t)i, on ? 256 : 90);
        }
        fxs_box(x + w * 72 / 100, y + h * 28 / 100, w * 28 / 100, h * 56 / 100, fxs_g(G_SLMODE) ? a : b, 70u);
        break;
    }
    case 10: {                                          /* SL PITCH: each step's pitch up or down from the middle; past LEN dim; the one KNOB 1 is on pink */
        int32_t len = clamp(fxs_g(G_SLPLEN), 1, 16), cb = w / 16, mid = y + h / 2;
        fxs_line((float)x, (float)mid, (float)(x + w), (float)mid, b, 5u, 150);
        for (i = 0; i < 16; i++) {
            int32_t pv = fxs_g(G_SLP0 + (uint32_t)i), bh = (h / 2 - 1) * (pv < 0 ? -pv : pv) / 24;
            uint16_t c = i == (int32_t)(sl_ui_step & 15u) ? a : i < len ? b : T_DIM;
            if (bh < 2)
                bh = 2;
            fxs_box(x + i * cb + 1, pv < 0 ? mid : mid - bh, cb - 3, bh, c, (uint32_t)i * 5u);
        }
        break;
    }
    case 11: {                                          /* MASTER: CLIP's curve (drive x1 .. x4 into the soft clip); DUCK a dip, PUNCH a spike at its right */
        int32_t cw = w * 55 / 100, rx = x + cw + 8, rw = w - cw - 8, dk = fxs_g(G_DUCK), pn = fxs_g(G_PUNCH), rel = fxs_g(G_DREL);
        float px = (float)rx, py = (float)(y + h * 35 / 100);
        fxs_curve(x, y, cw, h, 0, 1.0f + 3.0f * (float)fxs_g(G_CLIP) / 100.0f, a, b, 1u);
        for (i = 0; i <= 14; i++) {
            float u = (float)i / 14.0f, lv = 1.0f - (float)dk / 100.0f * (u < 0.2f ? u / 0.2f : 1.0f - (u - 0.2f) / (0.2f + 0.8f * (float)(rel + 1) / 128.0f) * 0.0f), nx = (float)rx + u * (float)rw, ny;
            if (u >= 0.2f)
                lv = 1.0f - (float)dk / 100.0f * fxs_dec((u - 0.2f) / 0.8f, 6.0f - 5.0f * (float)rel / 127.0f);
            if (u < 0.2f)
                lv = 1.0f - (float)dk / 100.0f * (u / 0.2f);
            ny = (float)(y + h * 35 / 100) + (float)(h * 55 / 100) * (1.0f - lv) - (u < 0.1f ? (float)pn / 100.0f * 5.0f : 0.0f);
            if (i)
                fxa_pen(px, py, nx, ny, b, 130u + (uint32_t)i, 256);
            px = nx;
            py = ny;
        }
        break;
    }
    default: {                                          /* PUNCH: the lane's steps as they are (LEN steps of RES) */
        static const uint8_t RES[3] = {8, 16, 32};
        int32_t steps = fxs_g(G_PFXLEN) * RES[clamp(fxs_g(G_PFXRES), 0, 2)] / 8, cw;
        steps = steps > 64 ? 64 : steps < 1 ? 1 : steps;
        cw = w / steps;
        cw = cw < 1 ? 1 : cw;
        for (i = 0; i < steps && i * cw < w; i++)
            if (pfx_lane_at((uint32_t)i)) {
                if (cw >= 4)
                    fxs_box(x + i * cw, y + h * 20 / 100, cw - 2, h * 60 / 100, a, (uint32_t)i * 5u);
                else
                    cv_rect(x + i * cw, y + h * 20 / 100, cw > 1 ? cw - 1 : 1, h * 60 / 100, a);
            }
        fxs_line((float)x, (float)(y + h), (float)(x + w), (float)(y + h), b, 9u, 256);
        break;
    }
    case SK_ENV: {                                      /* ENV: attack, decay, sustain level, release (the shape the settings give) */
        float a1 = 0.03f + 0.27f * (float)fxs_tv(P_ATK) / 127.0f, d1 = 0.03f + 0.27f * (float)fxs_tv(P_DEC) / 127.0f,
              r1 = 0.03f + 0.27f * (float)fxs_tv(P_REL) / 127.0f, sl = (float)fxs_tv(P_SUS) / 127.0f;
        float bx = (float)x, by = (float)(y + h), ex = (float)w, hh = (float)h - 1.0f, sx, hold;
        float px = bx + a1 * ex, dx = px + d1 * ex, rx = bx + ex - r1 * ex, sy = by - sl * hh;
        sx = dx;
        hold = rx > sx ? rx : sx;
        fxs_line(bx, by, px, by - hh, a, 40u, 256);
        fxs_line(px, by - hh, dx, sy, a, 41u, 256);
        fxs_line(sx, sy, hold, sy, a, 42u, 256);
        fxs_line(hold, sy, hold + r1 * ex, by, a, 43u, 256);
        fxs_line(bx, by, bx + ex, by, b, 44u, 120);
        break;
    }
    case SK_MSEQ: {                                     /* MSEQ: its 16 levels as bars, those past LEN dim */
        int32_t len = clamp(fxs_tv(P_MSLEN), 1, 16), cb = w / 16;
        for (i = 0; i < 16; i++) {
            int32_t bh = 2 + (h - 3) * fxs_tv(P_MS0 + (uint32_t)i) / 127;
            fxs_box(x + i * cb + 1, y + h - bh, cb - 3, bh, i < len ? a : T_DIM, (uint32_t)i * 5u);
        }
        break;
    }
    case SK_LFO:                                        /* LFO: its wave, more cycles with RATE, shifted by PHASE */
        fxs_cyc(x, y, w, h, (uint32_t)fxs_tv(P_LWAVE), 1.0f + (float)fxs_tv(P_LRATE) * (3.0f / 127.0f), (float)fxs_tv(P_LPHASE) / 128.0f, 0.9f, a, 50u);
        fxs_line((float)x, (float)(y + h / 2), (float)(x + w), (float)(y + h / 2), b, 49u, 140);
        break;
    }
}
#define FXA_SG 3                                        /* the gap under a section's last row (the focused row's frame is 2 px taller) */
static int fxa_last(uint32_t i) { return i + 1u == fxa_cnt || fxa_r[i + 1u].first; }   /* the last row of its section */
static void fxa_scroll(void)                            /* (the focused row is always at the top: nothing to scroll) */
{
    fxa_sync(fxa_kind_of(&PAGES[ui.page]));
    fxa_build();
    if (fxa_cur >= fxa_cnt)
        fxa_cur = (uint8_t)(fxa_cnt - 1u);
}
/* one cell of kn (knob c) at x, y */
static void fxa_cell(const kn_t *kn, uint32_t c, int32_t x0, int32_t y, int on)
{
    uint16_t kc = T_K(c), ec = ux_mix(T_SURF, kc, on ? 16 : 7), lit = on ? kc : ux_mix(T_SURF, kc, 38);
    uint16_t vc = kn->m ? T_THEME : kn->d ? T_DIM : on ? T_TEXT : ux_mix(T_SURF, T_TEXT, 50);
    int32_t bx, by, k, vw, uw, vx;
    uint32_t t = kn->r <= 0 ? 0u : kn->r >= 1000 ? 65536u : (uint32_t)kn->r * 65536u / 1000u;
    const char *nm = kn_name(kn->l);
    if (!kn->l[0] && !kn->v[0])
        return;
    cv_text_in(x0, y + CAP_IN(S, FXA_NAME_H), FXA_CW, &AF_S, nm, on ? kc : ux_mix(T_SURF, kc, 50), T_SURF);
    y += FXA_NAME_H;
    if (kn->pn) {                                       /* a pick: the grid, the one chosen lit */
        int32_t cols = kn->pn <= 2u ? 2 : kn->pn <= 16u ? 4 : 8, rows = (kn->pn + cols - 1) / cols;
        int32_t ch = (FXA_FILL_H - 2 * (rows - 1)) / rows, cw = (FXA_CW - 2 * (cols - 1)) / cols;
        int32_t gx = x0 + (FXA_CW - cols * cw - 2 * (cols - 1)) / 2;
        for (k = 0; k < kn->pn; k++) {
            int32_t in = k / cols < rows - 1 ? cols : kn->pn - (rows - 1) * cols;
            cv_rect(gx + (cols - in) * (cw + 2) / 2 + (k % cols) * (cw + 2), y + (k / cols) * (ch + 2), cw, ch, k == kn->pi ? lit : ec);
        }
    } else if (kn->r >= 0) {                            /* a value: the pixels lit in the cell's own order */
        for (by = 0; by < FXA_FILL_H / 2; by++)
            for (bx = 0; bx < FXA_CW / 2; bx = k) {
                int ison = kn_px(c, bx, by) < t;
                for (k = bx + 1; k < FXA_CW / 2 && (kn_px(c, k, by) < t) == ison; k++)
                    ;
                cv_rect(x0 + bx * 2, y + by * 2, (k - bx) * 2, 2, ison ? lit : ec);
            }
    }
    y += FXA_FILL_H;
    vw = text_w(&AF_S, kn->v), uw = kn->u[0] ? text_w(&AF_S, kn->u) + 2 : 0;
    vx = vw + uw > FXA_CW - 2 ? x0 + 1 : x0 + HALF_UP(FXA_CW - vw - uw);
    cv_text_fit(vx, y + CAP_IN(S, 13), &AF_S, kn->v, vc, T_SURF, FXA_CW - 2);
    if (uw && vw + uw <= FXA_CW - 2)
        cv_text(vx + vw + 2, y + CAP_IN(S, 13), &AF_S, kn->u, T_MID);
}
static void fxa_header(uint32_t sec, int32_t y, int32_t hh, int on)
{
    const char *nm = fxa_sn[sec % FXA_SEC];
    uint16_t bg = on ? T_ACCENT : T_RAISE;
    if (y + hh <= 0 || y >= (int32_t)graph_h())
        return;
    cv_rrect(5, y, 224, hh, 4, bg, T_SURF);              /* (the focused section's: filled, its name dark on it) */
    cv_text_on(117 - HALF_UP(text_w(&AF_B, nm)), y + CAP_IN(B, hh), &AF_B, nm, on ? T_BG : T_MID, bg);
}
static void fxa_picture(uint32_t sec, int32_t y)       /* a section's sketch, drawn from its settings, under its last row */
{
    if (y + FXA_PH <= 0 || y >= (int32_t)graph_h())
        return;
    cv_rrect(5, y, 224, FXA_PH, 4, T_RAISE, T_SURF);
    fxa_sketch(fxa_sk[sec % FXA_SEC], 14, y + 4, 206, FXA_PH - 8);
}
/* row i (cursor on it: on) at y */
static void fxa_row(uint32_t i, int32_t y, int on)
{
    kn_t save[4];
    uint32_t c;
    uint8_t sv = fxa_cur;
    if (y + FXA_RH + 1 <= 0 || y >= (int32_t)graph_h())
        return;
    fxa_cur = (uint8_t)i;
    for (c = 0; c < 4u; c++) {
        kn_buf[c].l[0] = kn_buf[c].u[0] = kn_buf[c].v[0] = 0;
        kn_buf[c].r = -1;
        kn_buf[c].m = kn_buf[c].d = kn_buf[c].n = 0;
    }
    draw_cards();
    for (c = 0; c < 4u; c++)
        save[c] = kn_buf[c];
    fxa_cur = sv;
    fxa_aux();                                          /* (the lane / operator the focused row shows, not the last one drawn) */
    if (on) {
        cv_rrect(4, y, 226, FXA_RH + 1, 4, T_SEL, T_SURF);       /* (a "/" reaches 1 px below the value) */
        cv_rrect(5, y + 1, 224, FXA_RH - 1, 3, T_SURF, T_SEL);
    }
    for (c = 0; c < 4u; c++)
        if (PAGES[fxa_r[i].pi].graph != GR_PUNCH || c < 2u)      /* (PUNCH: LEN RES; its PLAY / CLEAR cells are the REC layer's F4 now) */
            fxa_cell(&save[c], c, 6 + (int32_t)c * 56, y + 2, on);
}
/* the focused row at the top, its section's header directly above it (filled); below it the next rows, a section's header above its first
 * and its picture (if it has a sketch) after its last; the page goes round, the last row then the first */
static void fxa_paint(void)
{
    int32_t gh = (int32_t)graph_h(), y = 3, k;
    uint32_t cur = fxa_cur, r;
    fxa_header(fxa_r[cur].sec, y, FXA_HH, 1);
    y += FXA_HH + 2;
    for (k = 0; k < (int32_t)fxa_cnt && y < gh; k++) {
        r = (cur + (uint32_t)k) % fxa_cnt;
        if (k && fxa_r[r].first) {
            fxa_header(fxa_r[r].sec, y, FXA_HH, 0);
            y += FXA_HH + 2;
        }
        fxa_row(r, y, !k);
        y += FXA_RH;
        if (fxa_last(r)) {
            if (fxa_sk[fxa_r[r].sec % FXA_SEC] != SK_NONE) {
                y += 2;
                fxa_picture(fxa_r[r].sec, y);
                y += FXA_PH;
            }
            y += FXA_SG;
        }
    }
    {                                                   /* the place in the list */
        int32_t bh = gh - 8, h = 10, o = fxa_cnt > 1u ? (bh - h) * (int32_t)cur / ((int32_t)fxa_cnt - 1) : 0;
        cv_rect(233, 4 + clamp(o, 0, bh - h), 2, h, T_MID);
    }
}
static void fxall_draw(void)
{
    uint32_t pass, h0 = graph_h() / 2u, i, k, sig = 0x811C9DC5u;
    fxa_scroll();
    sig = (sig ^ fxa_cur ^ (uint32_t)fxa_k << 8 ^ (uint32_t)fxa_cnt << 12) * 16777619u;
    sig = (sig ^ ux.gen ^ settings.palette << 8 ^ (uint32_t)sys_load << 16 ^ (uint32_t)song.sel << 20) * 16777619u;
    sig = (sig ^ sl_ui_step ^ (uint32_t)TSEL->eng_req << 8 ^ (uint32_t)(ui_prefs & PREF_SNAKE) << 12) * 16777619u;
    for (i = 0; i < fxa_cnt; i++)
        for (k = 0; k < 4u; k++) {
            uint32_t id = fxa_r[i].id[k];
            if (id != 0xFFu)
                sig = (sig ^ (uint32_t)(PAGES[fxa_r[i].pi].scope == SC_GLOBAL ? (IS_MROW(id) ? (id - ID_MENU < MI_VALUES ? menu_get(id - ID_MENU) : 0) : song.g[id]) : IS_VF(id) ? fmx_get(TSEL->p, id - 0xA0u) : TSEL->p[id]) ^ id << 16) * 16777619u;
        }
    for (k = 0; k < 16u; k++)                           /* (what the sketches draw that no row's id names: SL PITCH's other steps, PUNCH's lane) */
        sig = (sig ^ (uint32_t)song.g[G_SLP0 + k] ^ (uint32_t)pfx_lane_at(k * 4u) << 8 ^ (uint32_t)pfx_lane_at(k * 4u + 1u) << 12 ^
               (uint32_t)pfx_lane_at(k * 4u + 2u) << 16 ^ (uint32_t)pfx_lane_at(k * 4u + 3u) << 20) * 16777619u;
    sig = (sig ^ (uint32_t)(uint16_t)trks_ui ^ (uint32_t)(uint16_t)load_ui << 16) * 16777619u;   /* (GLO's TRACKS, LOAD) */
    if (fxa_k == FXK_SND) {                             /* (what a kit's and FM6's old pages show, that no row's id names) */
        const uint8_t *q;
        uint32_t n;
        if (trk_eng(TSEL) == ENGI_FM6) {
            q = fm6_patch[song.sel % NTRK];
            for (n = 0; n < FP_SIZE; n++)
                sig = (sig ^ q[n]) * 16777619u;
            sig = (sig ^ fop_stg) * 16777619u;
        } else if (trk_eng(TSEL) == ENGI_DRUM) {
            q = (const uint8_t *)dx_kit;
            for (n = 0; n < sizeof dx_kit; n++)
                sig = (sig ^ q[n]) * 16777619u;
            for (n = 0; n < 8u; n++)
                sig = (sig ^ dx_lay[n] ^ (uint32_t)dx_snd[n] << 8) * 16777619u;
            for (n = 0; n < 4u; n++)
                sig = (sig ^ dx_mot[n]) * 16777619u;
            sig = (sig ^ dx_tone ^ (uint32_t)dx_hands << 8 ^ (uint32_t)dx_ui_side << 12) * 16777619u;
        }
    }
    if (!ui.force && sig == ui.graph_sig)
        return;
    ui.graph_sig = sig;
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, pass ? graph_h() - h0 : h0, T_BG);
        cv_oy = pass ? -(int32_t)h0 : 0;
        cv_scroll = 1;
        cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);
        cv_bg = T_SURF;
        fxa_paint();
        cv_scroll = 0;
        cv_oy = 0;
        cv_blit(0, graph_y() + pass * h0);
    }
}
