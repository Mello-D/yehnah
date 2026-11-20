/* SPDX-License-Identifier: GPL-3.0-only */
/* (2026-11-09, held menus) EDIT held past HOLD opens a list menu (the GLO lists' look: names left, the row's
 * choices or its range right) that STAYS open after the button is let go. Left with OCT- or a tap of the button (any other
 * button's tap closes it and acts). The piano keys play as normal: no key shortcuts here.
 *   (SCL: its STRUM is ARP LIST's row now (2026-11-12); SCL held does nothing)
 *   EDIT  the sound's named parameters (E1..E8)
 *   (2026-11-17: GLO's held menu is gone: the GLO page has the same four lists; GLO held does nothing, as the other buttons)
 * KNOB 1 the row, KNOB 2 its value (an action: a turn right asks). A tap of the button is still its page (ui_input.c).
 * Included after ui_menu.c (the list's drawing) and ui_input.c (edit_param). */
enum { HM_NONE, HM_SCL /* retired */, HM_EDIT };
enum { HR_TRK, HR_ALLTR };
typedef struct {
    const char *nm;
    uint16_t id;                       /* HR_TRK: the track parameter */
    uint8_t kind;
} hrow_t;
#define HM_MAXR 10u
static uint8_t hm_btn;                 /* the button that opened it */
static uint8_t hm_pos[3];               /* the row KNOB 1 is on (SCL, EDIT) */
static uint32_t hm_sig;

static uint32_t hm_rows(hrow_t *r)
{
    uint32_t n = 0, k;
    if (ui.hm == HM_EDIT) {
        for (k = 0; k < 8u; k++) {
            const param_desc_t *d = track_desc(TSEL, P_E0 + k);
            if (d && d->label && d->label[0] != '-' && d->max > d->min) {
                r[n].nm = d->label;
                r[n].id = (uint16_t)(P_E0 + k);
                r[n++].kind = HR_TRK;
            }
        }
    }
    return n;
}
static uint32_t hm_cur(uint32_t n)
{
    uint8_t *c = &hm_pos[ui.hm - 1u];
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
    if ((d = hm_desc(r, &vp)) != 0 && vp)
        param_format(d, *vp, v, &unit);
}
static void hm_detail(const hrow_t *r)
{
    int16_t *vp;
    const param_desc_t *d;
    char v[24], lo[16], hi[16];
    const char *unit = "";
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
    const char *mid = "EDIT";
    cv_begin(240, TF_PG_H, T_BG);
    cv_rrect(86, 1, PG_MID_W, TF_PG_H - 2, (TF_PG_H - 2) / 2, T_THEME, T_BG);
    cv_text_in(86, 1 + CAP_IN(B, TF_PG_H - 2), PG_MID_W, &AF_B, mid, T_INK, T_THEME);
    cv_blit(0, TF_PG_Y);
}
static void hm_draw(void)
{
    hrow_t r[HM_MAXR];
    uint32_t n = hm_rows(r), cur = hm_cur(n), k, sig, pass, h0 = graph_h() / 2u;
    char v[24];
    sig = ui.hm * 131u + cur * 17u + settings.palette * 8191u + ux.gen * 7u;
    for (k = 0; k < n; k++) {
        hm_valstr(&r[k], v);
        sig = sig * 31u + str_hash(7u, v) + (uint32_t)(uintptr_t)r[k].nm * 3u;
    }
    if (ui.force)                                       /* (not draw_frame: it clears rows inside this panel: a black bar flickered) */
        lcd_clear(0, graph_y() + graph_h(), 240, TF_PG_Y - graph_y() - graph_h(), T_BG);
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
    cpu_sample();
    if (ui.hm && (ui.menu || ui.uboot || name_on()))
        hm_close();
}
static uint32_t hm_btn_is(uint32_t b) { return ui.hm && b == hm_btn; }
/* a page button held past HOLD (ui_input.c): EDIT opens its menu */
static void hm_hold(uint32_t b)
{
    uint32_t w = b == B_EDIT ? HM_EDIT : 0u;
    if (!w || ui.hm || ui.ly || !layer_allowed())
        return;
    ui.hm = (uint8_t)w;
    hm_btn = (uint8_t)b;
    hm_sig = 0;
    ui.force = 1;
    ui.pg_down = 0;
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
        uint8_t *c = &hm_pos[ui.hm - 1u];
        *c = (uint8_t)wrap_add((int32_t)cur, s > 0 ? 1 : -1, (int32_t)n);
        ui.force = 1;
        return;
    }
    if (k != 1u || !n)
        return;
    switch (r[cur].kind) {
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
