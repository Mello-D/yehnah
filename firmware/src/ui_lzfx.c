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
/* the dots of a line: every other block */
static void fxb_dots(int32_t x, int32_t y, int32_t bx, int32_t by, int32_t n, int horiz, uint16_t c)
{
    int32_t i;
    for (i = 0; i < n; i += 2)
        fxb(x, y, bx + (horiz ? i : 0), by + (horiz ? 0 : i), 1, 1, c);
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
