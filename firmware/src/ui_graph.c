/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (graph_y() .. + graph_h()) between the header and the knob cells: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* (the big FM-1) a repaint the tablet asked for and nothing else (tab_screen.c's resync / trickle: lcd_tab_force, made
 * ui.force by ui_draw, lcd_quiet that frame): the panel already shows it, and so does the tablet (what it lacked the
 * resync filled with the background), so a fill that only clears under what is drawn next (lcd_clear: draw_tracks',
 * draw_frame's, the count-in's) is skipped. Drawn, it showed as a flash, the background before the drawing (MIXER /
 * SENDS, twice a second while the tablet was live): on the panel, then (sent to the tablet alone) on the tablet */
static uint8_t lcd_tab_force, lcd_quiet;
static void lcd_clear(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    if (!lcd_quiet)
        lcd_fill(x, y, w, h, c);
}

/* a strip cleared to BG */
static uint8_t glo_k, about_glo;                        /* (2026-11-02) GLO: the knob last turned on a MENU row's page (its
                                                         * values the panel's chips); ABOUT opened from GLO > TOOLS */
static void graph_glo(const page_t *pg);                /* (ui_menu.c) */
static void graph_sets(void);
static void lz_begin(uint32_t w, uint32_t h, uint32_t x, uint32_t y)
{
    (void)x;
    (void)y;
    cv_begin(w, h, T_BG);
}
static void lz_blit(uint32_t x, uint32_t y)
{
    cv_halo = 0;
    cv_blit(x, y);
}
/* (0.8.9) a tile in a lens's panel (FX's modules, ENV's tracks, MOD's slots): a rounded RAISE box (r 4), the one
 * picked framed in fc (the mock-up's card and its stroke). What lies under its words: lz_tile_bg */
static uint16_t lz_tile_bg(void) { return T_RAISE; }
static void lz_tile(int32_t x, int32_t y, int32_t w, int32_t h, int sel, uint16_t fc)
{
    if (sel) {
        cv_rrect(x, y, w, h, 4, fc, cv_bg);
        cv_rrect(x + 1, y + 1, w - 2, h - 2, 3, T_RAISE, fc);
    } else
        cv_rrect(x, y, w, h, 4, T_RAISE, cv_bg);
}

static const int32_t graph_ht = 100;                 /* the height ADSR and LFO are drawn on */
#include "ui_organic.c"                              /* Yehnah's organic line art (drawn by code) */
#include "ui_fm6op.c"                                /* (0.6.10, from Felucca 1.5) EDIT > OPERATOR, OP ENV, OPERATOR 2 */
static uint16_t heat_col(int32_t v);
#include "ui_being.c"                                /* (0.5) the beings' helpers left: the level, the sine, heat */
#include "ui_engline.c"                              /* (chord-harp fork) the engines' line drawings (was ui_engvis.c's creatures) */

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t a = 4 + t->p[pg->id[0]] * 50 / 127, d = 6 + t->p[pg->id[1]] * 50 / 127, r = 6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_line_t(x0, bot, x1, top, c, 2);                                  /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line_t(px, py, x1 + i, EGY(lvl), c, 2);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line_t(px, py, x3, EGY(sus), c, 2);                               /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line_t(px, py, x3 + i, EGY(sus * e >> 15), c, 2);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
    if (pg->id[0] == P_ATK) {                           /* (0.5) the track's envelope now: the loudest voice's */
        const voice_t *bv = 0;                          /* level as a dot on the curve, a line down to the axis */
        int32_t lvl, x;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].stage && t->v[i].stage < 4u && (!bv || t->v[i].env > bv->env))
                bv = &t->v[i];
        if (bv) {
            lvl = (int32_t)(((int64_t)bv->env * 1000) >> 24);
            if (bv->stage == 1u)
                x = x0 + a * lvl / 1000;
            else if (bv->stage == 2u)
                x = x1 + (sus < 1000 ? clamp((1000 - lvl) * d / (1000 - sus), 0, d) : 0);
            else
                x = x3 + (sus > 0 ? clamp((sus - lvl) * r / sus, 0, r) : r);
            cv_rect(x, EGY(lvl), 1, bot + 2 - EGY(lvl), T_MID);
            cv_rrect(x - 3, EGY(lvl) - 3, 7, 7, 3, T_TEXT, T_SURF);
        }
    }
#undef EGY
}

/* LFO: the wave as it runs (0.5): the phase now at the playhead (a quarter in), two cycles across, scrolling
 * at the LFO's own rate; behind the playhead dimmer (what it played), the level now a dot, its depth after FADE */
static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, xp = PANEL_W / 4;
    uint32_t span = 0xFFFFFFFFu / (PANEL_W / 2u), ph = t->lfo_ph - (uint32_t)xp * span;
    int32_t now = cy - mulq15(t->lfo_val, t->lfo_fade) * a / 32768;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {                  /* (lfo_wave only reads the track) */
        uint32_t px = ph + (uint32_t)x * span;
        int32_t y = cy - lfo_wave((track_t *)t, px) * a / 32768;
        if (t->p[P_LWAVE] == 4)                      /* S&H: a value per cycle part, held */
            y = cy - ((int32_t)((((px >> 29) + 1u) * 2654435761u) >> 16) - 32768) * a / 32768;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y,
                      x < xp ? ux_mix(T_SURF, c, 45) : c, 2);
        py = y;
    }
    cv_rect(PANEL_X0 + xp, cy - a - 4, 1, 2 * a + 8, T_MID);   /* the playhead */
    cv_rrect(PANEL_X0 + xp - 3, now - 3, 7, 7, 3, T_TEXT, T_SURF);
}


/* (0.6.45) EDIT > MORPH: a cycle of the wave as it morphs (from WAVE towards the next ones), heat coloured by its
 * height; under it the two waves it is between */
/* (0.8.5) the piano roll of the cursor's bar, as the approved mock-up (LENTE's BAR, under the bars' strip): its 16
 * steps as columns over the panel's width (x 4 .. 236, 14 or 15 px), PR_ROWS semitone rows of PR_RH px (4), the
 * highest on top, on a BG area; rows in the track's scale (SCL ROOT / SCALE; CHR: the white keys) tinted LANE; step
 * lines GRID, every 4th LINE. A note is a bar in its column, a px in from either side, PR_RH - 1 tall, as hot as it
 * plays (an accent: the highlight; another palette: notes ACCENT, accents TEXT); chords stacked; lane hits as their
 * notes; a RATCH step's bars in its 2..4 parts; a TIE carries the bars of the note before through its column; a REST
 * is empty; a slide reaches 2 px into the next column. On SEQ > NOTE the note in focus is the accent (the chord, CHORD:
 * all of it). A step with a component: a 3 px mark at the column's top (an arpeggio ACCENT, the others THEME). Notes
 * outside the view: a 1 px mark on the edge. The cursor: a THEME frame round its column; the playhead: a 1 px ACCENT
 * line at its column's left; a parameter lock: an ACCENT mark at the column's foot; a key held on the track: its row
 * a 2 px ACCENT mark at the left. The view (proll.lo, the bottom row) fits the notes of the page, else centres on the
 * cursor's note, and follows in steps (a third of the way per frame); graph_signature() holds it. (Until 0.8.4 a
 * keyboard strip and the C rows' names at the left, 21 rows of 5 px, 12 px columns.) */
#define PR_ROWS ((int32_t)proll.rows)               /* semitones shown: 30 (2.5 octaves) */
#define PR_RH 4                                     /* px per row */
#define PR_Y0 ((int32_t)proll.y0)                   /* the top row (canvas y): 20, under the bars' strip */
#define PR_RX 4                                     /* the roll: x 4 .. 236 */
#define PR_RW 232
#define PR_CX(i) (PR_RX + (int32_t)(i) * PR_RW / 16)   /* column i's left */
#define PR_CWI(i) (PR_CX((i) + 1) - PR_CX(i))          /* .. its width */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint8_t rows, y0;                                 /* PR_ROWS, PR_Y0: pr_geom */
    uint32_t frame;
} proll = {0, 0, 0, 0, 30, 20, 0};
static int lens_strip(void);
static void pr_geom(void)                             /* the roll's rows: under the bars' strip (a layer's map: the panel) */
{
    int s = lens_strip();
    proll.y0 = s ? 20u : 2u;
    proll.rows = (uint8_t)(((int32_t)graph_h() - (int32_t)proll.y0 - (s ? 0 : 2)) / PR_RH);
}
static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3; chord-harp: a CHORD's notes whole,
                                                         * seq.c step_expand), then its lane hits (-1: none) */
{
    uint8_t nl[4 + NLANE], own[4 + NLANE];
    uint32_t n = step_expand(st, step_chord_id(st), nl, own);
    if (j < n)
        return nl[j];
    return j - n < NLANE && ((st->hit >> (j - n)) & 1u) ? DRUM_LANE_NOTE[j - n] : -1;
}
/* (chord-harp) the notes a step goes on with from the column before (a CHORD sliding into the same chord: harp_tie):
 * their bars drawn on from it, unbroken (pr_bars) */
static uint8_t pr_jn[4 + NLANE], pr_jnn;
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_on[k] && kb_trk[k] == song.sel) {
            uint32_t i;
            for (i = 0; i < kb_on[k]; i++) {            /* (0.6.81: a chord's every note) */
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_snd[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
        }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap;
    pr_geom();
    snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || ANIM_SNAP();
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++) {
        if (pw >= 1) {
            cv_rect(x + k * (pw + 1), y, pw, h, c);
        } else {                                        /* (2026-11-10: up to 8 parts in a narrow bar: at fractions of the width) */
            int32_t x0 = x + k * w / (int32_t)hits, x1 = x + (k + 1) * w / (int32_t)hits;
            cv_rect(x0, y, x1 - x0 > 1 ? x1 - x0 - 1 : 1, h, c);
        }
    }
}
/* a note's colour in the lens's pictures: ACCENT, an accent TEXT (the highlight) */
static uint16_t lz_note(const step_t *st, int acc, uint16_t c)
{
    (void)st; (void)c;
    return acc ? T_TEXT : T_ACCENT;
}
/* a step's component mark (3 x 3 at y): an arpeggio ACCENT, another THEME; none: nothing */
static void lz_comp_mark(const step_t *st, int32_t x, int32_t y)
{
    uint32_t c = st->comp;
    if (!c && !st->comp2)
        return;
    cv_rect(x - 1, y, 3, 3, c >= CP_ARPN && c <= CP_ARPN + 2u ? T_ACCENT : T_THEME);
}
/* (0.8.3) LENTE's bars; (0.8.5) drawn as the approved mock-up has them: a bar of the pattern as a cell (RAISE; the bar
 * edited TINT), its sixteen steps as a tiny roll (each note-on step's first note a 2 px dash over the pattern's range;
 * a kit: its lanes' hits, the kick on top), its number at the top left (DIM; the bar edited THEME, the bar playing
 * ACCENT); a bar past LEN an empty dark cell. Two sizes: PATTERN's four big cells and BAR's strip (16 px) */
static void pat_range(const track_t *t, int32_t *plo, int32_t *phi)   /* the pattern's notes, 30 semitones at least */
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, j;
    int32_t lo = 127, hi = -1;
    for (i = 0; i < len; i++)
        for (j = 0; j < 4u + NLANE && seq_steps(t)[i].time == ST_NOTE && pr_note(&seq_steps(t)[i], j) >= 0 &&
                    (j < seq_steps(t)[i].n || step_chord_id(&seq_steps(t)[i])); j++) {   /* (chord-harp: a CHORD's too) */
            int32_t n = pr_note(&seq_steps(t)[i], j);
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    if (hi < lo)
        lo = hi = 60;
    if (hi - lo < 29) {                              /* (centred in the window) */
        lo -= (29 - (hi - lo)) / 2;
        hi = lo + 29;
    }
    *plo = lo;
    *phi = hi;
}
static void bar_cell(const track_t *t, uint32_t b, int32_t x, int32_t y, int32_t w, int32_t h, int32_t lo, int32_t hi,
                     uint16_t c)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, l, dr = drum_track(t);
    int sel = b == ui.bank;
    int32_t sw = w - 4, top = y + (h > 20 ? 14 : 3), nh = h - (h > 20 ? 17 : 5);
    uint16_t bg = T_RAISE, nc;
    char s[2];
    if (b * 16u >= len) {                            /* past LEN: a dark cell */
        cv_rect(x, y, w, h, T_BG);
        return;
    }
    if (sel) {                                       /* a rounded card, the bar edited framed THEME */
        cv_rrect(x, y, w, h, 3, T_THEME, cv_bg);
        cv_rrect(x + 1, y + 1, w - 2, h - 2, 2, T_RAISE, T_THEME);
    } else
        cv_rrect(x, y, w, h, 3, T_RAISE, cv_bg);
    for (i = 0; i < 16u && b * 16u + i < len; i++) {
        const step_t *st = &seq_steps(t)[b * 16u + i];
        int32_t sx = x + 2 + (int32_t)i * sw / 16, mw = sw / 16 - 1 > 1 ? sw / 16 - 1 : 1;
        if (!step_on(st))
            continue;
        if (dr) {
            for (l = 0; l < NLANE; l++)
                if ((step_lanes(st) >> l) & 1u)
                    cv_rect(sx, top + (int32_t)l * nh / (int32_t)NLANE, mw, nh / (int32_t)NLANE > 2 ? 2 : 1,
                            lz_note(st, (step_accents(st) >> l) & 1u, c));
            continue;
        }
        cv_rect(sx, top + nh - 2 - ((int32_t)st->note[0] - lo) * (nh - 2) / (hi - lo), mw, 2,
                lz_note(st, (st->flags & SF_ACCENT) != 0u, c));
    }
    s[0] = (char)('1' + b);
    s[1] = 0;
    nc = song.playing && seq_src(t, t->seq_idx) / 16u == b ? T_ACCENT : sel ? T_THEME : T_MID;
    GFX_HOOK_ALIGN(0, y, 0, y + (h > 20 ? 15 : h), AL_V | AL_PASS, "LENTE bar number at its cell's top");
    cv_text_on(x + 3, y + (h > 20 ? 3 : CAP_IN(X, h)), &AF_X, s, nc, bg);
}
/* the bars side by side from x0 (4 cells over w, 2 px apart), y .. y + h */
static void bar_cells(const track_t *t, int32_t x0, int32_t w, int32_t y, int32_t h, uint16_t c)
{
    int32_t lo, hi, bw = w / 4;
    uint32_t b;
    pat_range(t, &lo, &hi);
    for (b = 0; b < 4u; b++)
        bar_cell(t, b, x0 + (int32_t)b * bw + 1, y, bw - 2, h, lo, hi, c);
}
/* (0.8.5) BAR's strip over the roll and the grid (lens_strip): the bars small, 16 px at the panel's top */
#define LS_Y 0
#define LS_H 16
static int lens_strip(void) { return !ui.home && lz_on() && lens_level() == LV_BAR; }

/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note; fc: the note in focus's colour (0: none) and its note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    uint8_t nl[4 + NLANE], own[4 + NLANE];
    uint32_t nn = step_expand(st, step_chord_id(st), nl, own), k;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y, xj = x, wj = w;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= nn && ((st->acc >> (j - nn)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        for (k = 0; j < nn && k < pr_jnn && pr_jn[k] != n; k++)
            ;
        if (j < nn && k < pr_jnn) {                     /* (chord-harp) going on from the column before */
            xj -= 2;
            wj += 2;
        }
        if (y < PR_Y0)
            pr_split(xj, PR_Y0, wj, 1, c ? c : lz_note(st, acc, ACC), hits);
        else if (y >= ybot)
            pr_split(xj, ybot - 1, wj, 1, c ? c : lz_note(st, acc, ACC), hits);
        else
            pr_split(xj, y, wj, PR_RH - 1, c ? c : lz_note(st, acc, ACC), hits);
    }
}
static void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u, ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot, trw = 0, note_pg = cur_page()->id[0] == 4u || cur_page()->id[0] == 7u, outl;
    char tb[16];
    (void)c;
    pr_geom();
    ybot = PR_Y0 + PR_ROWS * PR_RH;
    if (song.g[G_STRN]) {                             /* (0.6.45) ALL TR (SCL KNOB 4, 0.7.6): the notes sound moved, shown */
        str_cpy(tb, "ALL TR ", sizeof tb);
        tb[7] = song.g[G_STRN] < 0 ? '-' : '+'; tb[8] = 0;
        fmt_int(tb + 8, song.g[G_STRN] < 0 ? -song.g[G_STRN] : song.g[G_STRN]);
        trw = text_w(&AF_S, tb) + 6;
    }
    if (lens_strip()) {                               /* LENTE's BAR: the bars over the roll (ALL TR after them) */
        bar_cells(t, PR_RX, PR_RW - trw, LS_Y, LS_H, ACC);
        if (trw) {
            GFX_HOOK_ALIGN(0, LS_Y, 0, LS_Y + LS_H, AL_V, "ALL TR on the bars' line");
            cv_text_r(PR_RX + PR_RW, LS_Y + CAP_IN(S, LS_H), &AF_S, tb, T_ACCENT, T_SURF);
        }
    }
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    cv_rect(PR_RX, PR_Y0, PR_RW, ybot - PR_Y0, T_BG);
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the scale's tinted */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        static const uint8_t KB[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
        int in = mask == 0xFFFu ? !KB[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in)
            cv_rect(PR_RX, y, PR_CX(ncol) - PR_RX, PR_RH, T_LANE);
    }
    for (i = 0; i < 16u; i++)                         /* step lines, the beats brighter */
        cv_rect(PR_CX(i), PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_LINE);
    if (ncol < 16u)                                   /* past LEN: dark */
        cv_rect(PR_CX(ncol), PR_Y0, PR_RX + PR_RW - PR_CX(ncol), ybot - PR_Y0, T_BG);
    for (r = 0; r < PR_ROWS; r++)                     /* the keys held: their rows marked at the left */
        if ((held >> r) & 1u)
            cv_rect(PR_RX, PR_Y0 + r * PR_RH, 2, PR_RH - 1, T_ACCENT);
    outl = note_pg && ui.cursor >= base && ui.cursor < base + ncol && ui.cursor < len && step_on(&seq_steps(t)[ui.cursor]);
    pr_jnn = 0;
    {   /* (2026-11-10) a HOLD: its bar on through the steps it rings over (drawn first: the notes over it come after) */
        uint32_t h, k;
        for (h = 0; h < len; h++) {
            const step_t *hs = &seq_steps(t)[h];
            uint32_t hl = hs->time == ST_NOTE && hs->n ? hold_len(hs, len - h) : 0u;
            for (k = 1; k < hl; k++) {
                uint32_t si = h + k;
                int32_t y2 = -10000;
                if (si >= base && si < base + ncol)
                    pr_bars(hs, PR_CX(si - base) - 1, PR_CWI(si - base) + 1, 0, (hs->flags & SF_ACCENT) != 0u, &y2, 1u);
            }
        }
    }
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_CX(i), w = PR_CWI(i), y = -10000;
        int slide;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        slide = (st->flags & SF_SLIDE) && step_ratchet(st) == 1u && ns->time == ST_NOTE && ns->n;
        pr_jnn = 0;                                   /* (chord-harp) the same chord slid into from the column before */
        if (s == si && i && step_chord_id(st) && (seq_steps(t)[si - 1u].flags & SF_SLIDE)) {
            uint32_t ps = pr_src(t, si - 1u, len);
            const step_t *pv = ps < NSTEP ? &seq_steps(t)[ps] : 0;
            uint8_t own[4 + NLANE];
            if (pv && step_chord_id(pv) == step_chord_id(st) && pv->note[0] == st->note[0])
                pr_jnn = (uint8_t)step_expand(pv, step_chord_id(pv), pr_jn, own);
        }
        if (s == ui.cursor && note_pg && ui.pr_focus && ui.pr_focus <= st->n) {   /* (0.6.81) SEQ > NOTE: the note in
                                                         * focus the accent */
            step_t one = *st;
            int32_t yy = -10000;
            pr_bars(st, s == si ? x + 1 : x - 1, s == si ? w - 2 : w + 1, 0, (st->flags & SF_ACCENT) != 0u, &y,
                    s == si && note_len(t, s, len) < 2u ? step_hits(st) : 1u);
            one.n = 1; one.note[0] = st->note[ui.pr_focus - 1u]; one.hit = 0;
            one.comp = one.comp2 = 0;                   /* (chord-harp: the note alone, not a CHORD from it) */
            pr_bars(&one, s == si ? x + 1 : x - 1, s == si ? w - 2 : w + 1, T_ACCENT, 0, &yy, s == si ? step_ratchet(st) : 1u);
        } else if (s == si) {                         /* (0.6.88: its TIMING off the grid, shown off it) */
            int32_t mo = ST_MICRO(st) * w / 64;     /* (0.9.1: 64ths) */
            pr_bars(st, x + 1 + mo < PR_RX ? PR_RX : x + 1 + mo, w - 2 + (slide ? 2 : 0),
                    s == ui.cursor && note_pg ? T_ACCENT : 0, (st->flags & SF_ACCENT) != 0u, &y, note_len(t, s, len) < 2u ? step_hits(st) : 1u);
        } else                                        /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, w + (slide && (si + 1u) % len != nx ? 0 : 0), s == ui.cursor && note_pg ? T_ACCENT : 0,
                    (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if (s == si)
            lz_comp_mark(st, x + w / 2, PR_Y0 + 1);
    }
    pr_jnn = 0;
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_CX(i);
        if (song.playing && si == seq_src(t, t->seq_idx))
            cv_rect(x, PR_Y0, 1, ybot - PR_Y0, T_ACCENT);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark at the column's foot */
            cv_rect(x + 3, ybot - 3, PR_CWI(i) - 6, 2, T_ACCENT);
        if (si == ui.cursor && !outl)                 /* the cursor: a frame round its column (0.8.9, the skins' mock-up:
                                                       * TEXT); on NOTE the note selected is outlined instead */
            cv_frame(x, PR_Y0, PR_CWI(i) + 1, ybot - PR_Y0, T_TEXT);
    }
    if (outl) {   /* (2026-11-10) SEQ > NOTE: the selected note outlined, over the steps it lasts (its TIEs, or its HOLD) */
        const step_t *os = &seq_steps(t)[ui.cursor];
        uint32_t span = note_len(t, ui.cursor, len), hl = hold_len(os, len - ui.cursor), c0 = ui.cursor - base, c1, j;
        int32_t ox0, ox1;
        if (hl > span) span = hl;
        c1 = c0 + span < ncol ? c0 + span : ncol;
        ox0 = PR_CX(c0); ox1 = PR_CX(c1);
        for (j = 0; j < 4u + NLANE; j++) {
            int32_t n = pr_note(os, j), oy;
            if (n < 0) continue;
            oy = pr_row_y(n);
            if (oy < PR_Y0 || oy >= ybot) continue;
            cv_frame(ox0, oy - 1, ox1 - ox0 + 1, PR_RH + 1, T_TEXT);
        }
    }
    if (ui.entry_open && !ui.ent_arp && ui.ent_nn && ui.ent_len > 1u) {   /* (0.6.81) a chord held, KNOB 1 its length:
                                                         * the length to be written, outlined from the cursor on */
        uint32_t k, j;
        for (k = 0; k < ui.ent_len && k < len; k++) {
            uint32_t si = (ui.cursor + k) % len;
            int32_t x;
            if (si < base || si >= base + ncol) continue;
            x = PR_CX(si - base);
            for (j = 0; j < ui.ent_nn; j++) {
                int32_t y = pr_row_y(ui.ent_notes[j]);
                if (y < PR_Y0 || y >= ybot) continue;
                cv_frame(k ? x - 1 : x + 1, y, k ? PR_CWI(si - base) + 1 : PR_CWI(si - base) - 1, PR_RH, T_ACCENT);
            }
        }
    }
}
/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing. (0.8.5) LENTE's BAR: the bars' strip
 * over it, the lanes 15 px under it */
static void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    uint64_t locks = motion_lock_steps(trk_index(t));
    int s = lens_strip();
    int32_t y0 = s ? 19 : 6, lp = 14, hy = 2;   /* (a lane's pitch, 2026-11-02: 14 under the strip too, in the panel's 138 rows; its hits' row in it) */
    if (s)
        bar_cells(t, PR_RX, PR_RW, LS_Y, LS_H, c);
    for (i = 0; i < 16u && base + i < len; i++)     /* a parameter lock: a mark over the step (under the bars: under it) */
        if ((locks >> (base + i)) & 1u)
            cv_rect(41 + (int32_t)i * 12, s ? y0 + 4 + 8 * lp - 1 : 1, 8, 2, T_ACCENT);
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + (int32_t)l * lp;
        int sel = l == ui.lane;
        uint16_t row = sel ? T_RAISE : T_SURF;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rect(6, y, 228, lp - 1, T_RAISE);        /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + hy, 0, y + hy + 10, AL_V, "drum lane label on its hits' line");
        cv_text_on(10, y + hy - 3, &AF_S, drum_lane_abbr(t, l), sel ? T_ACCENT : T_MID, row);   /* ink as the hits' rows */
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 40 + (int32_t)i * 12;
            if ((step_lanes(st) & b) && step_ratchet(st) > 1u)
                pr_split(x, y + hy, 10, 10, lz_note(st, step_accents(st) & b, c), step_ratchet(st));
            else if (step_lanes(st) & b)
                cv_rect(x, y + hy, 10, 10, lz_note(st, step_accents(st) & b, c));
            else
                cv_rect(x + 4, y + hy + 4, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + hy - 1, 12, 12, T_THEME);
        }
    }
    if (song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank)
        cv_rect(40 + (int32_t)(seq_src(t, t->seq_idx) % 16u) * 12, y0 - 2, 10, 2, T_ACCENT);
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
        h = (h ^ ((uint32_t)st->cwhen >> 4)) * 16777619u;   /* (0.9.1: MICRO's sixteenth; its 64ths are in the flags) */
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}



/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17

/* FM6's 32 algorithms as charts (0.9.6: as the colony, ui_engvis.c vx_fm6). FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every colony as drawn
 * (no two bichos overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
#if YEHNAH_FM4
/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static void graph_fm(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
#endif
/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++) mask |= (uint8_t)((project_name(i, graph_pname[i]) != 0) << i);
        graph_pname_sig = fnv(2166136261u, graph_pname, sizeof graph_pname);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}

/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer) */
static int roll_view(const page_t *pg);
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = trk_engp(t);
    uint32_t i, n = 0, k = 0;
    char nb[24];
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    str_cpy(nb, pg->title, sizeof nb);
    if (!page_over && !ui.home)                         /* (0.9.2) the page's name in its chain (2026-11-02: no groups) */
        str_cpy(nb, nav_page_name(), sizeof nb);
    else
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == pg->fam && page_visible(i)) {
                n++;
                if (i == ui.page)
                    k = n;
            }
    str_cpy(ti, pt ? pt : grid_on() && roll_view(pg) ? "GRID" : nb, 12);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_int(ti + str_len(ti), (int32_t)k);
        str_cpy(ti + str_len(ti), "/", 4);
        fmt_int(ti + str_len(ti), (int32_t)n);
    }
    if (roll_view(pg) && !drum_track(t)) {          /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* a value 0..256 as heat: the palette's surface toward its accent, then its text */
static uint16_t heat_col(int32_t v)
{
    v = clamp(v, 0, 256);
    return v < 128 ? ux_mix(T_SURF, T_ACCENT, v * 100 / 128) : ux_mix(T_ACCENT, T_TEXT, (v - 128) * 100 / 128);
}


/* (chord-harp fork) SAVE > MORPH: TRACK A on the left, TRACK B on the right, each named over its side in its knob's
 * colour ("T1 SAW LEAD"); the divider between them where the blend is (BLEND, MOVE's live: voice.c bl_now; the user's,
 * 2026-10-28): each side as wide as its share of the sound, its percentage in it. Two tracks not picked: says so */
#define BL_X0 8
#define BL_W 224
static void bl_name(char *o, uint32_t k)                /* "T1 SAW LEAD": track k (1..6) and its sound */
{
    const track_t *t = &trk[(k - 1u) % NTRK];
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    o[0] = 'T';
    o[1] = (char)('0' + k);
    o[2] = ' ';
    str_cpy(o + 3, t->user ? "USER" : t->preset < ENG_NP(e) ? e->presets[t->preset].name : "-", 18);
}
static void graph_blend(void)
{
    int32_t a = clamp(song.g[G_BLA], 0, NTRK), b = clamp(song.g[G_BLB], 0, NTRK), on = a && b && a != b;
    int32_t x = on && bl_a && song.g[G_BMOVE] ? bl_now : clamp(song.g[G_BLEND], 0, 127), H = (int32_t)graph_h();
    int32_t y0 = 42, y1 = H - 10, dx = BL_X0 + (on ? 127 - x : 127) * BL_W / 127, pa = on ? (127 - x) * 100 / 127 : 100;
    uint16_t ca = on ? ux_mix(T_SURF, ux.k[0], 40) : T_BG, cb = on ? ux_mix(T_SURF, ux.k[1], 22) : T_BG;   /* (B paler) */
    char p[24];
    cv_text(BL_X0, 6 - AF_X_CAP_Y, &AF_X, "TRACK A", a ? ux.k[0] : T_DIM);
    cv_text_r(BL_X0 + BL_W, 6 - AF_X_CAP_Y, &AF_X, "TRACK B", b ? ux.k[1] : T_DIM, T_SURF);
    if (a)
        bl_name(p, (uint32_t)a);
    cv_clip_text(BL_X0, 18, &AF_M, a ? p : "OFF", on && pa >= 50 ? T_TEXT : T_MID, T_SURF, 110);
    if (b)
        bl_name(p, (uint32_t)b);
    if (!b || text_w(&AF_M, p) <= 110)                  /* (right-aligned, as it fits) */
        cv_text_r(BL_X0 + BL_W, 18, &AF_M, b ? p : "OFF", on && pa < 50 ? T_TEXT : T_MID, T_SURF);
    else
        cv_clip_text(BL_X0 + BL_W - 110, 18, &AF_M, p, on && pa < 50 ? T_TEXT : T_MID, T_SURF, 110);
    if (!on) {
        cv_rect(BL_X0, y0, BL_W, y1 - y0, T_BG);
        cv_text_in(BL_X0, (y0 + y1) / 2 - 6, BL_W, &AF_M, a && a == b ? "TWO TRACKS, NOT ONE" : "PICK TWO TRACKS", T_MID, T_BG);
        return;
    }
    cv_rect(BL_X0, y0, dx - BL_X0, y1 - y0, ca);
    cv_rect(dx, y0, BL_X0 + BL_W - dx, y1 - y0, cb);
    if (dx - BL_X0 >= 40) {                             /* the shares, where they fit */
        fmt_int(p, pa);
        str_cpy(p + str_len(p), "%", sizeof p - str_len(p));
        cv_text_in(BL_X0, (y0 + y1) / 2 - 6, dx - BL_X0, &AF_M, p, T_TEXT, ca);
    }
    if (BL_X0 + BL_W - dx >= 40) {
        fmt_int(p, 100 - pa);
        str_cpy(p + str_len(p), "%", sizeof p - str_len(p));
        cv_text_in(dx, (y0 + y1) / 2 - 6, BL_X0 + BL_W - dx, &AF_M, p, T_TEXT, cb);
    }
    {                                                   /* the divider, a grip on it */
        int32_t gx = clamp(dx - 1, BL_X0, BL_X0 + BL_W - 2);
        cv_rect(gx, y0 - 4, 2, y1 - y0 + 8, T_TEXT);
        cv_rect(gx - 3, (y0 + y1) / 2 - 8, 8, 16, T_TEXT);
        cv_rect(gx - 1, (y0 + y1) / 2 - 5, 1, 10, T_SURF);
        cv_rect(gx + 2, (y0 + y1) / 2 - 5, 1, 10, T_SURF);
    }
}
#undef BL_X0
#undef BL_W

/* (0.6.52) SEQ > FUGUE: the steps (their notes as heat) with the sequencer's playhead, then a lane each for heads 2..4:
 * where it is, its RATE DIR INTERVAL; the head edited lit */
/* (0.8.2) the pages that edit the piano roll kept it in the panel. (0.8.3) LENTE (ui.c lens_*): the panel is the
 * level's: the roll (a kit: the grid) on BAR's STEP and NOTE; the bars on PATTERN's PATTERN, STRETCH and TRACK
 * (graph_bars); the cursor step large on STEP's NOTE, COMP and COMP 2 (graph_bigstep); PLAYHEADS, MUTATE, AUTOMATION
 * and AUTO LIST keep their own graphs */
static int roll_view(const page_t *pg)
{
    return pg->fam == FAM_SEQ && pg->graph == GR_ROLL && !(pg == &PAGES[ui.page] && lens_on() && lens_level() == LV_STEP);
}
static int bars_view(const page_t *pg)                  /* (0.8.3) LENTE's PATTERN level: the bars */
{
    return pg == &PAGES[ui.page] && lens_on() && lens_level() == LV_PAT && (pg->graph == GR_STEPS || pg->graph == GR_SHAPE);
}
static int bigstep_view(const page_t *pg)               /* .. its STEP level: the step large */
{
    return pg == &PAGES[ui.page] && lens_on() && lens_level() == LV_STEP && pg->graph == GR_ROLL;
}
/* (0.8.3) LENTE's PATTERN; (0.8.5) as the mock-up: the pattern's bars side by side (up to four, LEN 64) as big cells
 * (2 .. 62); under them the whole pattern as one small roll (80 .. 140, a BG area: the scale's
 * rows LANE, the step lines GRID and every 4th LINE, the notes as hot as they play, held through their TIEs, a
 * component's mark on top, past LEN dark) with the sequencer's playhead (ACCENT) and FUGUE's heads (a tick each at the
 * foot, as hot as its lane on the PLAYHEADS page) */
#define LB_Y 2
#define LB_H 60
#define LP_Y 80                                      /* the pattern's roll, to 140 */
#define LP_H 60
static void graph_bars(const track_t *t, uint16_t c)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), n = len < 16u ? 16u : len, i, j, k, l, dr = drum_track(t);
    uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu, ti = song.sel % NTRK, mask = scale_mask(t);
    int32_t lo, hi, rows, rh = 2;
    bar_cells(t, PR_RX, PR_RW, LB_Y, LB_H, c);
    pat_range(t, &lo, &hi);
    rows = LP_H / rh;
    if (hi - lo + 1 < rows)
        lo -= (rows - (hi - lo + 1)) / 2;
    hi = lo + rows - 1;
    cv_rect(PR_RX, LP_Y, PR_RW, LP_H, T_BG);
    if (!dr)
        for (k = 0; k < (uint32_t)rows; k++) {           /* the scale's rows */
            int32_t nn = hi - (int32_t)k;
            static const uint8_t KB[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
            int in = mask == 0xFFFu ? !KB[(uint32_t)(nn + 120) % 12u] : (int)((mask >> ((uint32_t)(nn - t->p[P_ROOT] + 120) % 12u)) & 1u);
            if (in)
                cv_rect(PR_RX, LP_Y + (int32_t)k * rh, PR_RW, rh, T_LANE);
        }
    for (i = 0; i <= n; i++)                         /* the step lines, the beats brighter */
        cv_rect(PR_RX + (int32_t)(i * PR_RW / n) - (i == n), LP_Y, 1, LP_H, i % 4u ? T_GRID : T_LINE);
    if (len < n)                                     /* past LEN: dark */
        cv_rect(PR_RX + (int32_t)(len * PR_RW / n), LP_Y, PR_RW - (int32_t)(len * PR_RW / n), LP_H, T_BG);
    for (i = 0; i < len; i++) {
        const step_t *st = &seq_steps(t)[i];
        int32_t x = PR_RX + (int32_t)(i * PR_RW / n), sw, L = 1;
        if (!step_on(st))
            continue;
        while (L < (int32_t)len - (int32_t)i && seq_steps(t)[i + (uint32_t)L].time == ST_TIE)
            L++;
        sw = PR_RX + (int32_t)((i + (uint32_t)L) * PR_RW / n) - x - 2;
        if (sw < 1)
            sw = 1;
        if (dr) {
            for (l = 0; l < NLANE; l++)
                if ((step_lanes(st) >> l) & 1u)
                    cv_rect(x + 1, LP_Y + 4 + (int32_t)l * (LP_H - 8) / (int32_t)NLANE, sw, 3,
                            lz_note(st, (step_accents(st) >> l) & 1u, c));
            continue;
        }
        for (j = 0; j < 4u + NLANE && pr_note(st, j) >= 0; j++) {   /* (chord-harp: a CHORD whole) */
            int32_t nn = clamp(pr_note(st, j), lo, hi);
            cv_rect(x + 1, LP_Y + (hi - nn) * rh, sw + ((st->flags & SF_SLIDE) ? 2 : 0), rh,
                    lz_note(st, (st->flags & SF_ACCENT) != 0u, c));
        }
        lz_comp_mark(st, x + (int32_t)(PR_RW / n) / 2, LP_Y + 1);
    }
    if (ph < len)
        cv_rect(PR_RX + (int32_t)(ph * PR_RW / n), LP_Y, 1, LP_H, T_ACCENT);
}
/* (0.8.3) LENTE's STEP: the cursor step large; (0.8.5) as the mock-up, on a BG area (4 .. 236): on top "STEP n" (S,
 * THEME) and at the right what it is (NOTE, TIE, REST; a note longer than a step: LEN n, MID). At the left its notes as
 * bars 70 px wide where their pitch puts them (2.6 px a semitone, C3 at the foot), each named at the right of it (S,
 * TEXT): as hot as it plays (moved by its MICRO, in its RATCH's parts; NOTE's note in focus the accent); a TIE the notes
 * it holds, MID; a REST, an empty step: said; a kit: the lanes it hits. At the right its chips (102 x 14, 17 px apart):
 * ACC, SLD, MICRO n, COMP1 x, COMP2 x, CHANCE n% (a value set: TINT with TEXT; else RAISE with DIM) */
#define BS_CX 130                                    /* the chips' column */
#define BS_CW 102
#define BS_Y 28                                      /* the first chip, 17 px apart */
#define BS_TY 4                                      /* the title's line (capitals 7..15) */
#define BS_NH 6                                      /* a note's bar */
static void bs_chip(uint32_t k, const char *s, int on)
{
    int32_t y = BS_Y + (int32_t)k * 17;
    uint16_t bg = on ? T_TINT : T_RAISE, fg = on ? T_TEXT : T_DIM == T_RAISE ? T_MID : T_DIM;
    cv_rect(BS_CX, y, BS_CW, 14, bg);
    GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "big step chip text centred up/down");
    cv_text_on(BS_CX + 5, y + CAP_IN(X, 14), &AF_X, s, fg, bg);
}
static void graph_bigstep(const track_t *t, const page_t *pg, uint16_t c)
{
    static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, dr = drum_track(t), nl;
    uint32_t src = pr_src(t, ui.cursor % NSTEP, len);
    const step_t *st = &seq_steps(t)[ui.cursor % NSTEP], *sn = src < NSTEP ? &seq_steps(t)[src] : st;
    int note = pg->id[0] == 4u, tie = st->time == ST_TIE, acc;
    int32_t m = ST_MICRO(st);
    char b[20];
    cv_rect(PR_RX, 0, PR_RW, (int32_t)cv_h, T_BG);
    str_cpy(b, "STEP ", sizeof b);
    fmt_int(b + 5, (int32_t)ui.cursor + 1);
    GFX_HOOK_ALIGN(0, BS_TY + 3 + AF_X_CAP_Y, 0, BS_TY + 3 + AF_X_CAP_Y + AF_X_CAP_H, AL_V | AL_PASS, "big step title");
    cv_text_on(10, BS_TY + 3, &AF_X, b, T_THEME, T_BG);
    nl = !dr && st->time == ST_NOTE && st->n ? note_len(t, ui.cursor % NSTEP, len) : 1u;   /* (0.8.4) a NOTE's length:
                                                         * LEN 3 (BAR > STEP's KNOB 3 sets it) */
    {
        uint32_t hl = !dr && st->time == ST_NOTE && st->n && ui.cursor < (uint32_t)len ? hold_len(st, len - ui.cursor) : 0u;   /* (a HOLD) */
        if (hl) nl = hl;
        str_cpy(b, hl ? "HOLD " : nl > 1u ? "LEN " : TIME_N[st->time % 3u], sizeof b);
        if (nl > 1u) fmt_int(b + str_len(b), (int32_t)nl);
        if (hl && !voice_poly((int)trk_vmode(t))) str_cpy(b + str_len(b), " MONO", 6);   /* (a mono voice: the next note takes the hold) */
    }
    acc = nl > 1u;                                  /* (the text lit) */
    if (!dr && st->time == ST_NOTE && st->n && (st->comp == CP_CHORD || st->comp2 == CP_CHORD)) {   /* (chord-harp) a
                                                         * CHORD: its name (and LEN) */
        step_chord_name(st, st->comp == CP_CHORD ? st->carg : st->carg2, b, 12);
        if (nl > 1u) { str_cpy(b + str_len(b), " LEN ", 6); fmt_int(b + str_len(b), (int32_t)nl); }
        acc = 1;
    }
    cv_text_r(230, BS_TY + 3, &AF_X, b, acc ? T_TEXT : T_MID, T_BG);
    if (dr) {                                        /* a kit: the lanes it hits (their accents hot), two columns */
        uint32_t k = 0, l;
        for (l = 0; l < NLANE; l++) {
            int32_t x = 14 + (int32_t)(k / 4u) * 52, y = 40 + (int32_t)(k % 4u) * 20;
            if (!(((st->time == ST_NOTE ? step_lanes(st) : 0u) >> l) & 1u))
                continue;
            cv_rect(x, y, 10, 10, lz_note(st, (step_accents(st) >> l) & 1u, c));
            GFX_HOOK_ALIGN(0, y, 0, y + 10, AL_V, "big step lane on its hit");
            cv_text_on(x + 15, y + CAP_IN(X, 10), &AF_X, drum_lane_abbr(t, l), T_TEXT, T_BG);
            k++;
        }
        if (!k)
            cv_text_in(4, 69, 120, &AF_X, st->time == ST_NOTE ? "NO HIT" : TIME_N[st->time % 3u], T_MID, T_BG);
    } else if ((tie && src < NSTEP) || (st->time == ST_NOTE && st->n)) {   /* the notes where their pitch puts them */
        uint32_t n = sn->n < 4u ? sn->n : 4u, f = note && !tie && ui.pr_focus <= n ? ui.pr_focus : 0u, j;
        uint8_t ord[4] = {0, 1, 2, 3};
        int32_t gy[4];
        for (i = 0; i < n; i++)                      /* highest first */
            for (j = i + 1u; j < n; j++)
                if (sn->note[ord[j]] > sn->note[ord[i]]) { uint8_t q = ord[i]; ord[i] = ord[j]; ord[j] = q; }
        for (i = 0; i < n; i++) {                    /* by pitch, 11 px apart at least (the names), inside 22 .. 128 */
            gy[i] = 112 - ((int32_t)sn->note[ord[i]] - 48) * 13 / 5;
            if (i && gy[i] < gy[i - 1u] + 11)
                gy[i] = gy[i - 1u] + 11;
        }
        if (n && gy[n - 1u] > 128)
            for (i = 0; i < n; i++) gy[i] -= gy[n - 1u] - 128;
        if (n && gy[0] < 22)
            for (i = n; i-- > 0;) gy[i] += 22 - gy[0];
        for (i = 0; i < n; i++) {
            int32_t y = gy[i], x = 14 + (tie ? 0 : m / 4);   /* (0.9.1: MICRO in 64ths; as far as before) */
            uint16_t col = tie ? T_MID : f ? (ord[i] + 1u == f ? T_ACCENT : lz_note(sn, (sn->flags & SF_ACCENT) != 0u, c)) :
                                              lz_note(sn, (sn->flags & SF_ACCENT) != 0u, c);
            pr_split(x, y, 70, BS_NH, col, tie || nl > 1u ? 1u : step_hits(sn));
            note_name(b, sn->note[ord[i]]);
            GFX_HOOK_ALIGN(0, y, 0, y + BS_NH, AL_V, "big step note name on its bar");
            cv_text_on(90, y + CAP_IN(X, BS_NH), &AF_X, b, tie ? T_MID : T_TEXT, T_BG);
        }
    } else {
        cv_text_in(4, 69, 120, &AF_X, st->time == ST_NOTE ? "EMPTY" : TIME_N[st->time % 3u], T_MID, T_BG);
    }
    acc = dr ? step_accents(st) != 0u : (st->flags & SF_ACCENT) != 0u;
    bs_chip(0, "ACC", acc);
    bs_chip(1, "SLD", !dr && (st->flags & SF_SLIDE));
    str_cpy(b, "MICRO ", sizeof b);
    if (!m) str_cpy(b + 6, "0", 4);
    else { b[6] = m < 0 ? '-' : '+'; fmt_int(b + 7, m < 0 ? -m : m); str_cpy(b + str_len(b), "/64", 4); }   /* (0.9.1) */
    bs_chip(2, b, m != 0);
    str_cpy(b, "COMP1 ", sizeof b);
    if (st->comp) str_cpy(b + 6, COMP_NAMES[(COMP_ARPN + st->comp - CP_ARPN) % COMP_N], 10);
    else if (step_ratchet(st) > 1u) { str_cpy(b + 6, "RATCH x", 10); b[13] = (char)('0' + step_ratchet(st)); b[14] = 0; }
    else str_cpy(b + 6, "-", 4);
    bs_chip(3, b, st->comp || step_ratchet(st) > 1u);
    str_cpy(b, "COMP2 ", sizeof b);
    str_cpy(b + 6, st->comp2 ? COMP_NAMES[(COMP_ARPN + st->comp2 - CP_ARPN) % COMP_N] : "-", 10);
    bs_chip(4, b, st->comp2 != 0u);
    str_cpy(b, "CHANCE ", sizeof b);
    fmt_int(b + 7, (int32_t)step_chance(st));
    str_cpy(b + str_len(b), "%", 4);
    bs_chip(5, b, step_chance(st) < 100u);
}

/* (0.6.61) the panels' cytoplasm: a few granules drifting behind the graph, as HOME's, so the screen breathes. LOAD
 * FULL and ANIM ON only, not on HOME (its own), the lists and the dense grids (the roll, the steps); (0.6.64) no
 * redraw of their own: they move when the panel redraws anyway (a knob, the LFO, the envelope), so they cost nothing */
#ifndef UI_NO_DRIFT
#define UI_NO_DRIFT 0                                /* (tests/ui_test.c: 1, its frames compared pixel by pixel) */
#endif
static int drift_on(void)
{
    uint32_t g = cur_page()->graph;
    if (UI_NO_DRIFT)
        return 0;
    return !ui.home && !sys_load && !roll_view(cur_page()) &&
           g != GR_ROLL && g != GR_SHAPE && g != GR_COMP &&   /* (0.8.3: LENTE's step, the heads) */
           g != GR_STEPS && g != GR_CHANCE && g != GR_BROWSE && g != GR_USER && g != GR_SLOTS &&
           g != GR_SETS && g != GR_SONG && g != GR_SEDIT && g != GR_TOOLS && g != GR_ERASE;
}
static void graph_drift(void)
{
    uint32_t k, gh = graph_h();
    for (k = 0; k < 14u; k++) {
        uint32_t h = k * 2654435761u + 0x9E37u;
        int32_t gx = 10 + (int32_t)((h >> 8) % 220u + ui.frame * (1u + (h >> 28)) / 8u) % 220;
        int32_t gy = 4 + (int32_t)(((h >> 16) % (gh - 8u)) + (uint32_t)(sine_i((ui.frame * 200u + h) << 16) >> 13)) % (int32_t)(gh - 8u);
        cv_rect(gx, gy, 1 + (int32_t)(h >> 31), 1 + (int32_t)(h >> 31), ux_mix(T_SURF, T_THEME, 18 + (int32_t)(h >> 29) * 3));
    }
}
/* (0.8.5) SAVE > LIBRARY's picture of the other projects (ui_lzsave.c) */
static struct {
    uint32_t gen;                                       /* lib_gen() when the scan began */
    uint8_t next;                                       /* the next project to scan (NSONG: done) */
    uint8_t mask[NSONG];                                /* sections stored, a bit each */
    char name[NSONG][13u];
} lib = {0xFFFFFFFFu, 0, {0}, {{0}}};
static void lib_poll(void)                              /* (graph_signature, once a frame) */
{
    if (lib.gen != lib_gen()) {
        lib.gen = lib_gen();
        lib.next = 0;
    }
    if (lib.next < NSONG) {
#if YEHNAH_FLASH
        lib.mask[lib.next] = (uint8_t)lib_scan(lib.next, lib.name[lib.next]);
#endif
        lib.next++;
    }
}
/* (0.9.5) a toast (ui_say) on a lens page: a pill of BG behind its words alone, near the panel's foot, over the page
 * drawn as ever (its graph, roll, beings live behind it); HOME and the layers: the header says it.
 * The pill keeps clear of the page's own words (gfx.c cv_txt: every text drawn on the panel this frame): near the foot
 * if nothing is written there, else the nearest rows lower, then higher, with none; a page written all over (MUTATE's
 * lines, AUTO LIST's rows where no gap is a pill high): no pill, the header says it alone, as on HOME */
static int toast_on(void) { return lz_on() && ui.msg_t && ui.msg[0] && !ui.layer && !ui.home; }
#define TOAST_H 17
#define TOAST_GAP_X 3                                /* (the clearance round a word: its 2 px halo and a pixel) */
#define TOAST_GAP_Y 2
static int16_t toast_at[4];                          /* the pill drawn last (screen x0 y0 x1 y1; x1 == x0: none) */
static int toast_clear(int32_t x, int32_t y, int32_t w)
{
    return !cv_txt_in(x - TOAST_GAP_X, y - TOAST_GAP_Y, x + w + TOAST_GAP_X, y + TOAST_H + TOAST_GAP_Y);
}
/* the pill's top row on the panel (canvas rows), -1: nowhere clear */
static int32_t toast_row(int32_t x, int32_t w)
{
    int32_t gh = (int32_t)graph_h(), y0 = gh >= 3 * TOAST_H ? gh - TOAST_H - 5 : (gh - TOAST_H) / 2, y;
    for (y = y0; y <= gh - TOAST_H - 1; y++)         /* where it was, or lower (nearer the foot) */
        if (toast_clear(x, y, w))
            return y;
    for (y = y0 - 1; y >= 1; y--)                    /* .. else higher */
        if (toast_clear(x, y, w))
            return y;
    return -1;
}
static void toast_pill(void)
{
    int32_t w = clamp(text_w(&AF_S, ui.msg) + 16, 40, 224), x = 120 - w / 2, y = toast_row(x, w);
    uint8_t hl = cv_halo;
    char f[40];
    toast_at[0] = toast_at[2] = 0;
    if (y < 0)                                       /* (no clear rows: the header's words alone) */
        return;
    toast_at[0] = (int16_t)x; toast_at[1] = (int16_t)(y + (int32_t)graph_y());
    toast_at[2] = (int16_t)(x + w); toast_at[3] = (int16_t)(toast_at[1] + TOAST_H);
    text_fit(f, sizeof f, ui.msg, &AF_S, w - 12);
    GFX_HOOK_COVER(x, y + cv_oy, x + w, y + cv_oy + TOAST_H);   /* (what the page has under it: hidden) */
    cv_halo = 1;                                     /* (its round ends blended onto what is there) */
    cv_rrect(x, y, w, TOAST_H, -(TOAST_H / 2), ux_mix(T_BG, T_ACCENT, 70), T_BG);
    cv_rrect(x + 1, y + 1, w - 2, TOAST_H - 2, -(TOAST_H / 2 - 1), T_BG, T_BG);
    cv_halo = 0;
    GFX_HOOK_ALIGN(x, y, x + w, y + TOAST_H, AL_HV, "toast message centred in its pill");
    cv_text_flags(ink_in(&AF_S, f, w) + x, y + CAP_IN(S, TOAST_H), &AF_S, f, T_ACCENT, T_BG, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    cv_halo = hl;
}
static void sound_name(const track_t *t, char *b);   /* (ui_draw.c) */
static void gen_panel(void);                         /* (0.8.5) gen.c: GEN in HOME's lens */
static uint32_t gen_draw_sig(void);
static uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (toast_on())                                  /* (0.8.5) the toast over the panel (0.9.5: a pill on the page) */
        h = str_hash(h ^ 0x70A57u, ui.msg);
    if (ui.confirm == CF_DICE_ALL)                   /* (0.8.5) GEN: its settings, its page */
        return h ^ gen_draw_sig();
    if (ui.home)
        return h ^ (ui.frame / 2u) ^ (home_level() * 7u + home_set()) * 104729u;   /* the beings: every other frame */
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if ((pg->graph == GR_NONE && !fx_on()) || pg->graph == GR_ARP || pg->graph == GR_MOTION) h ^= ui.frame / 2u;   /* (0.8.5:
                                                         * FX's lens pictures change with their values, */
    if (fx_on() && (song.playing || bg_energy() > 2))   /* .. and with the sound while it sounds: FX live) */
        h ^= ui.frame / 2u * 2246822519u;
    if (pg->graph == GR_LFO)                         /* (0.5) the LFO runs: its phase, its level */
        h ^= (t->lfo_ph >> 23) * 2654435761u ^ (uint32_t)(t->lfo_val * t->lfo_fade >> 22);
    if (pg->graph == GR_ADSR)                        /* the envelope's dot: every voice's stage and level */
        for (i = 0; i < NVOICE; i++)
            h = (h ^ ((uint32_t)t->v[i].stage << 24 | (uint32_t)t->v[i].env >> 16)) * 16777619u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    if (pg->fam == FAM_SCL && (pg->graph == GR_SCALE || pg->id[1] == P_CHRD) && !ui.home) {   /* (chord-harp fork) SCL's
                                                         * and CHORD's keyboards: the notes played, pressed keys shiver */
        uint32_t pc = play_pcs(t);
        h ^= pc * 3266489917u ^ (pc ? ui.frame / 2u * 2246822519u : 0u);
    }
    if (t->p[P_CHRD])                                /* (chord-harp) HARP OMNI PROG, CHORD's modes: the chord the
                                                         * plate follows, the keys down */
        h ^= (harp_cur[trk_index(t) % NTRK] + 1u) * 40503u ^ harp_held(t) * 2654435761u;
    if ((pg->id[3] == P_HPLATE || pg->id[3] == P_OSTR || pg->id[1] == P_PROG || pg->id[1] == P_CHRD) && pg->fam == FAM_SCL &&
        !ui.home)                                    /* (chord-harp fork) HARP / OMNI / PROG / CHORD's
                                                         * strings and keys alive: every other frame (ui_lzpage.c hk_*) */
        h ^= ui.frame / 2u * 2246822519u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u;
    if (pg->graph == GR_MSEQ)                        /* (Yehnah) the step picked, the one playing, the level now */
        h ^= (ms_ui_step + 1u) * 40503u + (song.playing ? ui.frame * 2654435761u : 0u);
    if (pg->graph == GR_SONG) {
        for (uint32_t r = 0; r < CHAIN_ROWS; r++)
            h = (h ^ (song_idx.scene[song_cur][r].on * (r + 1u))) * 16777619u;
        h ^= ui.song_row * 40503u + chain_config.count * 7919u + ui.arr_sect * 1299709u + ui.arr_bars * 15485863u;
        for (i = 0; i < CHAIN_ROWS; i++) {
            uint32_t q;
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
            for (q = 0; q < NTRK; q++)                  /* (2026-11-11: the shared view shows the cuts) */
                h = (h ^ (song_idx.scene[song_cur][i].cut[q][0] + 256u * song_idx.scene[song_cur][i].cut[q][1])) * 16777619u;
        }
        h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u + chain.pass * 15485863u : 0u;
        h ^= SONG_END(&chain_config) * 2246822519u;      /* (0.6.22: the END) */
        for (i = 0; i < 4u; i++) h ^= (uint32_t)graph_project_used(i) << (24u + i);
        h += graph_pname_sig;
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_slot + 1u) * 40503u;
    if (pg->graph == GR_STAGE)                       /* .. ENV > STAGE: the stage */
        h ^= (ui.stage + 1u) * 3266489917u;
    if (pg->graph == GR_PLIST) {                     /* (2026-11-11) EDIT > PATCH: the row, every value in the list */
        uint32_t r, k;
        h ^= (plist_cur + 1u) * 2654435761u;
        for (r = 0; r < PLIST_N; r++)
            for (k = 0; k < 3u; k++)
                if (PLIST[r].id[k] != 0xFFu)
                    h = (h ^ (uint32_t)(uint16_t)plist_val(t, PLIST[r].id[k])) * 16777619u;
    }
    if (pd_on()) {                                   /* .. a lens of ui_lens.c: its level (ENV's TRACKS: every track live) */
        h ^= (pd_level() + 1u) * 668265263u;
        if ((pd_cur() == PD_ENV && pd_level() == 0u) || (pd_cur() == PD_MOD && pg->graph != GR_MSEQ))   /* (MOD's pulse,
                                                         * LFO DEST's LFO) */
            h ^= ui.frame / 2u * 2654435761u;
    }
    if ((pg->graph == GR_SLCR || pg->graph == GR_SLPIT) && song.g[G_SLMODE])   /* the SLICER's step playing */
        h ^= (sl.idx + 1u) * 2654435761u;
    if (pg->graph == GR_SLCR || pg->graph == GR_SLPIT)   /* (its globals: the page's knobs and the bus) */
        for (i = 0; i < G_SL_N; i++)
            h = (h ^ (uint32_t)song.g[G_SLMODE + i]) * 16777619u;
    if (pg->graph == GR_SLPIT)
        h ^= (sl_ui_step + 1u) * 40503u;
    if (pg->graph == GR_TOOLS)                       /* (0.6.12) the action picked, the song's row */
        h ^= ui.act * 7919u + ui.song_row * 104729u;
    if (pg->graph == GR_SLOTS || pg->graph == GR_ERASE) {   /* (a checksum over each slot; the project, the music's section, the song) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)graph_project_used(i) << (20u + i);
        h += graph_pname_sig + song_cur * 2654435761u + (uint32_t)(live.cur + 1) * 40503u + chain_config.count * 7919u;
        for (i = 0; i < CHAIN_ROWS; i++)
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
    }
    if (pg->graph == GR_MOTION) h ^= motion_count(t) * 131u + motion_enabled(t);
    if (pg->graph == GR_CHANCE) h ^= ui.cursor * 40503u + step_chance(&t->step[ui.cursor]);
    if (pg->graph == GR_DXIMG)                       /* (0.6.48) IMAGE and how wide it plays */
        h = (h ^ ((uint32_t)t->p[P_DXIMG] << 16 | (uint32_t)dx_img_k(t))) * 16777619u;
    if (DX_KIT3(pg))                                 /* (0.8.8) KIT 3's colony lives every frame, as KIT's (was still) */
        h ^= ui.frame * 2654435761u;
    if (pg->graph >= GR_FMOP && pg->graph <= GR_FMENV)   /* (0.9.6) FM6's operators: the colony lives, as the beings */
        h ^= ui.frame / 2u * 2246822519u;
    if (pg->graph == GR_BLEND) {                     /* (chord-harp fork) the two tracks' sounds, the blend (MOVE's live) */
        uint32_t k;
        for (k = 0; k < NTRK; k++)
            h = (h ^ (trk[k].preset | (uint32_t)trk[k].user << 8 | (uint32_t)trk[k].eng_req << 9)) * 16777619u;
        h = (h ^ ((uint32_t)bl_now << 16 | (uint32_t)song.g[G_BLEND] << 8)) * 16777619u;
    }
    if (pg->scope == SC_ENGINE && (trk_engp(t) == &ENG_WHEEL || trk_eng(t) == ENGI_FM6 ||
                                   (YEHNAH_FM4 && trk_eng(t) == ENGI_DIGITAL)))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->scope == SC_ENGINE && trk_eng(t) == ENGI_DRUM)
        h ^= ui.frame * 2654435761u;                 /* DRUM-X: the specimen moves every frame */
    if (pg->graph == GR_DXSND) h ^= ui.frame * 2654435761u ^ dx_sig();
    if (pg->scope == SC_ENGINE && trk_eng(t) == ENGI_FM6)   /* the patch (PAT's algorithm, levels, FB) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u;
    if (fop_page(pg->graph))                         /* (0.6.77) OPERATOR, OP ENV (track pages): the patch, the operator */
        h = (h ^ ((fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u ^ (uint32_t)fop_op << 8 ^ fop_stg)) * 16777619u;
    if (save_on()) {                                 /* (0.8.5) SAVE's lens: the level, the library's scan, the being */
        lib_poll();
        h = (h ^ (save_level() + 1u) * 2654435761u ^ lib.next * 40503u ^ lib.gen) * 16777619u;
        if (pg->graph == GR_USER)
            h ^= bg_energy() > 2 ? ui.frame * 2654435761u : 0u;   /* (the being moves with the sound) */
        if (pg->graph == GR_SEDIT) {                     /* (SONG EDIT: the rows, their trims, the bar highlighted) */
            for (i = 0; i < CHAIN_ROWS; i++) {
                uint32_t q;
                h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
                for (q = 0; q < NTRK; q++)
                    h = (h ^ (song_idx.scene[song_cur][i].cut[q][0] + 256u * song_idx.scene[song_cur][i].cut[q][1])) * 16777619u;
            }
            h ^= ui.song_row * 40503u + chain_config.count * 7919u + ui.sed_trk * 1299709u;
            h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u : 0u;
        }
        if (pg->graph == GR_SONG || pg->graph == GR_ERASE) {   /* (ARRANGE: as SLOTS', the rows and where the song is) */
            for (i = 0; i < 4u; i++) h ^= (uint32_t)graph_project_used(i) << (20u + i);
            h += graph_pname_sig + (uint32_t)(live.cur + 1) * 40503u;
        }
        if (pg->graph == GR_SLOTS) {
            h ^= SONG_END(&chain_config) * 2246822519u + ui.song_row * 7919u;
            h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u : 0u;
            for (i = 0; i < CHAIN_ROWS; i++) h = (h ^ (uint32_t)song_idx.scene[song_cur][i].on * (i + 1u)) * 16777619u;
        }
        if (pg->graph == GR_BROWSE)
            h ^= favorites.filter * 977u + preset_favorite() * 31u;
    }
    if (fx_on()) {                                   /* (0.8.5) FX's lens: the level, the module, the buses' values, the
                                                         * slicer's step, the output (MASTER), the punch lane */
        h = (h ^ (fx_level() + 1u) * 2654435761u ^ ui.fxmod * 40503u) * 16777619u;
        for (i = 0; i < G_COUNT; i++)
            h = (h ^ (uint32_t)song.g[i]) * 16777619u;
        if (fx_level() == FXL_BUSES || (fx_level() == FXL_EFFECT && fx_mod_of(ui.page) == 3u))   /* (the SLICER drawn) */
            h ^= (song.playing && song.g[G_SLMODE] ? sl.idx + 1u : 0u) * 2654435761u;
        h ^= sl_ui_step * 7919u + (uint32_t)pfx_rec_ok() * 31u;
        if (fx_level() == FXL_BUSES || fx_mod_of(ui.page) == 4u)
            h ^= (uint32_t)(bg_energy() / 16) * 104729u;   /* (MASTER's level bar: 16 steps, not every frame) */
        for (i = 0; i < 32u; i++)
            h = (h ^ pfx_lane[i]) * 16777619u;
    }
    if (lz_on() && lz_fam() == LZ_GLO) {             /* (2026-11-02) GLO's rows (graph_glo): the values, the knob last
                                                         * turned */
        for (i = 0; i < G_COUNT; i++)
            h = (h ^ (uint32_t)song.g[i]) * 16777619u;
        for (i = 0; i < MI_VALUES; i++)
            h = (h ^ menu_get(i)) * 16777619u;
        h = (h ^ (glo_k + 1u) * 40503u ^ (uint32_t)sets_cur[0] * 7u ^ (uint32_t)sets_cur[1] * 61u ^ (uint32_t)sets_cur[2] * 509u ^ (uint32_t)sets_cur[3] * 4093u ^ settings_hold * 7u ^ (uint32_t)(uint16_t)trks_ui * 131u ^ (uint32_t)(uint16_t)load_ui * 977u) * 16777619u;
        if (cur_page()->id[0] == G_TRKS)             /* (SYSTEM: the CPU row and its line, a sample a quarter second) */
            h = (h ^ (fm1_ms / 250u + song.cpu_q8 * 7u)) * 16777619u;
    }
    if (lens_on()) {                                 /* (0.8.3) LENTE: the level; its STEP, the cursor step's every byte */
        h = (h ^ (lens_level() + 1u) * 2654435761u) * 16777619u;
        h = (h ^ (song.playing ? seq_src(t, t->seq_idx) / 16u + 1u : 0u) * 40503u) * 16777619u;   /* (0.8.5) the bar
                                                         * playing: its number lit in the bars */
        if (bigstep_view(pg)) {
            const uint8_t *sb = (const uint8_t *)&seq_steps(t)[ui.cursor % NSTEP];
            for (i = 0; i < sizeof(step_t); i++)
                h = (h ^ sb[i]) * 16777619u;
            h = (h ^ (ui.pr_focus + 1u) * 40503u) * 16777619u;
        }
    }
    if (pg->graph == GR_STEPS || pg->graph == GR_ROLL || pg->graph == GR_CHANCE || pg->graph == GR_SHAPE || roll_view(pg)) {
        uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu, rl = roll_view(pg) || pg->graph == GR_CHANCE;
        if (rl && ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u;
        if (rl) {                                    /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (rl && !drum_track(t)) {                  /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
            h = (h ^ (ui.pr_focus | (uint32_t)pg->id[0] << 4 | (uint32_t)(ui.cp_slot & 1u) << 28 | (ui.entry_open ? (uint32_t)ui.ent_len << 8 : 0u) |
                      (uint32_t)ui.ent_nn << 16)) * 16777619u;   /* (0.6.81: NOTE's focus, the length being held) */
        }
    }
    return h;
}

/* a list row (17 px): the selected one a THEME bar with INK text; tag at x 14, free text from x 54 to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + CAP_IN(S, 16), &AF_S, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(54, y + CAP_IN(S, 16), &AF_S, name, sel ? T_INK : nc, bg, x1 - 54);
}

/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE: the GLO layer's black keys), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];                              /* (the cards: Yehnah's six columns, else the four from trk_base4) */
    uint8_t meter[NTRK];
    uint32_t frame;                                  /* (0.9.0) ui.frame of the meters' last step */
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = trk_engp(t);
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (ENG_NP(e))
        str_cpy(b, e->presets[t->preset % ENG_NP(e)].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* (0.6) the Yehnah style's mixer: the six tracks side by side, 38 px each. The track's cushion (armed: a dot,
 * red recording), the sound's name (muted: MUTE), LEVEL as the thermal column with the meter brighter over it, its dB
 * beside it, PAN (from the middle) and the REV send as the two bars under it */
#define T6_W 38
#define T6_H 122                                     /* (a column's height, under the panel's top 9 px) */
#define T6_X(c) (3 + 39 * (int32_t)(c))
static void track_col6(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t hot, uint32_t lvl, int32_t pan,
                       int32_t rv, const char *b)
{
    char v[8];
    int32_t k, lv = (int32_t)lvl * 14 / 127, mt = ts.meter[c] * 14 / TS_MH, pw = T6_W - 8;
    uint16_t lc = hot == 1u ? T_ACCENT : lvl ? (mute ? T_DIM : T_TEXT) : T_DIM;
    cv_begin(T6_W, T6_H, T_BG);
    cv_rrect(0, 0, T6_W, T6_H, 5, T_SURF, T_BG);
    cv_trk(4, 5, 16, c, sel ? T_ACCENT : mute ? T_DIM : T_MID, T_SURF);
    if (st == 1u || st == 2u)                        /* armed: a dot (recording: red) */
        cv_rrect(T6_W - 11, 10, 6, 6, 3, st == 1u ? T_REC : T_ACCENT, T_SURF);
    if (mute == 2u)                                  /* asleep (TRACKS 4) */
        cv_text_in(2, 22, T6_W - 4, &AF_S, "OFF", T_DIM, T_SURF);
    else {                                           /* (0.6.24) the sound's name cut to the column (its first word, as
                                                      * many letters as fit, no "..."); muted: in MUTE's colour, still
                                                      * there (it said MUTE instead: which sound, gone) */
        char sn[13];
        uint32_t n;
        for (n = 0; b[n] && b[n] != ' ' && n < 12u; n++)
            sn[n] = b[n];
        sn[n] = 0;
        while (n > 1u && text_w(&AF_S, sn) > T6_W - 7)
            sn[--n] = 0;
        cv_text_on(4, 22, &AF_S, sn, mute ? T_ACCENT : sel ? T_TEXT : T_MID, T_SURF);
    }
    if (cur_page()->id[0] == P_DIST) {               /* (0.8.5) GLO > MIX > SENDS: DIST CHORUS DLY REV as bars */
        static const char *const SN[4] = {"D", "C", "D", "R"};
        for (k = 0; k < 4; k++) {
            int32_t v = trk[c].p[P_DIST + (uint32_t)k] * 60 / 127;   /* (P_DIST P_CHOR P_DLY P_REV: in a row) */
            cv_rect(4 + k * 8, 38, 6, 62, T_RAISE);
            if (v)
                cv_rect(4 + k * 8, 100 - v, 6, v, mute ? T_DIM : sel && hot == (uint32_t)k + 1u ? T_ACCENT : T_THEME);
            cv_text(4 + k * 8, 106 - AF_X_CAP_Y, &AF_X, SN[k], T_DIM);
        }
        cv_blit((uint32_t)T6_X(c), graph_y() + 9u);
        return;
    }
    for (k = 0; k < 14; k++) {                       /* LEVEL: cold at the bottom, hot at the top */
        int32_t sy = 95 - k * 4;
        uint16_t sc = heat_col(20 + k * 220 / 13);
        sc = mute ? (k < lv ? T_DIM : T_RAISE) : k < mt ? sc : k < lv ? ux_mix(T_SURF, sc, 42) : T_RAISE;
        cv_rect(4, sy, 12, 3, sc);
    }
    if (lvl) {
        int32_t d = LEVEL_DB_X10[lvl];
        fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
    } else {
        str_cpy(v, "OFF", sizeof v);
    }
    cv_text_in(17, 44, T6_W - 18, &AF_S, v, lc, T_SURF);
    cv_text_in(17, 58, T6_W - 18, &AF_S, "dB", T_DIM, T_SURF);
    cv_rect(4, 104, pw, 3, T_RAISE);                 /* PAN, from the middle */
    cv_rect(4 + pw / 2, 102, 1, 7, hot == 2u ? T_ACCENT : T_MID);
    if (pan)
        cv_rect(pan > 0 ? 4 + pw / 2 + 1 : 4 + pw / 2 + pan * (pw / 2) / 64, 104, (pan > 0 ? pan : -pan) * (pw / 2) / 64 + 1,
                3, mute ? T_DIM : hot == 2u ? T_ACCENT : heat_col(70 + (pan > 0 ? pan : -pan) * 2));
    cv_rect(4, 113, pw, 3, T_RAISE);                 /* the REV send */
    if (rv)
        cv_rect(4, 113, rv * pw / 127 + 1, 3, mute ? T_DIM : hot == 3u ? T_ACCENT : heat_col(40 + rv * 3 / 2));
    cv_blit((uint32_t)T6_X(c), graph_y() + 9u);
}
static void draw_tracks(void)
{
    uint32_t c;                                      /* (0.6.80: the six columns with every COLOR and STYLE) */
    if (ui.force) {
        lcd_clear(0, graph_y(), 240, graph_h(), T_BG);
        if (ui.frame - ts.frame > 1u)                /* (0.9.0) a redraw while they fall (a toast, a knob's page)
                                                      * keeps them falling: only meters left long ago start at 0
                                                      * (they jumped to 0 and the panel kept the old segments lit) */
            for (c = 0; c < NTRK; c++)
                ts.meter[c] = 0;
    }
    ts.frame = ui.frame;
    for (c = 0; c < NTRK; c++) {                     /* (0.6: the six) */
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0;
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        if (c >= trk_live)                           /* (0.6.22) TRACKS 4: asleep, grey as muted, "OFF" (mute 2) */
            mute = 2u;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV (4: the DJ FILTER, on its card) */
        int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        char b[16];
        t->peak = 0;
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + c * 0x51u +
              (cur_page()->id[0] == P_DIST ? 0x7F4Au + (uint32_t)t->p[P_DIST] * 3u + (uint32_t)t->p[P_CHOR] * 389u +
                                (uint32_t)t->p[P_DLY] * 6151u + (uint32_t)t->p[P_REV] * 98317u : 0u);   /* (SENDS) */
        if (!ui.force && sig == ts.col[c])
            continue;
        ts.col[c] = sig;
        track_col6(c, sel, st, mute, hot, lvl, pan, rv, b);
    }
}
/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void scope_at(int32_t x0, int32_t pw, int32_t cy, int32_t a, uint16_t c);
static void graph_scope(uint16_t c)
{
    scope_at(PANEL_X0, PANEL_W, (int32_t)cv_h / 2, (int32_t)cv_h / 2 - 6, c);
}
/* .. pw px wide from x0, about the line cy, +-a px (0.8.5: EDIT's SOUND, in its well) */
static void scope_at(int32_t x0, int32_t pw, int32_t cy, int32_t a, uint16_t c)
{
    static int16_t snap[SCOPE_N];
    uint32_t w = scope_w, i, trig = 0;
    int32_t py = cy, x, peak = 1500;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(x0, cy, pw, 1, T_RAISE);
    for (x = 0; x < pw; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;    /* auto-scaled */
        if (x)
            cv_line_t(x0 - 1 + x, py, x0 + x, y, c, 2);
        py = y;
    }
}

/* SONG: the rows, each a section (the saved project A..D, sounds and steps) and its bars (song_chain.c) */
/* (Yehnah) MSEQ: the 16 levels as bars (past LEN dim), the step KNOB 1 picked framed, the one playing in the accent, and
 * the level the matrix reads now (STEP, slewed) as a line */
static void graph_mseq(const track_t *t, uint16_t c)
{
    uint32_t i, len = t->p[P_MSLEN] > 0 ? (uint32_t)t->p[P_MSLEN] : 1u;
    uint32_t per = div_samples((uint32_t)t->p[P_MSDIV]), now = song.playing && per ? ms_clock / per % len : 0xFFu;
    int32_t y0 = 92, hmax = 80;
    for (i = 0; i < 16u; i++) {
        int32_t x = 13 + (int32_t)i * 13, h = t->p[P_MS0 + i] * hmax / 127;
        uint16_t col = i >= len ? T_RAISE : i == now ? T_ACCENT : c;
        cv_rrect(x, y0 - hmax, 10, hmax, 2, T_RAISE, T_SURF);
        if (h > 0)
            cv_rrect(x, y0 - (h < 3 ? 3 : h), 10, h < 3 ? 3 : h, 2, col, T_RAISE);
        if (i == ms_ui_step)
            cv_rect(x, y0 + 3, 10, 2, T_TEXT);
    }
    if (song.playing)
        cv_rect(13, y0 - t->ms_v * hmax / 32767, 205, 1, T_TEXT);
}
/* (0.6.22) ARRANGE / SEQ > SONG, after SLOOP's arranger in Yehnah's colours: the song as a column of rows, each a section
 * chip (A..D, each its own heat), its bars as blocks (one a bar, up to 16; past 16 each a 16th of it) filling as they
 * play, the bars' count, a dot for its scene; under them + ADD (the rows used of CHAIN_ROWS) and the song's END (STOP,
 * x2..x8, LOOP). On the right the whole song as a strip, each row's height its share of the bars, the play head on it,
 * the bars in all. KNOB 1 the row, 2 its section (on END: STOP / x2 .. / LOOP), 3 its bars, 4 its scene */
static uint16_t sec_col(uint32_t s)
{
    static const uint8_t HO[4] = {120, 160, 200, 240};
    return heat_col(HO[s & 3u]);
}
#include "ui_tech.c"                                /* (0.5) the technical graphs (FX, DEST, VOICE, GLOBAL) */
#include "ui_lzsave.c"                              /* (0.8.5) SAVE's lens: its levels' panels */
#include "ui_lzfx.c"                                /* (0.8.5) FX's lens: its levels' panels */
#include "ui_lzhome.c"                              /* (0.8.5) HOME's lens: ECO, BEING, the critters */
#include "ui_lzedit.c"                               /* (0.8.5) EDIT's lens panels */
#include "ui_lzlist.c"                               /* (2026-11-11) EDIT > PATCH: FMINI's list */
#include "ui_lzpage.c"                               /* (0.8.5) ENV's, LFO . MOD's, SCL . ARP's, GLO's */
static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    {   /* (0.6.57) LOAD LITE / ECO: the panel's animation (the beings, the scope, LFO, the envelope's dot, a turn
         * redrawing a curve) at most every 50 / 70 ms (~20 / 14 frames a second, FULL: as fast as it changes), so the
         * main loop keeps its time for the keys, the knobs and the LCD. A new page, track or engine: at once */
        static uint32_t gr_ms, gr_key;
        uint32_t key = ui.page * 7u + song.sel * 131u + ui.home * 3u + TSEL->eng_req * 1031u;
        if (sys_load && !ui.force && key == gr_key && fm1_ms - gr_ms < (sys_load == LOAD_ECO ? 70u : 50u))
            return;                                  /* (graph_sig stays: drawn on a later frame) */
        gr_key = key;
        gr_ms = fm1_ms;
    }
    ui.graph_sig = sig;
    if (sets_full() && lz_on() && lz_fam() == LZ_GLO && pg->graph == GR_SETS) {   /* (2026-11-09, topic E) a list page: the panel is taller
                                                         * than the canvas (164 rows): two bands */
        uint32_t pass, h0 = graph_h() / 2u;
        for (pass = 0; pass < 2u; pass++) {
            cv_begin(240, pass ? graph_h() - h0 : h0, T_BG);
            cv_oy = pass ? -(int32_t)h0 : 0;
            cv_scroll = 1;
            cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);
            cv_bg = T_SURF;
            graph_sets();
            cv_scroll = 0;
            cv_oy = 0;
            cv_blit(0, graph_y() + pass * h0);
        }
        return;
    }
    cv_begin(240, graph_h(), T_BG);
    if (lz_on())                                /* (0.8.5) a lens page: the mock-up's panel, 4 .. 236 (0.8.9 COLORS: */
        cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);   /* rounded, as the 0.6's) */
    else
        cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    if (drift_on())
        graph_drift();
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (ui.confirm == CF_DICE_ALL) {          /* (0.8.5) GEN, HOME's lens's last level (gen.c) */
        cv_oy = 0;
        gen_panel();
    } else if (ui.home) {
        cv_oy = 0;
        graph_home_lz();                             /* (0.8.5: HOME's lens, ECO / BEING; 0.5: the system's
                                                      * beings, was the scope) */
    } else if (pg->graph == GR_PLIST) {              /* (2026-11-11) EDIT > PATCH (FMINI): the list */
        cv_oy = 0;
        graph_plist(t);
    } else if (bars_view(pg)) {                      /* (0.8.3) LENTE's PATTERN: PATTERN STRETCH TRACK, the bars */
        cv_oy = 0;
        graph_bars(t, c);
    } else if (bigstep_view(pg)) {                   /* .. its STEP: NOTE COMP COMP 2, the step large */
        cv_oy = 0;
        graph_bigstep(t, pg, c);
    } else if (lz_on() && lz_fam() == LZ_SAVE) {     /* (0.8.5) SAVE's lens */
        cv_oy = 0;
        graph_save_lz(pg);
    } else if (lz_on() && lz_fam() == LZ_FX) {       /* (0.8.5) FX's lens */
        cv_oy = 0;
        graph_fx_lz(t, pg);
    } else if (lz_on() && lz_fam() == LZ_EDIT && (cv_oy = 0, graph_edit_lz(t, pg))) {   /* (0.8.5) EDIT's (ui_lzedit.c) */
    } else if (lz_on() && lz_fam() == LZ_ENV && (cv_oy = 0, graph_env_lz(t, pg))) {     /* .. ENV's (ui_lzpage.c) */
    } else if (lz_on() && lz_fam() == LZ_MOD && (cv_oy = 0, graph_mod_lz(t, pg))) {     /* .. LFO . MOD's */
    } else if (lz_on() && lz_fam() == LZ_KEY && (cv_oy = 0, graph_key_lz(t, pg))) {     /* .. SCL . ARP's */
    } else if (lz_on() && lz_fam() == LZ_GLO && pg->graph == GR_SETS) {              /* .. GLO's lists (topic C) */
        cv_oy = 0;
        graph_sets();
    } else if (lz_on() && lz_fam() == LZ_GLO && pg->id[0] == G_TRKS) {               /* .. GLO's SYSTEM */
        cv_oy = 0;
        graph_system();
    } else if (lz_on() && lz_fam() == LZ_GLO && (IS_MROW(pg->id[0]) || IS_MROW(pg->id[2]))) {   /* .. its MENU rows' */
        cv_oy = 0;
        graph_glo(pg);
    } else {
        cv_oy = GOY;
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_BLEND:
            graph_blend();
            break;
        case GR_DXIMG:                               /* KIT 3: the colony (2026-11-10: STEREO merged in) */
            cv_oy = 0;
            vx_kit_page(t, -1);                      /* (chord-harp fork: the kit, was the colony) */
            break;
        case GR_ROLL:
        case GR_CHANCE:
        case GR_SHAPE:                               /* (0.6.85) STRETCH: the whole sequence, as the roll shows it */
            cv_oy = 0;
            if (drum_track(t))
                graph_grid(t, c);
            else
                graph_roll(t, c);
            break;
        case GR_FMOP:                                /* (0.6.10) FM6's operators: (0.9.6) the colony (ui_engvis.c), the operator */
        case GR_FMOP2:                               /* picked lit */
            if (fop_ok()) vx_fm6_page(t, fop_op % 6u, pg->graph == GR_FMOP2);
            break;
        case GR_FMENV:                               /* .. its envelope, its bicho riding it */
            if (fop_ok()) vx_fm6env(t);
            break;
        case GR_DXSND:                               /* EDIT > SOUND: the kit's lanes, the sound edited singled out */
            cv_oy = 0;
            if (pg->title[0] == 'S')                 /* (0.9.4) SOUND 1..4: the drum kit, the sound edited lit (ui_engvis.c) */
                vx_kit_page(t, (int32_t)(dx_ui_lane & 7u));
            else
                vx_kit_page(t, (int32_t)(dx_ui_lane & 7u));   /* (chord-harp fork: the kit, was the colony) */
            break;
        default:
            if (pg->scope == SC_ENGINE && trk_eng(t) == ENGI_DRUM) {   /* (0.8.5: WHEEL's and FM6's EDIT 1
                                                      * and 2 their beings, as every synth's: EDIT's lens, SOUND) */
                cv_oy = 0;
                vx_kit_page(t, -1);                  /* (chord-harp fork: KIT, KIT 2, X-MOD the kit; was KIT 2 the colony) */
            }
#if YEHNAH_FM4
            else if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && trk_eng(t) == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
#endif
            else if (pg->scope == SC_ENGINE) {       /* (chord-harp fork: the engine's pixel art; 0.5: the being) */
                cv_oy = 0;
                if (!vx_draw(t, 4, 0, 232, (int32_t)graph_h(), -1))
                    graph_scope(c);
            }
            else {                                   /* the FX, DEST, VOICE, GLOBAL pages: their graphs (else the scope) */
                cv_oy = 0;
                if (!graph_tech(t, pg))
                    graph_scope(c);
            }
            break;
        }
    }
    cv_oy = 0;
    if (toast_on())                                  /* (0.9.5) a toast: a small pill near the panel's foot, over the page,
                                                      * which goes on drawing and moving behind it (0.8.5 .. 0.9.4: the
                                                      * panel cleared, the message boxed alone in it) */
        toast_pill(), rdm.on = 0;                    /* (the big FM-1: a toast over the road, the tablet's tiles) */
    else
        toast_at[0] = toast_at[2] = 0;
    cv_blit(0, graph_y());                           /* (rdm.on: the tablet draws this scene itself, tab_screen.c ts_road) */
    rdm.on = 0;
}
