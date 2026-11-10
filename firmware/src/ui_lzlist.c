/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (2026-11-11) EDIT > PATCH on FMINI: the whole patch as a list on the panel, a row each (params.c PLIST), its (up to) three settings
 * as "LBL value". The row KNOB 1 is on is lit; KNOB 2..4 (the cells below) turn its settings. Included by ui_graph.c. */
static void graph_plist(const track_t *t)
{
    int32_t rh = ((int32_t)cv_h - 6) / (int32_t)PLIST_N, r, k, y;
    static const int16_t X[3] = {64, 121, 178};
    if (rh > 14)
        rh = 14;
    for (r = 0; r < (int32_t)PLIST_N; r++) {
        int on = (uint32_t)r == plist_cur;
        const plist_row_t *w = &PLIST[r];
        uint16_t bg = on ? T_SEL : T_SURF;
        y = 4 + r * rh;
        if (on)
            cv_rrect(6, y, 228, rh - 1, 3, T_SEL, T_SURF);
        cv_text_on(11, y + (rh - 1 - AF_X_CAP_H) / 2 - AF_X_CAP_Y, &AF_X, w->name, on ? T_INK : T_MID, bg);
        for (k = 0; k < 3; k++) {
            char b[16], val[12];
            const char *unit = "";
            const param_desc_t *d;
            if (w->id[k] == 0xFFu)
                continue;
            d = track_desc(t, w->id[k]);
            if (!d)
                continue;
            param_format(d, (int16_t)plist_val(t, w->id[k]), val, &unit);
            str_cpy(b, w->s[k], 5);
            str_cpy(b + str_len(b), " ", 2);
            str_cpy(b + str_len(b), val, 8);
            cv_text_on(X[k], y + (rh - 1 - AF_X_CAP_H) / 2 - AF_X_CAP_Y, &AF_X, b, on ? T_INK : T_TEXT, bg);
        }
    }
}
