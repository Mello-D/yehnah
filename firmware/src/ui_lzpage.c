/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) the lens panels of ENV, LFO . MOD, SCL . ARP and GLO (ui_lens.c), as the approved mock-up's; included by
 * ui_graph.c after ui_lzedit.c (ed_cap, ed_chip). The canvas is the lens's panel (240 x 140, x 4..236).
 *   ENV  TRACKS: the six tracks' envelopes as tiles (the selected one tinted, its live level a dot; DEST: the three
 *        amounts under each); ENV keeps graph_adsr / the DEST columns; STAGE: the envelope with the stage picked
 *        shaded, the stage's own curve large under it, its time */
/* track t's envelope in the box (x, y, w, h): attack straight, decay and release as three bends, sustain flat */
static void env_mini(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, int32_t th)
{
    int32_t a = 2 + t->p[P_ATK] * (w / 4) / 127, d = 3 + t->p[P_DEC] * (w / 4) / 127, r = 3 + t->p[P_REL] * (w / 4) / 127;
    int32_t s = t->p[P_SUS] * h / 127, b = y + h, x1 = x + a, x3 = x + w - r, k, px = x1, py = y;
    cv_line_t(x, b, x1, y, c, th);
    for (k = 1; k <= 4; k++) {                          /* (1/2, 3/4, 7/8, 15/16 of the way) */
        int32_t xx = x1 + d * k / 4, yy = b - s - (h - s) / (1 << k);
        cv_line_t(px, py, xx, yy, c, th);
        px = xx;
        py = yy;
    }
    cv_line_t(px, py, x3, b - s, c, th);
    px = x3;
    py = b - s;
    for (k = 1; k <= 4; k++) {
        int32_t xx = x3 + r * k / 4, yy = b - (k == 4 ? 0 : s / (1 << k));
        cv_line_t(px, py, xx, yy, c, th);
        px = xx;
        py = yy;
    }
}
static int32_t env_now(const track_t *t)                /* the loudest voice's level now, 0..1000 (-1: none) */
{
    int32_t lv = -1;
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].stage && t->v[i].stage < 4u)
            lv = clamp((int32_t)(((int64_t)t->v[i].env * 1000) >> 24), lv, 1000);
    return lv;
}
static void graph_env_tracks(int dest)
{
    uint32_t c, k;
    for (c = 0; c < NTRK; c++) {
        const track_t *t = &trk[c];
        int32_t x = 4 + (int32_t)(c % 3u) * 78, y = 2 + (int32_t)(c / 3u) * 68, sel = c == song.sel, lv;
        uint16_t bg = sel ? T_TINT : T_RAISE, ink = c >= trk_live ? T_DIM : sel ? T_THEME : T_MID;
        char n[3] = {'T', (char)('1' + c), 0};
        cv_rect(x, y, 76, 66, bg);
        if (sel) {
            cv_rect(x, y, 76, 1, T_THEME); cv_rect(x, y + 65, 76, 1, T_THEME);
            cv_rect(x, y, 1, 66, T_THEME); cv_rect(x + 75, y, 1, 66, T_THEME);
        }
        cv_text(x + 4, y + 4 - AF_X_CAP_Y, &AF_X, n, ink);
        cv_clip_text(x + 22, y + 4 - AF_X_CAP_Y, &AF_X, trk_engp(t)->name, T_DIM, bg, 50);
        env_mini(t, x + 5, y + 16, 66, dest ? 34 : 44, c >= trk_live ? T_DIM : T_THEME, sel ? 2 : 1);
        if (!dest && (lv = env_now(t)) >= 0)
            og_node((x + 70) * 16, (y + 60 - lv * 44 / 1000) * 16, 2, T_TEXT);
        for (k = 0; dest && k < 3u; k++) {              /* FILTER PITCH SHAPE: a bar each from the middle */
            int32_t v = t->p[P_ED_FLT + k], w = v * 34 / 128;
            cv_rect(x + 5, y + 54 + (int32_t)k * 4, 66, 1, T_LINE);
            cv_rect(w < 0 ? x + 38 + w : x + 38, y + 53 + (int32_t)k * 4, w < 0 ? -w : w ? w : 1, 3,
                    k == 0u ? T_THEME : k == 1u ? T_ACCENT : T_MID);
        }
    }
}
static void graph_stage(const track_t *t)
{
    static const char *const SN[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
    uint32_t sg = ui.stage & 3u, id = P_ATK + sg;
    int32_t a = 2 + t->p[P_ATK] * 56 / 127, d = 3 + t->p[P_DEC] * 56 / 127, r = 3 + t->p[P_REL] * 56 / 127;
    int32_t x0 = 10, x1 = x0 + a, x2 = x1 + d, x3 = 226 - r, sx[5] = {x0, x1, x2, x3, 226}, x, py = 0;
    int32_t s = t->p[P_SUS] * 1000 / 127, v = t->p[id] * 1000 / 127;
    const char *u = "";
    char b[24];
    cv_rect(sx[sg], 4, sx[sg + 1u] - sx[sg], 40, T_TINT);   /* the stage picked, on the whole envelope */
    env_mini(t, 10, 6, 216, 36, T_MID, 1);
    for (x = 0; x <= 216; x++) {                        /* its own shape, large: up, down to SUS, held, down to 0 */
        int32_t q = x * 1000 / 216, e = 1000 - (int32_t)((int64_t)(1000 - q) * (1000 - q) * (1000 - q) / 1000000), y;
        y = sg == 0u ? q : sg == 1u ? 1000 - (1000 - s) * e / 1000 : sg == 2u ? s : s - s * e / 1000;
        if (sg == 0u)                                   /* (ATK: slow or fast as it is) */
            y = 1000 - (int32_t)((int64_t)(1000 - q) * (1000 - q) / 1000) * (1000 - v / 2) / 1000 - (1000 - q) * v / 2000;
        y = 122 - y * 66 / 1000;
        if (x)
            cv_line_t(10 + x - 1, py, 10 + x, y, T_THEME, 2);
        py = y;
    }
    cv_rect(10, 124, 217, 1, T_RAISE);
    ed_cap(10, 50, SN[sg], T_THEME);
    param_format(&TP[id], t->p[id], b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_text_r(226, 50 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_SURF);
    cv_text(10, 128 - AF_X_CAP_Y, &AF_X, "0", T_DIM);
    cv_text_r(226, 128 - AF_X_CAP_Y, &AF_X, sg == 2u ? "HELD" : b, T_DIM, T_SURF);
}
/* the lens's panel for ENV's page pg (1), else 0 (its own) */
static int graph_env_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_STAGE)
        graph_stage(t);
    else if (pd_level() == 0u)
        graph_env_tracks(pg->id[0] != P_ATK);
    else
        return 0;
    return 1;
}

/* LFO . MOD  MOD: the matrix as a net, the sources down the left (those a slot reads lit), a curve from each slot's
 *            source to its box D1..D4 on the right (DEST, AMT as a bar), the slot KNOB 1 picked framed, a pulse
 *            running along its curve; LFO: graph_lfo under "RATE . WAVE"; LFO DEST: the LFO small on the left, the four
 *            amounts as rows, each lit by what it sends now; MSEQ, MSEQ 2: graph_mseq under its caption */
static void graph_mod_net(const track_t *t)
{
    uint32_t k, s, used = 0;
    for (k = 0; k < NMSLOT; k++)
        used |= 1u << clamp(t->p[P_M1SRC + 3u * k], 0, MS_N - 1);
    for (s = 1; s < (uint32_t)MS_N; s++)
        cv_text(10, 6 + (int32_t)(s - 1u) * 9 - AF_X_CAP_Y, &AF_X, N_MSRC[s], used >> s & 1u ? T_TEXT : T_DIM);
    for (k = 0; k < NMSLOT; k++) {
        const int16_t *p = &t->p[P_M1SRC + 3u * k];
        int32_t y = 4 + (int32_t)k * 33, on = p[0] && p[1], sel = k == mod_ui_slot, sy = 9 + (p[0] - 1) * 9, w;
        uint16_t bg = sel ? T_TINT : T_RAISE, lc = sel ? T_THEME : on ? T_MID : T_DIM;
        char b[16];
        const char *u = "";
        if (on) {
            og_quad_a(48 * 16, sy * 16, 110 * 16, sy * 16, 158 * 16, (y + 11) * 16, lc, 16u, 256);
            if (sel) {   /* (the pulse: a quadratic's point at u = frame) */
                int32_t q = (int32_t)(ui.frame * 6u % 256u), r = 256 - q;
                og_node((48 * r * r + 2 * 110 * r * q + 158 * q * q) >> 12, (sy * r * r + 2 * sy * r * q + (y + 11) * q * q) >> 12,
                        2, T_TEXT);
            }
        } else
            cv_text(124, y + 9 - AF_X_CAP_Y, &AF_X, "----", T_DIM);
        cv_rect(160, y, 72, 26, bg);
        if (sel) {
            cv_rect(160, y, 72, 1, T_THEME); cv_rect(160, y + 25, 72, 1, T_THEME);
            cv_rect(160, y, 1, 26, T_THEME); cv_rect(231, y, 1, 26, T_THEME);
        }
        b[0] = 'D'; b[1] = (char)('1' + k); b[2] = ' '; b[3] = 0;
        str_cpy(b + 3, on ? mod_dst_name(t, p[1]) : "OFF", 13);
        cv_clip_text(165, y + 5 - AF_X_CAP_Y, &AF_X, b, on ? T_TEXT : T_DIM, bg, 64);
        param_format(&TP[P_M1AMT], p[2], b, &u);
        w = p[2] * 30 / 128;
        cv_rect(166, y + 17, 60, 2, T_LINE);
        if (on && w)
            cv_rect(w < 0 ? 196 + w : 196, y + 16, w < 0 ? -w : w, 4, p[2] < 0 ? T_ACCENT : T_THEME);
    }
}
static void lfo_cap(const track_t *t, int32_t x, int32_t y)   /* "1.18Hz . SIN" */
{
    const char *u = "";
    char b[32], w[12];
    param_format(&TP[P_LRATE], t->p[P_LRATE], b, &u);
    str_cpy(b + str_len(b), u, 32 - str_len(b));
    str_cpy(b + str_len(b), " \xB7 ", 32 - str_len(b));
    param_format(&TP[P_LWAVE], t->p[P_LWAVE], w, &u);
    str_cpy(b + str_len(b), w, 32 - str_len(b));
    ed_cap(x, y, b, T_TEXT);
}
static void graph_lfodest(const track_t *t, const page_t *pg)
{
    int32_t x, py = 0, src = mulq15(t->lfo_val, t->lfo_fade), k;
    uint32_t span = 0xFFFFFFFFu / 40u, ph = t->lfo_ph - 20u * span;
    lfo_cap(t, 10, 6);
    cv_rect(10, 70, 80, 1, T_RAISE);
    for (x = 0; x < 80; x++) {                          /* (two cycles, the phase now a third in) */
        int32_t y = 70 - lfo_wave((track_t *)t, ph + (uint32_t)x * span) * 44 / 32768;
        if (x)
            cv_line_t(10 + x - 1, py, 10 + x, y, T_THEME, 2);
        py = y;
    }
    cv_rect(30, 22, 1, 96, T_MID);
    og_node(30 * 16, (70 - src * 44 / 32768) * 16, 3, T_TEXT);
    for (k = 0; k < 4; k++) {                           /* the amounts, lit by the LFO now */
        int16_t *vp;
        const param_desc_t *d = page_desc(pg, (uint32_t)k, &vp);
        int32_t y = 8 + k * 32, w = *vp * 46 / 100, live = w * src / 32767;
        const char *u = "";
        char b[16];
        if (!d)
            continue;
        cv_rect(100, y, 132, 26, T_RAISE);
        ed_cap(105, y + 4, d->label, T_MID);
        param_format(d, *vp, b, &u);
        str_cpy(b + str_len(b), u, 16 - str_len(b));
        cv_text_r(228, y + 4 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_RAISE);
        cv_rect(106, y + 17, 120, 2, T_LINE);
        if (w)
            cv_rect(w < 0 ? 166 + w : 166, y + 16, w < 0 ? -w : w, 4, ux_mix(T_RAISE, T_THEME, 45));
        if (live)
            cv_rect(live < 0 ? 166 + live : 166, y + 16, live < 0 ? -live : live, 4, T_THEME);
    }
}
static void mseq_cap(const track_t *t, const page_t *pg)
{
    const char *u = "";
    char b[24];
    if (pg->id[0] == P_MSDIV) {                         /* MSEQ 2: "DIV 1/16" */
        str_cpy(b, "DIV ", 24);
        param_format(&TP[P_MSDIV], t->p[P_MSDIV], b + 4, &u);
        ed_cap(10, 6, b, T_ACCENT);
    } else
        ed_cap(10, 6, "MOD SRC: STEP", T_TEXT);
    str_cpy(b, "LEN ", 24);
    fmt_int(b + 4, t->p[P_MSLEN]);
    cv_text_r(228, 6 - AF_X_CAP_Y, &AF_X, b, T_DIM, T_SURF);
}
/* the lens's panel for LFO . MOD's page pg (1), else 0 (its own) */
static int graph_mod_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_MOD)
        graph_mod_net(t);
    else if (pg->graph == GR_LFO) {
        lfo_cap(t, 10, 6);
        cv_oy = GOY + 12;
        graph_lfo(t, T_THEME);
    } else if (pg->id[0] == P_LD_PIT)
        graph_lfodest(t, pg);
    else if (pg->graph == GR_MSEQ || pg->id[0] == P_MSDIV) {
        mseq_cap(t, pg);
        cv_oy = GOY + 12;
        graph_mseq(t, T_THEME);
    } else
        return 0;
    return 1;
}

/* (chord-harp fork) SCL's HARP, OMNI and PROG tabs (mockups/harp_omni_tabs.html): the chord keys drawn as a piece of keyboard
 * in the pen line (ui_engline.c el_*), the plate's strings under them. A key: a box a little rounded at its corners; a
 * black key filled dark and hatched across its foot. A chord down (or latched, the plate's) is pressed in, tinted and
 * its outline shivers. Each string bows and brightens as it is plucked (hk_amp: its key's hit, falling). Black keys
 * TEAL (the palette's theme), white keys and strings CORAL (its accent) */
#define HK_TOP 2                                        /* the keys' top, their height, a black key's (HARP) */
#define HK_H 78
#define HK_BH 46
static uint8_t hk_amp[27];                             /* each key's string: 255 at its hit, falling a drawing */
static uint32_t hk_down;                               /* the plate keys down at the last drawing (bit k) */
static uint16_t hk_col(int blk)
{
    return og_col(blk ? OG_TEAL : OG_CORAL);
}
/* a key's box, x y w h px; jit > 0: its edges shiver (ms the clock) */
static void hk_box(float x, float y, float w, float h, uint16_t c, uint32_t seed, int32_t al, float jit)
{
    static const int8_t SX[4] = {1, 0, -1, 0}, SY[4] = {0, 1, 0, -1};
    float px[4] = {x + 2.5f, x + w, x + w - 2.5f, x}, py[4] = {y, y + 2.5f, y + h, y + h - 2.5f};
    float len[4] = {w - 5.0f, h - 5.0f, w - 5.0f, h - 5.0f};
    uint32_t sd, i;
    for (sd = 0; sd < 4u; sd++)
        for (i = 0; i < 5u; i++) {
            float u = (float)i / 5.0f, j = jit * el_sin(u * 9.0f + (float)fm1_ms * (1.0f / 60.0f) + (float)seed) * (i & 1u ? 1.0f : -1.0f);
            el_pt(px[sd] + (float)SX[sd] * len[sd] * u - (float)SY[sd] * j, py[sd] + (float)SY[sd] * len[sd] * u + (float)SX[sd] * j);
        }
    el_pt(px[0], py[0]);
    el_stroke(c, seed, al);
}
/* a chord's name stacked down a key, a part a line, each line as large as it goes: M where it fits, else S
 * (2026-11-02: the L face gone), else X. Parts: the root, its sharp or flat (tight: kept with the root), then the rest broken where a
 * chord name reads (before "(" "/" "add" "M" "sus", after ","; between a figure and a letter). Laid from y0 down
 * (top) or up from y1 (!top); a line reaching above ny keeps within nw px (a white key's top, between the black keys).
 * The block's height, or -1 when it does not go in */
typedef struct { char s[12]; const aafont_t *f; int8_t cy, ch, cut; } hk_ln_t;
static void hk_font(hk_ln_t *l, int32_t w, int small)   /* small: 1 S, 2 X */
{
    l->f = small == 2 ? &AF_X : small ? &AF_S : text_w(&AF_M, l->s) <= w ? &AF_M : &AF_S;
    if (l->f == &AF_S && text_w(&AF_S, l->s) > w)
        l->f = &AF_X;                                   /* (the last before cutting it: "add" on a black key) */
    l->cy = (int8_t)(l->f == &AF_M ? AF_M_CAP_Y : l->f == &AF_S ? AF_S_CAP_Y : AF_X_CAP_Y);
    l->ch = (int8_t)(l->f == &AF_M ? AF_M_CAP_H : l->f == &AF_S ? AF_S_CAP_H : 7);
    {   /* (a descender or a bracket: its ink below the capitals, room for it before the next line) */
        const char *c;
        for (c = l->s; *c; c++)
            if (*c == 'j' || *c == '(' || *c == ')' || *c == ',' || *c == 'g' || *c == 'p' || *c == 'q' || *c == 'y') {
                l->ch = (int8_t)(l->ch + (l->f == &AF_X ? 2 : 3));
                break;
            }
    }
    l->cut = (int8_t)(text_w(l->f, l->s) > w);          /* (cut to fit: the next way of laying it, if there is one) */
    if (l->cut) {
        char b[12];
        text_fit(b, sizeof b, l->s, &AF_X, w);
        str_cpy(l->s, b, sizeof l->s);
    }
}
static uint32_t hk_split(const char *nm0, uint32_t mode, int32_t w, hk_ln_t *ln)   /* mode 0 loose, 1 tight, 2 the rest
                                                         * in S, 3 all in X, its brackets and commas left out */
{
    char nm[16];
    uint32_t n = 0, len, r = nm0[0] && (nm0[1] == '#' || nm0[1] == 'b') ? 2u : 1u, s = 0, p, i;
    int tight = mode == 1u || mode == 3u, small = mode >= 2u;
    for (i = 0, p = 0; nm0[i] && p < 15u; i++)
        if (mode < 3u || (nm0[i] != '(' && nm0[i] != ')' && nm0[i] != ','))
            nm[p++] = nm0[i];
    nm[p] = 0;
    len = p;
    while (s < len && n < 6u) {
        uint32_t e = s == 0u ? (tight ? r : 1u) : s < r ? r : 0u;
        if (!e)                                         /* the rest: the longest run of parts that fits */
            for (p = s + 1u; p <= len; p++) {
                char c = nm[p], q = nm[p - 1u];
                int dg = c >= '0' && c <= '9', dq = q >= '0' && q <= '9';
                if (p < len && c != '(' && c != '/' && q != ',' && !(c == 'a' && nm[p + 1u] == 'd') && c != 'M' &&
                    !(c == 's' && nm[p + 1u] == 'u') && (dg == dq || q == '(' || c == ','))
                    continue;                           /* (not a place to break) */
                for (i = s; i < p && i - s < 11u; i++) ln[n].s[i - s] = nm[i];
                ln[n].s[i - s] = 0;
                if (!e || text_w(small ? &AF_S : &AF_M, ln[n].s) <= w) e = p;
            }
        for (i = s; i < e && i - s < 11u; i++) ln[n].s[i - s] = nm[i];
        ln[n].s[i - s] = 0;
        hk_font(&ln[n++], w, mode == 3u ? 2 : small && s >= r);
        s = e;
    }
    return n;
}
static void hk_name(const char *nm, int32_t x, int32_t w, int32_t y0, int32_t y1, int top, int32_t ny, int32_t nw,
                    uint16_t fg, uint16_t bg)
{
    hk_ln_t ln[6];
    uint32_t n = 0, i, a;
    int32_t h = 0, y;
    for (a = 0; a < 4u; a++) {                          /* loose, tight, the rest small, all small */
        int32_t gap = a ? 2 : 3, yy;
        n = hk_split(nm, a, w, ln);
        for (i = 0, h = -gap; i < n; i++)
            h += ln[i].ch + gap + (ln[i].cut && a < 3u ? 999 : 0);
        if (h > y1 - y0)
            continue;
        for (i = 0, yy = top ? y0 : y1 - h; i < n; yy += ln[i].ch + gap, i++)   /* (above ny: within nw) */
            if (yy < ny && text_w(ln[i].f, ln[i].s) > nw)
                break;
        if (i == n)
            break;
    }
    for (i = 0, y = top ? y0 : y1 - h; i < n; y += ln[i].ch + (a ? 2 : 3), i++)
        cv_text_in(x, y - ln[i].cy, w, ln[i].f, ln[i].s, fg, bg);
}
/* a black key: its box filled dark, hatched across its foot, the chord's name from its top; on 1 pressed, -1 faint
 * (SCL's keyboard: off the scale) */
static void hk_black(float x, float y, float w, float h, const char *nm, int on, uint32_t seed)
{
    uint16_t c = hk_col(1), bg = on > 0 ? og_mix(T_SURF, c, 30) : T_SURF;
    float dy = on > 0 ? 1.5f : 0.0f, hy = y + h * 0.8f + dy;
    int i;
    cv_rect((int32_t)x + 1, (int32_t)(y + dy) + 1, (int32_t)w - 2, (int32_t)h - 2, bg);
    hk_box(x, y + dy, w, h, on > 0 ? c : og_mix(T_SURF, c, on ? 35 : 70), seed, 256, on > 0 ? 0.9f : 0.0f);
    for (i = -1; i < 3; i++) {
        float x0 = x + 2.0f + (float)i * w / 3.0f, x1 = x + 2.0f + ((float)i + 1.3f) * w / 3.0f;
        el_line(x0 < x + 2.0f ? x + 2.0f : x0, y + h - 2.0f + dy, x1 > x + w - 2.0f ? x + w - 2.0f : x1, hy,
                og_mix(T_SURF, c, on > 0 ? 90 : on ? 25 : 45), seed + (uint32_t)i + 7u, 220);
    }
    hk_name(nm, (int32_t)x + 1, (int32_t)w - 2, (int32_t)(y + dy) + 4, (int32_t)(hy + 3.0f), 1, -99, 99,
            on > 0 ? T_TEXT : on ? T_DIM : T_MID, bg);   /* (a little into the hatching) */
}
/* the plate: n strings x0..x1, y0..y1, notes nt, string s the key ks[s]'s; the note names under them */
static void hk_plate(const uint8_t *nt, const uint8_t *ks, uint32_t n, float x0, float x1, float y0, float y1)
{
    uint16_t c = hk_col(0);
    float dx = (x1 - x0) / (float)n, ph = (float)fm1_ms * (1.0f / 22.0f);
    uint32_t s, i;
    el_line(x0, y0 - 3.0f, x1, y0 - 3.0f, T_MID, 5, 150);
    el_line(x0, y1 + 3.0f, x1, y1 + 3.0f, T_MID, 6, 150);
    for (s = 0; s < n; s++) {
        float x = x0 + dx * ((float)s + 0.5f), a = (float)hk_amp[ks[s]] * (1.0f / 255.0f);
        char b[6];
        if (a > 0.05f)
            cv_rect((int32_t)x - 4, (int32_t)y0, 9, (int32_t)(y1 - y0), og_mix(T_SURF, c, (int32_t)(a * 20.0f)));
        for (i = 0; i <= 12u; i++) {
            float u = (float)i / 12.0f;
            el_pt(x + el_sin(u * 3.1415926f) * el_sin(ph + (float)s) * a * 3.5f, y0 + u * (y1 - y0));
        }
        el_stroke(og_mix(og_mix(T_SURF, c, 55), T_TEXT, (int32_t)(a * 80.0f)), s + 20u, 200 + (int32_t)(a * 56.0f));
        str_cpy(b, HARP_PC[nt[s] % 12u], sizeof b);
        fmt_int(b + str_len(b), (int32_t)nt[s] / 12 - 1);
        cv_text_in((int32_t)(x - dx * 0.5f), (int32_t)y1 + 6 - AF_X_CAP_Y, (int32_t)dx, &AF_X, b, a > 0.3f ? T_TEXT : T_MID, T_SURF);
    }
}
/* the plate keys' hits (their strings' level), the chord keys down (bit k), the chord the plate follows */
static uint32_t hk_keys(const track_t *t)
{
    uint32_t k, ti = trk_index(t), cur = harp_cur[ti % NTRK], on = harp_held(t), dn = 0;
    for (k = 0; k < 27u; k++) {
        uint32_t d = kb_sn[k] && kb_trk[k] == ti && !harp_ck(t, k);
        dn |= d << k;
        hk_amp[k] = d && !((hk_down >> k) & 1u) ? 255u : (uint8_t)(hk_amp[k] * 200u >> 8);
        if (d && hk_amp[k] < 90u)
            hk_amp[k] = 90;                             /* (a key held: its string stays lit) */
    }
    hk_down = dn;
    if (key_latch(t) && t->nheld)
        on |= 1u << cur;
    return on;
}
static void hk_begin(void) { E.s = 1.0f, E.ox = 0.0f, E.oy = 0.0f; }   /* (the pen in canvas px) */
/* (chord-harp fork) the black keys over the plate, short and hatched as the chord keys' black ones, hanging from the
 * plate's top between its strings (x0..x1, n of them) where they sit on the keyboard: the bends (seq.c harp_bend_k: -8
 * / +8 an octave, -2 / +2 a whole tone), LATCH (L, lit while it holds), SCL > PROG's WRITE (W on F#4, in while picked),
 * the others blank (silent); a key down pressed in */
static void hk_bends(const track_t *t, float x0, float x1, float y, uint32_t n)
{
    float dx = (x1 - x0) / (float)n;
    uint32_t k, s = 0;
    for (k = harp_cm(t) ? HARP_NCM : HARP_NCH; k < 27u; k++) {
        int32_t b = harp_bend_k(k);
        int wr = k == HARP_KWRITE && harp_prog(t) && !ui.home && cur_page()->graph == GR_PROG;   /* (PROG's WRITE: W) */
        int dn = k == HARP_KLATCH ? key_latch(t) : wr ? ui.act != 0u : (int)((kb_bend >> k) & 1u);
        float x = x0 + dx * (float)s - 9.0f;
        char l[4] = "L";
        if (!key_black(k)) {
            s++;                                        /* (the strings so far: the black key after them) */
            continue;
        }
        if (!s)
            continue;                                   /* (CHORD's C#: before the plate, silent) */
        if (wr)
            l[0] = 'W';
        else if (k != HARP_KLATCH) {
            l[0] = b ? (b < 0 ? '-' : '+') : 0;
            l[1] = (char)(b == 3072 || b == -3072 ? '8' : '2');
            l[2] = 0;
        }
        hk_black(x, y, 18.0f, 22.0f, "", dn, k + 80u);
        cv_text_in((int32_t)x + 1, (int32_t)y + (dn ? 6 : 5) - AF_X_CAP_Y, 16, &AF_X, l, dn ? T_TEXT : T_MID,
                   dn ? og_mix(T_SURF, hk_col(1), 30) : T_SURF);   /* (one line: hk_name would break "-8") */
    }
}
static void graph_harp(const track_t *t)
{
    static const uint8_t BX[5] = {0, 1, 2, 4, 5};       /* a black key's place: after this white key */
    uint32_t on = hk_keys(t), k, s = 0, nk = harp_cm(t) ? HARP_NCM : HARP_NCH, np = harp_np(t);
    int32_t gh = (int32_t)graph_h();
    uint8_t nt[HARP_NCMP], ks[HARP_NCMP];
    char nm[16];
    const float X0 = 8.0f, W = nk == HARP_NCM ? 44.0f : 32.0f, BW = W * 0.625f;   /* (CHORD: 5 white keys, wider) */
    hk_begin();
    for (k = 0; k < nk; k++) {                    /* the white keys, their chords at their foot */
        float x, dy;
        uint16_t bg;
        int o = (on >> k) & 1u;
        if (key_black(k))
            continue;
        x = X0 + (float)key_place(k) * W;
        dy = o ? 1.5f : 0.0f;
        if (o)
            cv_rect((int32_t)x + 2, HK_TOP + 2 + (int32_t)dy, (int32_t)W - 4, HK_H - 3, og_mix(T_SURF, hk_col(0), 22));
        bg = o ? og_mix(T_SURF, hk_col(0), 22) : T_SURF;
        hk_box(x + 1.0f, (float)HK_TOP + dy, W - 2.0f, (float)HK_H, o ? hk_col(0) : og_mix(T_SURF, hk_col(0), 60), k + 3u, 256,
               o ? 0.9f : 0.0f);
        harp_key_name(t, k, nm);
        hk_name(nm, (int32_t)x + 2, (int32_t)W - 4, HK_TOP + 4, HK_TOP + HK_H - 4 + (int32_t)dy, 0, HK_TOP + HK_BH + 2,
                8, o ? T_TEXT : T_MID, bg);        /* (above the black keys' foot: between them, clear of their outline) */
    }
    for (k = 0; k < nk; k++)                            /* the black keys over them */
        if (key_black(k)) {
            harp_key_name(t, k, nm);
            hk_black(X0 + (float)(BX[key_place(k)] + 1u) * W - BW * 0.5f, (float)HK_TOP - 1.0f, BW, (float)HK_BH, nm,
                     (int)((on >> k) & 1u), k + 40u);
        }
    for (k = 0;                                         /* the plate: its notes as its keys play them */ k < 27u && s < np; k++)
        if (!harp_ck(t, k) && !key_black(k) && key_place(k) >= harp_p0(t)) {
            ks[s] = (uint8_t)k;
            nt[s++] = (uint8_t)harp_map(t, k);
        }
    hk_plate(nt, ks, s, 12.0f, 228.0f, (float)(HK_TOP + HK_H + 8), (float)(gh - 18));
    hk_bends(t, 12.0f, 228.0f, (float)(HK_TOP + HK_H + 4), s);
}
/* (chord-harp fork) SCL > SCL: one octave of keys C..B as HARP's (hk_*), each named by its note; lit the pitch
 * classes the keys sound now (seq.c play_pcs: a chord's every note), pressed in; the scale's keys drawn plain, the
 * others faint, the root's name in the accent (CHORD: HARP's keys, graph_harp) */
static void hk_kb12(const track_t *t, uint32_t lit)
{
    static const uint8_t BX[5] = {0, 1, 2, 4, 5};
    uint32_t mask = scale_mask(t), rt = (uint32_t)clamp(t->p[P_ROOT], 0, 11), k;
    const float X0 = 8.0f, W = 32.0f;
    hk_begin();
    for (k = 0; k < 12u; k++) {                         /* the white keys */
        uint32_t in = mask >> (k + 12u - rt) % 12u & 1u;
        int o = (int)(lit >> k & 1u);
        float x = X0 + (float)key_place(k) * W, dy = o ? 1.5f : 0.0f;
        uint16_t bg = o ? og_mix(T_SURF, hk_col(0), 22) : T_SURF;
        if (key_black(k))
            continue;
        if (o)
            cv_rect((int32_t)x + 2, HK_TOP + 2 + (int32_t)dy, (int32_t)W - 4, HK_H - 3, bg);
        hk_box(x + 1.0f, (float)HK_TOP + dy, W - 2.0f, (float)HK_H, o ? hk_col(0) : og_mix(T_SURF, hk_col(0), in ? 60 : 22),
               k + 3u, 256, o ? 0.9f : 0.0f);
        cv_text_in((int32_t)x + 2, HK_TOP + HK_H - 18 + (int32_t)dy - AF_M_CAP_Y, (int32_t)W - 4, &AF_M, HARP_PC[k],
                   k == rt ? T_ACCENT : o ? T_TEXT : in ? T_MID : T_DIM, bg);
    }
    for (k = 0; k < 12u; k++)                           /* the black keys over them */
        if (key_black(k)) {
            uint32_t in = mask >> (k + 12u - rt) % 12u & 1u;
            float x = X0 + (float)(BX[key_place(k)] + 1u) * W - 10.0f;
            int o = lit >> k & 1u ? 1 : in ? 0 : -1;
            hk_black(x, (float)HK_TOP - 1.0f, 20.0f, (float)HK_BH, "", o, k + 40u);
            cv_text_in((int32_t)x + 1, HK_TOP + 6 + (o > 0) - AF_X_CAP_Y, 18, &AF_X, HARP_PC[k],
                       k == rt ? T_ACCENT : o > 0 ? T_TEXT : o ? T_DIM : T_MID,
                       o > 0 ? og_mix(T_SURF, hk_col(1), 30) : T_SURF);
        }
}
/* SCL: the keyboard, the root and scale under it, QUANT (SCL held) and ALL TR at the right */
static void graph_key(const track_t *t, const page_t *pg)
{
    const char *u = "";
    char b[24], v[12];
    int16_t *vp;
    const param_desc_t *d = page_desc(pg, 3, &vp);
    hk_kb12(t, play_pcs(t));
    str_cpy(b, HARP_PC[clamp(t->p[P_ROOT], 0, 11)], sizeof b);
    str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
    param_format(&TP[P_SCALE], t->p[P_SCALE], v, &u);
    str_cpy(b + str_len(b), v, sizeof b - str_len(b));
    cv_text(10, HK_TOP + HK_H + 10 - AF_M_CAP_Y, &AF_M, b, T_TEXT);
    str_cpy(b, "QUANT ", sizeof b);
    param_format(&TP[P_QUANT], t->p[P_QUANT], b + 6, &u);
    cv_text(10, HK_TOP + HK_H + 26 - AF_X_CAP_Y, &AF_X, b, T_DIM);
    if (d) {
        param_format(d, *vp, v, &u);
        str_cpy(b, d->label, sizeof b);
        str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
        str_cpy(b + str_len(b), v, sizeof b - str_len(b));
        cv_text_r(230, HK_TOP + HK_H + 26 - AF_X_CAP_Y, &AF_X, b, T_DIM, T_SURF);
    }
}
static void graph_arp(const track_t *t)
{
    static const uint8_t TRI[3] = {0, 4, 7};
    const char *u = "";
    char b[40], v[12];
    uint32_t i, md = (uint32_t)t->p[P_AMODE], oc = (uint32_t)clamp(t->p[P_AOCT], 1, 4), n = 3u * oc;
    param_format(&TP[P_AMODE], t->p[P_AMODE], b, &u);
    str_cpy(b + str_len(b), " \xB7 ", 40 - str_len(b));
    param_format(&TP[P_ARATE], t->p[P_ARATE], v, &u);
    str_cpy(b + str_len(b), v, 40 - str_len(b));
    str_cpy(b + str_len(b), " \xB7 OCT ", 40 - str_len(b));
    fmt_int(b + str_len(b), (int32_t)oc);
    ed_cap(10, 6, b, T_TEXT);
    param_format(&TP[P_AORDER], t->p[P_AORDER], b, &u);
    cv_text_r(228, 6 - AF_X_CAP_Y, &AF_X, b, T_DIM, T_SURF);
    if (!md) {
        cv_text_in(4, 56 - AF_M_CAP_Y, 232, &AF_M, "ARP OFF", T_MID, T_SURF);
        cv_text_in(4, 76 - AF_X_CAP_Y, 232, &AF_X, "KEYS PLAY STRAIGHT", T_DIM, T_SURF);
        return;
    }
    for (i = 0; i < 16u; i++) {                         /* the pattern over a triad, OCT octaves */
        uint32_t pp = n > 1u ? 2u * n - 2u : 1u, j = i % pp;
        uint32_t k = md == 2u ? n - 1u - i % n : md == 3u ? (j < n ? j : pp - j) : md == 4u ? (i * 2654435761u >> 16) % n :
                     md == 6u ? i / 2u % n : i % n;
        int32_t semi = TRI[k % 3u] + 12 * (int32_t)(k / 3u), x = 10 + (int32_t)i * 14 + (i & 1u ? t->p[P_ASWING] * 6 / 127 : 0);
        int32_t w = 2 + t->p[P_AGATE] * 10 / 127, y = 118 - semi * 2;
        uint32_t on = (i * 40503u >> 8) % 100u < (uint32_t)t->p[P_APROB] * 100u / 127u;
        cv_rect(x, y, w, 4, on ? T_THEME : T_RAISE);
    }
    cv_rect(10, 124, 220, 1, T_RAISE);
}
/* the lens's panel for SCL . ARP's page pg (1), else 0 */
static int graph_key_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_SCALE)
        graph_key(t, pg);
    else if (pg->id[3] == P_HPLATE || pg->id[1] == P_PROG || pg->id[3] == P_OSTR || pg->id[1] == P_CHRD)   /* (chord-harp
                                                         * fork) SCL > HARP, PROG, OMNI, CHORD: the same keys (CHORD's 7) */
        graph_harp(t);
    else if (pg->fam == FAM_ARP)
        graph_arp(t);
    else
        return 0;
    return 1;
}

/* GLO  MIX: the six strips (draw_tracks, track_col6; SENDS: the four sends as bars); GLOBAL keeps its bar of sixteenths;
 *      SYSTEM (2026-11-02: as the other GLO pages): its four rows (graph_glo), then the CPU load over the last seconds
 *      as a line under them (USB's knob turned: its choices instead) */
static void graph_system(void)
{
    static uint8_t cpu[56], cpu_at;
    static uint32_t cpu_ms;
    uint32_t k;
    int32_t py = 0, y0 = 80, yb = (int32_t)cv_h - 6;
    if (fm1_ms - cpu_ms >= 250u) {                      /* (a sample each quarter second) */
        cpu_ms = fm1_ms;
        cpu[cpu_at++ % 56u] = (uint8_t)clamp((int32_t)(song.cpu_q8 * 100u / 256u), 0, 100);
    }
    graph_glo(cur_page());
    if (IS_MROW(cur_page()->id[glo_k % 4u]))            /* (USB: its choices, as the other rows') */
        return;
    for (k = 0; k < 56u; k++) {
        int32_t y = yb - 1 - cpu[(cpu_at + k) % 56u] * (yb - 1 - y0) / 100;
        if (k)
            cv_line_t(8 + (int32_t)(k - 1u) * 4, py, 8 + (int32_t)k * 4, y, T_THEME, 2);
        py = y;
    }
    cv_rect(8, yb, 224, 1, T_RAISE);
}
