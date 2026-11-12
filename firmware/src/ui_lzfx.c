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
/* ---- the live state (once a frame, fxg_tick): the output's level and its onsets (a hit: the level jumped), the peak held,
 * a change of any value (the knob just turned), a few particles; all of it the screen's own, nothing of the audio path */
static struct {
    uint32_t fr, key, since, rnd;
    int32_t e, hit, peak, hold, chg;
    uint8_t gd[2][32], gk, gt;                          /* DIST's curve: the last drawn and the one before it, its key, the ghost's time */
} FL;
typedef struct { int16_t x, y; int8_t vx, vy; uint8_t life, id; } fxpt_t;   /* (a particle: 1/8 block, a block per 8 per frame) */
static fxpt_t FPT[20];
static uint32_t fxs_fr[4];
static int32_t fxg_rnd(void) { FL.rnd = FL.rnd * 1664525u + 1013904223u; return (int32_t)(FL.rnd >> 16); }
static void fxg_tick(const track_t *t)
{
    uint32_t key = 0, i;
    int32_t e;
    if (FL.fr == ui.frame)
        return;
    FL.fr = ui.frame;
    e = bg_energy();
    if (song.playing && e - FL.e > 16) { FL.hit = 8; FL.since = 0; }
    else if (FL.hit > 0) FL.hit--;
    FL.since++;
    FL.e = e;
    if (e >= FL.peak) { FL.peak = e; FL.hold = 14; }
    else if (FL.hold > 0) FL.hold--;
    else FL.peak = FL.peak > e + 5 ? FL.peak - 5 : e;
    for (i = 0; i < G_COUNT; i++) key = key * 31u + (uint32_t)song.g[i];
    for (i = 0; i < sizeof t->p / sizeof t->p[0]; i++) key = key * 31u + (uint32_t)t->p[i];
    if (FL.key && key != FL.key) FL.chg = 10;           /* (a value turned: the picture's frame lights) */
    else if (FL.chg > 0) FL.chg--;
    FL.key = key ? key : 1u;
}
/* a block the colour of a hit: brighter by the hit's flash */
static uint16_t fxg_hot(int32_t v) { return FXP_H(clamp(v + FL.hit * 24, 0, 255)); }
static void fxs_spawn(uint8_t id, int32_t bx, int32_t by, int32_t vx, int32_t vy, int32_t life)
{
    uint32_t i;
    for (i = 0; i < NELEM(FPT); i++)
        if (!FPT[i].life) {
            FPT[i].x = (int16_t)(bx * 8), FPT[i].y = (int16_t)(by * 8), FPT[i].vx = (int8_t)vx, FPT[i].vy = (int8_t)vy;
            FPT[i].life = (uint8_t)life, FPT[i].id = id;
            return;
        }
}
/* moves (once a frame) and draws picture id's particles, in blocks of the picture at x, y (FP set) */
static void fxs_run(uint8_t id, int32_t x, int32_t y, int32_t cols, int32_t rows)
{
    uint32_t i, mv = fxs_fr[id] != FL.fr;
    fxs_fr[id] = FL.fr;
    for (i = 0; i < NELEM(FPT); i++) {
        fxpt_t *p = &FPT[i];
        int32_t bx, by;
        if (!p->life || p->id != id)
            continue;
        if (mv) {
            p->x += p->vx, p->y += p->vy;
            p->life--;
        }
        bx = p->x / 8, by = p->y / 8;
        if (bx < 0 || by < 0 || bx >= cols || by >= rows) { p->life = 0; continue; }
        fxb(x, y, bx, by, 1, 1, p->life > 5 ? T_TEXT : FXP_H(p->life * 45));
    }
}
/* the output's wave across n columns (the last 512 samples), full scale; a sine while it is quiet */
static int32_t fxg_samp(int32_t i, int32_t n)
{
    int32_t v;
    if (!fxg_en())
        return bg_sin((uint32_t)(i * 65536 * 2 / (n ? n : 1)));
    v = scope_buf[(scope_w - 512u + (uint32_t)(i * 512 / (n ? n : 1))) & (SCOPE_N - 1u)] * 27 / 10;
    return clamp(v, -32767, 32767);
}
static void fxg_delay(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    char b[24];
    const char *u;
    int32_t i, a = 1000, mix = song.g[G_DMIX], fb = song.g[G_DFDBK], en = fxg_en();
    int32_t tr = RATIO(&GP[G_DTIME], enum_rank(&GP[G_DTIME], song.g[G_DTIME]));
    int32_t cols, rows, t0, bw, sb, pulse, scan;
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : 1, bw = FP > 2 ? 3 : 2;
    sb = w * (500 + tr) / 10000 / FP;
    if (sb < bw + 1) sb = bw + 1;
    pulse = en ? (int32_t)(FL.since * 2u / (uint32_t)FP % (uint32_t)(9 * sb + 6)) : -99;   /* (starts at each hit) */
    scan = (int32_t)(ui.frame / 2u);
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
        for (k = 0; k < bh; k++)                        /* (a block a row, the ramp stepping up the bar; the top bright; a scan climbs) */
            fxb(x, y, bx, rows - 2 - k, bw, 1, k == bh - 1 || hit ? fxg_hot(hit ? 255 : 150 + a * 105 / 1000) :
                en && k == scan % (bh + 5) ? FXP_H(255) : fxg_hot(40 + a * 150 / 1000 + k * 40 / bh));
        if (song.g[G_DPING] && bh > 2)                  /* (PING-PONG: the echo's side as a notch) */
            fxb(x, y, bx + (i & 1 ? bw - 1 : 0), rows - 2 - bh, 1, 1, T_DIM);
        if (hit && w > 120 && pulse == i * sb)          /* (the echo arriving throws a spark) */
            fxs_spawn(0, bx + bw / 2, rows - 2 - bh, (fxg_rnd() & 7) - 3, -5, 9);
        a = a * (fb > 127 ? 127 : fb) / 127;
    }
    if (pulse >= 0 && pulse + 2 < cols)                 /* (the pulse itself, on the line) */
        fxb(x, y, pulse, rows - 1, 2, 1, T_TEXT);
    if (w > 120) fxs_run(0, x, y, cols, rows);
    if (lab) {
        param_format(&GP[G_DTIME], song.g[G_DTIME], b, &u);
        fxg_label(x, y, b, T_MID, 0);
    }
}
static void fxg_reverb(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    int32_t i, sz = song.g[G_RSIZE], ty = song.g[G_RTYPE], e = 1000, cols, rows, t0, top, hh, pre, en = fxg_en(), sweep, k;
    int32_t hs[64], sp;
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : 0;
    top = t0, hh = rows - t0;
    k = 1000 - (5000 - sz * 4300 / 127) / (cols ? cols : 1) * 2;
    pre = clamp(song.g[G_RPRE], 0, 100) * cols / 4 / 100;   /* (the pre-delay: a gap before the tail) */
    sp = en ? (int32_t)(FL.since * 3u / (uint32_t)FP) : -1;   /* (the sweep rides the tail from each hit) */
    sweep = sp >= 0 ? sp % (cols + 8) : -1;
    fxb(x, y, 0, rows - 1, pre + 1, 1, T_LINE);
    for (i = 0; i < cols && i < 64; i++) hs[i] = 0;
    for (i = pre; i < cols; i++) {                      /* the decay, a column of blocks each: its height the envelope */
        int32_t j = i - pre, rise = cols / 25 + 1, env = j < rise ? e * j / rise : e, n = 1000, ch;
        if (ty == 4) n = 500 + (bg_sin((uint32_t)(j * 65536 / cols * 6)) >> 6) * 500 / 512;
        else if (ty == 1) n = 700 + (bg_sin((uint32_t)(j * 65536 / cols * 14)) >> 6) * 300 / 512;
        ch = hh * 9 / 10 * env / 1000 * n / 1000;
        if (en)                                         /* (the tail grows with the output, and with each hit) */
            ch = ch * (110 + clamp(en, 0, 288) * 146 / 288) / 256 * (256 + FL.hit * 6) / 256;
        if (ch > hh - 1) ch = hh - 1;
        if (i < 64) hs[i] = ch;
        if (ch > 0) {
            uint16_t c = i == sweep ? T_TEXT : fxg_hot(230 - i * 190 / cols);
            fxb(x, y, i, top + hh - ch, 1, ch, c);
            if (!(i & 1))                               /* (a dithered edge: every other column a block taller) */
                fxb(x, y, i, top + hh - ch - 1, 1, 1, c);
        }
        if (j >= rise) e = e * k / 1000;
    }
    if (sweep >= 0 && sweep < cols)                     /* (the sweep's foot on the line, seen when the tail is low) */
        fxb(x, y, sweep, rows - 1, 1, 1, T_TEXT);
    if (w > 120 && en) {                                /* sparkles twinkling over the tail */
        if (!(ui.frame & 1) && cols > pre + 4) {
            int32_t c = pre + 2 + fxg_rnd() % (cols - pre - 3);
            if (c < 64 && hs[c] > 1) fxs_spawn(1, c, top + hh - hs[c] - 1, 0, -2, 8);
        }
        fxs_run(1, x, y, cols, rows);
    }
    if (lab)
        fxg_label(x, y, N_RTYPE[clamp(ty, 0, 5)], T_MID, 0);
}
static void fxg_chorus(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    int32_t k, i, dep = song.g[G_CDEPTH], st = song.g[G_CSTER], cols, rows, t0, mid;
    uint32_t ph = fxg_en() ? ui.frame * (uint32_t)(song.g[G_CRATE] + 8) * 48u : 0u;   /* (running at RATE while it sounds) */
    char b[16];
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : 1, mid = t0 + (rows - t0) / 2;
    for (k = 0; k < 3; k++) {
        int32_t py = 0, tap = (k + 1) * cols / 4, ty = 0;
        for (i = 0; i < cols; i++) {
            int32_t amp = (rows - t0) * 38 / 100 * dep / 127 * (10 + 2 * k) / 10 * (128 + st) / 128;
            int32_t yy = mid + (bg_sin((uint32_t)(i * 65536 / cols * 2) + (uint32_t)k * 21000u + ph) * amp >> 15);
            yy = clamp(yy, t0, rows - 2);
            fxb_trace(x, y, i, yy, py, FP > 2 ? 2 : 1, fxg_hot(60 + k * 80));
            if (i == tap) ty = yy;
            py = yy;
        }
        if (fxg_en() && w > 120)                        /* (a tap: the delay line's read head riding its wave) */
            fxb(x, y, tap - 1, ty - 1, 3, FP > 2 ? 4 : 3, T_TEXT);
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
    int32_t cols, rows, t0, cb, x0, bb, py = 0;
    char b[32];
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, t0 = lab ? 6 : w > 120 ? 4 : 1, cb = cols / 16, x0 = (cols - 16 * cb) / 2, bb = rows - 2 - t0;
    for (i = 0; i < 16u; i++) {
        int on = ((pat >> i) & 1u) && mode;
        int32_t cx = x0 + (int32_t)i * cb, k, bh = on ? bb : 2;
        for (k = 0; k < bh; k++)                        /* (a lit step: stacked blocks, the ramp up with DEPTH) */
            fxb(x, y, cx, t0 + bb - 1 - k + 1, cb > 1 ? cb - 1 : 1, 1,
                on ? fxg_hot(70 + song.g[G_SLDEP] * 110 / 127 + k * 75 / bb) : T_RAISE);
        if (i == play) {                                /* (the one playing: a cap and a foot of white) */
            fxb(x, y, cx, t0 - 1 + 1, cb > 1 ? cb - 1 : 1, 1, T_TEXT);
            fxb(x, y, cx, t0 + bb + 1, cb > 1 ? cb - 1 : 1, 1, T_TEXT);
        }
        if (!(i & 3))                                   /* (a beat's tick under every fourth step) */
            fxb(x, y, cx, rows - 1, 1, 1, T_DIM);
    }
    if (w > 120) {                                      /* the gate chopping the wave: the output across the steps, flat where shut */
        int32_t mid = t0 + 1 + bb / 2, amp = bb / 2 - 1;
        for (i = 0; i < (uint32_t)(cb * 16); i++) {
            int32_t st = (int32_t)i / cb, yy;
            int on = ((pat >> st) & 1u) && mode;
            yy = on || !mode ? mid - (int32_t)(fxg_samp((int32_t)i, cb * 16) * (int64_t)amp >> 15) : mid;
            fxb_trace(x, y, x0 + (int32_t)i, yy, py, 1, on || !mode ? T_TEXT : T_DIM);
            py = yy;
        }
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
    int32_t rel = song.g[G_DREL], cols, rows, nseg, seg, mrows, t0, ch, cy, clip = song.g[G_CLIP], ys[64], dx;
    FP = fxp_size(w);
    cols = w / FP, rows = h / FP, mrows = FP > 2 ? 3 : 2;
    seg = 3, nseg = cols / seg;                         /* (a segmented LED meter: 2 blocks lit, 1 dark) */
    for (i = 0; i < nseg; i++) {
        int on = i * 256 / nseg < lvl;
        fxb(x, y, 1 + i * seg, 1, seg - 1, mrows, on ? fxg_hot(i * 250 / nseg) : T_RAISE);
    }
    if (song.playing || en > 2) {                       /* the peak held, falling slowly */
        int32_t ps = clamp(FL.peak * nseg / 256, 0, nseg - 1);
        fxb(x, y, 1 + ps * seg, 0, seg - 1, 1, T_TEXT);
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
            fxb_trace(x, y, i, yy, py, 1, fxg_hot(190));
            if (!(i & 1))
                fxb(x, y, i, yy + 2, 1, 1, T_LINE);
            if (i < 64) ys[i] = yy;
            py = yy;
            e = e * (1000 - kk / cols * 2) / 1000;
        }
    }
    dx = (int32_t)(FL.since * (uint32_t)cols / 36u);    /* the duck playing out: a dot riding the curve from each hit */
    if (song.playing && dx < cols && dx < 64)
        fxb(x, y, dx, ys[dx] - 1, 2, 2, T_TEXT);
}
static void fxg_punch(int32_t x, int32_t y, int32_t w, int32_t h, int lab)
{
    static const uint8_t RES[3] = {8, 16, 32};
    int32_t steps = song.g[G_PFXLEN] * RES[clamp(song.g[G_PFXRES], 0, 2)] / 8, i, cw, t0 = lab ? 14 : w > 120 ? 40 : 2;
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
                cv_rect(x + i * cw, y + t0 + bh - (k + 1) * FP, bw, FP, at ? T_TEXT : fxg_hot(90 + k * 165 / (bh / FP)));
        } else
            cv_rect(x + i * cw, y + h - 6, bw, 2, T_LINE);
    }
    if (w > 120) {                                      /* what PUNCH does to a hit: the dry level, and the wet one with its spike */
        int32_t pn = song.g[G_PUNCH], c, base = 3, cols = 44;
        for (c = 0; c < cols; c++) {
            int32_t wet = c < 3 ? base + 1 + pn * 4 / 100 : c < 14 ? base + pn * 3 / 100 : base - (c - 14) / 12;
            int32_t dry = c < 3 ? base + 1 : c < 14 ? base : base - (c - 14) / 12;
            if (dry < 1) dry = 1;
            if (wet < 1) wet = 1;
            fxb(x, y, 1 + c, 8 - wet, 1, wet, fxg_hot(110 + wet * 20));
            fxb(x, y, 1 + c, 8 - dry, 1, 1, T_DIM);
        }
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
                i == (int32_t)sl_ui_step ? T_THEME : i < len ? fxg_hot(100 + (v + 24) * 3 + k * 20 / bh) : T_RAISE);
    }
}
/* the distortion's transfer curve in a square of s px (blocks), the level a dot on it; when the drive or TYPE just changed the
 * old curve stays dim a moment (the ghost) */
static void fxd_curve(const track_t *t, int32_t x, int32_t y, int32_t s)
{
    int32_t d = t->p[P_DIST], g = 4096 + d * d * 2, i, py = 0, n, cy;
    uint32_t type = (uint32_t)t->p[P_DTYPE] > DT_FUZZ ? DT_SOFT : (uint32_t)t->p[P_DTYPE];
    uint8_t gk = (uint8_t)(d * 7 + type * 31);
    FP = s >= 100 ? 4 : 2;
    n = s / FP, cy = n / 2;
    if (n > 32) n = 32;
    if (s >= 100 && gk != FL.gk) {                      /* (the picture moved: the last one becomes the ghost) */
        for (i = 0; i < 32; i++) FL.gd[1][i] = FL.gd[0][i];
        FL.gk = gk, FL.gt = 14;
    }
    fxb(x, y, 0, 0, n, 1, T_LINE), fxb(x, y, 0, n - 1, n, 1, T_LINE);
    fxb(x, y, 0, 0, 1, n, T_LINE), fxb(x, y, n - 1, 0, 1, n, T_LINE);
    fxb_dots(x, y, 1, cy, n - 2, 1, T_LINE);
    fxb_dots(x, y, cy, 1, n - 2, 0, T_LINE);
    if (s >= 100 && FL.gt) {
        for (i = 0; i < n; i++) fxb(x, y, i, FL.gd[1][i], 1, 1, T_DIM);
        FL.gt--;
    }
    for (i = 0; i < n; i++) {
        int32_t xx = (i - n / 2) * 131072 / (n / 2), yy = cy - tk_dist_y(type, xx, d ? g : 4096, d) * (n / 2 - 2) / 32767;
        yy = clamp(yy, 1, n - 3);
        fxb_trace(x, y, i, yy, py, FP > 2 ? 2 : 1, fxg_hot(110 + d));
        if (s >= 100) FL.gd[0][i] = (uint8_t)yy;
        py = yy;
    }
    if (fxg_en() && n > 12) {                           /* the level now, a dot on the curve */
        int32_t xx = clamp(bg_energy(), 0, 256) * 131072 / 256, px = cy + xx * (n / 2) / 131072;
        int32_t yy = clamp(cy - tk_dist_y(type, xx, d ? g : 4096, d) * (n / 2 - 2) / 32767, 2, n - 4);
        fxb(x, y, clamp(px, 2, n - 4), yy - 1, 2, 2, T_TEXT);
    }
}
/* the output's own wave (a sine when quiet) in, or out through the distortion (its clipped peaks hot), over w x h px */
static void fxd_scope(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, int out)
{
    int32_t d = t->p[P_DIST], g = 4096 + d * d * 2, i, py = 0, cols, rows, mid;
    uint32_t type = (uint32_t)t->p[P_DTYPE] > DT_FUZZ ? DT_SOFT : (uint32_t)t->p[P_DTYPE];
    FP = 2;
    cols = w / FP, rows = h / FP, mid = rows / 2;
    fxb(x, y, 0, 0, cols, 1, T_LINE), fxb(x, y, 0, rows - 1, cols, 1, T_LINE);
    fxb(x, y, 0, 0, 1, rows, T_LINE), fxb(x, y, cols - 1, 0, 1, rows, T_LINE);
    fxb_dots(x, y, 1, mid, cols - 2, 1, T_LINE);
    for (i = 1; i < cols - 1; i++) {
        int32_t v = fxg_samp(i - 1, cols - 2), yy, hot = 0;
        if (out) {
            v = tk_dist_y(type, v * 4, d ? g : 4096, d);
            hot = (v > 29000 || v < -29000) && d > 8;   /* (a peak flattened against the ceiling) */
        }
        yy = clamp(mid - v * (mid - 3) / 32767, 1, rows - 3);
        fxb_trace(x, y, i, yy, py, 1, hot ? T_TEXT : out ? fxg_hot(110 + d) : T_DIM);
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
    static int16_t trail[5];                            /* the ball's last places (blocks from the middle), a frame apart */
    static uint32_t trail_fr;
    int32_t dep = t->p[P_APDEP], i, cols, rows, lane, sw, tr0, tr1, tm, py = 0, ba;
    uint32_t wv = (uint32_t)clamp(t->p[P_APWAVE], 0, 4), ph = fxg_en() ? ui.frame * (uint32_t)(t->p[P_APRATE] + 6) * 96u : 0u;
    FP = 4;
    cols = w / FP, rows = h / FP, lane = 6, sw = dep * (cols / 2 - 8) / 127;
    ba = (int32_t)(fxg_lfo(wv, ph) * (int64_t)sw >> 15);
    if (trail_fr != ui.frame) {
        for (i = 4; i > 0; i--) trail[i] = trail[i - 1];
        trail[0] = (int16_t)ba;
        trail_fr = ui.frame;
    }
    fxb_dots(x, y, 3, lane + 2, cols - 6, 1, T_LINE);   /* the lane, L to R, a tick at the middle */
    fxb(x, y, cols / 2, lane - 1, 1, 7, T_LINE);
    fxg_label(x, y + (lane + 2) * FP - 5, "L", T_DIM, 0);
    fxg_label(x + w, y + (lane + 2) * FP - 5, "R", T_DIM, 1);
    if (sw > 0)
        fxb(x, y, cols / 2 - sw, lane + 2, 2 * sw + 1, 1, FXP_H(110 + dep));
    if (fxg_en())                                       /* the trail: smaller and dimmer the older */
        for (i = 4; i > 0; i--)
            fxb(x, y, cols / 2 + trail[i] - 1, lane + 1, 3, 3, FXP_H(40 + (5 - i) * 30));
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
 * two notches where the sweep has them now (voice.c ph_ph: ~0.41 and ~2.4 times the break) marked and tinted, AMOUNT their
 * depth, FDBK wider (a sketch: each a dip of its own) */
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
        int near = (d0 < 0 ? -d0 : d0) < wd / 3 || (d1 < 0 ? -d1 : d1) < wd / 3;
        uint16_t col;
        ch = hh - hh * dep * (dip > 4096 ? 4096 : dip) / (127 * 4096);
        if (ch < 1) ch = 1;
        col = FXP_H(60 + ch * 190 / hh);
        fxb(x, y, i, top + hh - ch, 1, ch, col);
        if (!(i & 1))
            fxb(x, y, i, top + hh - ch - 1, 1, 1, col);
        if (near && dep > 8)                            /* (inside a notch: the floor of it lit) */
            fxb(x, y, i, top + hh - 2, 1, 2, T_TEXT);
    }
    fxb(x, y, 0, rows, cols, 1, T_LINE);
    fxb(x, y, (c - 325) * cols / 2048 - 1, rows, 3, 1, T_TEXT);   /* (the notches' places) */
    fxb(x, y, (c + 325) * cols / 2048 - 1, rows, 3, 1, T_TEXT);
    fxg_label(x, y + h - 8, "40", T_DIM, 0);
    fxg_label(x + w, y + h - 8, "10K", T_DIM, 1);
}
/* the sends' pictures: one card a send (DIST CHORUS DELAY REVERB, above its knob), the picture drawn as the effect is and
 * dithered away by the send's level (ordered dither: nothing at 0, whole at full), the track's stem down into it with a
 * block of the sound running down it */
static void fxg_sends(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h)
{
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    int32_t i, cw = w / 4, ih = h - 16;
    for (i = 0; i < 4; i++) {
        int32_t v = t->p[P_DIST + i], cx = x + i * cw + 2, px = cx + 4, pw = cw - 12, py = y + 14, ph = ih - 8, bx, by;
        int32_t lv = v ? 7 + v * 9 / 127 : 0;           /* (0: hidden whole; 16: every block) */
        uint16_t save = cv_bg;
        cv_rect(cx + cw / 2 - 3 - 2, y, 4, 10, v ? fxg_hot(60 + v * 195 / 127) : T_LINE);   /* (the stem) */
        if (v && fxg_en())                              /* (the sound running down it) */
            cv_rect(cx + cw / 2 - 3 - 2, y + (int32_t)((ui.frame * 2u + (uint32_t)i * 3u) % 8u), 4, 2, T_TEXT);
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
    fxg_tick(t);
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
            if (ok) {
                fxg_mod(m, x + 4, y + 14, 68, 50);
                FP = 2;                                 /* (a tiny sprite top right: DELAY's ring going out, MASTER's cone thumping) */
                if (m == 0 && fxg_en()) {
                    int32_t r = (int32_t)(FL.since / 2u % 4u);
                    fxb(x + 58, y + 4, 2 - r / 2, 2 - r / 2, 1 + r, 1 + r, FXP_H(60 + (4 - r) * 50));
                } else if (m == 4) {
                    int32_t th = FL.hit > 3 ? 1 : 0;
                    fxb(x + 58, y + 5, 0, 1, 1, 2, T_TEXT);
                    fxb(x + 58, y + 4, 1, 0 - th, 1 + th, 4 + 2 * th, T_TEXT);
                }
            } else
                cv_text_on(x + 4, y + 33, &AF_X, "ECO", T_DIM, bg);
            cv_bg = T_SURF;
        }
    } else {
        uint32_t m = fx_mod_of(ui.page), k = fx_mod_k(ui.page);
        fx_dots(fx_mod_n(m), k, FX_MOD[m], pg->title);
        fxg_mod(m, 8, 14, 224, 118);
    }
    if (FL.chg > 0 && lv != FXL_BUSES)                  /* a value just turned: the picture's frame lights, fading */
        cv_frame(2, 12, 236, 124, FL.chg > 4 ? T_THEME : T_DIM);
}
