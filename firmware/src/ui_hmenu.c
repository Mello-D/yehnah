/* SPDX-License-Identifier: GPL-3.0-only */
/* (2026-11-09, held menus) SCL, EDIT and GLO held past HOLD open a list menu (the GLO lists' look: names left, the row's
 * choices or its range right) that STAYS open after the button is let go. Left with OCT- or a tap of the button (any other
 * button's tap closes it and acts). The piano keys play as normal: no key shortcuts here.
 *   SCL   ROOT, SCALE, QUANT, ALL TR, STRUM
 *   EDIT  the sound's named parameters (E1..E8), INIT, RECALL
 *   GLO   the four GLO lists (MAIN DEVICE CONTROL SYSTEM): KNOB 3 picks the list, the rows are the GLO pages' own
 * KNOB 1 the row, KNOB 2 its value (an action: a turn right asks). A tap of the button is still its page (ui_input.c).
 * Included after ui_menu.c (the list's drawing) and ui_input.c (edit_param). */
enum { HM_NONE, HM_SCL, HM_EDIT, HM_GLO };
enum { HR_TRK, HR_ALLTR, HR_GLB, HR_INIT, HR_RECALL };
typedef struct {
    const char *nm;
    uint16_t id;                       /* HR_TRK: the track parameter; HR_GLB: the list row (SETS) */
    uint8_t kind;
} hrow_t;
#define HM_MAXR 10u
static uint8_t hm_btn;                 /* the button that opened it */
static uint8_t hm_pos[3], hm_list;     /* the row KNOB 1 is on (SCL, EDIT); GLO: the list (its rows: sets_cur) */
static uint32_t hm_sig;
static const char *const HM_LISTS[4] = {"MAIN", "DEVICE", "CONTROL", "SYSTEM"};

static uint32_t hm_rows(hrow_t *r)
{
    uint32_t n = 0, k;
    if (ui.hm == HM_SCL) {
        static const hrow_t S[5] = {{"ROOT", P_ROOT, HR_TRK}, {"SCALE", P_SCALE, HR_TRK}, {"QUANT", P_QUANT, HR_TRK},
                                    {"ALL TR", 0, HR_ALLTR}, {"STRUM", P_CSTR, HR_TRK}};
        for (; n < 5u; n++)
            r[n] = S[n];
    } else if (ui.hm == HM_EDIT) {
        for (k = 0; k < 8u; k++) {
            const param_desc_t *d = track_desc(TSEL, P_E0 + k);
            if (d && d->label && d->label[0] != '-' && d->max > d->min) {
                r[n].nm = d->label;
                r[n].id = (uint16_t)(P_E0 + k);
                r[n++].kind = HR_TRK;
            }
        }
        r[n].nm = "INIT"; r[n].id = 0; r[n++].kind = HR_INIT;
        r[n].nm = "RECALL"; r[n].id = 0; r[n++].kind = HR_RECALL;
    } else if (ui.hm == HM_GLO) {
        uint32_t ln = sets_n(hm_list & 3u);
        for (; n < ln; n++) {
            r[n].id = SETS[hm_list & 3u][n];
            r[n].nm = sets_label(r[n].id);
            r[n].kind = HR_GLB;
        }
    }
    return n;
}
static uint32_t hm_cur(uint32_t n)
{
    uint8_t *c = ui.hm == HM_GLO ? &sets_cur[hm_list & 3u] : &hm_pos[ui.hm - 1u];
    if (*c >= n)
        *c = (uint8_t)(n ? n - 1u : 0u);
    return *c;
}
static const param_desc_t *hm_desc(const hrow_t *r, int16_t **vp)
{
    *vp = 0;
    if (r->kind == HR_TRK) {
        *vp = &TSEL->p[r->id];
        return track_desc(TSEL, r->id);
    }
    if (r->kind == HR_ALLTR) {
        *vp = &song.g[G_STRN];
        return &GP[G_STRN];
    }
    return 0;
}
static void hm_valstr(const hrow_t *r, char *v)
{
    int16_t *vp;
    const param_desc_t *d;
    const char *unit = "";
    v[0] = 0;
    if (r->kind == HR_GLB) {
        sets_val(r->id, v);
        return;
    }
    if ((d = hm_desc(r, &vp)) != 0 && vp)
        param_format(d, *vp, v, &unit);
}
static void hm_detail(const hrow_t *r)
{
    int16_t *vp;
    const param_desc_t *d;
    char v[24], lo[16], hi[16];
    const char *unit = "";
    if (r->kind == HR_GLB) {
        sets_detail(r->id);
        return;
    }
    if (r->kind == HR_INIT || r->kind == HR_RECALL) {
        cv_text_in(SETS_RX, 56, SETS_RW, &AF_S, "KNOB 2 RIGHT", T_THEME, T_SURF);
        cv_text_in(SETS_RX, 78, SETS_RW, &AF_S, r->kind == HR_INIT ? "INIT THE SOUND" : "RECALL THE SOUND", T_MID, T_SURF);
        return;
    }
    if ((d = hm_desc(r, &vp)) == 0 || !vp)
        return;
    if ((d->fmt == F_ENUM || d->fmt == F_ONOFF) && d->max > d->min && d->max - d->min < 8) {
        sets_choices(r->id, (uint32_t)(d->max - d->min + 1), (uint32_t)(*vp - d->min), d);
        return;
    }
    hm_valstr(r, v);
    if (d->max > d->min && d->fmt != F_ENUM) {
        int32_t w = SETS_RW - 20, f = (int32_t)(*vp - d->min) * w / (d->max - d->min);
        cv_text_in(SETS_RX, 60, SETS_RW, &AF_M, v, T_THEME, T_SURF);
        param_format(d, d->min, lo, &unit);
        param_format(d, d->max, hi, &unit);
        cv_rrect(SETS_RX + 10, 92, w, 6, 3, T_RAISE, T_SURF);
        if (f > 0)
            cv_rrect(SETS_RX + 10, 92, f < 6 ? 6 : f, 6, 3, T_THEME, T_RAISE);
        cv_text_on(SETS_RX + 10, 104 - AF_X_CAP_Y, &AF_X, lo, T_MID, T_SURF);
        cv_text_r(SETS_RX + SETS_RW - 10, 104 - AF_X_CAP_Y, &AF_X, hi, T_MID, T_SURF);
    } else
        cv_text_in(SETS_RX, 60, SETS_RW, &AF_S, v, T_THEME, T_SURF);
}
static void hm_panel(void)
{
    hrow_t r[HM_MAXR];
    uint32_t n = hm_rows(r), cur = hm_cur(n), k;
    int32_t rh = n <= 8u ? 22 : 16, pitch = rh + (n <= 8u ? 1 : 2);
    for (k = 0; k < n; k++) {
        int32_t y = 5 + (int32_t)k * pitch;
        int on = k == cur;
        uint16_t bg = on ? T_SEL : T_SURF;
        if (on)
            cv_rrect(8, y, 112, rh, 4, T_SEL, T_SURF);
        cv_text_on(15, y + CAP_IN(S, rh), &AF_S, r[k].nm, on ? T_INK : T_TEXT, bg);
    }
    cv_rect(SETS_RX - 4, 8, 1, (int32_t)graph_h() - 16, T_LINE);
    if (n)
        hm_detail(&r[cur]);
}
static void hm_pagerow(void)
{
    const char *mid = ui.hm == HM_SCL ? "SCL" : ui.hm == HM_EDIT ? "EDIT" : HM_LISTS[hm_list & 3u];
    cv_begin(240, TF_PG_H, T_BG);
    cv_rrect(86, 1, PG_MID_W, TF_PG_H - 2, (TF_PG_H - 2) / 2, T_THEME, T_BG);
    cv_text_in(86, 1 + CAP_IN(B, TF_PG_H - 2), PG_MID_W, &AF_B, mid, T_INK, T_THEME);
    if (ui.hm == HM_GLO) {
        const char *l = HM_LISTS[(hm_list + 3u) & 3u], *rr = HM_LISTS[(hm_list + 1u) & 3u];
        cv_rrect(43, 4, PG_SIDE_W, 15, 7, T_RAISE, T_BG);
        cv_text_in(43, 4 + CAP_IN(X, 15), PG_SIDE_W, &AF_X, pg_short(l), T_TEXT, T_RAISE);
        cv_rrect(157, 4, PG_SIDE_W, 15, 7, T_RAISE, T_BG);
        cv_text_in(157, 4 + CAP_IN(X, 15), PG_SIDE_W, &AF_X, pg_short(rr), T_TEXT, T_RAISE);
    }
    cv_blit(0, TF_PG_Y);
}
static void hm_draw(void)
{
    hrow_t r[HM_MAXR];
    uint32_t n = hm_rows(r), cur = hm_cur(n), k, sig, pass, h0 = graph_h() / 2u;
    char v[24];
    sig = ui.hm * 131u + cur * 17u + hm_list * 7u + settings.palette * 8191u + ux.gen * 7u;
    for (k = 0; k < n; k++) {
        hm_valstr(&r[k], v);
        sig = sig * 31u + str_hash(7u, v) + (uint32_t)(uintptr_t)r[k].nm * 3u;
    }
    if (ui.force)
        draw_frame();
    draw_head();
    if (!ui.force && sig == hm_sig)
        return;
    hm_sig = sig;
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, pass ? graph_h() - h0 : h0, T_BG);
        cv_oy = pass ? -(int32_t)h0 : 0;
        cv_scroll = 1;
        cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);
        cv_bg = T_SURF;
        hm_panel();
        cv_scroll = 0;
        cv_oy = 0;
        cv_blit(0, graph_y() + pass * h0);
    }
    hm_pagerow();
}

static void hm_close(void)
{
    if (!ui.hm)
        return;
    ui.hm = 0;
    ui.force = 1;
    ui.graph_sig = 0;
}
static void hm_tick(void)
{
    if (ui.hm && (ui.menu || ui.uboot || name_on()))
        hm_close();
}
static uint32_t hm_btn_is(uint32_t b) { return ui.hm && b == hm_btn; }
/* a page button held past HOLD (ui_input.c): SCL, EDIT, GLO open their menu */
static void hm_hold(uint32_t b)
{
    uint32_t w = b == B_SCL ? HM_SCL : b == B_EDIT ? HM_EDIT : b == B_GLO ? HM_GLO : 0u;
    if (!w || ui.hm || ui.ly || !layer_allowed())
        return;
    ui.hm = (uint8_t)w;
    hm_btn = (uint8_t)b;
    hm_sig = 0;
    ui.force = 1;
    ui.pg_down = 0;
}
static uint32_t hm_glo_page(void)
{
    uint32_t k, s = 0;
    for (k = 0; k < NPAGES; k++)
        if (PAGES[k].graph == GR_SETS) {
            if (s == (hm_list & 3u))
                return k;
            s++;
        }
    return ui.page;
}
static void hm_knob(uint32_t k, int32_t s)
{
    hrow_t r[HM_MAXR];
    uint32_t n = hm_rows(r), cur = hm_cur(n);
    int16_t *vp;
    const param_desc_t *d;
    ui.hot_col = (uint8_t)k;
    ui.hot_t = 40;
    if (k == 0u) {
        uint8_t *c = ui.hm == HM_GLO ? &sets_cur[hm_list & 3u] : &hm_pos[ui.hm - 1u];
        *c = (uint8_t)wrap_add((int32_t)cur, s > 0 ? 1 : -1, (int32_t)n);
        ui.force = 1;
        return;
    }
    if (k == 2u && ui.hm == HM_GLO) {
        hm_list = (uint8_t)wrap_add((int32_t)hm_list, s > 0 ? 1 : -1, 4);
        ui.force = 1;
        return;
    }
    if (k != 1u || !n)
        return;
    switch (r[cur].kind) {
    case HR_GLB: {                                      /* the GLO page's own edit (TRACKS, LOAD, the MENU rows, the dialogs) */
        uint8_t sp = ui.page;
        ui.page = (uint8_t)hm_glo_page();
        edit_param(1, s);
        ui.page = sp;
        break;
    }
    case HR_INIT:
    case HR_RECALL:
        if (s > 0) {
            if (chain_busy())
                ui_message("STOP TO EDIT");
            else
                confirm_open(r[cur].kind == HR_INIT ? CF_INIT_SOUND : CF_RECALL_SOUND, song.sel);
        }
        break;
    default:
        d = hm_desc(&r[cur], &vp);
        if (!d || !vp || d->max == d->min)
            return;
        *vp = (int16_t)param_turn(d, *vp, accel(EN_K1 + 1u, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
        if (r[cur].kind == HR_TRK)
            motion_capture(TSEL, r[cur].id, *vp);
        break;
    }
    ui.force = 1;
}
