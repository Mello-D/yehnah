/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of MENU > USB TABLET's screen as pixels (the big FM-1, docs/plans/BIG-FM1-PLAN.md step 2; src/tab_screen.c):
 * the browser emulator's firmware (web/emu/yehnah_web.c: the UI drawing into web_fb) with the tile pass on, its TILE
 * stream drained as the link would and decoded into the tablet's 240 x 240 canvas, against web_fb:
 *   A  an endless link: after HELLO, the canvas equals web_fb after every millisecond of a scripted session (pages,
 *      knobs, presets, the song playing)
 *   B  the real ring (2 KB) drained at the link's measured ~114 KB/s: the same session; pieces are refused, and on a
 *      page without the road (FX) the resync and the repaint bring the canvas back to web_fb within 2 s
 *      and tiles never take the ring's last TS_SPARE bytes (the FM-1's replies, STATS 67 B, must still fit)
 *   C  the trickle: a tile wrong on the tablet (on FX) comes back within one round (15 x TS_TRICKLE_MS)
 * In A and B every FRAME marked whole comes after the messages of a whole picture: the canvas then is the screen as it
 * was when the FRAME went (a hash of web_fb then), so the tablet showing its canvas on FRAME never shows a part
 * and the cost: what goes over the link on each part of the session, the host time of the pass */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
static void tab_scr_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px);
static void tab_scr_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c);
#define TAB_SCR_BLIT tab_scr_blit
#define TAB_SCR_FILL tab_scr_fill
#include "../web/emu/yehnah_web.c"

/* the ring, as usb.c's tab_open / tab_byte / tab_close (the size changes per part) */
#define QS (1u << 20)
static uint8_t q[QS];
static uint32_t tq_n = 2048u, qw, qr, qwp;
static int tb_live;
#define TQ_N tq_n
#define TS_POOL
#define TAB_SCR_LIVE() tb_live
static uint32_t room_min = 0xFFFFFFFFu;                 /* the least room a TILE left */
/* (step 4) each ROAD queued: the scene as the emulator drew it then (lcd_blit before the tile pass), a FIFO the decoder
 * takes them from; tests/tablet_road.mjs checks web/road/road.js draws the same from the message (the record, argv[1]) */
#define RS_N 64u
static uint16_t road_px[RS_N][240 * 138];
static uint32_t road_w, road_r, road_lost;
static void road_snap(void)
{
    if (road_w - road_r == RS_N) {
        road_lost++;
        return;
    }
    memcpy(road_px[road_w++ % RS_N], web_fb + graph_y() * 240u, sizeof road_px[0]);
}
/* each FRAME queued: a hash of the screen then, checked against the canvas when the link brings it */
#define FS_N 4096u
static uint32_t frame_h[FS_N], frame_w, frame_r;
static uint32_t fb_hash(const uint16_t *p)
{
    uint32_t i, h = 2166136261u;
    for (i = 0; i < 240u * 240u; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}
static int tab_open(uint32_t type, uint32_t n)
{
    if (!tb_live || tq_n - (qw - qr) < n + 3u)
        return 0;
    if (tq_n - (qw - qr) - n - 3u < room_min)
        room_min = tq_n - (qw - qr) - n - 3u;
    if (type == 5u)                                     /* (ROAD: the scene the FM-1 drew, as the tablet's road.js draws it) */
        road_snap();
    if (type == 6u)
        frame_h[frame_w++ % FS_N] = fb_hash(web_fb);
    qwp = qw;
    q[qwp++ % QS] = (uint8_t)type;
    q[qwp++ % QS] = (uint8_t)n;
    q[qwp++ % QS] = (uint8_t)(n >> 8);
    return 1;
}
static void tab_byte(uint32_t b) { q[qwp++ % QS] = (uint8_t)b; }
static void tab_close(void) { qw = qwp; }
static uint32_t tab_room(void) { return tq_n - (qw - qr); }
#include "../firmware/src/tab_screen.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok)
        fails++;
}

/* the tablet: the stream back into messages, TILE into the canvas (as android/big's decoder) */
static uint16_t canvas[240 * 240];
static uint8_t sb[4096];
static uint32_t sb_n, bad_msgs, link_bytes, roads, flashes;
static uint32_t frames_ok, frames_bad, frames_part, frame_at, frame_gap;   /* (FRAMEs whole and the screen, whole and
                                                         * not, not whole (TS_FRAME_MS); the link's ms of the last, the
                                                         * longest between two: what the tablet waits on a page change) */   /* (flashes: pieces not what the screen shows, painted
                                                         * over later in the same frame: a clear before a drawing) */
static FILE *rec;
static void dec_road(const uint8_t *m, uint32_t n)
{
    if (road_r == road_w) {
        bad_msgs++;
        return;
    }
    memcpy(canvas + graph_y() * 240u, road_px[road_r % RS_N], sizeof road_px[0]);
    if (rec && roads % 7u == 0u) {                      /* (a frame in 7 recorded) */
        uint8_t h[2] = {(uint8_t)n, (uint8_t)(n >> 8)};
        fwrite(h, 1, 2, rec), fwrite(m, 1, n, rec), fwrite(road_px[road_r % RS_N], 2, 240 * 138, rec);
    }
    road_r++, roads++;
}
static void dec_frame(const uint8_t *m, uint32_t n)
{
    if (n != 1u || frame_r == frame_w) {
        bad_msgs++;
        return;
    }
    if (!m[0])
        frames_part++;
    else if (fb_hash(canvas) == frame_h[frame_r % FS_N])
        frames_ok++;
    else
        frames_bad++;
    frame_r++;
    if (fm1_ms - frame_at > frame_gap)
        frame_gap = fm1_ms - frame_at;
    frame_at = fm1_ms;
}
static void dec_tile(const uint8_t *m, uint32_t n)
{
    uint32_t x, y, w, h, i, j, k, c, np, bpp;
    if (n < 7u)
        goto bad;
    x = m[0]; y = m[1]; w = m[2] + 1u; h = m[3] + 1u;
    if (x + w > 240u || y + h > 240u)
        goto bad;
    if (m[4] == TE_FILL) {
        if (n != 7u)
            goto bad;
        c = m[5] | m[6] << 8;
        for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
                canvas[(y + j) * 240u + x + i] = (uint16_t)c;
    } else if (m[4] == TE_RAW) {
        if (n != 5u + 2u * w * h)
            goto bad;
        for (k = 0, j = 0; j < h; j++)
            for (i = 0; i < w; i++, k += 2u)
                canvas[(y + j) * 240u + x + i] = (uint16_t)(m[5 + k] | m[6 + k] << 8);
    } else if (m[4] == TE_PAL) {
        np = m[5] + 1u;
        bpp = np <= 2u ? 1u : np <= 4u ? 2u : 4u;
        if (np < 2u || np > 16u || n != 6u + 2u * np + (w * h * bpp + 7u) / 8u)
            goto bad;
        for (k = 0, j = 0; j < h; j++)
            for (i = 0; i < w; i++, k++) {
                uint32_t bit = k * bpp, ix = m[6u + 2u * np + bit / 8u] >> (8u - bpp - bit % 8u) & ((1u << bpp) - 1u);
                if (ix >= np)
                    goto bad;
                canvas[(y + j) * 240u + x + i] = (uint16_t)(m[6 + 2 * ix] | m[7 + 2 * ix] << 8);
            }
    } else
        goto bad;
    for (j = 0; j < h; j++)                             /* (the frame is drawn whole before the link drains it) */
        for (i = 0; i < w; i++)
            if (canvas[(y + j) * 240u + x + i] != web_fb[(y + j) * 240u + x + i]) {
                flashes++;
                return;
            }
    return;
bad:
    bad_msgs++;
}

static void drain(uint32_t budget)                      /* the link: up to budget bytes off the ring this ms */
{
    uint32_t len;
    while (budget && qr != qw) {
        sb[sb_n++] = q[qr++ % QS];
        budget--;
        link_bytes++;
        if (sb_n >= 3u) {
            len = sb[1] | (uint32_t)sb[2] << 8;
            if (len > sizeof sb - 3u) {
                bad_msgs++;
                sb_n = 0;
            } else if (sb_n == 3u + len) {
                if (sb[0] == TB_TILE)
                    dec_tile(sb + 3, len);
                else if (sb[0] == TB_ROAD)
                    dec_road(sb + 3, len);
                else if (sb[0] == TB_FRAME)
                    dec_frame(sb + 3, len);
                else
                    bad_msgs++;
                sb_n = 0;
            }
        }
    }
}

static uint32_t differ(void)                            /* pixels the canvas has wrong */
{
    uint32_t i, n = 0;
    for (i = 0; i < 240u * 240u; i++)
        n += canvas[i] != web_fb[i];
    return n;
}

/* the session: a step every so many ms */
static uint32_t budget = 0xFFFFFFFFu;                   /* bytes a ms the link takes */
static uint32_t ms_checked, ms_wrong, worst;
static int checking;
static void run_ms(uint32_t n)
{
    while (n--) {
        web_ms();
        tab_scr_service();                              /* (main.c's tab_service, every pass of its wait loop) */
        drain(budget);
        if (checking) {
            uint32_t d = differ();
            ms_checked++;
            if (d) {
                ms_wrong++;
                if (d > worst)
                    worst = d;
            }
        }
    }
}
static void press(uint32_t b) { web_buttons(1u << b); run_ms(40); web_buttons(0); run_ms(160); }
static void turn(uint32_t e, int32_t n, uint32_t times) { while (times--) { web_enc(e, n); run_ms(30); } run_ms(100); }

struct part { const char *name; uint32_t ms, bytes, sent, same, refused, frames, forced; };
static struct part parts[32];
static uint32_t nparts;
static void part_begin(const char *name)
{
    struct part *p = &parts[nparts];
    p->name = name;
    p->ms = fm1_ms; p->bytes = link_bytes; p->sent = tscr.sent; p->same = tscr.same; p->refused = tscr.refused;
    p->frames = tscr.frames; p->forced = tscr.forced;
}
static void part_end(void)
{
    struct part *p = &parts[nparts++];
    p->ms = fm1_ms - p->ms; p->bytes = link_bytes - p->bytes; p->sent = tscr.sent - p->sent;
    p->same = tscr.same - p->same; p->refused = tscr.refused - p->refused;
    p->frames = tscr.frames - p->frames; p->forced = tscr.forced - p->forced;
}

static void session(void)
{
    uint32_t b;
    part_begin("HOME idle 2 s"); press(B_HOME); run_ms(1800); part_end();
    part_begin("K1..K4 turned"); turn(EN_K1, 1, 10); turn(EN_K2, -1, 10); turn(EN_K3, 1, 6); turn(EN_K4, 1, 6); part_end();
    for (b = B_FX; b <= B_SEQ; b++)
        if (b != B_HOME && b != B_SAVE) {
            static const char *nm[] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "", "", "ARP", "SEQ"};
            static char a[24][24];
            snprintf(a[nparts], sizeof a[0], "%s, its knobs", nm[b]);
            part_begin(a[nparts]);
            press(b);
            turn(EN_K1, 1, 3);
            turn(EN_SELECT, 1, 3);
            part_end();
            snprintf(a[nparts], sizeof a[0], "%s idle 1 s", nm[b]);
            part_begin(a[nparts]);
            run_ms(1000);
            part_end();
            press(b);                                   /* (again: some pages toggle off) */
        }
    part_begin("presets"); press(B_HOME); turn(EN_PRESET, 1, 6); turn(EN_ALGO, 1, 2); part_end();
    part_begin("playing on HOME 3 s"); press(B_PLAY); run_ms(3000); part_end();
    part_begin("stopped"); press(B_PLAY); part_end();
}

static void settle(void) { run_ms(1500); }

int main(int argc, char **argv)
{
    uint32_t i;
    clock_t c0;
    double t_off, t_on;
    printf("-- MENU > USB TABLET: the screen as pixels (tiles into the tablet's canvas)\n");
    web_nor_erase();
    web_boot();
    for (i = 0; i < 4000u; i++)
        web_ms();
    for (i = 0; i < 240u * 240u; i++)
        canvas[i] = 0x1234u;                            /* (unknown: everything must be sent) */

    /* the pass's host cost: the session with the tablet off, then A with it on */
    c0 = clock();
    session();
    t_off = (double)(clock() - c0) / CLOCKS_PER_SEC;
    nparts = 0;

    printf(" A: an endless link\n");
    tq_n = QS;
    tb_live = 1;
    tab_scr_hello();
    rec = argc > 1 ? fopen(argv[1], "wb") : 0;
    run_ms(60);                                         /* (HELLO's resync and the first repaint) */
    check("after HELLO the canvas is the screen", differ() == 0);
    checking = 1;
    c0 = clock();
    session();
    t_on = (double)(clock() - c0) / CLOCKS_PER_SEC;
    settle();
    checking = 0;
    printf("       %u ms checked, %u with a pixel wrong (the most: %u)\n", ms_checked, ms_wrong, worst);
    check("the canvas equals the screen after every millisecond", ms_checked > 10000u && ms_wrong == 0);
    check("every message well formed", bad_msgs == 0);
    printf("       FRAME: %u whole and the screen, %u whole and not, %u not whole\n", frames_ok, frames_bad, frames_part);
    check("every FRAME a whole picture, the screen as it was (the tablet never shows a part)",
          frames_ok > 500u && !frames_bad && !frames_part);
    if (rec)
        fclose(rec), rec = 0;
    printf("       %u ROAD (HOME's scene drawn by the tablet), %u snapshots lost\n", roads, road_lost);
    for (i = 0; i < nparts && !strstr(parts[i].name, "playing"); i++)
        ;
    check("HOME playing: the scene as ROAD, under 12 KB/s (was ~146 as tiles)", roads > 100u && road_lost == 0 && i < nparts &&
          parts[i].bytes / (double)parts[i].ms < 12.0);
    for (i = 0; i < nparts; i++)
        printf("       %-22s %6u ms %8u B (%5.1f KB/s)  tiles %5u sent %6u unchanged\n", parts[i].name, parts[i].ms,
               parts[i].bytes, parts[i].bytes / (double)parts[i].ms, parts[i].sent, parts[i].same);
    printf("       host time of the session: %.2f s off, %.2f s on (the pass: +%.0f %%)\n", t_off, t_on,
           100.0 * (t_on - t_off) / t_off);

    {                                                   /* (2026-11-03, the user's: MIXER / SENDS flashed on the tablet) */
        uint32_t k, f0, wrong = 0;
        printf(" A2: HOME > MIXER and SENDS idle, the tablet's repaints (the trickle's, twice a second)\n");
        press(B_HOME);
        for (k = 0; k < 8u && !(!ui.home && cur_page()->graph == GR_TRK); k++)
            turn(EN_SELECT, 1, 1);
        check("HOME > MIXER reached", !ui.home && cur_page()->graph == GR_TRK);
        for (k = 0; k < 2u; k++) {
            run_ms(300);
            f0 = flashes;
            for (i = 0; i < 2000u; i++) {
                run_ms(1);
                wrong += differ() != 0u;
            }
            printf("       %s: %u pieces drawn over in 2 s (a flash), %u ms wrong\n", k ? "SENDS" : "MIXER", flashes - f0, wrong);
            check(k ? "SENDS: no clear under the repaints (no flash on the tablet), the canvas right" :
                      "MIXER: no clear under the repaints (no flash on the tablet), the canvas right", flashes == f0 && !wrong);
            turn(EN_SELECT, 1, 1);
        }
    }

    printf(" B: the real ring (2 KB) at ~114 KB/s\n");
    tq_n = 2048u;
    budget = 114u;
    nparts = 0;
    ms_checked = ms_wrong = worst = 0;
    frames_ok = frames_bad = frames_part = frame_gap = 0;
    frame_at = fm1_ms;
    checking = 1;
    session();
    checking = 0;
    printf("       FRAME: %u whole and the screen, %u whole and not, %u not whole; the longest wait %u ms\n", frames_ok,
           frames_bad, frames_part, frame_gap);
    check("every FRAME marked whole is the screen as it was; one at least every TS_FRAME_MS + 100 ms", frames_ok > 100u &&
          !frames_bad && frame_gap <= TS_FRAME_MS + 100u);
    press(B_HOME);
    press(B_FX);                                        /* (a page without the road: HOME's never settles at this
                                                         * speed until the road is drawn on the tablet, step 4) */
    {
        uint32_t t0 = fm1_ms, ok = 0;
        while (fm1_ms - t0 < 2000u && !(ok = differ() == 0 && qr == qw && !tscr.owed))
            run_ms(1);
        printf("       %u of %u ms right during the session; idle again: right after %u ms\n", ms_checked - ms_wrong,
               ms_checked, fm1_ms - t0);
        check("refused pieces healed: the canvas is the screen within 2 s on FX after the session", ok);
    }
    check("every message well formed", bad_msgs == 0);
    for (i = 0; i < nparts; i++)
        printf("       %-22s %6u ms %8u B (%5.1f KB/s)  tiles %5u sent %6u unchanged %5u refused  FRAME %4u (%u not whole)\n",
               parts[i].name, parts[i].ms, parts[i].bytes, parts[i].bytes / (double)parts[i].ms, parts[i].sent,
               parts[i].same, parts[i].refused, parts[i].frames, parts[i].forced);
    check("the ring was full at times (the test reaches the resync)", tscr.refused > 0);
    check("tiles always left room for a reply (STATS: 67 B)", room_min >= 67u);

    printf(" C: the trickle\n");
    budget = 0xFFFFFFFFu;
    tq_n = QS;
    settle();
    for (i = 0; i < 16u * 16u; i++)
        canvas[(80u + i / 16u) * 240u + 80u + i % 16u] ^= 0x0F0Fu;   /* tile (5, 5) wrong on the tablet */
    {
        uint32_t t0 = fm1_ms;
        while (fm1_ms - t0 < 16u * TS_TRICKLE_MS && differ())
            run_ms(5);
        printf("       healed after %u ms\n", fm1_ms - t0);
        check("a tile wrong on the tablet is healed within one round of the trickle", differ() == 0);
    }

    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
