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
static void graph_plist(const track_t *t)
{
    int32_t rh = ((int32_t)cv_h - 6) / SND_ROWS, r, k, y, top, vis, nc = 1, cw;
    uint32_t n = sound_n(t);
    for (r = 0; r < (int32_t)n; r++)                     /* the columns: the most settings of any row of this engine (2 or 3): only two
                                                         * everywhere = two cells; a mix = three, a row of two uses the first two */
        for (k = nc; k < 3; k++)
            if (sound_row(t, (uint32_t)r)->id[k] != 0xFFu)
                nc = k + 1;
    if (nc < 2)
        nc = 2;
    cw = SND_W / nc;
    vis = (int32_t)n < SND_ROWS ? (int32_t)n : SND_ROWS;
    top = (int32_t)plist_cur - 2;
    if (top > (int32_t)n - vis)
        top = (int32_t)n - vis;
    if (top < 0)
        top = 0;
    for (r = 0; r < vis; r++) {                          /* names only, the values are the knob cards' (the row on) */
        int on = (uint32_t)(top + r) == plist_cur;
        const sound_row_t *w = sound_row(t, (uint32_t)(top + r));
        uint16_t bg = on ? T_SEL : T_SURF;
        y = 4 + r * rh;
        if (on)
            cv_rrect(SND_X0, y, SND_W, rh - 1, 3, T_SEL, T_SURF);
        for (k = 0; k < 3; k++) {
            const param_desc_t *d;
            const char *lb;
            const aafont_t *f = &AF_S;
            int32_t cx = SND_X0 + k * cw, tw, ch = AF_S_CAP_H, cy = AF_S_CAP_Y;
            if (k >= nc || w->id[k] == 0xFFu || !(d = track_desc(t, w->id[k])))
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
        cv_rect(SND_X0 + k * cw, 6, 1, vis * rh - 4, T_DIM);
    if ((int32_t)n > vis) {                              /* the window in the list */
        int32_t bh = vis * rh - 2, h = bh * vis / (int32_t)n, o = (bh - h) * top / ((int32_t)n - vis);
        cv_rect(236, 5 + o, 2, h, T_MID);
    }
}
