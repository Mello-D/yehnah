/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (2026-11-11) The SOUND page: the engine's settings as a list on the panel, a row each (params.c sound_row), its (up to) three
 * settings as "LBL value" in three equal columns split by thin lines, one per knob (KNOB 2..4); the row's name is KNOB 1's
 * card (not repeated here). A window of SND_ROWS rows of 16 px follows the row KNOB 1 is on (lit); a thin bar at the right edge
 * shows the window in a long list. Included by ui_graph.c. */
static const char *kn_name(const char *l);   /* (ui_draw.c) the full word of a knob label */
#define SND_ROWS 8
#define SND_X0 6                                         /* the list's left, its width, a column's */
#define SND_W 226
static void graph_plist(const track_t *t)
{
    int32_t rh = ((int32_t)cv_h - 6) / SND_ROWS, r, k, y, top, vis;
    uint32_t n = sound_n(t);
    if (rh > 16)
        rh = 16;
    vis = (int32_t)n < SND_ROWS ? (int32_t)n : SND_ROWS;
    top = (int32_t)plist_cur - 3;
    if (top > (int32_t)n - vis)
        top = (int32_t)n - vis;
    if (top < 0)
        top = 0;
    for (r = 0; r < vis; r++) {                          /* (2026-11-11) names only, the values are the knob cards' (the row on); a row's
                                                         * columns are its own settings: 1, 2 or 3 equal cells with thin dividers */
        int on = (uint32_t)(top + r) == plist_cur;
        const sound_row_t *w = sound_row(t, (uint32_t)(top + r));
        uint16_t bg = on ? T_SEL : T_SURF;
        int32_t nc = 0, cw, c = 0;
        for (k = 0; k < 3; k++)
            if (w->id[k] != 0xFFu && track_desc(t, w->id[k]))
                nc++;
        if (!nc)
            continue;
        cw = SND_W / nc;
        y = 4 + r * rh;
        if (on)
            cv_rrect(SND_X0, y, SND_W, rh - 1, 3, T_SEL, T_SURF);
        for (k = 0; k < 3; k++) {
            const param_desc_t *d;
            const char *lb;
            int32_t cx = SND_X0 + c * cw, tw;
            if (w->id[k] == 0xFFu || !(d = track_desc(t, w->id[k])))
                continue;
            lb = kn_name(w->s[k] ? w->s[k] : d->label);
            tw = text_w(&AF_X, lb);
            cv_text_on(cx + (cw - tw) / 2, y + (rh - 1 - AF_X_CAP_H) / 2 - AF_X_CAP_Y, &AF_X, lb, on ? T_INK : T_TEXT, bg);
            if (c > 0)
                cv_rect(cx, y + 3, 1, rh - 7, T_DIM);
            c++;
        }
    }
    if ((int32_t)n > vis) {                              /* the window in the list */
        int32_t bh = vis * rh - 2, h = bh * vis / (int32_t)n, o = (bh - h) * top / ((int32_t)n - vis);
        cv_rect(236, 5 + o, 2, h, T_MID);
    }
}
