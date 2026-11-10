/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* --------------------------------------------------------------------------------- HOME: the road --- */
/* (chord-harp fork, 2026-10-29; 2026-11-03 the user's cyberpunk redesign: mockups/cyber_road.py) HOME's TRACKS: the
 * selected track's robot at 3 x on a city street at dusk or by night. Behind it a megacity: two layers of towers going by
 * at a quarter and a half of the road's speed (lit windows, antennas, hologram billboards), an elevated rail and its
 * train, flying cars and a dogfight in the sky; in front the street's buildings (EAT, SLEEP, the club's PARTY and its
 * visualiser, REPEAT, the ones in between), the road wet and mirroring them. The street is RD_LOOP art
 * pixels, round and round. The road moves a pixel a 1/16 of the tempo (G_BPM), always, the song playing or not (the robot
 * walks; it dances to its track's notes, and slides between five places on them). MENU > ANIM OFF: still.
 * A day is dusk and night only (the user's, 2026-11-03), RD_DAY of road. The colours that change are pairs (RD_COLS):
 * night, dusk, mixed for the time (rd_pal); the neon is the same in both.
 * The panel 240 x 141 over the steps, art pixels at 2 x, 120 x 71; colours 0xRRGGBB, made RGB565 as they are drawn.
 * Included by ui_robot.c. */
#define RD_LOOP 360                                     /* the street */
#define RD_DAY (2 * RD_LOOP)
#define RD_WX 2520                                      /* (the road's patterns: 12s, 14s, 20s, 40s and the street) */
#define RD_WRAP (RD_DAY * 112)                          /* (a whole number of days and of RD_WX) */
#define RD_FY 108                                       /* the robot's feet: screen row (the panel 141 tall) */
#define RD_MK 57                                        /* the markings' top art row (rd_marks; 2026-11-05: the road's middle) */
#define RD_MR 11                                        /* the scene's rows 33..43, mirrored in the road's 46..56 */
#define RB_RGB(r, g, b) ((uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))
static const uint32_t RD_SKY[2][8] = {                  /* night, dusk: the top down to the horizon */
    {RB_RGB(0x04, 0x03, 0x0c), 0x07061a, 0x0c0a26, 0x140e34, 0x1e1240, 0x2c164c, 0x3e1c58, 0x582466},
    {0x0c0824, 0x1a0c38, 0x34104c, 0x5c1660, 0x8e2268, 0xc23a66, 0xf2645a, 0xff9a5a}};
/* the colours that change: name, night, dusk */
#define RD_COLS(X)                                                                                                          \
    X(FAR, 0x0e0a1e, 0x22143c) X(FAR_E, 0x141028, 0x2c1a4a) X(MID, 0x08060f, 0x170d2a) X(HAZE, 0x6a2a7a, 0xff5aa0)      \
    X(BLD0, 0x1a1430, 0x241840) X(BLD1, 0x161226, 0x1e1636) X(BLD2, 0x1e1636, 0x2a1c48)
#define RD_E(n, a, b) RD_##n,
#define RD_N(n, a, b) a,
#define RD_D(n, a, b) b,
enum { RD_COLS(RD_E) RD_NPAL };
static const uint32_t RD_NC[RD_NPAL] = {RD_COLS(RD_N)}, RD_DC[RD_NPAL] = {RD_COLS(RD_D)};
#undef RD_E
#undef RD_N
#undef RD_D
#define NE_CY 0x30f0ffu                                 /* the neon */
#define NE_PK 0xff3caau
#define NE_VI 0x9a5cffu
#define NE_GR 0x40ff9au
static const uint32_t RD_NEON[5] = {NE_CY, NE_PK, 0xffd23c, NE_VI, NE_GR};
static const uint32_t RD_VW[5] = {0xff71ce, 0xb967ff, 0x01cdfe, 0x05ffa1, 0xfffb96};   /* vaporwave: pink, purple, blue,
                                                                                         * mint, yellow (the visualiser) */
static uint32_t rd_pal[RD_NPAL] __attribute__((section(".lowbuf")));   /* the colours now (the time of day's) */
static struct {                                         /* each screen column's: */
    int8_t ft[120], mt[120];                            /* a far tower's top (99 none), a near one's */
    uint8_t fk[120], ml[120], mw[120];                  /* far: 1 its lit edge, 2 a window column, 4 an antenna; near: the
                                                         * column in it, its width */
    uint8_t rech[NSTEP];                                /* (2026-11-02) each step as it was when recording began (rd_hash) */
    uint16_t mir[RD_MR][120];                           /* the scene's rows 33..43 as drawn (the wet road mirrors them) */
} rdc __attribute__((section(".lowbuf")));
static struct {
    uint32_t pos, ms, rem;                              /* the way so far (1/256 art px, mod RD_WRAP), the last frame (fm1_ms) */
    uint32_t way;                                       /* this frame's (art px) */
    float d, hz, vt;                                    /* dusk 0..1 (0: night), the haze's, the visualiser's loudest 0..1 */
    float sl[2];                                        /* the searchlights' slopes */
    uint32_t sky[8], step, now;
    int16_t car[4], sp[2][4];                           /* the flying cars' x; the dogfights': the raider's x y, the fighter's */
    uint8_t plain, pulse, vm;                           /* (the sky bare), the neon's beat, the visualiser's mode */
    uint8_t vl[10];                                     /* its bars (0..11) */
    uint8_t face, recw;                                 /* (2026-11-02) TRACKS (trk_face): the steps on the road; the track
                                                         * recording + 1 (rdc.rech its steps then) */
    uint32_t stc[6];                                    /* the steps' colours: empty (a road marking's white), a note (the
                                                         * track's robot's colour: the user, 2026-11-03), the playhead,
                                                         * just recorded; the loop's first step empty (75 % ACCENT: the
                                                         * user, 2026-11-02: 45 % was hard to see), a note (ACCENT) */
    uint64_t recm;                                      /* the steps written since recording began */
    uint8_t ext;                                        /* (the splash) the sky's rows carried up above the scene */
    uint8_t back;                                       /* (2026-11-03) moonwalking: its walk frame 1..4 (backwards), 0 not */
    int8_t mfrom, mto;                                  /* the slide (a dance move: between RD_SLIDE's places): from, to (px) */
    uint32_t mnote, mms;                                /* .. the notes last seen (vxs.nn), the slide's start (fm1_ms) */
} rd;
/* (the big FM-1, docs/plans/BIG-FM1-PLAN.md step 4) HOME's scene as its inputs, for the tablet's road (web/road/road.js
 * Road.frame): rd_draw's and rd_robot's, read by tab_screen.c ts_road, which sends them as ROAD in place of the scene's
 * tiles when on (rb_home drew it, draw_graph's blit is the scene alone: no toast) */
static struct {
    uint32_t way, now;
    float bp;
    int8_t dy;
    uint8_t sh, vm, pulse, face, on;
    uint8_t n, ns, sk[16];                              /* the steps: 16, how many shown, each one's colour + 1 (a nibble) */
    uint8_t nr;
    struct {
        uint8_t ri, sc;
        int16_t cx, fy;
        rb_in_t s;
    } r[2];                                             /* the robots as drawn */
} rdm __attribute__((section(".lowbuf")));
static const int32_t *rd_lv;                            /* each track's level 0..256 (ui_lzhome.c eco.en, graph_tracks); 0
                                                         * none (the splash: the visualiser runs on its own) */
#define P(n) rd_pal[RD_##n]
#define RD_WXAT(x) ((int32_t)((rd.way + (uint32_t)(x)) % RD_WX))   /* the road's x at screen column x */
#define RD_FXAT(x) ((int32_t)((rd.way / 4u + (uint32_t)(x)) % 28000u))   /* the far towers' (a quarter of the speed) */
#define RD_MXAT(x) ((int32_t)((rd.way / 2u + (uint32_t)(x)) % 27600u))   /* the near towers' (a half) */

static const uint8_t RB_B4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static float rd_b(int32_t x, int32_t y) { return (float)RB_B4[(y & 3) * 4 + (x & 3)] * (1.0f / 16.0f); }
static uint16_t rb_565(uint32_t c) { return (uint16_t)(((c >> 19) & 31u) << 11 | ((c >> 10) & 63u) << 5 | ((c >> 3) & 31u)); }
static uint32_t rb_tint(uint32_t c, uint32_t l, float a)   /* the page's tint: c towards l by a */
{
    uint32_t k, o = 0, ai = (uint32_t)(a * 256.0f);
    for (k = 0; k < 24u; k += 8u)
        o |= ((((c >> k) & 255u) * (256u - ai) + ((l >> k) & 255u) * ai + 128u) >> 8) << k;
    return o;
}
static int rb_in_box(int32_t x, int32_t y, int32_t x0, int32_t y0, int32_t w, int32_t h) { return x >= x0 && x < x0 + w && y >= y0 && y < y0 + h; }
static uint32_t rd_888(uint16_t c)
{
    return ((uint32_t)(c >> 11) * 255u / 31u) << 16 | ((uint32_t)(c >> 5 & 63u) * 255u / 63u) << 8 | (uint32_t)(c & 31u) * 255u / 31u;
}
static int32_t rd_abs(int32_t v) { return v < 0 ? -v : v; }
/* whether x y is on s's letters (3 x 5, from x0 y0, gap apart; down: a column) */
static int rd_text(int32_t x, int32_t y, const char *s, int32_t x0, int32_t y0, int32_t gap, int down)
{
    static const char G[] = "EATSLPRY";
    static const uint8_t F[8][5] = {{7, 4, 6, 4, 7}, {7, 5, 7, 5, 5}, {7, 2, 2, 2, 2}, {7, 4, 7, 1, 7},
                                    {4, 4, 4, 4, 7}, {7, 5, 7, 4, 4}, {7, 5, 6, 5, 5}, {5, 5, 2, 2, 2}};   /* (bit 2 the left) */
    for (; *s; s++, x0 += down ? 0 : gap, y0 += down ? 6 : 0)
        if (rb_in_box(x, y, x0, y0, 3, 5)) {
            uint32_t g = 0;
            while (G[g] && G[g] != *s) g++;
            return F[g & 7u][y - y0] >> (2 - (x - x0)) & 1u;
        }
    return 0;
}

/* the sky: its gradient, dusk's sun (big, low, striped), the haze over the city, by night the searchlights */
static uint32_t rd_sky(int32_t x, int32_t y)
{
    int32_t k = RB_RND((float)(y + rd.ext) / (float)(40 + rd.ext) * 7.0f + (rd_b(x, y) - 0.5f) * 0.9f), i;
    uint32_t c = rd.sky[k < 0 ? 0 : k > 7 ? 7 : k];
    if (rd.d > 0.0f) {
        float dx = (float)x + 0.5f - 56.0f, dy = (float)y + 0.5f - 24.0f;
        if (dx * dx + dy * dy < 210.0f && !(y > 18 && (y - 18) % 4 >= 3 - (y - 18) / 6))
            c = rb_tint(c, rb_tint(0xfff0a0, 0xff3c8c, (float)(y - 10) / 29.0f), rd.d);
    }
    if (y > 26)
        c = rb_tint(c, P(HAZE), (float)(y - 26) / 18.0f * rd.hz);
    for (i = 0; rd.d < 0.5f && i < 2; i++) {
        float ddx = (float)x + 0.5f - (i ? 96.0f : 22.0f), ddy = (float)(44 - y), e = ddx - ddy * rd.sl[i];
        if (ddy > 0.0f && (e < 0.0f ? -e : e) < ddy * (0.035f + 0.0006f * ddy))
            c = rb_tint(c, 0x8ae0ff, 0.18f * (1.0f - ddy / 100.0f) * (1.0f - 2.0f * rd.d));
    }
    return c;
}
/* the megacity: the far towers (windows, antennas blinking), the near ones (edges lit in neon, hologram billboards, now and
 * then a megatower with a spire), the elevated rail, its pillars and its train */
static uint32_t rd_far(int32_t x, int32_t y, uint32_t c)
{
    int32_t ft = rdc.ft[x], fx = RD_FXAT(x), mt = rdc.mt[x], ml = rdc.ml[x], mw = rdc.mw[x], mx = RD_MXAT(x), mb = mx / 23, tr;
    uint32_t fk = rdc.fk[x];
    if (y >= ft) {
        c = fk & 1u ? P(FAR_E) : P(FAR);
        if ((fk & 2u) && !(y & 1) && rb_hsh((uint32_t)(fx * 31 + y * 7)) % 5u == 0u)
            c = rb_tint(P(FAR), rd.d > 0.5f ? 0xffb070 : RD_NEON[rb_hsh((uint32_t)(fx + y)) % 3u], 0.55f);
    } else if ((fk & 4u) && y >= ft - 5)
        c = y == ft - 5 && (rd.now / 600u + (uint32_t)(fx / 7)) % 3u == 0u ? 0xff3040 : P(FAR);
    if (ml < mw && y >= mt) {
        uint32_t k = P(MID);
        c = k;
        if (ml == 0 || ml == mw - 1)
            c = rb_tint(k, RD_NEON[mb % 5], 0.65f - 0.25f * rd.d);
        else if (y % 3 == 0 && (ml & 1) && rb_hsh((uint32_t)(mx * 13 + y * 5)) % 4u == 0u)
            c = rb_tint(k, rb_hsh((uint32_t)mb) & 1u ? 0x8ac8ff : 0xffb4d8, 0.35f);
        if (mb % 4 == 1 && rb_in_box(ml, y, 2, mt + 6, mw - 4, 12)) {   /* a hologram: a diamond pulsing, scanlines */
            int32_t e = rd_abs(ml - mw / 2) + rd_abs(y - mt - 12), r = 2 + (int32_t)((rd.now / 300u + (uint32_t)mb) % 3u);
            c = rb_tint(k, RD_NEON[(uint32_t)(mb / 4 + (int32_t)(rd.now / 2000u)) % 5u], (e <= r && e >= r - 1 ? 0.85f : 0.25f) * (y & 1 ? 0.6f : 1.0f));
        }
    } else if (ml == mw / 2 && y < mt && y >= mt - (mt < 0 ? 9 : 3))
        c = y == mt - 9 && (rd.now / 500u) & 1u ? 0xff3040 : P(MID);
    if (y >= 34 && y <= 36) {
        c = y == 34 ? 0x2a2440 : 0x120e20;
        if (y == 35 && mx % 6 == 0) c = rd.d > 0.5f ? 0x6aa0c0 : NE_CY;
    }
    if (y > 36 && mx % 40 < 2) c = 0x0e0a18;
    tr = (int32_t)((rd.now / 20u) % 400u) - 140;
    if (y >= 31 && y <= 33 && x >= tr && x < tr + 34)
        c = y == 31 ? 0x3a3458 : (x - tr) % 4 < 2 && y == 32 ? NE_CY : 0x221c36;
    return c;
}
/* the sky's traffic: the flying cars (a cyan nose, a red tail, a glow under), the dogfights (a fighter's cyan bolts
 * after a raider, its red ones back, now and then a hit) */
static const int8_t RD_CAR[4][3] = {{-30, 30, 1}, {-8, 22, -1}, {8, 40, 1}, {20, 18, -1}};   /* row, ms a px, way */
static uint32_t rd_air(int32_t x, int32_t y, uint32_t c)
{
    uint32_t i, k, n = rd.now;
    for (i = 0; i < 4u; i++) {
        int32_t dy = y - RD_CAR[i][0], dx = x - rd.car[i];
        if (dy == 0 && dx >= -3 && dx <= 3) c = dx == 3 * RD_CAR[i][2] ? NE_CY : dx == 3 || dx == -3 ? 0xff3040 : 0x1a1628;
        else if (dy == 1 && dx >= -1 && dx <= 1) c = 0x0c0a14;
        else if (dy == 2 && dx >= -2 && dx <= 2) c = rb_tint(c, NE_VI, 0.4f);
    }
    for (i = 0; i < 2u; i++) {
        int32_t tx = rd.sp[i][0], ty = rd.sp[i][1], fx = rd.sp[i][2], fy = rd.sp[i][3], dx = x - tx, dy = y - ty, rx;
        if ((y < ty - 2 && y < fy - 2) || (y > ty + 2 && y > fy + 2))
            continue;
        if ((dy == 0 && dx >= -2 && dx <= 2) || ((dy == 1 || dy == -1) && (dx == -2 || dx == -1))) c = dx == 2 ? 0xff3040 : 0x5a1a2a;
        if (dy == 0 && dx == -3) c = 0xffb050;
        dx = x - fx, dy = y - fy;
        if (dy == 0 && dx >= -2 && dx <= 2) c = dx == 1 ? 0xc8fcff : 0x3a4a6a;
        if ((dy == 1 || dy == -1) && dx >= -2 && dx <= 0) c = NE_CY;
        if (dy == 0 && dx == -3) c = NE_VI;
        for (k = 0; k < 3u; k++) {
            int32_t bx = fx + 4 + (int32_t)((n / 8u + k * 7u) % 20u);
            if (y == fy && x >= bx && x < bx + 3 && bx + 3 < tx - 2) c = 0x80ffff;
        }
        rx = tx - 4 - (int32_t)((n / 10u + 5u) % 18u);
        if (y == ty + 1 && x > rx - 2 && x <= rx && rx > fx + 3) c = 0xff5050;
        if ((n / 8u) % 20u > 16u && rd_abs(x - tx + 2) + rd_abs(y - ty) <= 2) c = rb_tint(c, 0xfff0a0, 0.8f);
    }
    return c;
}
/* the club's screen (40 x 22 art px, lx ly in it): the visualiser in turn mirrored bars, a sunset over a grid, two waves
 * (rd.vm: each 16 beats, stopped each 8 s), driven by the tracks' levels (rd.vl, rd.vt), in vaporwave's colours */
static uint32_t rd_viz(int32_t lx, int32_t ly)
{
    float t = (float)(rd.now & 0xFFFFFu);
    if (rd.vm == 0) {                                   /* the bars, out from the middle both ways */
        int32_t e = ly < 11 ? 10 - ly : ly - 11, h = rd.vl[lx / 4];
        if (lx % 4 == 3) return 0x0c0616;
        return e < h ? RD_VW[e < 4 ? 0 : e < 8 ? 1 : 2] : ly == 10 || ly == 11 ? 0x2a1040 : 0x120820;
    }
    if (rd.vm == 1) {                                   /* the sunset: the sun swelling with the music, the grid coming on */
        if (ly < 11) {
            float dx = (float)lx - 19.5f, dy = (float)ly - 10.5f, r = 6.0f + rd.vt * 3.0f;
            if (dx * dx + dy * dy < r * r && !(ly > 5 && (ly - 5) % 3 == 2))
                return rb_tint(RD_VW[4], RD_VW[0], (float)ly / 10.0f);
            return rb_tint(0x1a0830, 0x4a1060, (float)ly / 10.0f);
        } else {
            float r = (float)(ly - 10), u = ((float)lx - 19.5f) * 3.0f / r, z = 6.0f / r + t * 0.0012f * (1.0f + rd.vt);
            float fu = u - (float)RB_RND(u), fz = z - (float)rb_fl(z);
            if ((fu < 0.0f ? -fu : fu) < 0.06f * r || fz < 0.16f)
                return rb_tint(RD_VW[2], RD_VW[0], fz < 0.16f ? 0.6f : 0.0f);
            return 0x14062a;
        }
    }
    {                                                   /* the waves: pink and blue, their height the music's */
        float a = 1.5f + rd.vt * 8.0f, p = (float)lx * 0.35f, ya, yb;
        ya = 10.5f + a * rb_sin(p + t * 0.006f) * rb_sin((float)lx * 0.08f + t * 0.0013f);
        yb = 10.5f + a * 0.7f * rb_sin(p * 1.3f - t * 0.005f + 1.0f);
        ya = ya - (float)ly - 0.5f, yb = yb - (float)ly - 0.5f;
        ya = ya < 0.0f ? -ya : ya, yb = yb < 0.0f ? -yb : yb;
        if (ya < 0.8f) return RD_VW[0];
        if (yb < 0.8f) return RD_VW[2];
        if (ya < 1.8f) return rb_tint(0x100820, RD_VW[0], 0.35f);
        if (yb < 1.8f) return rb_tint(0x100820, RD_VW[2], 0.35f);
        return ly == 10 ? 0x1c0c30 : 0x100820;
    }
}
/* a building in between (its roof's left x0, w wide, its top): a lit edge, a few windows, an AC unit, a pipe, at the foot a
 * shutter or a small shop; k its look */
static const uint32_t RD_WIN[3] = {0xffb050, 0x6ad8ff, 0xff8ccc};
static uint32_t rd_fill(int32_t sx, int32_t y, uint32_t c, int32_t x0, int32_t w, int32_t top, uint32_t k)
{
    int32_t lx = sx - x0;
    if (!rb_in_box(sx, y, x0, top, w, 45 - top))
        return c;
    c = k % 3u == 0u ? P(BLD0) : k % 3u == 1u ? P(BLD1) : P(BLD2);
    if (y == top) c = 0x3a2e5a;
    else if (lx % 3 == 1 && (y - top) % 3 == 2 && y < 38 && rb_hsh((uint32_t)(sx * 17 + y * 3) + k) % 4u == 0u)
        c = RD_WIN[rb_hsh((uint32_t)(sx + y)) % 3u];
    if (rb_in_box(lx, y, w - 5, top + 4 + (int32_t)(k % 5u), 3, 2)) c = y & 1 ? 0x4a4a64 : 0x5a5c78;
    if (lx == 1 + (int32_t)(k % 3u) && y > top) c = 0x2a2440;
    if (y >= 39 && lx >= 1 && lx < w - 1)
        c = k & 1u ? (y & 1 ? 0x3a3450 : 0x2a2640) : y < 42 ? 0xffd080 : 0xc88a40;
    if ((k & 1u) && rb_in_box(lx, y, 2, top + 2, 3, 4))
        c = rb_hsh((uint32_t)(lx + y * 3) + k) % 3u ? RD_NEON[k % 5u] : rb_tint(RD_NEON[k % 5u], 0, 0.6f);
    return c;
}
/* the street at its x sx (the user's order, 2026-11-03: EAT, SLEEP, PARTY, REPEAT): the noodle bar, an alley's vending
 * machines, the capsule hotel, the traffic light, the club (its visualiser, PARTY), the arcade, an empty lot; between
 * them the buildings in between */
static const int16_t RD_FILL[6][3] = {{32, 18, 16}, {88, 17, 20}, {116, 26, 12}, {212, 18, 22}, {264, 20, 18}, {286, 16, 26}};
static uint32_t rd_street(int32_t sx, int32_t y, uint32_t c)
{
    uint32_t k;
    for (k = 0; k < 6u; k++)
        c = rd_fill(sx, y, c, RD_FILL[k][0], RD_FILL[k][1], RD_FILL[k][2], k);
    if (rb_in_box(sx, y, 0, 22, 28, 23)) {              /* the noodle bar: windows, an awning, the counter, lanterns */
        c = (sx + y / 3) % 9 ? 0x1a1230 : 0x221840;
        if (rb_in_box(sx, y, 2, 24, 24, 6) && (sx - 2) % 6 < 4 && (y - 24) % 5 < 3)
            c = rb_hsh((uint32_t)((sx / 6) * 7 + y / 5)) % 3u ? 0x6ad8ff : 0x281e44;
        if (y == 31) c = ((uint32_t)sx + rd.now / 120u) % 4u ? NE_CY : 0x186878;
        if (y == 32 || y == 33) c = (sx / 2) % 2 ? NE_CY : 0x1a4a5a;
        if (rb_in_box(sx, y, 3, 35, 22, 10)) c = y < 40 ? 0xffb050 : 0x8a4a28;
        if (rb_in_box(sx, y, 6, 36, 3, 3) || rb_in_box(sx, y, 18, 36, 3, 3)) c = 0xff3040;
    }
    if (sx == 14 && y == 21) c = 0x2a2c3a;
    if (rb_in_box(sx, y, 8, 14, 13, 7))                 /* EAT, on the beat */
        c = rd_text(sx, y, "EAT", 9, 15, 4, 0) ? rd.pulse || rd.d > 0.5f ? NE_PK : 0xa02060 : 0x140c22;
    if (rb_in_box(sx, y, 53, 30, 7, 15)) {              /* the vending machines */
        c = rb_in_box(sx, y, 53, 32, 7, 7) && sx != 56 ? sx < 56 ? 0x2a6aff : NE_PK : 0x101828;
        if (y == 41 && sx != 56) c = NE_CY;
    }
    if (rb_in_box(sx, y, 62, 22, 23, 23)) {             /* the capsule hotel, SLEEP over it */
        c = 0x181024;
        if (rb_in_box(sx, y, 64, 24, 19, 20) && (y - 24) % 4 < 3 && (sx - 64) % 6 < 5)
            c = rb_hsh((uint32_t)((sx - 64) / 6 + y / 4 * 5)) % 4u == 0u ? NE_VI : 0x2a1e40;
    }
    if (rb_in_box(sx, y, 62, 13, 23, 9))
        c = rd_text(sx, y, "SLEEP", 64, 15, 4, 0) ? NE_VI : y == 13 || y == 21 ? 0x5a3aa0 : 0x100a1a;
    if (rb_in_box(sx, y, 110, 34, 2, 11)) c = 0x2a2c3a; /* the traffic light: red, amber, green in turn */
    if (rb_in_box(sx, y, 109, 26, 4, 9)) {
        static const uint32_t ON[3] = {0xff4040, 0xffc040, 0x40ff80};
        uint32_t j = (uint32_t)(y - 27) / 3u;
        c = (sx == 110 || sx == 111) && (y - 27) % 3 == 0 && j < 3u ? (rd.now / 3000u) % 3u == 2u - j ? ON[j] : rb_tint(0x14141c, ON[j], 0.12f) : 0x14141c;
    }
    if (rb_in_box(sx, y, 146, 8, 62, 37)) {             /* the club: dark glass, the screen (a frame on the beat), the door */
        c = (sx - 146) % 8 ? 0x120c20 : 0x1c1430;
        if (rb_in_box(sx, y, 150, 10, 44, 26))
            c = rb_in_box(sx, y, 152, 12, 40, 22) ? rd_viz(sx - 152, y - 12) : rd.pulse ? NE_PK : 0x9a2a7a;
        if (rb_in_box(sx, y, 154, 37, 36, 8))
            c = (sx - 154) % 9 == 0 || (sx - 154) % 9 == 8 ? 0x2a1a30 : y > 39 ? 0xff70c8 : 0xb04a8a;
    }
    if (rb_in_box(sx, y, 197, 0, 9, 34))                /* PARTY down its sign */
        c = rd_text(sx, y, "PARTY", 200, 2, 0, 1) ? !rd.pulse && rd.d < 0.5f ? 0xffa0d8 : NE_PK :
            sx == 197 || sx == 205 || y == 0 || y == 33 ? NE_PK : 0x1a0820;
    if (rb_in_box(sx, y, 234, 20, 26, 25)) {            /* the arcade: its screens flickering, REPEAT over it */
        static const uint32_t SC[3] = {NE_CY, NE_PK, NE_VI};
        c = 0x160e26;
        if (rb_in_box(sx, y, 236, 30, 22, 9))
            c = (sx - 236) % 4 < 3 && y < 37 ? SC[((uint32_t)sx / 4u + rd.now / 300u) % 3u] : 0x0a0814;
        if (rb_in_box(sx, y, 240, 39, 14, 6)) c = y < 42 ? 0x6ad8ff : 0x3a6a8a;
        if (y < 28)
            c = rd_text(sx, y, "REPEAT", 236, 21, 4, 0) ? NE_GR : y == 20 || y == 27 ? 0x1a6a44 : 0x0e1a14;
    }
    if (rb_in_box(sx, y, 304, 38, 50, 7) && (y == 38 || ((sx + y) % 3 == 0 && (sx - y) % 3 == 0)))
        c = 0x3a3458;                                   /* an empty lot's fence, the city behind it */
    return c;
}
/* the road: wet, the street mirrored in it (rdc.mir, rippled), its puddles catching the neon near the front; the markings
 * drawn over it (rd_marks). (2026-11-04, the user's: the kerb and the pavement gone, the road down to the panel's foot) */
static uint32_t rd_road(int32_t x, int32_t y)
{
    float b = rd_b(x, y);
    int32_t wx = RD_WXAT(x);
    uint32_t c;
    if (y == 45) return 0x6a5aa8;                       /* (2026-11-05, the user's: the road apart from the buildings) the
                                                         * kerb's lit edge, the gutter's shadow, the asphalt cooler */
    if (y == 46) return 0x07050c;
    c = b > 0.6f ? 0x1a1a28 : 0x141420;
    if (y <= 56 && (x + y * 3) % 4) {
        int32_t mx = x + (y % 3 == 0) - (y % 5 == 0);
        uint16_t m = rdc.mir[56 - y][mx < 0 ? 0 : mx > 119 ? 119 : mx];
        if ((m >> 11) * 77u * 8u + (m >> 5 & 63u) * 151u * 4u + (m & 31u) * 28u * 8u > 89u * 256u)
            c = rb_tint(c, rd_888(m), 0.42f - (float)(y - 46) * 0.018f);
    }
    if (y > 63 && rb_hsh((uint32_t)(wx / 5 * 13 + y)) % 11u == 0u)
        c = rb_tint(c, wx % 40 < 20 ? NE_PK : NE_CY, 0.18f);
    return c;
}
/* (2026-11-04, the user's) the road's markings under the robot's feet, art rows RD_MK and RD_MK + 1, straight: TRACKS the
 * steps (rd_steps: rdm.sk, each a dash, painted on the road: they move with it, a ring of its 120 px), else the centre
 * line's dashes */
static void rd_mark(int32_t a, int32_t w, uint32_t c, int32_t dy)   /* a dash from art column a, w wide (a ring) */
{
    cv_rect(2 * a, dy + 2 * RD_MK, 2 * w, 4, rb_565(c));
    if (a + w > 120)
        cv_rect(2 * a - 240, dy + 2 * RD_MK, 2 * w, 4, rb_565(c));
}
static void rd_marks(uint32_t way, int32_t dy)
{
    int32_t s, a;
    if (!rd.face) {
        for (a = (int32_t)((14u - way % 14u) % 14u) - 14; a < 120; a += 14)
            rd_mark(a, 7, 0x1aa0b0, dy);
        return;
    }
    for (s = 0; s < (int32_t)rdm.ns; s++) {
        uint32_t k = rdm.sk[s / 2] >> (s & 1 ? 0 : 4) & 15u;
        a = (int32_t)((1u + (uint32_t)s * 120u / rdm.n + 120u - way % 120u) % 120u);
        rd_mark(a, rdm.n == 16u ? 5 : 2, rd.stc[k - 1u], dy);
    }
}
/* the art pixel x y: the sky, the city, the sky's traffic, the street; the road (2026-11-03: the night's rain gone, the
 * user's: too busy) */
static uint32_t rd_px(int32_t x, int32_t y)
{
    uint32_t c, c0;
    if (y < 45) {
        c = c0 = rd_sky(x, y);
        c = rd_street(RD_WXAT(x) % RD_LOOP, y, rd_air(x, y, rd_far(x, y, c)));
        rd.plain = c == c0;                             /* (the sky bare: a star may shine there) */
    } else
        c = rd_road(x, y), rd.plain = 0;
    return c;
}
/* the time of day (u 0..1: dusk held, a turn to night, night held, a turn back) and the colours for it */
static void rd_time(float u)
{
    float d = u < 0.4f ? 1.0f : u < 0.5f ? (0.5f - u) * 10.0f : u < 0.9f ? 0.0f : (u - 0.9f) * 10.0f;
    uint32_t k;
    d = d * d * (3.0f - 2.0f * d);
    rd.d = d, rd.hz = 0.4f - 0.15f * d;
    for (k = 0; k < 8u; k++)
        rd.sky[k] = rb_tint(RD_SKY[0][k], RD_SKY[1][k], d);
    for (k = 0; k < RD_NPAL; k++)
        rd_pal[k] = rb_tint(RD_NC[k], RD_DC[k], d);
}
/* the frame's movers: the searchlights, the flying cars, the dogfights, the visualiser's bars (each track's level, a
 * shimmer on top; the splash: its own) */
static void rd_movers(uint32_t now)
{
    float t = (float)(now & 0xFFFFFu), kick = 1.0f - (float)(now % 500u) / 300.0f, top = 0.0f;
    uint32_t i;
    rd.sl[0] = 0.55f * rb_sin(t * 0.0007f), rd.sl[1] = 0.55f * rb_sin(t * 0.0007f + 2.1f);
    for (i = 0; i < 4u; i++) {
        int32_t px = (int32_t)((now / (uint32_t)RD_CAR[i][1] + i * 53u) % 170u) - 25;
        rd.car[i] = (int16_t)(RD_CAR[i][2] > 0 ? px : 120 - px);
    }
    for (i = 0; i < 2u; i++) {                          /* (2026-11-03, the user's: less often: each a pass in 880 px, the
                                                         * two in turn, one about each 11 s) */
        uint32_t ph = i * 440u;
        int32_t cy = i ? -34 : 5, tx = (int32_t)((now / 25u + ph) % 880u) - 40;
        float a = t * 0.002f + (float)ph;
        rd.sp[i][0] = (int16_t)tx, rd.sp[i][1] = (int16_t)(cy + RB_RND(2.0f * rb_sin(a)));
        rd.sp[i][2] = (int16_t)(tx - 24), rd.sp[i][3] = (int16_t)(cy + RB_RND(2.0f * rb_sin(a - 0.8f)));
    }
    if (rd_lv)
        for (i = 0; i < NTRK; i++)
            top = (float)rd_lv[i] / 256.0f > top ? (float)rd_lv[i] / 256.0f : top;
    for (i = 0; i < 10u; i++) {
        float w = rb_sin(t * 0.006f * (1.0f + (float)i * 0.37f) + (float)i * 1.7f) * 0.5f + 0.5f, a;
        if (rd_lv)
            a = (0.55f * (float)rd_lv[i * NTRK / 10u] / 256.0f + 0.45f * top) * (0.7f + 0.3f * w) + 0.06f * w;
        else
            a = w * (0.55f + 0.45f * (kick > 0.0f ? kick : 0.0f) * (i < 3u ? 1.0f : 0.4f));
        rd.vl[i] = (uint8_t)clamp(RB_RND(a * 11.0f), 0, 11);
    }
    rd.vt = rd_lv ? top : 0.6f;
    rd.vm = (uint8_t)((song.playing ? rbs.beat / 16u : now / 8000u) % 3u);
}
/* the road at its way so far (art px): each column's towers (a quarter and a half of the way) */
static void rd_cols(uint32_t way)
{
    int32_t x;
    rd.way = way;
    for (x = 0; x < 120; x++) {
        int32_t fx = RD_FXAT(x), mx = RD_MXAT(x);
        int32_t fb = fx / 7, fl = fx % 7, fw = 4 + (int32_t)(rb_hsh((uint32_t)fb) % 4u), mb = mx / 23;
        rdc.ft[x] = (int8_t)(fl < fw ? 20 + (int32_t)(rb_hsh((uint32_t)(fb * 3 + 1)) % 14u) : 99);
        rdc.fk[x] = (uint8_t)((fl == 0) | (fl & 1) << 1 | (fl == fw / 2 && rb_hsh((uint32_t)fb) % 3u == 0u) << 2);
        rdc.mw[x] = (uint8_t)(8u + rb_hsh((uint32_t)(mb * 5)) % 6u), rdc.ml[x] = (uint8_t)(mx % 23);
        rdc.mt[x] = (int8_t)(mb % 5 == 2 ? -22 : 8 + (int32_t)(rb_hsh((uint32_t)(mb * 7 + 2)) % 14u));
    }
}
/* a track's robot walking the road at cx (its pose's shift on top), its shadow on the road (above the markings): at 3 x, dancing to its
 * notes, tipped over its planted foot on one leg */
#define RD_Q 9                                          /* (2026-11-05, the user's: a quarter smaller) the robot's scale in
                                                         * quarters: 2.25 x, the art's columns and rows 2 or 3 px */
static void rd_blitq(uint32_t ri, int32_t x, int32_t y, int32_t q, int32_t sh)
{                                                       /* rb_blit at q / 4 (rb_shear: a screen row at a time) */
    const rb_robot_t *sp = &RB_ROBOTS[ri % RB_NROBOTS];
    int32_t i, j, n, sy;
    for (j = 0; j < RB_H; j++)
        for (sy = y + j * q / 4; sy < y + (j + 1) * q / 4; sy++) {
            int32_t ox = rb_shear != 0.0f ? RB_RND((float)(rb_piv - sy) * rb_shear) : 0;
            if (sy < 0 || sy >= sh)
                continue;
            for (i = 0; i < RB_W; i += n) {
                uint32_t v = rb_px[j][i], rim = v ? rb_rim(i, j) : 0u;
                int32_t sx, ex;
                for (n = 1; i + n < RB_W && rb_px[j][i + n] == v && rb_rim(i + n, j) == rim; n++)
                    ;
                if (!v)
                    continue;
                sx = x + ox + i * q / 4, ex = x + ox + (i + n) * q / 4;
                if (sx < 0) sx = 0;
                if (ex > 240) ex = 240;
                if (ex > sx)
                    cv_rect(sx, sy, ex - sx, 1, rb_lit(rb_col(sp, v), rim));
            }
        }
}
static void rd_robot(const track_t *t, uint32_t ri, int32_t cx, int32_t fy, int32_t sh, uint32_t way, int32_t en_home)
{
    int32_t sc = (sh - 8) / RB_H > 3 ? 3 : (sh - 8) / RB_H < 1 ? 1 : (sh - 8) / RB_H, q = sc >= 3 ? RD_Q : 4 * sc,
            rx = 28 * q / 12, lift, x, y, y0;
    uint16_t shc = rb_565(0x08060f);
    rb_in_t s;
    if (ri >= RB_NROBOTS || !RES_OK())                  /* (chord-harp fork: the parts in the resource region) */
        return;
    s = rb_in(t, ri, en_home);
    cx += RB_POSE[s.pose].shift * q / 4;
    for (y = fy - 3; y < fy + 3; y++)
        for (x = cx - rx - 1; x < cx + rx + 1; x++) {
            float ex = ((float)x + 0.5f - (float)cx) / (float)rx, ey = ((float)y + 0.5f - (float)fy) / 2.6f, d = rb_sqrt(ex * ex + ey * ey);
            if (d < 1.0f && (d < 0.7f || ((x + y) & 1)) && y >= 0 && y < sh)
                cv_rect(x, y, 1, 1, shc);
        }
    if (rd.back)                                        /* sliding: the walk's frames backwards, quick (the moonwalk) */
        s.walk = rd.back, s.dip = s.walk & 1u, s.pose = 0;
    else if (s.walk)                                    /* walking (no move to the notes): a foot up each 1/8, the left, the right */
        s.walk = (uint8_t)(1u + (way / 2u & 3u)), s.dip = s.walk & 1u;
    if (rdm.nr < 2u) {
        rdm.r[rdm.nr].ri = (uint8_t)ri, rdm.r[rdm.nr].sc = (uint8_t)q;   /* (in quarters) */
        rdm.r[rdm.nr].cx = (int16_t)cx, rdm.r[rdm.nr].fy = (int16_t)fy, rdm.r[rdm.nr++].s = s;
    }
    rb_build(ri, &s);
    lift = s.walk ? 0 : RB_POSE[s.pose].lift;           /* the pivot: on one leg, tipped over the planted foot (the user's) */
    rb_shear = lift == 1 ? 0.17f : lift == 2 ? -0.17f : 0.0f;
    y0 = fy - 32 * q / 4 - RB_RND(s.hit * 0.375f * (float)q);
    rb_piv = y0 + 33 * q / 4;
    rd_blitq(ri, cx - 15 * q / 4, y0, q, sh);
    rb_shear = 0.0f;
}
/* (2026-11-02, the redesign) TRACKS: the track's steps (the 16 of the page being played) as the road's
 * road's markings, painted on the road (the user's: they move with it, the 16 a ring the road's 120 px: one leaving
 * on the left comes back on the right; 2026-11-04: under the robot's feet, not behind it: rd_marks): a note the track's robot's colour, an empty step a road marking's white (2026-11-03, the user's: to stand
 * out), the playhead TEXT, a step written since recording began REC; past
 * the pattern's length none */
static uint8_t rd_hash(const step_t *st)
{
    const uint8_t *b = (const uint8_t *)st;
    uint32_t h = 2166136261u, k;
    for (k = 0; k < sizeof *st; k++)
        h = (h ^ b[k]) * 16777619u;
    return (uint8_t)(h ^ h >> 8 ^ h >> 16 ^ h >> 24);
}
static void rd_steps(const track_t *t)
{
    const step_t *sp = seq_steps(t);
    uint32_t me = song.sel % NTRK, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), n = 16u;   /* (the user, 2026-11-10: always the 16 of the page played) */
    uint32_t pos = song.playing ? seq_src(t, t->seq_idx) : 0u, base = pos / n * n, s, k, ri = rb_of(trk_eng(t));
    if ((song.rec >> me & 1u) && rd.recw != me + 1u) {  /* recording began (or moved to this track): its steps now */
        for (s = 0; s < NSTEP; s++)
            rdc.rech[s] = rd_hash(&sp[s]);
        rd.recw = (uint8_t)(me + 1u), rd.recm = 0;
    } else if (!(song.rec >> me & 1u))
        rd.recw = 0, rd.recm = 0;
    rd.stc[0] = 0xb4b2c0, rd.stc[1] = rd_888(RB_PAL[(ri < RB_NROBOTS ? RB_ROBOTS[ri].ac : RB_STEEL) + 3u]);
    rd.stc[2] = 0xffffff, rd.stc[3] = rd_888(T_REC);
    rd.stc[4] = rd_888(ux_mix(T_BG, T_ACCENT, 75)), rd.stc[5] = rd_888(T_ACCENT);
    rdm.n = (uint8_t)n, rdm.ns = 0;
    for (s = 0; s < n && base + s < len; s++) {
        uint32_t si = base + s;
        if (rd.recw && rd_hash(&sp[si]) != rdc.rech[si])
            rd.recm |= 1ull << si;
        k = song.playing && si == pos ? 3u : rd.recm >> si & 1u ? 4u : step_on(&sp[si]) ? 2u : 1u;
        if (si == 0u && k < 3u)                         /* (the user's: the loop's first step marked) */
            k += 4u;
        rdm.sk[s / 2u] = (uint8_t)(s & 1u ? (rdm.sk[s / 2u] & 0xF0u) | k : k << 4), rdm.ns = (uint8_t)(s + 1u);
    }
}
/* HOME's TRACKS (the user's, 2026-10-28 .. 29): the selected track's robot at 3 x walking the road (rd_robot), the
 * scenes going by; under them its 16 steps (the page it plays) in its robot's colours, a note lit, the playhead white on
 * a note. (2026-10-29) A track in SAVE > MORPH's blend: its robot and the other's together, A left, B right, the bigger
 * share in front */
/* the scene at its way (art px), the clock now (ms), the beat's pulse bp: the sky, the road, its scenes, the stars, from
 * screen row dy (the canvas sh tall); t's steps on the road (TRACKS), t 0 none (the splash) */
static void rd_draw(const track_t *t, uint32_t way, uint32_t now, float bp, int32_t dy, int32_t sh)
{
    int32_t x, y, k, i;
    uint16_t row[120];
    rd.now = now;
    rd.step = song.playing ? rbs.beat * 4u + (clk_step & 3u) : now / 250u;   /* (stopped: EAT flickers on the clock) */
    rd_time((float)(way % RD_DAY) / (float)RD_DAY);
    rd.pulse = (uint8_t)(bp > 0.3f || (!song.playing && rd.step % 4u == 0u));   /* (the neon on the beat) */
    rd_movers(now);
    rd_cols(way);
    rd.ext = (uint8_t)(dy > 0 ? (dy + 1) / 2 : 0);
    rd.face = (uint8_t)(t && kn_cells());               /* (TRACKS, MACRO: the new layout's; the splash: none) */
    rdm.way = way, rdm.now = now, rdm.bp = bp, rdm.dy = (int8_t)dy, rdm.sh = (uint8_t)sh, rdm.vm = rd.vm;
    rdm.pulse = rd.pulse, rdm.face = rd.face, rdm.nr = 0;
    if (rd.face)
        rd_steps(t);
    for (y = dy > 0 ? -(dy + 1) / 2 : 0; y < 71; y++) {   /* (the splash: the sky carried up to the screen's top) */
        int32_t sy = dy + 2 * y, off = sy + 2 <= -cv_oy || sy >= sh || sy >= (int32_t)cv_h - cv_oy;   /* (the splash: one
                                                         * band at a time) */
        if (off && (y < 33 || y >= 33 + RD_MR))         /* (the rows the road mirrors: always) */
            continue;
        for (x = 0; x < 120; x++)
            row[x] = rb_565(rd_px(x, y));
        if (y >= 33 && y < 33 + RD_MR)
            memcpy(rdc.mir[y - 33], row, sizeof row);
        if (off)
            continue;
        for (x = 0; x < 120; x = k) {
            for (k = x + 1; k < 120 && row[k] == row[x]; k++)
                ;
            cv_rect(2 * x, sy < 0 ? 0 : sy, 2 * (k - x), sy < 0 ? 1 : sy + 2 > sh ? sh - sy : 2, row[x]);
        }
    }
    rd_marks(way, dy);
    if (rd.face)                                        /* (2026-11-05, the user's) the scene's foot: a rule over the knobs */
        cv_rect(0, sh - 2, 240, 1, rb_565(0x07050c)), cv_rect(0, sh - 1, 240, 1, rb_565(0x6a5aa8));
    for (i = 0; rd.d < 0.34f && i < (int32_t)RB_NSTARS; i++) {   /* the stars (the night's) twinkling, brighter on the beat */
        int32_t sx = RB_STARS[i][0], sy = RB_STARS[i][1], z = RB_STARS[i][2];
        float tw;
        if (dy + 2 * sy < 0 || (rd_px(sx, sy), !rd.plain) || (rd_px(sx, sy + 1), !rd.plain))   /* (not on a ridge's edge) */
            continue;
        tw = ((rb_sin((float)(now & 0xFFFFFu) * 0.0022f * (float)z + (float)i * 1.37f) * 0.5f + 0.5f) * 0.85f + bp * 0.25f) * (1.0f - rd.d * 3.0f);
        if (tw < 0.35f)
            continue;
        cv_rect(sx * 2, dy + sy * 2, 2, 2, rb_565(tw > 0.85f ? 0xffffff : tw > 0.6f ? 0xb4b0e4 : 0x56528c));
        if (z == 3 && tw > 0.9f) {
            uint16_t g = rb_565(0x6a66a8);
            cv_rect(sx * 2 - 1, dy + sy * 2, 1, 1, g); cv_rect(sx * 2 + 2, dy + sy * 2, 1, 1, g);
            cv_rect(sx * 2, dy + sy * 2 - 1, 1, 1, g); cv_rect(sx * 2, dy + sy * 2 + 2, 1, 1, g);
        }
    }
}
static void rb_home(int32_t en_home)
{
    const track_t *t = &trk[song.sel % NTRK];
    int32_t H = (int32_t)graph_h(), sh = kn_cells() ? H : H - 15, dy = sh - 141;
    uint32_t ri = rb_of(trk_eng(t)), now, way, dm;
    float bp;
    rb_clock();
    now = fm1_ms;                                   /* (the user's: the scene lives in silence too, its own clock) */
    dm = fm1_ms - rd.ms, rd.ms = fm1_ms;
    {                                                   /* the road: a pixel a 1/16 (the tempo's: 256ths of a pixel) */
        uint32_t n = (dm > 100u ? 100u : dm) * (uint32_t)clamp(song.g[G_BPM], 40, 240) * 256u + rd.rem;
        rd.pos = (rd.pos + n / 15000u) % (RD_WRAP * 256u), rd.rem = n % 15000u;
    }
    way = rd.pos >> 8;
    bp = song.playing && fm1_ms - rbs.bms < 300u ? 1.0f - (float)(fm1_ms - rbs.bms) / 300.0f : 0.0f;
    rd_draw(t, way, now, bp, dy, sh);
    {                                                   /* the robot (a blend's: both, the same slide: kept apart) */
        uint32_t me = song.sel % NTRK + 1u, other = bl_a == me ? bl_b : bl_b == me ? bl_a : 0u;
        static const int8_t RD_SLIDE[5] = {-48, -24, 0, 24, 48};
        uint32_t e = fm1_ms - rd.mms, k = VX_K(t), nn = vxs.nn[k], i;   /* (2026-11-03, the moonwalk, the user's: a dance
                                                         * move, a note in 10 of the track's at random: a quick slide to
                                                         * another of five places, there till the next; the dance over
                                                         * (no note for 1.2 s): back to the middle) */
        int32_t mo = e < 240u ? rd.mfrom + (rd.mto - rd.mfrom) * (int32_t)e / 240 : rd.mto;
        if (e >= 240u && (nn != rd.mnote ? rb_hsh(nn * 7u + 3u) % 10u == 0u : rd.mto && fm1_ms - vxs.nms[k] >= 1200u)) {
            for (i = 0; i < 4u && RD_SLIDE[i] != rd.mto; i++)
                ;
            rd.mfrom = (int8_t)mo, rd.mto = nn != rd.mnote ? RD_SLIDE[(i + 1u + rb_hsh(nn) % 4u) % 5u] : 0;
            rd.mms = fm1_ms, e = 0;
        }
        rd.mnote = nn;
        rd.back = (uint8_t)(e < 240u ? 4u - (e / 60u & 3u) : 0u);
        if (other)                                      /* (two robots: the inner places only) */
            mo = clamp(mo, -24, 24);
        if (!other)
            rd_robot(t, ri, 120 + mo, dy + RD_FY, sh, way, en_home);
        else if (bl_now >= 64u) {                       /* (B's the bigger share: in front) */
            rd_robot(&trk[bl_a - 1u], rb_of(trk_eng(&trk[bl_a - 1u])), 72 + mo, dy + RD_FY, sh, way, en_home);
            rd_robot(&trk[bl_b - 1u], rb_of(trk_eng(&trk[bl_b - 1u])), 168 + mo, dy + RD_FY, sh, way, en_home);
        } else {
            rd_robot(&trk[bl_b - 1u], rb_of(trk_eng(&trk[bl_b - 1u])), 168 + mo, dy + RD_FY, sh, way, en_home);
            rd_robot(&trk[bl_a - 1u], rb_of(trk_eng(&trk[bl_a - 1u])), 72 + mo, dy + RD_FY, sh, way, en_home);
        }
        rd.back = 0;
    }
    rdm.on = rd.face;                                   /* (the tablet's road: the scene alone, the strip none) */
    if (!rd.face) {                                     /* the steps (not the knob cells' layout: a strip under the scene) */
        const rb_robot_t *sp = &RB_ROBOTS[ri < RB_NROBOTS ? ri : 0u];
        uint32_t ac = ri < RB_NROBOTS ? sp->ac : RB_STEEL, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        uint32_t pos = song.playing ? seq_src(t, t->seq_idx) : 0u, base = pos / 16u * 16u, s;
        cv_rect(0, sh, 240, 1, rb_565(0x241f38));
        cv_rect(0, sh + 1, 240, H - sh - 1, rb_565(0x0a0814));
        for (s = 0; s < 16u; s++) {
            uint32_t si = base + s, on = si < len && step_on(&seq_steps(t)[si]), now = song.playing && si == pos;
            cv_rect(1 + (int32_t)s * 15, sh + 2, 13, H - sh - 3, now ? RB_PAL[on ? RB_C_ffffff : ac + 2u] :
                    si >= len ? rb_565(0x141020) : RB_PAL[on ? ac + 3u : ac]);   /* (past its length: off) */
        }
    }
}
/* (2026-11-02, the user's) the power-on splash (ui_draw.c splash_scene): the road at ms after the start, the city coming
 * up, the power-on T1's robot (engines.c TRK_DEF: its first frame comes before the tracks are set) walking it (a pixel
 * a 1/16 at 120 BPM), moving as T1 sounds; the canvas sh tall (cv_begin'd by the caller) */
static void rd_splash(uint32_t ms, int32_t sh)
{
    uint32_t way = 3u * RD_DAY + 135u + ms / 125u;      /* (dusk, the club coming up) */
    rd_lv = 0;
    rd_draw(0, way, ms, 0.0f, sh - 141, sh);            /* (sh 240: the whole screen, the sky carried up) */
    rd_robot(&trk[0], rb_of(TRK_DEF[0][0]), 120, sh - 141 + RD_FY, sh, way, 0);
}
#undef P
