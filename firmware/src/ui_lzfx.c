/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) FX's lens (ui.c fx_*): the panels of its three levels, drawn as the approved HTML mock-up (FX, the proposal
 * "LENTE"), in the lens's panel (240 x 140 at y 84; the mock-up's coordinates less 84). Included by ui_graph.c.
 *   TRACK   the track's set (SENDS, DIST, AUTO PAN): its place as dots and its name at the top, its picture (the sends
 *           as four bars, DIST's curve, AUTO PAN's swing), the DIST curve small under it
 *   BUSES   the six modules as tiles (3 x 2, 76 x 68: DELAY REVERB CHORUS SLICER MASTER PUNCH), each its name and its
 *           picture small; the one PRESETS picked TINT with a THEME frame; one LOAD ECO took away: dark, "ECO"
 *   EFFECT  the module picked large: its pages as dots, "DELAY . DLY 2" at the right, its picture
 * The pictures (fxg_*, any size): DELAY the echoes as bars, MIX high, each FDBK lower, TIME apart (PING-PONG: L / R),
 * REVERB its decay filled (its TYPE: SPRING rippled, CLOUD wavy), CHORUS three sines DEPTH deep, STEREO apart, SLICER its
 * 16 steps (the pattern's open ones lit, the step playing framed), MASTER the output now as a bar over CLIP, the
 * DUCK's curve, PUNCH the lane's steps, SL PITCH its 16 pitches from the middle, the sends four bars, DIST its transfer
 * curve in a box with TYPE and TONE, AUTO PAN the swing's span with its dot; heat colours (Yehnah's thermal ramp; another
 * palette: its theme toward its accent).
 * (0.8.5, FX live) While it sounds (playing, or the output above silence: bg_energy) they move with it: the echoes
 * swell with the level and a pulse runs along them, the tail swells and a line sweeps it, the chorus lines run at
 * RATE, the slicer's step playing is framed, MASTER's meter follows the output, PUNCH's playhead walks the lane, DIST's
 * level is a dot on its curve, AUTO PAN's dot swings at RATE. Drawn on the panel's redraws only (every other frame
 * while it sounds: graph_signature); nothing of the audio path */
/* (2026-11-10, pixel-art redo) every picture is made of chunky square blocks (FP screen px: 4 on the big panel, 2 in a
 * tile), a heat ramp of four steps, dithered edges, no smooth lines; nothing is written that a knob card or the page
 * dots already say (the numbers, the TYPE / RATE words): a tile keeps one short word. */
#define FXG_H(v) heat_col((int32_t)(v))                 /* (0..256) */
#define FXP_H(v) heat_col(40 + (clamp((int32_t)(v), 0, 255) >> 6) * 72)   /* (the ramp in four steps) */
static int32_t FP = 2;                                  /* the block's side, px */
static int32_t fxg_en(void) { return song.playing || bg_energy() > 2 ? 32 + bg_energy() : 0; }   /* 0: still */
static void fxg_label(int32_t x, int32_t y, const char *s, uint16_t c, int right)
{
    if (right)                                          /* (AF_X 3 rows down: S's capitals' middle) */
        cv_text_r(x, y + 3, &AF_X, s, c, cv_bg);
    else
        cv_text(x, y + 3, &AF_X, s, c);
}
static void fxb(int32_t x, int32_t y, int32_t bx, int32_t by, int32_t bw, int32_t bh, uint16_t c)   /* blocks */
{
    if (bw > 0 && bh > 0)
        cv_rect(x + bx * FP, y + by * FP, bw * FP, bh * FP, c);
}
static int32_t fxp_size(int32_t w) { return w > 120 ? 4 : 2; }
/* a trace: the block at row yy of column i, joined to the one before it (py) */
static void fxb_trace(int32_t x, int32_t y, int32_t i, int32_t yy, int32_t py, int32_t th, uint16_t c)
{
    int32_t a = i && py < yy ? py : yy, b = i && py > yy ? py : yy;
    fxb(x, y, i, a, 1, b - a + th, c);
}
/* the dots of a line: every other block */
static void fxb_dots(int32_t x, int32_t y, int32_t bx, int32_t by, int32_t n, int horiz, uint16_t c)
{
    int32_t i;
    for (i = 0; i < n; i += 2)
        fxb(x, y, bx + (horiz ? i : 0), by + (horiz ? 0 : i), 1, 1, c);
}
/* a ball sprite, 5 x 5 blocks (the corners cut) with its shine */
static void fxb_ball(int32_t x, int32_t y, int32_t bx, int32_t by, uint16_t c)
{
    fxb(x, y, bx + 1, by, 3, 5, c);
    fxb(x, y, bx, by + 1, 5, 3, c);
    fxb(x, y, bx + 1, by + 1, 1, 1, T_TEXT);
}
static void fxg_delay(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    char b[24];
    const char *u;
    int32_t i, a = 1000, mix = song.g[G_DMIX], fb = song.g[G_DFDBK], en = fxg_en();
    int32_t tr = RATIO(&GP[G_DTIME], enum_rank(&GP[G_DTIME], song.g[G_DTIME]));
    int32_t cols, rows, t0, bw, sb, pulse;
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : 1, bw = FP > 2 ? 3 : 2;
    sb = w * (500 + tr) / 10000 / FP;
    if (sb < bw + 1) sb = bw + 1;
    pulse = en && sb > 0 ? (int32_t)(ui.frame * 2u / (uint32_t)FP % (uint32_t)(9 * sb)) : -99;   /* (the echo running) */
    fxb_dots(x, y, 0, rows - 1, cols, 1, T_LINE);
    for (i = 0; i < 9 && a > 8; i++) {
        int32_t bx = 2 + i * sb, bh = (rows - 1 - t0) * 9 / 10 * a / 1000 * (mix * 7 / 10 + 38) / 127;
        int hit = pulse >= i * sb && pulse < (i + 1) * sb;
        int32_t k;
        if (bx + bw > cols)
            break;
        if (en)
            bh = bh * (176 + clamp(en, 0, 288) * 80 / 288) / 256;
        if (bh < 1) bh = 1;
        for (k = 0; k < bh; k++)                        /* (a block a row, the ramp stepping up the bar; the top one bright) */
            fxb(x, y, bx, rows - 2 - k, bw, 1, k == bh - 1 || hit ? FXP_H(hit ? 255 : 150 + a * 105 / 1000) : FXP_H(40 + a * 150 / 1000 + k * 40 / bh));
        if (song.g[G_DPING] && bh > 2)                  /* (PING-PONG: the echo's side as a notch) */
            fxb(x, y, bx + (i & 1 ? bw - 1 : 0), rows - 2 - bh, 1, 1, T_DIM);
        a = a * (fb > 127 ? 127 : fb) / 127;
    }
    if (pulse >= 0 && pulse + 2 < cols)                 /* (the pulse itself, on the line) */
        fxb(x, y, pulse, rows - 1, 2, 1, T_TEXT);
    if (lab) {
        param_format(&GP[G_DTIME], song.g[G_DTIME], b, &u);
        fxg_label(x, y, b, T_MID, 0);
    }
}
static void fxg_reverb(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    int32_t i, sz = song.g[G_RSIZE], ty = song.g[G_RTYPE], e = 1000, cols, rows, t0, top, hh, pre, en = fxg_en(), sweep, k;
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : 0;
    top = t0, hh = rows - t0;
    k = 1000 - (5000 - sz * 4300 / 127) / (cols ? cols : 1) * 2;
    pre = clamp(song.g[G_RPRE], 0, 100) * cols / 4 / 100;   /* (the pre-delay: a gap before the tail) */
    sweep = en && cols > 0 ? (int32_t)(ui.frame * 3u / (uint32_t)FP % (uint32_t)cols) : -1;
    fxb(x, y, 0, rows - 1, pre + 1, 1, T_LINE);
    for (i = pre; i < cols; i++) {                      /* the decay, a column of blocks each: its height the envelope */
        int32_t j = i - pre, rise = cols / 25 + 1, env = j < rise ? e * j / rise : e, n = 1000, ch;
        if (ty == 4) n = 500 + (bg_sin((uint32_t)(j * 65536 / cols * 6)) >> 6) * 500 / 512;
        else if (ty == 1) n = 700 + (bg_sin((uint32_t)(j * 65536 / cols * 14)) >> 6) * 300 / 512;
        ch = hh * 9 / 10 * env / 1000 * n / 1000;
        if (en)                                         /* (the tail swells with the output) */
            ch = ch * (168 + clamp(en, 0, 288) * 88 / 288) / 256;
        if (ch > 0) {
            uint16_t c = i == sweep ? T_TEXT : FXP_H(230 - i * 190 / cols);
            fxb(x, y, i, top + hh - ch, 1, ch, c);
            if (!(i & 1))                               /* (a dithered edge: every other column a block taller) */
                fxb(x, y, i, top + hh - ch - 1, 1, 1, c);
        }
        if (j >= rise) e = e * k / 1000;
    }
    if (sweep >= 0)                                     /* (the sweep's foot on the line, seen when the tail is low) */
        fxb(x, y, sweep, rows - 1, 1, 1, T_TEXT);
    if (lab)
        fxg_label(x, y, N_RTYPE[clamp(ty, 0, 4)], T_MID, 0);
}
static void fxg_chorus(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    int32_t k, i, dep = song.g[G_CDEPTH], st = song.g[G_CSTER], cols, rows, t0, mid;
    uint32_t ph = fxg_en() ? ui.frame * (uint32_t)(song.g[G_CRATE] + 8) * 48u : 0u;   /* (running at RATE while it sounds) */
    char b[16];
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : 1, mid = t0 + (rows - t0) / 2;
    for (k = 0; k < 3; k++) {
        int32_t py = 0;
        for (i = 0; i < cols; i++) {
            int32_t amp = (rows - t0) * 38 / 100 * dep / 127 * (10 + 2 * k) / 10 * (128 + st) / 128;
            int32_t yy = mid + (bg_sin((uint32_t)(i * 65536 / cols * 2) + (uint32_t)k * 21000u + ph) * amp >> 15);
            yy = clamp(yy, t0, rows - 2);
            fxb_trace(x, y, i, yy, py, FP > 2 ? 2 : 1, FXP_H(60 + k * 80));
            py = yy;
        }
    }
    if (lab) {
        str_cpy(b, "STEREO ", sizeof b);
        b[7] = st > 0 ? '+' : 0;
        fmt_int(b + 7 + (st > 0), st);
        fxg_label(x, y, b, T_MID, 0);
    }
}
static void fxg_slicer(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    uint32_t i, pat = sl_pattern(), mode = (uint32_t)song.g[G_SLMODE], play = mode && song.playing ? sl.idx % 16u : 99u;
    int32_t cols, rows, t0, cb, x0, bb;
    char b[32];
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : w > 120 ? 4 : 1, cb = cols / 16, x0 = (cols - 16 * cb) / 2, bb = rows - 2 - t0;
    for (i = 0; i < 16u; i++) {
        int on = ((pat >> i) & 1u) && mode;
        int32_t cx = x0 + (int32_t)i * cb, k, bh = on ? bb : 2;
        for (k = 0; k < bh; k++)                        /* (a lit step: stacked blocks, the ramp up with DEPTH) */
            fxb(x, y, cx, t0 + bb - 1 - k + 1, cb > 1 ? cb - 1 : 1, 1,
                on ? FXP_H(70 + song.g[G_SLDEP] * 110 / 127 + k * 75 / bb) : T_RAISE);
        if (i == play) {                                /* (the one playing: a cap and a foot of white) */
            fxb(x, y, cx, t0 - 1 + 1, cb > 1 ? cb - 1 : 1, 1, T_TEXT);
            fxb(x, y, cx, t0 + bb + 1, cb > 1 ? cb - 1 : 1, 1, T_TEXT);
        }
        if (!(i & 3))                                   /* (a beat's tick under every fourth step) */
            fxb(x, y, cx, rows - 1, 1, 1, T_DIM);
    }
    if (lab) {
        str_cpy(b, mode == SL_OFF ? "OFF" : mode == SL_STUT ? "STUT" : "GATE", sizeof b);
        fxg_label(x, y, b, T_MID, 0);
    } else if (w > 120) {                               /* the tracks in it, FX: chips at the right, T1 .. T5 and FX */
        for (i = 0; i < NTRK + 1u; i++) {
            int on = i < NTRK ? song.g[G_SLT1 + i] != 0 : song.g[G_SLFX] != 0;
            int32_t cx = cols - 3 * (int32_t)(NTRK + 1u - i);
            fxb(x, y, cx, 0, 2, 2, on ? FXP_H(200) : T_RAISE);
        }
    }
}
static void fxg_master(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    int32_t en = bg_energy(), lvl = song.playing || en > 2 ? 128 + en / 2 : 26, i, py = 0, duck = song.g[G_DUCK];
    int32_t rel = song.g[G_DREL], cols, rows, nseg, seg, mrows, t0, ch, cy, clip = song.g[G_CLIP];
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, mrows = FP > 2 ? 3 : 2;
    seg = 3, nseg = cols / seg;                         /* (a segmented LED meter: 2 blocks lit, 1 dark) */
    for (i = 0; i < nseg; i++) {
        int on = i * 256 / nseg < lvl;
        fxb(x, y, 1 + i * seg - 1 + 1, 1, seg - 1, mrows, on ? FXP_H(i * 250 / nseg) : T_RAISE);
    }
    fxb(x, y, 1 + (clip * (nseg - 1) / 100) * seg, 1 + mrows, seg - 1, 1, T_TEXT);   /* CLIP: a tick under its segment */
    t0 = 2 + mrows + 2;                                 /* (CLIP is the tick under the meter: no words) */
    (void)lab;
    cy = t0, ch = rows - cy - 1;
    {   /* the duck's gain over a beat: down by DUCK, back over REL (e^-u k, a factor a column); the ground under it dithered */
        int32_t kk = clamp(8000 - rel * 50, 1600, 8000), e = 1000;
        for (i = 0; i < cols; i++) {
            int32_t d, yy;
            d = 1000 - e * duck / 100;
            yy = cy + ch - d * ch / 1000;
            fxb_trace(x, y, i, yy, py, 1, FXP_H(190));
            if (!(i & 1))
                fxb(x, y, i, yy + 2, 1, 1, T_LINE);
            py = yy;
            e = e * (1000 - kk / cols * 2) / 1000;
        }
    }
}
static void fxg_punch(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    static const uint8_t RES[3] = {8, 16, 32};
    int32_t steps = song.g[G_PFXLEN] * RES[clamp(song.g[G_PFXRES], 0, 2)] / 8, i, cw, t0 = lab ? 14 : 2;
    if (steps > 64) steps = 64;
    if (steps < 1) steps = 1;
    cw = w / steps;
    if (cw < 1) cw = 1;
    FP = fxp_size(w);
    for (i = 0; i < steps && i * cw < w; i++) {
        int on = pfx_lane_at((uint32_t)i) != 0u, at = song.playing && (uint32_t)i == pfx_lane_pos();   /* (the playhead) */
        int32_t bw = cw > 2 ? cw - 1 : 1, bh = (h - t0 - 6) / FP * FP, k;
        if (on || at) {
            for (k = 0; k < bh / FP; k++)               /* (the open steps as stacks of blocks, the ramp climbing) */
                cv_rect(x + i * cw, y + t0 + bh - (k + 1) * FP, bw, FP, at ? T_TEXT : FXP_H(90 + k * 165 / (bh / FP)));
        } else
            cv_rect(x + i * cw, y + h - 6, bw, 2, T_LINE);
    }
    if (lab && pfx_rec_ok())
        fxg_label(x, y, "REC", T_MID, 0);
}
static void fxg_slpitch(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t i, cols, rows, mid, len = clamp(song.g[G_SLPLEN], 1, 16), cb;
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, mid = rows / 2, cb = cols / 16;
    fxb_dots(x, y, 0, mid, cols, 1, T_LINE);
    for (i = 0; i < 16; i++) {
        int32_t v = song.g[G_SLP0 + i], bh = ((v < 0 ? -v : v) * (mid - 3) + 23) / 24 + 1, k;
        for (k = 0; k < bh; k++)
            fxb(x, y, i * cb, v >= 0 ? mid - 1 - k : mid + 1 + k, cb > 1 ? cb - 1 : 1, 1,
                i == (int32_t)sl_ui_step ? T_THEME : i < len ? FXP_H(100 + (v + 24) * 3 + k * 20 / bh) : T_RAISE);
    }
}
/* the distortion's transfer curve in a square of s px (blocks), the level a dot on it */
static void fxd_curve(const track_t *t, int32_t x, int32_t y, int32_t s)
{
    int32_t d = t->p[P_DIST], g = 4096 + d * d * 2, i, py = 0, n, cy;
    uint32_t type = (uint32_t)t->p[P_DTYPE] > DT_AMP ? DT_SOFT : (uint32_t)t->p[P_DTYPE];
    FP = s >= 100 ? 4 : 2;
    n = s / FP, cy = n / 2;
    fxb(x, y, 0, 0, n, 1, T_LINE), fxb(x, y, 0, n - 1, n, 1, T_LINE);
    fxb(x, y, 0, 0, 1, n, T_LINE), fxb(x, y, n - 1, 0, 1, n, T_LINE);
    fxb_dots(x, y, 1, cy, n - 2, 1, T_LINE);
    fxb_dots(x, y, cy, 1, n - 2, 0, T_LINE);
    for (i = 0; i < n; i++) {
        int32_t xx = (i - n / 2) * 131072 / (n / 2), yy = cy - tk_dist_y(type, xx, d ? g : 4096, d) * (n / 2 - 2) / 32767;
        yy = clamp(yy, 1, n - 3);
        fxb_trace(x, y, i, yy, py, FP > 2 ? 2 : 1, FXP_H(110 + d));
        py = yy;
    }
    if (fxg_en() && n > 12) {                           /* the level now, a dot on the curve */
        int32_t xx = clamp(bg_energy(), 0, 256) * 131072 / 256, px = cy + xx * (n / 2) / 131072;
        int32_t yy = clamp(cy - tk_dist_y(type, xx, d ? g : 4096, d) * (n / 2 - 2) / 32767, 2, n - 4);
        fxb(x, y, clamp(px, 2, n - 4), yy - 1, 2, 2, T_TEXT);
    }
}
/* a sine in (dim) or out through the distortion (heat), over w x h px */
static void fxd_scope(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, int out)
{
    int32_t d = t->p[P_DIST], g = 4096 + d * d * 2, i, py = 0, cols, rows, mid;
    uint32_t type = (uint32_t)t->p[P_DTYPE] > DT_AMP ? DT_SOFT : (uint32_t)t->p[P_DTYPE];
    FP = 2;
    cols = w / FP, rows = h / FP, mid = rows / 2;
    fxb(x, y, 0, 0, cols, 1, T_LINE), fxb(x, y, 0, rows - 1, cols, 1, T_LINE);
    fxb(x, y, 0, 0, 1, rows, T_LINE), fxb(x, y, cols - 1, 0, 1, rows, T_LINE);
    fxb_dots(x, y, 1, mid, cols - 2, 1, T_LINE);
    for (i = 1; i < cols - 1; i++) {
        int32_t s = bg_sin((uint32_t)(i * 65536 / (cols - 2) * 2)), v = s * 4 >> 2, yy;   /* (4 / 4: full scale) */
        if (out)
            v = tk_dist_y(type, v * 4, d ? g : 4096, d);
        yy = clamp(mid - v * (mid - 3) / 32767, 1, rows - 3);
        fxb_trace(x, y, i, yy, py, 1, out ? FXP_H(110 + d) : T_DIM);
        py = yy;
    }
}
/* a wave of the pan's LFO, -32767 .. 32767, at phase p (16 bit) */
static int32_t fxg_lfo(uint32_t w, uint32_t p)
{
    p &= 65535u;
    switch (w) {
    case 1: return (int32_t)(p < 32768u ? p : 65535u - p) * 2 - 32767;
    case 2: return (int32_t)p - 32768;
    case 3: return p < 32768u ? 32767 : -32767;
    case 4: { uint32_t s = (p >> 13) * 2654435761u; return (int32_t)((s >> 16) & 0xFFFFu) - 32768; }
    default: return bg_sin(p);
    }
}
static void fxg_pan(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t dep = t->p[P_APDEP], i, cols, rows, lane, sw, tr0, tr1, tm, py = 0, ba;
    uint32_t wv = (uint32_t)clamp(t->p[P_APWAVE], 0, 4), ph = fxg_en() ? ui.frame * (uint32_t)(t->p[P_APRATE] + 6) * 96u : 0u;
    FP = 4;
    cols = w / FP, rows = h / FP, lane = 6, sw = dep * (cols / 2 - 8) / 127;
    ba = (int32_t)(fxg_lfo(wv, ph) * (int64_t)sw >> 15);
    fxb_dots(x, y, 3, lane + 2, cols - 6, 1, T_LINE);   /* the lane, L to R, a tick at the middle */
    fxb(x, y, cols / 2, lane - 1, 1, 7, T_LINE);
    fxg_label(x, y + (lane + 2) * FP - 5, "L", T_DIM, 0);
    fxg_label(x + w, y + (lane + 2) * FP - 5, "R", T_DIM, 1);
    if (sw > 0)
        fxb(x, y, cols / 2 - sw, lane + 2, 2 * sw + 1, 1, FXP_H(110 + dep));
    fxb_ball(x, y, cols / 2 + ba - 2, lane, FXP_H(210));
    tr0 = lane + 9, tr1 = rows - 1, tm = (tr0 + tr1) / 2;   /* below: the wave in time, the same phase marked */
    fxb_dots(x, y, 1, tm, cols - 2, 1, T_LINE);
    for (i = 0; i < cols; i++) {
        int32_t yy = tm - (int32_t)(fxg_lfo(wv, (uint32_t)(i * 65536 * 2 / cols)) * (int64_t)((tr1 - tr0) / 2 - 1) * dep / 127 >> 15);
        fxb_trace(x, y, i, yy, py, 1, FXP_H(100 + dep));
        py = yy;
    }
    fxb(x, y, (int32_t)((ph & 65535u) * (uint32_t)cols / 2u / 65536u) % cols, tr0, 1, tr1 - tr0 + 1, T_TEXT);
}
/* (chord-harp fork) PHASER: the part's response over 40 Hz .. 10 kHz (eight octaves) as a spectrum of block columns, its
 * two notches where the sweep has them now (voice.c ph_ph: ~0.41 and ~2.4 times the break), AMOUNT their depth, FDBK
 * wider (a sketch: each a dip of its own) */
static void fxg_phaser(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint32_t ph = ph_ph[(uint32_t)(t - trk) % NTRK], tri = (ph >> 31 ? ~ph : ph) >> 16;
    int32_t c = 489 + (int32_t)((tri * 1133u) >> 15), wd = 70 + t->p[P_PHFB] * 60 / 127, cols, rows, top, hh;
    int32_t i, dep = t->p[P_PHAMT];
    FP = 4;
    cols = w / FP, rows = h / FP - 2, top = 1, hh = rows - top;
    for (i = 0; i < cols; i++) {
        int32_t q = i * 2048 / cols, d0 = q - (c - 325), d1 = q - (c + 325), ch;
        int32_t dip = 4096 * wd * wd / (wd * wd + d0 * d0) + 4096 * wd * wd / (wd * wd + d1 * d1);
        ch = hh - hh * dep * (dip > 4096 ? 4096 : dip) / (127 * 4096);
        if (ch < 1) ch = 1;
        fxb(x, y, i, top + hh - ch, 1, ch, FXP_H(60 + ch * 190 / hh));
        if (!(i & 1))
            fxb(x, y, i, top + hh - ch - 1, 1, 1, FXP_H(60 + ch * 190 / hh));
    }
    fxb(x, y, 0, rows, cols, 1, T_LINE);
    fxg_label(x, y + h - 8, "40", T_DIM, 0);
    fxg_label(x + w, y + h - 8, "10K", T_DIM, 1);
}
/* the sends' pictures: one card a send (DIST CHORUS DELAY REVERB, above its knob), the picture drawn as the effect is and
 * dithered away by the send's level (ordered dither: nothing at 0, whole at full), the track's stem down into it */
static void fxg_sends(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h)
{
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    int32_t i, cw = w / 4, ih = h - 16;
    for (i = 0; i < 4; i++) {
        int32_t v = t->p[P_DIST + i], cx = x + i * cw + 2, px = cx + 4, pw = cw - 12, py = y + 14, ph = ih - 8, bx, by;
        int32_t lv = v ? 7 + v * 9 / 127 : 0;           /* (0: hidden whole; 16: every block) */
        uint16_t save = cv_bg;
        cv_rect(cx + cw / 2 - 3 - 2, y, 4, 10, v ? FXP_H(60 + v * 195 / 127) : T_LINE);   /* (the stem) */
        lz_tile(cx, y + 10, cw - 4, ih + 6, 0, T_THEME);
        cv_bg = lz_tile_bg();
        if (i == 0) fxd_curve(t, px, py + (ph - pw) / 2, pw);
        else if (i == 1) fxg_chorus(px, py, pw, ph, 0);
        else if (i == 2) fxg_delay(px, py, pw, ph, 0);
        else fxg_reverb(px, py, pw, ph, 0);
        FP = 2;
        for (by = 0; by < ph / FP; by++)                /* (the dither: a block is hidden where the level is below its threshold) */
            for (bx = 0; bx < pw / FP; bx++)
                if (BAYER[(by & 3) * 4 + (bx & 3)] >= lv)
                    cv_rect(px + bx * FP, py + by * FP, FP, FP, T_RAISE);
        cv_bg = save;
    }
}
/* module m's picture; lab: a tile (its one short word) */
static void fxg_mod(uint32_t m, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int lab = w <= 120;
    switch (m) {
    case 0: fxg_delay(x, y, w, h, lab); break;
    case 1: fxg_reverb(x, y, w, h, lab); break;
    case 2: fxg_chorus(x, y, w, h, lab); break;
    case 3: if (cur_page()->graph == GR_SLPIT && w > 120) fxg_slpitch(x, y, w, h); else fxg_slicer(x, y, w, h, lab); break;
    case 4: fxg_master(x, y, w, h, lab); break;
    default: fxg_punch(x, y, w, h, lab); break;
    }
}
/* the set's place: n dots (5 x 3, 7 apart) at the top left; its name at the right */
static void fx_dots(uint32_t n, uint32_t k, const char *a, const char *b)
{
    char s[32];
    uint32_t i;
    for (i = 0; i < n; i++)
        cv_rect(6 + (int32_t)i * 7, 6, 5, 3, i == k ? T_THEME : T_DIM);
    str_cpy(s, a, sizeof s);
    str_cpy(s + str_len(s), " \xB7 ", 4);
    str_cpy(s + str_len(s), b, 16);
    GFX_HOOK_ALIGN(0, 3, 0, 12, AL_V | AL_PASS, "FX lens set name");
    cv_text_r(234, 3, &AF_X, s, T_DIM, T_SURF);
}
static void graph_fx_lz(const track_t *t, const page_t *pg)
{
    uint32_t lv = fx_level();
    if (lv == FXL_TRACK) {
        int32_t k = fx_trk_k(ui.page);
        char tn[3] = {'T', (char)('1' + song.sel % NTRK), 0};
        fx_dots(4, (uint32_t)k, k == 0 ? "FX" : PAGES[ui.page].title, tn);
        if (k == 0) fxg_sends(t, 0, 12, 240, 122);
        else if (k == 1) {                              /* the curve, and a sine going in and coming out */
            fxd_curve(t, 8, 14, 116);
            fxd_scope(t, 132, 14, 100, 56, 0);
            fxd_scope(t, 132, 74, 100, 56, 1);
        } else if (k == 2) fxg_pan(t, 8, 14, 224, 118);
        else fxg_phaser(t, 8, 14, 224, 118);
    } else if (lv == FXL_BUSES) {
        uint32_t m;
        for (m = 0; m < FX_NMOD; m++) {
            int32_t x = 4 + (int32_t)(m % 3u) * 78, y = (int32_t)(m / 3u) * 72;
            int sel = m == ui.fxmod, ok = fx_mod_ok(m);
            uint16_t bg = lz_tile_bg();                 /* (a rounded card, the one picked framed THEME) */
            lz_tile(x, y, 76, 68, sel, T_THEME);
            cv_bg = bg;
            GFX_HOOK_ALIGN(0, y + 3, 0, y + 12, AL_V | AL_PASS, "FX module name");
            cv_text_on(x + 4, y + 3, &AF_X, FX_MOD[m], sel ? T_THEME : T_TEXT, bg);
            if (ok)
                fxg_mod(m, x + 4, y + 14, 68, 50);
            else
                cv_text_on(x + 4, y + 33, &AF_X, "ECO", T_DIM, bg);
            cv_bg = T_SURF;
        }
    } else {
        uint32_t m = fx_mod_of(ui.page), k = fx_mod_k(ui.page);
        fx_dots(fx_mod_n(m), k, FX_MOD[m], pg->title);
        fxg_mod(m, 8, 14, 224, 118);
    }
}
