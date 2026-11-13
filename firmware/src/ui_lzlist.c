/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (2026-11-11) The SOUND page: the engine's settings as a list on the panel, a row each (params.c sound_row), its (up to) three
 * settings as "LBL value" in three equal columns split by thin lines, one per knob (KNOB 2..4); the row's name is KNOB 1's
 * card (not repeated here). A window of SND_ROWS rows of 16 px follows the row KNOB 1 is on (lit); a thin bar at the right edge
 * shows the window in a long list. Included by ui_graph.c. */
static const char *kn_name(const char *l);   /* (ui_draw.c) the full word of a knob label */
#define SND_ROWS 6                                       /* (2026-11-11) six rows, the names in the 12 px face */
#define SND_X0 6                                         /* the list's left, its width */
#define SND_W 226
/* the rows of list page pg in the band y0 .. y0 + bh (rows of rh), the cursor row lit; SOUND's is the whole panel (rh 22) */
static void graph_rows(const track_t *t, const page_t *pg, int32_t y0, int32_t rh, int32_t maxrows)
{
    int32_t r, k, y, top, vis, nc = 1, cw;
    uint32_t n = lp_n(pg), cur = *lp_cur(pg);
    for (r = 0; r < (int32_t)n; r++)                     /* the columns: the most settings of any row of this engine (2 or 3): only two
                                                         * everywhere = two cells; a mix = three, a row of two uses the first two */
        for (k = nc; k < 3; k++)
            if (lp_row(pg, (uint32_t)r)->id[k] != 0xFFu)
                nc = k + 1;
    if (nc < 2)
        nc = 2;
    cw = SND_W / nc;
    vis = (int32_t)n < maxrows ? (int32_t)n : maxrows;
    top = (int32_t)cur - (maxrows > 3 ? 2 : 0);
    if (top > (int32_t)n - vis)
        top = (int32_t)n - vis;
    if (top < 0)
        top = 0;
    for (r = 0; r < vis; r++) {                          /* names only, the values are the knob cards' (the row on) */
        int on = (uint32_t)(top + r) == cur;
        const sound_row_t *w = lp_row(pg, (uint32_t)(top + r));
        uint16_t bg = on ? T_SEL : T_SURF;
        y = y0 + r * rh;
        if (on)
            cv_rrect(SND_X0, y, SND_W, rh - 1, 3, T_SEL, T_SURF);
        for (k = 0; k < 3; k++) {
            const param_desc_t *d;
            const char *lb;
            const aafont_t *f = &AF_S;
            int32_t cx = SND_X0 + k * cw, tw, ch = AF_S_CAP_H, cy = AF_S_CAP_Y;
            if (k >= nc || w->id[k] == 0xFFu || !(d = lp_desc(pg, t, w->id[k])))
                continue;
            lb = kn_name(w->s[k] ? w->s[k] : d->label);
            tw = text_w(f, lb);
            if (tw > cw - 6) {                           /* too wide for its cell: the small pixel face */
                f = &AF_X; ch = AF_X_CAP_H; cy = AF_X_CAP_Y;
                tw = text_w(f, lb);
            }
            cv_text_on(cx + (cw - tw) / 2, y + (rh - 1 - ch) / 2 - cy, f, lb, on ? T_INK : T_TEXT, bg);
        }
    }
    for (k = 1; k < nc; k++)                             /* the knobs' dividers: KNOB 2 | 3 | 4 */
        cv_rect(SND_X0 + k * cw, y0 + 2, 1, vis * rh - 4, T_DIM);
    if ((int32_t)n > vis) {                              /* the window in the list */
        int32_t bh = vis * rh - 2, h = bh * vis / (int32_t)n, o = (bh - h) * top / ((int32_t)n - vis);
        cv_rect(236, y0 + 1 + o, 2, h, T_MID);
    }
}
static void graph_plist(const track_t *t)
{
    graph_rows(t, cur_page(), 4, ((int32_t)cv_h - 6) / SND_ROWS, SND_ROWS);
}
/* (2026-11-12) the row-list pages (RL_TAB): the picture strip on top where the picture carries information (the envelope with the loudest
 * voice's dot, the LFO running, DELAY's taps, REVERB's tail), the rows under it (rows of 20, 18 when there are four); a list with no
 * picture (CHORUS, SL BUS, MASTER, ARP) takes the whole panel like SOUND */
#define RL_RH 20
static void graph_rlist(const track_t *t, const page_t *pg)
{
    uint32_t kind = rl_idx(pg), n = lp_n(pg);
    int32_t rh = n > 3u ? 18 : RL_RH, y0 = (int32_t)cv_h - (int32_t)n * rh - 4, bh = y0 - 8, x = 10, w = 216;
    if (kind > 3u && kind != 8u) {                      /* no picture: the whole panel (a few rows: big ones) */
        int32_t rows = n < 3u ? 3 : n > SND_ROWS ? SND_ROWS : (int32_t)n;
        graph_rows(t, pg, 4, ((int32_t)cv_h - 6) / rows, rows);
        return;
    }
    if (kind >= 2u) {                                   /* DELAY's taps, REVERB's tail, PUNCH's lane (the FX lens's pictures) */
        fxg_tick(t);
        if (kind == 2u)
            fxg_delay(8, 6, 224, bh, 0);
        else if (kind == 3u)
            fxg_reverb(8, 6, 224, bh, 0);
        else
            fxg_punch(8, 6, 224, bh, 0);
        graph_rows(t, pg, y0, rh, (int32_t)n);
        return;
    }
    if (kind == 0u) {
        int32_t a = 2 + t->p[P_ATK] * (w / 4) / 127, d = 3 + t->p[P_DEC] * (w / 4) / 127, r = 3 + t->p[P_REL] * (w / 4) / 127;
        int32_t s = t->p[P_SUS] * bh / 127, b = 6 + bh, x1 = x + a, x3 = x + w - r, i;
        const voice_t *bv = 0;
        cv_rect(x, b + 1, w, 1, T_RAISE);
        env_mini(t, x, 6, w, bh, T_THEME, 2);
        for (i = 0; i < NVOICE; i++)                    /* the loudest voice: its level as a dot (stage 1 on the attack, 2 the decay, 3 the release) */
            if (t->v[i].stage && t->v[i].stage < 4u && (!bv || t->v[i].env > bv->env))
                bv = &t->v[i];
        if (bv) {
            int32_t lvl = (int32_t)(((int64_t)bv->env * 1000) >> 24), dx, dy = b - lvl * bh / 1000;
            dx = bv->stage == 1u ? x + a * lvl / 1000
               : bv->stage == 2u ? x1 + (s < bh ? clamp((1000 - lvl) * d / 1000, 0, d) : 0)
                                 : x3 + (s > 0 ? clamp(r - lvl * r / 1000, 0, r) : r);
            cv_rect(dx, dy, 1, b + 2 - dy, T_MID);
            cv_rrect(dx - 3, dy - 3, 7, 7, 3, T_TEXT, T_SURF);
        }
    } else {
        int32_t cy = 6 + bh / 2, amp = bh / 2 - 2, px = 0, py = cy, xx, xp = w / 4;
        uint32_t span = 0xFFFFFFFFu / ((uint32_t)w / 2u), ph = t->lfo_ph - (uint32_t)xp * span;
        cv_rect(x, cy, w, 1, T_RAISE);
        for (xx = 0; xx < w; xx++) {
            uint32_t q = ph + (uint32_t)xx * span;
            int32_t y = cy - lfo_wave((track_t *)t, q) * amp / 32768;
            if (t->p[P_LWAVE] == 4)
                y = cy - ((int32_t)((((q >> 29) + 1u) * 2654435761u) >> 16) - 32768) * amp / 32768;
            if (xx)
                cv_line_t(x + px, py, x + xx, y, xx < xp ? ux_mix(T_SURF, T_THEME, 45) : T_THEME, 2);
            px = xx;
            py = y;
        }
        cv_rect(x + xp, 6, 1, bh, T_MID);
        cv_rrect(x + xp - 3, cy - mulq15(t->lfo_val, t->lfo_fade) * amp / 32768 - 3, 7, 7, 3, T_TEXT, T_SURF);
    }
    graph_rows(t, pg, y0, rh, (int32_t)n);
}
