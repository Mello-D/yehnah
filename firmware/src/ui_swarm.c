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
    if (sw.mode == 2u) {                                /* XY has no rows */
        nav_select(s);
        return;
    }
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
    if (sw.mode == 2u)                                  /* XY: Z = REV DLY CHORUS LEVEL (F3 .. B3), XY on / off (C4), CAPTURE, RE-FIT */
        return a < XY_NZ || a == SWA_ONOFF ? 1 : a <= SWA_X6 ? xy.on : 0;
    if (a >= SWA_X5)
        return 0;
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
    if (sw.mode == 2u)
        return a < XY_NZ ? xy.zsel == a : a == SWA_ONOFF && xy.on;
    return a == SWA_FREEZE ? sw.frozen : a == SWA_REVERSE ? (sw.mode ? 0 : sw.rev) : a == SWA_ONOFF ? sw.on : 0;
}
static void swarm_act(uint32_t a)
{
    uint32_t i;
    if (!sw.inited)
        swarm_defaults();
    if (!swarm_act_ok(a)) {
        if (sw.mode == 2u && a >= SWA_X5 && !xy.on)
            ui_message("XYZ IS OFF");
        else if (sw.mode != 2u && !sw.on && a != SWA_ONOFF && a < SWA_X5)
            ui_message(sw.mode ? "TRACE IS OFF" : "SWARM IS OFF");
        return;
    }
    if (sw.mode == 2u) {
        if (a < XY_NZ) {                                /* Z: the parameter knob 4 turns */
            xy.zsel = (uint8_t)a;
            ui_message(XY_ZNAME[a]);
            ui.force = 1;
            return;
        }
        switch (a) {
        case SWA_X6:                                    /* RE-FIT: the dot to the nearest place to the sound as it is now */
            xy_fit(&trk[xy.trk]);
            xy_anchor(&trk[xy.trk]);
            ui_message("XYZ: DOT RE-FITTED");
            break;
        case SWA_X5:
            xy_off(0);
            ui_message("SOUND KEPT: XYZ OFF");
            break;
        default:
            if (xy.on) {
                xy_off(1);
                xy.manual_off = 1;
                ui_message("SOUND AS IT WAS");
            } else {
                xy_start();
                ui_message("XYZ ON");
            }
            break;
        }
        ui.force = 1;
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
static void xy_knob(uint32_t slot, int32_t steps);
static void xy_cards(void);
static void xy_graph(void);
static void swarm_knob(uint32_t slot, int32_t steps)
{
    sw_dot_t *d;
    int32_t st;
    if (!sw.inited)
        swarm_defaults();
    if (sw.mode == 2u) {
        xy_knob(slot, steps);
        return;
    }
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
    if (sw.mode == 2u) {
        xy_cards();
        return;
    }
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
    if (sw.mode == 2u) {
        xy_graph();
        return;
    }
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

/* ---- XY (swarm.c has the idea): knobs 1 / 2 the dot, 3 the lines' rotation, 4 the Z parameter ---- */
static const char *xy_lname(uint32_t i)                 /* a line's name (the engine's own: its parameter's label) */
{
    const track_t *t = &trk[xy.on ? xy.trk : song.sel];
    return XY_LNAME[i][0] ? XY_LNAME[i] : sw_desc(t, xy.id[i])->label;
}
static int32_t xy_pct(uint32_t i)                       /* a line's real value now, 0..100 */
{
    const track_t *t = &trk[xy.on ? xy.trk : song.sel];
    return (int32_t)(xy_norm(t, xy.id[i]) * 100.0f + 0.5f);
}
static void xy_knob(uint32_t slot, int32_t steps)
{
    track_t *t;
    int32_t st = accel(EN_K1 + (slot & 3u), steps, 100);
    if (!xy.inited || !xy.on) {
        if (!xy.manual_off)
            xy_start();
        else
            return;
    }
    xy_follow();
    t = &trk[xy.trk];
    switch (slot & 3u) {
    case 0: xy.x = fm_clampf(xy.x + (float)st * 0.03f, -1.0f, 1.0f); break;
    case 1: xy.y = fm_clampf(xy.y + (float)st * 0.03f, -1.0f, 1.0f); break;
    case 2: xy.rot = (uint8_t)(((int32_t)xy.rot + st % 100 + 100) % 100); break;
    default: {                                          /* Z: the parameter itself */
        const param_desc_t *pd = sw_desc(t, XY_ZID[xy.zsel]);
        int32_t v = (int32_t)t->p[XY_ZID[xy.zsel]] + st * 2;   /* (twice the knob's step: Z is quick) */
        v = v < pd->min ? pd->min : v > pd->max ? pd->max : v;
        t->p[XY_ZID[xy.zsel]] = xy.put[XY_NL + xy.zsel] = (int16_t)v;
        break;
    }
    }
    ui.force = 1;
}
static void xy_cards(void)
{
    uint32_t i;
    char val[12];
    const track_t *t = &trk[xy.on ? xy.trk : song.sel];
    for (i = 0; i < 4u; i++) {
        const char *lab = i == 0u ? "DOT X" : i == 1u ? "DOT Y" : i == 2u ? "ROTATE" : XY_ZNAME[xy.zsel];
        int32_t g, v;
        if (i == 0u)
            v = (int32_t)(xy.x * 50.0f + 50.5f);
        else if (i == 1u)
            v = (int32_t)(xy.y * 50.0f + 50.5f);
        else if (i == 2u)
            v = (int32_t)xy.rot;
        else
            v = (int32_t)(xy_norm(t, XY_ZID[xy.zsel]) * 100.0f + 0.5f);
        g = v * 10;
        fmt_int(val, v);
        draw_column(i, lab, val, "%", VAL(i), g, ICON_NONE);
    }
}
/* the panel: the pad is a rectangle to the panel's edges (the fourteen ends: seven lines). Each line's 100 % end is a bright dot on the pad's edge (0 %: a faint one
 * opposite) with its name and value just inside it, so rotating carries the texts round the edge. The dot is a ring that shrinks and
 * fades as Z rises (further away), over a faint tunnel (frames shrinking towards the middle). The four knobs' values run along the foot. */
static void xy_text_at(int32_t mx, int32_t my, int32_t edge, const char *b, uint16_t c, int32_t X0, int32_t W)
{
    int32_t w = text_w(&AF_X, b), tx, ty;
    if (edge == 1) { tx = mx - 8 - w; ty = my - 4; }            /* right edge: the text inside, to the left */
    else if (edge == 3) { tx = mx + 8; ty = my - 4; }           /* left edge */
    else if (edge == 0) { tx = mx - w / 2; ty = my + 7; }       /* top edge: below the dot */
    else { tx = mx - w / 2; ty = my - 15; }                     /* bottom edge: above it */
    tx = tx < X0 + 2 ? X0 + 2 : tx > X0 + W - 2 - w ? X0 + W - 2 - w : tx;
    cv_text(tx, ty, &AF_X, b, c);
}
static void xy_graph(void)
{
    const int32_t X0 = 4, Y0 = 3, W = 232, H = (int32_t)graph_h() - 20, M = 7;   /* (the pad, and the margin that keeps an edge dot whole) */
    const track_t *t = &trk[xy.on ? xy.trk : song.sel];
    int32_t px, py, cx = X0 + W / 2, cy = Y0 + H / 2, rx = W / 2 - M, ry = H / 2 - M, rr;
    uint32_t i, k;
    float z = xy_norm(t, XY_ZID[xy.zsel]);
    char b[40];
    uint16_t dc = xy.on ? T_K(0) : ux_mix(T_BG, T_K(0), 90);
    /* the depth: a tunnel of SIX frames at fixed depths (1.15 .. 6.6) running down to the middle of the pad, seen from a camera that Z
     * flies forward: a frame is drawn at 1 / (its depth - the camera), so turning Z up grows each frame towards the edge and it is gone
     * once it reaches it, the outermost first. At Z 100 % the last one has passed: no squares are left (and a Z of 0 shows all six
     * again). Four rails run from the corners to the middle (the tunnel's walls) */
    {
        static const float XD[6] = {1.15f, 1.6f, 2.3f, 3.3f, 4.7f, 6.7f};
        float cam = z * (XD[5] - 1.0f);                  /* (at Z 100 % the last frame has reached the edge and stays: the floor) */
        int32_t fxl = (int32_t)((float)rx * (1.0f / XD[5])), fyl = (int32_t)((float)ry * (1.0f / XD[5]));
        uint16_t rc = ux_mix(T_SURF, T_TEXT, 16);
        cv_line(cx - rx, cy - ry, cx - fxl, cy - fyl, rc);
        cv_line(cx + rx, cy - ry, cx + fxl, cy - fyl, rc);
        cv_line(cx + rx, cy + ry, cx + fxl, cy + fyl, rc);
        cv_line(cx - rx, cy + ry, cx - fxl, cy + fyl, rc);
        for (k = 0; k < 6u; k++) {
            float d = XD[k] - cam, sc;
            int32_t fx, fy;
            uint16_t fc;
            if (d < 0.999f)                             /* (it has passed the camera) */
                continue;
            sc = 1.0f / d;
            fx = (int32_t)((float)rx * sc);
            fy = (int32_t)((float)ry * sc);
            fc = ux_mix(T_SURF, T_TEXT, 10 + (int32_t)(sc * 40.0f));
            cv_line(cx - fx, cy - fy, cx + fx, cy - fy, fc);
            cv_line(cx + fx, cy - fy, cx + fx, cy + fy, fc);
            cv_line(cx + fx, cy + fy, cx - fx, cy + fy, fc);
            cv_line(cx - fx, cy + fy, cx - fx, cy - fy, fc);
        }
    }
    {   /* the floor: the cross in the middle grows as you go down to it, and Z's name is written on it, faint far away, clear at the bottom */
        int32_t arm = 4 + (int32_t)(z * 8.0f + 0.5f);
        const char *zn = XY_ZNAME[xy.zsel];
        cv_line(cx - arm, cy, cx + arm, cy, ux_mix(T_SURF, T_TEXT, 60));
        cv_line(cx, cy - arm, cx, cy + arm, ux_mix(T_SURF, T_TEXT, 60));
        if (z > 0.15f)
            cv_text_on(cx - HALF_UP(text_w(&AF_B, zn)), cy + arm + 4, &AF_B, zn, ux_mix(T_SURF, T_TEXT, (int32_t)(z * z * 85.0f)), T_SURF);
    }
    for (i = 0; i < XY_NL; i++) {                       /* each line's two ends on the pad's edge */
        float c, sn, kk, m;
        int32_t ex, ey, edge;
        uint16_t col = i < 4u ? T_K(i) : i == 4u ? ux_mix(T_K(1), T_K(2), 50) : i == 5u ? ux_mix(T_K(3), T_K(0), 50) : ux_mix(T_K(0), T_K(2), 50);   /* (the lane colours' blends: no two lines alike) */
        const char *nm = xy_lname(i);
        xy_dir(i, &c, &sn, &kk);
        m = kk > 0.0f ? 1.0f / kk : 1.0f;               /* (the ray from the centre meets the edge at (c, s) / max) */
        ex = cx + (int32_t)(c * m * (float)rx);
        ey = cy - (int32_t)(sn * m * (float)ry);
        edge = fm_fabsf(c) >= fm_fabsf(sn) ? (c > 0.0f ? 1 : 3) : (sn > 0.0f ? 0 : 2);
        og_node((cx - (ex - cx)) << 4, (cy - (ey - cy)) << 4, 4, ux_mix(T_SURF, col, 35));        /* 0 %: faint */
        og_node(ex << 4, ey << 4, 5, col);                                                    /* 100 %: bright, its text inside it */
        k = 0;
        while (nm[k] && k < 8u) { b[k] = nm[k]; k++; }
        b[k++] = ' ';
        fmt_int(b + k, xy_pct(i));
        xy_text_at(ex, ey, edge, b, col, X0, W);
    }
    px = cx + (int32_t)(xy.x * (float)rx);
    py = cy - (int32_t)(xy.y * (float)ry);
    rr = 7 - (int32_t)(z * 2.0f + 0.5f);                /* the dot: a little smaller as Z rises */
    {
        uint16_t c2 = xy.on ? ux_mix(T_SURF, T_K(0), 100 - (int32_t)(z * 20.0f)) : dc;
        int32_t sh = (int32_t)(z * 6.0f + 0.5f);        /* its shadow on the floor: further from the dot as it goes deeper */
        if (sh > 0)
            og_node((px + sh) << 4, (py + sh) << 4, rr > 2 ? rr - 1 : 1, ux_mix(T_SURF, T_TEXT, 40));
        og_node(px << 4, py << 4, rr, c2);
        og_node(px << 4, py << 4, rr > 3 ? rr - 2 : 1, T_SURF);
        og_node(px << 4, py << 4, 1, c2);
    }
    {   /* the four knobs' values along the foot; the Z parameter named beside its value as ROT is */
        k = 0;
        if (!xy.on) {
            b[k++] = 'O'; b[k++] = 'F'; b[k++] = 'F'; b[k++] = ' '; b[k++] = ' ';
        }
        b[k++] = 'X'; b[k++] = ' ';
        fmt_int(b + k, (int32_t)(xy.x * 50.0f + 50.5f));
        k = str_len(b);
        b[k++] = ' '; b[k++] = ' '; b[k++] = 'Y'; b[k++] = ' ';
        fmt_int(b + k, (int32_t)(xy.y * 50.0f + 50.5f));
        k = str_len(b);
        b[k++] = ' '; b[k++] = ' '; b[k++] = 'R'; b[k++] = 'O'; b[k++] = 'T'; b[k++] = ' ';
        fmt_int(b + k, (int32_t)xy.rot);
        k = str_len(b);
        b[k++] = ' '; b[k++] = ' ';
        str_cpy(b + k, XY_ZNAME[xy.zsel], 8);
        k = str_len(b);
        b[k++] = ' ';
        fmt_int(b + k, (int32_t)(z * 100.0f + 0.5f));
        cv_text(X0 + W / 2 - text_w(&AF_X, b) / 2, (int32_t)graph_h() - 14, &AF_X, b, T_TEXT);
    }
}
