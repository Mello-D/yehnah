/* SPDX-License-Identifier: GPL-3.0-only */
/* SWARM's page (HOME's chain, after SENDS): the panel is the four boxes with a dot in each, the cards under it are the
 * row SELECT picked, K1..K4 the four dots (swarm.c has the idea and the numbers). Included after ui_input.c. */
static int swarm_page_on(void)
{
    return !ui.home && !page_over && !ui.layer && !ui.menu && GR_IS_SWARM(PAGES[ui.page].graph);
}
/* SELECT: a row a detent; past either end it goes on to the chain's next page, as FX ALL's rows do */
static void swarm_select(int32_t s)
{
    int32_t r = (int32_t)(sw.mode ? sw.trow : sw.row) + (s > 0 ? 1 : -1);
    if (r < 0 || r >= (int32_t)(sw.mode ? TR_ROWS : SWR_N)) {
        nav_select(s);
        return;
    }
    if (sw.mode)
        sw.trow = (uint8_t)r;
    else
        sw.row = (uint8_t)r;
    ui.force = 1;
}
/* the HOME layer's actions (ui_layer.c): can it be done now, is it on now, do it */
static int swarm_act_ok(uint32_t a)
{
    switch (a) {
    case SWA_FREEZE: return sw.on && !sw.rev;
    case SWA_REVERSE: return sw.mode ? 1 : sw.on && (sw.rev || sw.hn > 1u);   /* (TRACE: RANDOM, always) */
    case SWA_THROW: return sw.mode ? 1 : sw.on;                                /* (TRACE: REROLL the dot last turned) */
    case SWA_ONOFF: return 1;
    default: return sw.on;
    }
}
static int swarm_act_on(uint32_t a)
{
    return a == SWA_FREEZE ? sw.frozen : a == SWA_REVERSE ? (sw.mode ? 0 : sw.rev) : a == SWA_ONOFF ? sw.on : 0;
}
static void swarm_act(uint32_t a)
{
    uint32_t i;
    if (!sw.inited)
        swarm_defaults();
    if (!swarm_act_ok(a)) {
        if (!sw.on && a != SWA_ONOFF)
            ui_message(sw.mode ? "TRACE IS OFF" : "SWARM IS OFF");
        return;
    }
    if (sw.mode && (a == SWA_REVERSE || a == SWA_THROW)) {   /* TRACE: shuffle the shapes (all four, or the dot last turned) */
        if (a == SWA_REVERSE) {
            for (i = 0; i < SW_N; i++)
                tr_random(i);
            ui_message("TRACE: NEW SHAPES");
        } else {
            tr_random(sw.sel & 3u);
            ui_message(TR_NAME[sw.tshape[sw.sel & 3u]]);
        }
        ui.force = 1;
        return;
    }
    switch (a) {
    case SWA_FREEZE:
        sw.frozen = (uint8_t)!sw.frozen;
        ui_message(sw.frozen ? (sw.mode ? "TRACE FROZEN" : "SWARM FROZEN") : (sw.mode ? "TRACE MOVING" : "SWARM MOVING"));
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
        ui_message(sw.mode ? "SOUND KEPT: TRACE OFF" : "SOUND KEPT: SWARM OFF");
        break;
    default:
        if (sw.on) {
            swarm_off(1);
            ui_message("SOUND AS IT WAS");
        } else {
            swarm_start();
            ui_message(sw.mode ? "TRACE ON" : "SWARM ON");
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
    sw.sel = (uint8_t)slot;
    st = accel(EN_K1 + slot, steps, 100);
    switch (sw_rowid()) {
    case SWR_TRAVEL:                                    /* TRACE: along the shape */
        tr_travel(slot, st);
        break;
    case SWR_POS:
        d->pos = (uint8_t)sw_clamp((int32_t)d->pos + st, 0, 100);
        if (steps >= 2 || steps <= -2)                  /* a fast turn throws a THROW dot */
            swarm_throw(slot);
        break;
    case SWR_SHAPE:
        if (sw.mode) {                                  /* TRACE: the shapes in a ring */
            tr_shape_set(slot, (sw.tshape[slot] + (steps > 0 ? 1u : TR_N - 1u)) % TR_N);
            break;
        }
        d->shape = (uint8_t)sw_clamp((int32_t)d->shape + (steps > 0 ? 1 : -1), 0, (int32_t)SWS_N - 1);
        sw_seed(d);
        break;
    case SWR_SIZE:
        d->size = (uint8_t)sw_clamp((int32_t)d->size + st, 0, 100);
        break;
    case SWR_SPEED:
        d->speed = (uint8_t)sw_clamp((int32_t)d->speed + st, 5, 100);
        break;
    case SWR_ROT:                                       /* TRACE: a turn is 100 (it wraps) */
        sw.trot[slot] = (uint8_t)(((int32_t)sw.trot[slot] + st % 100 + 100) % 100);
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
    switch (sw_rowid()) {
    case SWR_TRAVEL: fmt_int(val, (int32_t)(sw.tph[i] * 100.0f / (float)tr_laps(i)) % 100); str_cpy(val + str_len(val), "%", 2); return val;
    case SWR_ROT: fmt_int(val, (int32_t)sw.trot[i]); str_cpy(val + str_len(val), "%", 2); return val;
    case SWR_POS: fmt_int(val, (int32_t)d->pos); str_cpy(val + str_len(val), "%", 2); return val;
    case SWR_SHAPE: return sw.mode ? TR_NAME[sw.tshape[i]] : SW_SHAPE[d->shape];
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
        const char *u = "", *l = lab, *v = val, *dm;
        int32_t g = -1;
        lab[0] = 'D'; lab[1] = 'O'; lab[2] = 'T'; lab[3] = ' '; lab[4] = (char)('1' + i); lab[5] = 0;
        switch (sw_rowid()) {
        case SWR_TRAVEL: v = sw_row_text(i, val, &dm); break;
        case SWR_ROT: v = sw_row_text(i, val, &dm); g = (int32_t)sw.trot[i] * 10; break;
        case SWR_POS: fmt_int(val, (int32_t)d->pos); u = "%"; g = (int32_t)d->pos * 10; break;
        case SWR_SHAPE: v = sw.mode ? TR_NAME[sw.tshape[i]] : SW_SHAPE[d->shape]; g = sw.mode ? 0 : pick_of(SWS_N, d->shape); break;
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
/* a dot's icon says its movement: ORBIT a ring, DRIFT a dot with wavy ticks behind it, CHAOS a bow-tie (the Lorenz butterfly),
 * THROW a puck with a tick along its velocity */
static void sw_icon(int32_t px, int32_t py, const sw_dot_t *d, uint16_t c)
{
    int32_t r;
    float v;
    switch (d->shape) {
    case SWS_ORBIT:
        og_node(px << 4, py << 4, 6, c);
        og_node(px << 4, py << 4, 3, T_SURF);
        og_node(px << 4, py << 4, 1, c);
        break;
    case SWS_DRIFT:
        og_node(px << 4, py << 4, 4, c);
        cv_line(px - 7, py + 3, px - 9, py - 1, ux_mix(T_SURF, c, 170));
        cv_line(px - 9, py - 1, px - 11, py + 3, ux_mix(T_SURF, c, 120));
        cv_line(px - 11, py + 3, px - 13, py - 1, ux_mix(T_SURF, c, 80));
        break;
    case SWS_CHAOS:
        cv_line(px - 6, py - 5, px + 6, py + 5, c);
        cv_line(px + 6, py + 5, px + 6, py - 5, c);
        cv_line(px + 6, py - 5, px - 6, py + 5, c);
        cv_line(px - 6, py + 5, px - 6, py - 5, c);
        og_node(px << 4, py << 4, 2, c);
        break;
    default:
        og_node(px << 4, py << 4, 4, c);
        v = d->vx * d->vx + d->vy * d->vy;
        if (v > 0.0004f) {
            v = fm_rsqrtf(v);
            r = (int32_t)(d->vx * v * 9.0f);
            cv_line(px, py, px + r, py + (int32_t)(d->vy * v * 9.0f), c);
        }
        break;
    }
}
/* the panel: 240 x 138, the four boxes, a dot in each with its trail, the parameters named in the corners */
static void swarm_graph(void)
{
    const int32_t X0 = 4, Y0 = 3, W = 232, H = SWARM_FULL ? (int32_t)graph_h() - 21 : 112;   /* (the full layout: the boxes take what the foot strip leaves) */
    const track_t *t = &trk[sw.on ? sw.trk : song.sel];
    uint32_t i, a, k;
    char b[24];
    if (!sw.inited)
        swarm_defaults();
    if (sw.mode)                                        /* TRACE: each dot's shape, faint, where it walks (the sky's stars brighter) */
        for (i = 0; i < SW_N; i++) {
            uint32_t sh = sw.tshape[i], n = TR_OFF[sh + 1u] - TR_OFF[sh], mob = tr_laps(i) > 1u, m = mob ? 48u : n;
            uint16_t lc = ux_mix(T_SURF, T_K(i), 80), sc = ux_mix(T_SURF, T_K(i), 200);
            float cx, cy, s = (float)sw.d[i].size * 0.0025f, bx0 = (float)(i & 1u) * 0.5f, by0 = (float)(i >> 1) * 0.5f, ca = fm_cosf((float)sw.trot[i] * 0.0628319f), sa = fm_sinf((float)sw.trot[i] * 0.0628319f);
            int32_t qx = 0, qy = 0, fx = 0, fy = 0;
            sw_centre(&sw.d[i], i, &cx, &cy);
            for (k = 0; k <= m; k++) {
                float ox, oy;
                int32_t px, py;
                if (k == m && !mob && n > 0u) {         /* (closing line) */
                    px = fx;
                    py = fy;
                } else {
                    uint32_t kk = k < m ? k : 0u;
                    if (mob)
                        tr_eval(i, (float)kk * (2.0f / 48.0f), &ox, &oy);
                    else {
                        float rx = (float)TR_PTS[(TR_OFF[sh] + kk) * 2u] * (1.0f / 127.0f), ry = (float)TR_PTS[(TR_OFF[sh] + kk) * 2u + 1u] * (1.0f / 127.0f);
                        ox = rx * ca - ry * sa;
                        oy = rx * sa + ry * ca;
                    }
                    px = X0 + (int32_t)(fm_clampf(cx + ox * s, bx0, bx0 + 0.5f) * (float)W);
                    py = Y0 + (int32_t)(fm_clampf(cy + oy * s, by0, by0 + 0.5f) * (float)H);
                    if (k == 0u)
                        fx = px, fy = py;
                    if ((TR_FLAG[sh] & TRF_SKY) && k < m)
                        cv_rect(px - 1, py - 1, 3, 3, sc);
                }
                if (k)
                    cv_line(qx, qy, px, py, lc);
                qx = px;
                qy = py;
            }
        }
    for (i = 0; i < SW_N; i++) {                        /* each dot's two parameters and the row's value, in its own screen corner, in its KNOB's colour */
        int32_t right = (int32_t)(i & 1u), bot = (int32_t)(i >> 1), ty = bot ? Y0 + H - 33 : Y0 + 2;
        uint16_t kc = T_K(i);
        const char *lab, *v;
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
            if (right)
                cv_text_r(X0 + W - 4, ty + (int32_t)a * 9, &AF_X, b, kc, T_SURF);
            else
                cv_text(X0 + 4, ty + (int32_t)a * 9, &AF_X, b, kc);
        }
        v = sw_row_text(i, b, &lab);                    /* (the row's value: white while its knob is being turned) */
        if (right)
            cv_text_r(X0 + W - 4, ty + 19, &AF_S, v, (sw.mode ? sw.sel == i : ui.hot_t && ui.hot_col == i) ? T_TEXT : ux_mix(T_SURF, kc, 190), T_SURF);
        else
            cv_text(X0 + 4, ty + 19, &AF_S, v, (sw.mode ? sw.sel == i : ui.hot_t && ui.hot_col == i) ? T_TEXT : ux_mix(T_SURF, kc, 190));
    }
    for (i = 0; i < SW_N; i++) {                        /* each dot: its trail, its centre, itself */
        const sw_dot_t *d = &sw.d[i];
        uint16_t c = T_K(i);
        int32_t px, py, qx = 0, qy = 0;
        float cx, cy, hx, hy;
        sw_centre(d, i, &cx, &cy);
        if (!sw.mode)                                   /* (TRACE: the shape shows where the centre is) */
            cv_rect(X0 + (int32_t)(cx * (float)W) - 1, Y0 + (int32_t)(cy * (float)H) - 1, 3, 3, ux_mix(T_BG, c, 120));
        for (k = 0; k < 16u && sw.on && !sw.mode; k++) {
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
        sw_icon(px, py, d, sw.on ? c : ux_mix(T_BG, c, 90));
    }
    {   /* the row's name centred in a header chip, as the rows pages' headers; what the dots are doing at its right end */
        int32_t cy0 = (int32_t)graph_h() - 18;
        const char *nm = sw_rowname();
        cv_rrect(5, cy0, 224, 16, 4, T_ACCENT, T_SURF);
        cv_text_on(117 - HALF_UP(text_w(&AF_B, nm)), cy0 + CAP_IN(B, 16), &AF_B, nm, T_BG, T_ACCENT);
        if (!sw.on)
            str_cpy(b, "OFF", 8);
        else if (sw.rev) {
            str_cpy(b, "REV ", 10);
            fmt_int(b + 4, (int32_t)(sw.age * 0.1f + 0.5f));
            str_cpy(b + str_len(b), " S", 3);
        } else if (sw.frozen)
            str_cpy(b, "FROZEN", 8);
        else
            b[0] = 0;
        if (b[0])
            cv_text_r(5 + 224 - 6, cy0 + 4, &AF_X, b, T_BG, T_ACCENT);
    }
}
