/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (2026-11-11) The SOUND page: the engine's settings as a list on the panel, a row each (params.c sound_rows), its (up to) three
 * settings as "LBL value" in three equal columns split by thin lines, one per knob (KNOB 2..4); the row's name is KNOB 1's
 * card (not repeated here). A window of SND_ROWS rows of 16 px follows the row KNOB 1 is on (lit); a thin bar at the right edge
 * shows the window in a long list. Included by ui_graph.c. */
#define SND_ROWS 8
#define SND_X0 6                                         /* the list's left, its width, a column's */
#define SND_W 226
static void graph_plist(const track_t *t)
{
    int32_t rh = ((int32_t)cv_h - 6) / SND_ROWS, r, k, y, top, vis;
    uint32_t n;
    const sound_row_t *rw = sound_rows(t, &n);
    int32_t nc = 1, cw;
    for (r = 0; r < (int32_t)n; r++)                     /* the columns: as many as the most settings of a row (2 or 3) */
        for (k = nc; k < 3; k++)
            if (rw[r].id[k] != 0xFFu)
                nc = k + 1;
    if (nc < 2)
        nc = 2;
    cw = SND_W / nc;
    if (rh > 16)
        rh = 16;
    vis = (int32_t)n < SND_ROWS ? (int32_t)n : SND_ROWS;
    top = (int32_t)plist_cur - 3;
    if (top > (int32_t)n - vis)
        top = (int32_t)n - vis;
    if (top < 0)
        top = 0;
    for (r = 0; r < vis; r++) {
        int on = (uint32_t)(top + r) == plist_cur;
        const sound_row_t *w = &rw[top + r];
        uint16_t bg = on ? T_SEL : T_SURF;
        y = 4 + r * rh;
        if (on)
            cv_rrect(SND_X0, y, SND_W, rh - 1, 3, T_SEL, T_SURF);
        for (k = 0; k < 3; k++) {
            char b[16], val[12];
            const char *unit = "";
            const param_desc_t *d;
            int32_t cx = SND_X0 + k * cw + 2, tw;
            if (k >= nc || w->id[k] == 0xFFu)
                continue;
            d = track_desc(t, w->id[k]);
            if (!d)
                continue;
            param_format(d, (int16_t)plist_val(t, w->id[k]), val, &unit);
            {                                          /* (the label gives way to the value: a column's width) */
                char lb[6];
                uint32_t ll;
                str_cpy(lb, w->s[k] ? w->s[k] : d->label, 6);
                for (ll = str_len(lb);; ll--) {
                    str_cpy(b, lb, ll + 1u);
                    str_cpy(b + str_len(b), " ", 2);
                    str_cpy(b + str_len(b), val, 8);
                    if (ll <= 2u || text_w(&AF_X, b) <= cw - 6)
                        break;
                }
            }
            tw = text_w(&AF_X, b);
            cv_text_on(cx + (cw - 4 - tw) / 2, y + (rh - 1 - AF_X_CAP_H) / 2 - AF_X_CAP_Y, &AF_X, b, on ? T_INK : T_TEXT, bg);
        }
    }
    for (k = 1; k < nc; k++)                             /* the knobs' dividers: KNOB 2 | 3 | 4 */
        cv_rect(SND_X0 + k * cw, 6, 1, vis * rh - 4, T_DIM);
    if ((int32_t)n > vis) {                              /* the window in the list */
        int32_t bh = vis * rh - 2, h = bh * vis / (int32_t)n, o = (bh - h) * top / ((int32_t)n - vis);
        cv_rect(236, 5 + o, 2, h, T_MID);
    }
}
