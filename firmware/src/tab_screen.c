/* SPDX-License-Identifier: GPL-3.0-only */
/* (chord-harp: the big FM-1, docs/plans/BIG-FM1-PLAN.md step 2) the screen to the tablet as pixels. There is no frame
 * buffer: everything reaches the panel through gfx.c's cv_blit and lcd.c's lcd_fill, and while the tablet is live both
 * also come here (TAB_SCR_BLIT / TAB_SCR_FILL). What they draw is cut on a 16 x 16 grid (15 x 15 tiles); each piece
 * (one tile's part of the draw) is hashed with its place and size, and sent only when the tablet does not show it yet:
 *   TILE (3)  x, y, w-1, h-1, enc, then
 *             enc 0 FILL  the colour (2 bytes)
 *             enc 1 RAW   w x h colours, row by row
 *             enc 2 PAL   n-1, n colours, then each pixel's index, row by row through the piece, high bits first:
 *                         1 bit a pixel for n <= 2, 2 for n <= 4, 4 for n <= 16
 *             a colour is RGB565 as the panel takes it (high byte first). A FILL may be any size (the resync's)
 *   ROAD (5)  (step 4) HOME's scene as its inputs, drawn by the tablet (ts_road; web/road/road.js Road.decode)
 *   FRAME (6) whole: what came before it is a whole picture (1), or a page busier than the link shown anyway (0:
 *             after TS_FRAME_MS, then every TS_BUSY_MS until a whole one: SCL, LFO's moving graph). The tablet draws TILE / ROAD out of sight and shows its canvas on FRAME, so a page change
 *             comes at once, not in blocks (sent between ui_draws, once something went and nothing is owed or waiting on
 *             the repaint, tab_scr_service)
 * What it shows is kept per tile column (ts_col: each piece's hash and rows), since strips share tiles (their edges at
 * y 24, 160, 162, 164, ...: up to three in one tile); a piece sent drops the ones its rows overlap (x is not kept: two
 * pieces side by side in a column drop each other, a cost, never a wrong picture), a full column its oldest. So a
 * forced repaint of an unchanged page sends nothing.
 * The ring full: the piece is refused, its rows owed in its tile. Once the ring is nearly empty again the owed rows are
 * filled with the background on the tablet alone (lcd_fill's T_BG, which the repaint draws over, as after an update) and
 * forgotten, then a repaint (lcd_tab_force: ui_draw's ui.force, quiet on the panel, ui_graph.c lcd_clear) sends what is
 * still missing (a resync at most every TS_RESYNC_MS). The trickle forgets one tile row every TS_TRICKLE_MS and asks for
 * a repaint, so a hash collision heals within ~7.5 s.
 * Needs (tablet.c, or a test's own): tab_open / tab_byte / tab_close / tab_room, TAB_SCR_LIVE(), TQ_N, fm1_ticks */
#define TS_N 15u                                        /* tiles a side */
#ifndef TS_TRICKLE_MS
#define TS_TRICKLE_MS 500u
#endif
#define TS_RESYNC_MS 15u                                /* (a frame: the road's pages overflow the link every frame until
                                                         * step 4, a repaint forced at most this often) */
#define TS_FRAME_MS 500u                                /* a FRAME at least this often while pieces go (a page change: ~300) */
#define TS_BUSY_MS 50u                                  /* (after one not whole: the page moves, blocks and all, as before) */
#define TS_SPARE 96u                                    /* ring bytes tiles leave free: the replies (INFO, the LEDs) */
enum { TB_TILE = 3, TB_ROAD = 5, TB_FRAME = 6 };
enum { TE_FILL = 0, TE_RAW = 1, TE_PAL = 2 };
#ifndef TS_POOL
#define TS_POOL __attribute__((section(".pool")))     /* (~1.4 KB: the pool's spare, not the RAM's last KB) */
#endif
#define TS_NE 20u                                       /* pieces kept a column (15 tile rows, the strips' edges) */
static struct {
    struct {
        uint16_t hash;
        uint8_t y0, y1;                                 /* its rows */
    } col[TS_N][TS_NE];                                 /* the pieces the tablet shows, each column's, oldest first */
    uint8_t ncol[TS_N];
#define TS_NONE 0x0Fu
    uint8_t owed[TS_N * TS_N];                          /* the rows refused in each tile: y1 << 4 | y0 (TS_NONE: none;
                                                         * set from HELLO on, before which nothing is drawn here) */
} tsm TS_POOL;
static struct {
    uint8_t owed;                                       /* any tile owed */
    uint8_t row;                                        /* the trickle's next tile row */
    uint8_t drawn;                                      /* something sent since the last FRAME */
    uint8_t busy;                                       /* the last FRAME was not whole */
    uint32_t shown;                                     /* fm1_ms of the last FRAME */
    uint32_t next;                                      /* fm1_ms of its next step */
    uint32_t resync;                                    /* fm1_ms of the last resync */
    uint32_t sent, same, refused, bytes;                /* (the tests'; bytes: ROAD's too) */
    uint32_t roads;                                     /* ROAD messages sent */
    uint32_t frames, forced;                            /* FRAMEs sent; of them not whole (TS_FRAME_MS) */
} tscr;

/* a piece's hash (its place, size and pixels): px its top-left pixel, stride pixels a row; px NULL: all c */
static uint32_t ts_phash(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint32_t c)
{
    uint32_t hs = 2166136261u ^ (x | y << 8 | (w - 1u) << 16 | (h - 1u) << 20), i, j;
    if (!px) {
        for (i = w * h; i; i--)
            hs = (hs ^ c) * 16777619u;
    } else {
        for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
                hs = (hs ^ px[j * stride + i]) * 16777619u;
    }
    return hs;
}

static int ts_open_t(uint32_t type, uint32_t len)      /* a TILE (ROAD) into the ring, TS_SPARE left over (0: refused) */
{
    if (tab_room() < len + 3u + TS_SPARE || !tab_open(type, len))
        return 0;
    tscr.drawn = 1;
    return 1;
}
#define ts_open(len) ts_open_t(TB_TILE, len)

/* one piece, inside one tile, into the ring (0: refused, its rows owed) */
__attribute__((noinline)) static int ts_psend(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint32_t c)
{
    uint32_t t = y / 16u * TS_N + x / 16u, i, j, k, n = 1, len, bpp, acc, nb;
    uint16_t pal[16];
    uint8_t ix[256];
    if (!px) {
        pal[0] = (uint16_t)c;
    } else {                                            /* the colours, up to 16 (17: too many, RAW) */
        uint32_t m = 0;                                 /* (the last pixel's index) */
        pal[0] = px[0];
        for (j = 0, k = 0; j < h && n <= 16u; j++)
            for (i = 0; i < w; i++) {
                uint32_t v = px[j * stride + i];
                if (pal[m] != v) {
                    for (m = 0; m < n && pal[m] != v; m++)
                        ;
                    if (m == n) {
                        if (n == 16u) {
                            n = 17u;
                            break;
                        }
                        pal[n++] = (uint16_t)v;
                    }
                }
                ix[k++] = (uint8_t)m;
            }
    }
    bpp = n <= 2u ? 1u : n <= 4u ? 2u : 4u;
    len = 5u + (n == 1u ? 2u : n <= 16u ? 1u + 2u * n + (w * h * bpp + 7u) / 8u : 2u * w * h);
    if (!ts_open(len)) {
        uint32_t o = tsm.owed[t], y0 = y % 16u, y1 = y0 + h - 1u;
        if (o != TS_NONE) {
            y0 = y0 < (o & 15u) ? y0 : o & 15u;
            y1 = y1 > o >> 4 ? y1 : o >> 4;
        }
        tsm.owed[t] = (uint8_t)(y1 << 4 | y0);
        tscr.owed = 1;
        tscr.refused++;
        return 0;
    }
    tab_byte(x);
    tab_byte(y);
    tab_byte(w - 1u);
    tab_byte(h - 1u);
    if (n == 1u) {
        tab_byte(TE_FILL);
        tab_byte(pal[0]);
        tab_byte(pal[0] >> 8);
    } else if (n > 16u) {
        tab_byte(TE_RAW);
        for (j = 0; j < h; j++)
            for (i = 0; i < w; i++) {
                tab_byte(px[j * stride + i]);
                tab_byte(px[j * stride + i] >> 8);
            }
    } else {
        tab_byte(TE_PAL);
        tab_byte(n - 1u);
        for (i = 0; i < n; i++) {
            tab_byte(pal[i]);
            tab_byte(pal[i] >> 8);
        }
        for (k = 0, acc = 0, nb = 0; k < w * h; k++) {
            acc = acc << bpp | ix[k];
            if ((nb += bpp) == 8u) {
                tab_byte(acc);
                acc = nb = 0;
            }
        }
        if (nb)
            tab_byte(acc << (8u - nb));
    }
    tab_close();
    tscr.sent++;
    tscr.bytes += len + 3u;
    return 1;
}

static void ts_drop(uint32_t col, uint32_t y0, uint32_t y1)  /* the column's pieces overlapping rows y0..y1 */
{
    uint32_t i, k = 0, n = tsm.ncol[col];
    for (i = 0; i < n; i++)
        if (tsm.col[col][i].y1 < y0 || tsm.col[col][i].y0 > y1)
            tsm.col[col][k++] = tsm.col[col][i];
    tsm.ncol[col] = (uint8_t)k;
}

/* a draw cut into its pieces (px NULL: a fill of c, panel order) */
__attribute__((noinline)) static void ts_draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t c)
{
    uint32_t stride = w, cx, cy, cw, ch, x1, y1, hs, i, col, n;
    const uint16_t *pp;
    if (!TAB_SCR_LIVE() || !w || !h || x >= 240u || y >= 240u)
        return;
    x1 = x + w > 240u ? 240u : x + w;
    y1 = y + h > 240u ? 240u : y + h;
    for (cy = y; cy < y1; cy = (cy | 15u) + 1u)
        for (cx = x; cx < x1; cx = (cx | 15u) + 1u) {
            col = cx / 16u;
            cw = ((cx | 15u) + 1u < x1 ? (cx | 15u) + 1u : x1) - cx;
            ch = ((cy | 15u) + 1u < y1 ? (cy | 15u) + 1u : y1) - cy;
            pp = px ? px + (cy - y) * stride + (cx - x) : 0;
            hs = ts_phash(cx, cy, cw, ch, pp, stride, c);
            hs = (hs ^ hs >> 16) & 0xFFFFu;
            for (i = 0, n = tsm.ncol[col]; i < n && (tsm.col[col][i].hash != hs || tsm.col[col][i].y0 != cy); i++)
                ;
            if (i < n) {
                tscr.same++;
                continue;
            }
            if (!ts_psend(cx, cy, cw, ch, pp, stride, c))
                continue;
            ts_drop(col, cy, cy + ch - 1u);
            if ((n = tsm.ncol[col]) == TS_NE) {
                for (i = 1; i < n; i++)
                    tsm.col[col][i - 1u] = tsm.col[col][i];
                n--;
            }
            tsm.col[col][n].hash = (uint16_t)hs;
            tsm.col[col][n].y0 = (uint8_t)cy;
            tsm.col[col][n].y1 = (uint8_t)(cy + ch - 1u);
            tsm.ncol[col] = (uint8_t)(n + 1u);
        }
}

static void ts_le(uint32_t v, uint32_t n)               /* n bytes of v, low first */
{
    for (; n; n--, v >>= 8)
        tab_byte(v);
}
static uint32_t ts_f32(float f)
{
    uint32_t v;
    memcpy(&v, &f, 4);
    return v;
}
/* (step 4) HOME's scene as ROAD, its inputs (ui_road.c rdm; the layout: web/road/road.js Road.decode), in place of its
 * tiles: the tablet draws it. What was sent there is forgotten (the tablet's road is there now: the next page's panel goes
 * out in full); refused: a quiet repaint (still, ANIM OFF, it would not come again) */
__attribute__((noinline)) static void ts_road(void)
{
    uint32_t i, k, nl = rd_lv ? NTRK : 0u, len;
    if (!TAB_SCR_LIVE())
        return;
    for (i = 0; i < TS_N; i++)
        ts_drop(i, graph_y(), graph_y() + graph_h() - 1u);
    len = 17u + 2u * nl + (rdm.face ? 2u + (rdm.ns + 1u) / 2u + 18u : 0u) + 1u + 28u * rdm.nr;
    if (!ts_open_t(TB_ROAD, len)) {
        tscr.refused++;
        lcd_tab_force = 1;
        return;
    }
    ts_le(rdm.way, 4), ts_le(rdm.now, 4), ts_le(ts_f32(rdm.bp), 4);
    tab_byte((uint8_t)rdm.dy), tab_byte(rdm.sh), tab_byte(rdm.pulse | rdm.face << 1), tab_byte(rdm.vm);
    tab_byte(nl);
    for (i = 0; i < nl; i++)
        ts_le((uint32_t)clamp(rd_lv[i], 0, 65535), 2);
    if (rdm.face) {
        tab_byte(rdm.n), tab_byte(rdm.ns);
        for (i = 0; i < (rdm.ns + 1u) / 2u; i++)
            tab_byte(rdm.sk[i]);
        for (i = 0; i < 6u; i++)
            for (k = 24u; k; k -= 8u)
                tab_byte(rd.stc[i] >> (k - 8u));
    }
    tab_byte(rdm.nr);
    for (i = 0; i < rdm.nr; i++) {
        const rb_in_t *s = &rdm.r[i].s;
        tab_byte(rdm.r[i].ri), ts_le((uint32_t)rdm.r[i].cx, 2), ts_le((uint32_t)rdm.r[i].fy, 2), tab_byte(rdm.r[i].sc);
        ts_le(ts_f32(s->t), 4), ts_le(ts_f32(s->hit), 4), ts_le(ts_f32(s->level), 4), ts_le(ts_f32(s->ang), 4);
        ts_le((uint32_t)s->note, 2), tab_byte(s->blink), tab_byte(s->pose), tab_byte(s->dip), tab_byte(s->walk);
    }
    tab_close();
    tscr.roads++;
    tscr.bytes += len + 3u;
}

static void tab_scr_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    if (rdm.on && x == 0u && y == graph_y() && w == 240u && h == graph_h()) {
        ts_road();
        return;
    }
    ts_draw(x, y, w, h, px, 0);
}

static void tab_scr_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    ts_draw(x, y, w, h, 0, (uint16_t)((c >> 8) | (c << 8)));
}

/* HELLO: the tablet knows nothing yet. Every tile owed: the resync's background, then the repaint */
static void tab_scr_hello(void)
{
    uint32_t i;
    for (i = 0; i < sizeof tsm.owed; i++)
        tsm.owed[i] = 0xF0u;                             /* (rows 0..15) */
    for (i = 0; i < TS_N; i++)
        tsm.ncol[i] = 0;
    tscr.owed = 1;
    tscr.next = fm1_ms + TS_TRICKLE_MS;
    tscr.resync = fm1_ms - TS_RESYNC_MS;
    tscr.shown = fm1_ms;
}

/* the main loop, each pass: the resync once the ring has room, the trickle */
__attribute__((noinline)) static void tab_scr_service(void)
{
    uint32_t r, a, b, t, o;
    if (!TAB_SCR_LIVE())
        return;
    if (tscr.owed && tab_room() >= TQ_N - 256u && fm1_ms - tscr.resync >= TS_RESYNC_MS) {
        tscr.owed = 0;
        tscr.resync = fm1_ms;
        for (r = 0; r < TS_N; r++)
            for (a = 0; a < TS_N; a = b) {              /* each run of tiles owing the same rows: one FILL */
                o = tsm.owed[r * TS_N + a];
                for (b = a + 1u; b < TS_N && tsm.owed[r * TS_N + b] == o; b++)
                    ;
                if (o == TS_NONE)
                    continue;
                if (!ts_open(7u)) {
                    tscr.owed = 1;                        /* (the rest next time) */
                    r = TS_N;
                    break;
                }
                tab_byte(a * 16u);
                tab_byte(r * 16u + (o & 15u));
                tab_byte((b - a) * 16u - 1u);
                tab_byte((o >> 4) - (o & 15u));
                tab_byte(TE_FILL);
                tab_byte(T_BG >> 8);
                tab_byte(T_BG);
                tab_close();
                for (t = r * TS_N + a; t < r * TS_N + b; t++) {
                    tsm.owed[t] = TS_NONE;
                    ts_drop(t % TS_N, r * 16u + (o & 15u), r * 16u + (o >> 4));   /* (its repaint is sent) */
                }
            }
        lcd_tab_force = 1;
    }
    if ((int32_t)(fm1_ms - tscr.next) >= 0) {
        tscr.next = fm1_ms + TS_TRICKLE_MS;
        for (t = 0; t < TS_N; t++)
            ts_drop(t, tscr.row * 16u, tscr.row * 16u + 15u);
        tscr.row = (uint8_t)((tscr.row + 1u) % TS_N);
        lcd_tab_force = 1;
    }
    if (tscr.drawn) {                                   /* FRAME: a whole picture, or the wait too long */
        uint32_t whole = !tscr.owed && !lcd_tab_force;
        if ((whole || fm1_ms - tscr.shown >= (tscr.busy ? TS_BUSY_MS : TS_FRAME_MS)) && ts_open_t(TB_FRAME, 1u)) {
            tab_byte(whole);
            tab_close();
            tscr.drawn = 0;
            tscr.shown = fm1_ms;
            tscr.frames++;
            tscr.forced += !whole;
            tscr.busy = (uint8_t)!whole;
        }
    }
}
