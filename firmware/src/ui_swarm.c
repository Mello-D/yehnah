/* SPDX-License-Identifier: GPL-3.0-only */
/* SWARM's page (HOME's chain, after SENDS): the panel is the four boxes with a dot in each, the cards under it are the
 * row SELECT picked, K1..K4 the four dots (swarm.c has the idea and the numbers). Included after ui_input.c. */
static int swarm_page_on(void)
{
    return !ui.home && !page_over && !ui.layer && !ui.menu && PAGES[ui.page].graph == GR_SWARM;
}
/* SELECT: a row a detent; past either end it goes on to the chain's next page, as FX ALL's rows do */
static void swarm_select(int32_t s)
{
    int32_t r = (int32_t)sw.row + (s > 0 ? 1 : -1);
    if (r < 0 || r >= (int32_t)SWR_N) {
        nav_select(s);
        return;
    }
    sw.row = (uint8_t)r;
    ui.force = 1;
}
/* the HOME layer's actions (ui_layer.c): can it be done now, is it on now, do it */
static int swarm_act_ok(uint32_t a)
{
    switch (a) {
    case SWA_FREEZE: return sw.on && !sw.rev;
    case SWA_REVERSE: return sw.on && (sw.rev || sw.hn > 1u);
    case SWA_ONOFF: return 1;
    default: return sw.on;
    }
}
static int swarm_act_on(uint32_t a)
{
    return a == SWA_FREEZE ? sw.frozen : a == SWA_REVERSE ? sw.rev : a == SWA_ONOFF ? sw.on : 0;
}
static void swarm_act(uint32_t a)
{
    uint32_t i;
    if (!sw.inited)
        swarm_defaults();
    if (!swarm_act_ok(a)) {
        if (!sw.on && a != SWA_ONOFF)
            ui_message("SWARM IS OFF");
        return;
    }
    switch (a) {
    case SWA_FREEZE:
        sw.frozen = (uint8_t)!sw.frozen;
        ui_message(sw.frozen ? "SWARM FROZEN" : "SWARM MOVING");
        break;
    case SWA_REVERSE:
        if (sw.rev) {
            swarm_rev_stop();
            ui_message("SWARM FORWARD");
        } else {
            sw.rev = 1;
            sw.dir = 1;
            sw.age = 0.0f;
            sw.frozen = 0;
            ui_message("SWARM REVERSE");
        }
        break;
    case SWA_THROW:
        for (i = 0; i < SW_N; i++)
            swarm_throw(i);
        break;
    case SWA_CAPTURE:
        swarm_capture();
        ui_message("SOUND KEPT: SWARM OFF");
        break;
    default:
        if (sw.on) {
            swarm_off(1);
            ui_message("SOUND AS IT WAS");
        } else {
            swarm_start();
            ui_message("SWARM ON");
        }
        break;
    }
    ui.force = 1;
}
static int32_t sw_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static void swarm_knob(uint32_t slot, int32_t steps)
{
    sw_dot_t *d;
    int32_t st;
    if (!sw.inited)
        swarm_defaults();
    swarm_follow();
    slot &= 3u;
    d = &sw.d[slot];
    st = accel(EN_K1 + slot, steps, 100);
    switch (sw.row) {
    case SWR_POS:
        d->pos = (uint8_t)sw_clamp((int32_t)d->pos + st, 0, 100);
        if (steps >= 2 || steps <= -2)                  /* a fast turn throws a THROW dot */
            swarm_throw(slot);
        break;
    case SWR_SHAPE:
        d->shape = (uint8_t)sw_clamp((int32_t)d->shape + (steps > 0 ? 1 : -1), 0, (int32_t)SWS_N - 1);
        sw_seed(d);
        break;
    case SWR_SIZE:
        d->size = (uint8_t)sw_clamp((int32_t)d->size + st, 0, 100);
        break;
    case SWR_SPEED:
        d->speed = (uint8_t)sw_clamp((int32_t)d->speed + st, 5, 100);
        break;
    case SWR_GAIN:
        d->gain = (uint8_t)sw_clamp((int32_t)d->gain + st, 0, 100);
        break;
    }
    ui.force = 1;
}
/* the row's value for dot i, as text (the box in the full layout, the card otherwise); the label is the card's */
static const char *sw_row_text(uint32_t i, char *val, const char **lab)
{
    const sw_dot_t *d = &sw.d[i];
    *lab = "";
    switch (sw.row) {
    case SWR_POS: fmt_int(val, (int32_t)d->pos); str_cpy(val + str_len(val), "%", 2); return val;
    case SWR_SHAPE: return SW_SHAPE[d->shape];
    case SWR_SIZE: fmt_int(val, (int32_t)d->size); str_cpy(val + str_len(val), "%", 2); return val;
    case SWR_SPEED: fmt_int(val, (int32_t)d->speed); str_cpy(val + str_len(val), "%", 2); return val;
    case SWR_GAIN: fmt_int(val, (int32_t)d->gain); str_cpy(val + str_len(val), "%", 2); return val;
    default: return "";
    }
}
static void swarm_cards(void)
{
    uint32_t i;
    char val[12], lab[8];
    for (i = 0; i < SW_N; i++) {
        const sw_dot_t *d = &sw.d[i];
        const char *u = "", *l = lab, *v = val;
        int32_t g = -1;
        lab[0] = 'D'; lab[1] = 'O'; lab[2] = 'T'; lab[3] = ' '; lab[4] = (char)('1' + i); lab[5] = 0;
        switch (sw.row) {
        case SWR_POS: fmt_int(val, (int32_t)d->pos); u = "%"; g = (int32_t)d->pos * 10; break;
        case SWR_SHAPE: v = SW_SHAPE[d->shape]; g = pick_of(SWS_N, d->shape); break;
        case SWR_SIZE: fmt_int(val, (int32_t)d->size); u = "%"; g = (int32_t)d->size * 10; break;
        case SWR_SPEED: fmt_int(val, (int32_t)d->speed); u = "%"; g = (int32_t)d->speed * 10; break;
        case SWR_GAIN: fmt_int(val, (int32_t)d->gain); u = "%"; g = (int32_t)d->gain * 10; break;
        }
        draw_column(i, l, v, u, VAL(i), g, ICON_NONE);
    }
}
/* the full layout's panel: down to the page row (graph_h() rows, more than the canvas holds): two passes, as FX ALL's */
static void swarm_graph(void);
static void swarm_draw(void)
{
    static uint32_t last_ms;
    uint32_t pass, h0 = graph_h() / 2u;
    if (!ui.force && sys_load && fm1_ms - last_ms < (sys_load == LOAD_ECO ? 70u : 50u))
        return;
    last_ms = fm1_ms;
    ui.graph_sig = 0;
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, pass ? graph_h() - h0 : h0, T_BG);
        cv_oy = pass ? -(int32_t)h0 : 0;
        cv_scroll = 1;
        cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);
        cv_bg = T_SURF;
        swarm_graph();
        cv_scroll = 0;
        cv_oy = 0;
        cv_blit(0, graph_y() + pass * h0);
    }
}
/* the panel: 240 x 138, the four boxes, a dot in each with its trail, the parameters named in the corners */
static void swarm_graph(void)
{
    const int32_t X0 = 4, Y0 = 3, W = 232, H = SWARM_FULL ? (int32_t)graph_h() - 17 : 112;   /* (the full layout: the boxes take what the foot strip leaves) */
    const track_t *t = &trk[sw.on ? sw.trk : song.sel];
    uint32_t i, a, k;
    char b[24];
    if (!sw.inited)
        swarm_defaults();
    for (i = 0; i < SW_N; i++) {                        /* the boxes */
        int32_t bx = X0 + (int32_t)(i & 1u) * (W / 2), by = Y0 + (int32_t)(i >> 1) * (H / 2);
        cv_rect(bx + 1, by + 1, W / 2 - 2, H / 2 - 2, ux_mix(T_BG, sec_col(i), 8));
        cv_frame(bx, by, W / 2, H / 2, ux_mix(T_BG, sec_col(i), 110));
    }
    for (i = 0; i < SW_N; i++) {                        /* the parameters each dot drives (X, then Y) with their values, in its box's corner */
        int32_t bx = X0 + (int32_t)(i & 1u) * (W / 2), by = Y0 + (int32_t)(i >> 1) * (H / 2);
        for (a = 0; a < 2u; a++) {
            uint32_t id = sw_pid(t, i, a);
            const param_desc_t *pd;
            int32_t pct;
            if (id == 0xFFFFu)
                continue;
            pd = sw_desc(t, id);
            pct = pd->max > pd->min ? ((int32_t)t->p[id] - pd->min) * 100 / (pd->max - pd->min) : 0;
            b[0] = a ? 'Y' : 'X';
            b[1] = ' ';
            b[2] = 0;
            str_cpy(b + 2, id == P_PHAMT ? "PHASER" : id == P_FCUT ? "FILTER" : id == P_FRES ? "FRES" : pd->label, 8);
            k = str_len(b);
            b[k] = ' ';
            fmt_int(b + k + 1u, pct);
            cv_text(bx + 4, by + 3 + (int32_t)a * 9, &AF_X, b, T_TEXT);
        }
    }
    if (SWARM_FULL)                                     /* the row's value in each box (there are no knob cells) */
        for (i = 0; i < SW_N; i++) {
            int32_t bx = X0 + (int32_t)(i & 1u) * (W / 2), by = Y0 + (int32_t)(i >> 1) * (H / 2);
            const char *lab, *v = sw_row_text(i, b, &lab);
            if (lab[0]) {
                cv_text_r(bx + W / 2 - 4, by + H / 2 - 24, &AF_X, lab, T_MID, T_BG);
            }
            cv_text_r(bx + W / 2 - 4, by + H / 2 - 15, &AF_S, v, T_TEXT, T_BG);
            if (ui.hot_t && ui.hot_col == i)
                cv_frame(bx, by, W / 2, H / 2, T_TEXT);
        }
    for (i = 0; i < SW_N; i++) {                        /* each dot: its trail, its centre, itself */
        const sw_dot_t *d = &sw.d[i];
        uint16_t c = sec_col(i);
        int32_t px, py, qx = 0, qy = 0;
        float cx, cy, hx, hy;
        sw_centre(d, i, &cx, &cy);
        cv_rect(X0 + (int32_t)(cx * (float)W) - 1, Y0 + (int32_t)(cy * (float)H) - 1, 3, 3, ux_mix(T_BG, c, 120));
        for (k = 0; k < 16u && sw.on; k++) {
            sw_hist(i, sw.rev ? sw.age + (float)k * 0.5f : (float)k * 0.5f, &hx, &hy);
            px = X0 + (int32_t)(hx * (float)W);
            py = Y0 + (int32_t)(hy * (float)H);
            if (k && (px - qx < 60 && qx - px < 60 && py - qy < 40 && qy - py < 40))
                cv_line(qx, qy, px, py, ux_mix(T_BG, c, 200 - (int32_t)k * 11));
            qx = px;
            qy = py;
        }
        px = X0 + (int32_t)(d->x * (float)W);
        py = Y0 + (int32_t)(d->y * (float)H);
        og_node(px << 4, py << 4, 5, sw.on ? c : ux_mix(T_BG, c, 90));   /* (no digit in it: a dot over a name is not a clash) */
    }
    str_cpy(b, SW_ROW[sw.row], 12);                     /* the row, and what it is doing */
    cv_text(X0, Y0 + H + 4, &AF_X, b, T_THEME);
    if (!sw.on)
        str_cpy(b, "OFF", 8);
    else if (sw.rev) {
        str_cpy(b, "REVERSE ", 10);
        fmt_int(b + 8, (int32_t)(sw.age * 0.1f + 0.5f));
        str_cpy(b + str_len(b), " S", 3);
    } else if (sw.frozen)
        str_cpy(b, "FROZEN", 8);
    else if (SWARM_FULL)
        b[0] = 0;                                       /* (running: nothing to say; the page row names the page) */
    else
        str_cpy(b, "SWARM", 8);
    if (b[0])
        cv_text_r(X0 + W, Y0 + H + 4, &AF_X, b, sw.on ? T_TEXT : T_DIM, T_BG);
}
