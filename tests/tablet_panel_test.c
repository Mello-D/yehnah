/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of MENU > USB TABLET's panel (the big FM-1, docs/plans/BIG-FM1-PLAN.md step 3; src/tab_panel.c): the
 * browser emulator's firmware (web/emu/yehnah_web.c) with the tablet's BUTTONS / KEYS / ENC messages and the LEDS
 * stream decoded as android/big's page does:
 *   A  after HELLO one LEDS, equal to what the browser page reads (web_lit_buttons ...); then, through a session
 *      (pages, the song playing, a key held), the last LEDS decoded equals the LEDs after every ms
 *   B  the tablet's buttons work as the panel's: FX, HOME; SELECT from the tablet moves the page as the panel's does
 *      (+1 there, -1 here: back where it was); a key held from the tablet is a held key
 *   C  the panel's and the tablet's together: a button held on both stays held until both let go
 *   D  the tablet gone (not live) lets go of what it held
 *   E  a refused LEDS (the ring full) goes out once there is room */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../web/emu/yehnah_web.c"

/* the ring, as usb.c's tab_open / tab_byte / tab_close */
#define QS 4096u
static uint8_t q[QS];
static uint32_t tq_n = QS, qw, qr, qwp;
static int tb_live;
#define TAB_SCR_LIVE() tb_live
static int tab_open(uint32_t type, uint32_t n)
{
    if (!tb_live || tq_n - (qw - qr) < n + 3u)
        return 0;
    qwp = qw;
    q[qwp++ % QS] = (uint8_t)type;
    q[qwp++ % QS] = (uint8_t)n;
    q[qwp++ % QS] = (uint8_t)(n >> 8);
    return 1;
}
static void tab_byte(uint32_t b) { q[qwp++ % QS] = (uint8_t)b; }
static void tab_close(void) { qw = qwp; }
#include "../firmware/src/tab_panel.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok)
        fails++;
}

/* the tablet: the stream back into messages, LEDS as the page's drawLights takes them */
static uint8_t sb[256];
static uint32_t sb_n, bad_msgs, leds_n, got[8];
static uint32_t le(const uint8_t *p, uint32_t n)
{
    uint32_t v = 0, i;
    for (i = 0; i < n; i++)
        v |= (uint32_t)p[i] << (8u * i);
    return v;
}
static void dec_leds(const uint8_t *m, uint32_t n)
{
    static const uint8_t W[8] = {2, 4, 2, 4, 1, 2, 4, 4};
    uint32_t i, o = 0;
    if (n != 23u) {
        bad_msgs++;
        return;
    }
    for (i = 0; i < 8u; i++) {
        got[i] = le(m + o, W[i]);
        o += W[i];
    }
    leds_n++;
}
static void drain(void)
{
    uint32_t len;
    while (qr != qw) {
        sb[sb_n++] = q[qr++ % QS];
        if (sb_n >= 3u) {
            len = sb[1] | (uint32_t)sb[2] << 8;
            if (len > sizeof sb - 3u) {
                bad_msgs++;
                sb_n = 0;
            } else if (sb_n == 3u + len) {
                if (sb[0] == TB_LEDS)
                    dec_leds(sb + 3, len);
                else
                    bad_msgs++;
                sb_n = 0;
            }
        }
    }
}
static int leds_match(void)
{
    uint32_t w[8] = {web_lit_buttons(), web_lit_keys(), web_dim_buttons(), web_dim_keys(), web_dim_level(),
                     web_breath_buttons(), web_breath_keys(), web_mid_keys()};
    return memcmp(w, got, sizeof w) == 0;
}

static uint32_t ms_checked, ms_wrong, link_on = 1;
static int checking;
static void run_ms(uint32_t n)
{
    while (n--) {
        web_ms();
        tab_pnl_service();                              /* (tablet.c's tab_service, every main-loop pass) */
        if (link_on)
            drain();
        if (checking) {
            ms_checked++;
            ms_wrong += !leds_match();
        }
    }
}
static void t_buttons(uint32_t lab) { uint8_t a[2] = {(uint8_t)lab, (uint8_t)(lab >> 8)}; tab_pnl_msg(TH_BUTTONS, a, 2u); }
static void t_keys(uint32_t m) { uint8_t a[4] = {(uint8_t)m, (uint8_t)(m >> 8), (uint8_t)(m >> 16), (uint8_t)(m >> 24)}; tab_pnl_msg(TH_KEYS, a, 4u); }
static void t_enc(uint32_t role, int32_t n) { uint8_t a[2] = {(uint8_t)role, (uint8_t)(int8_t)n}; tab_pnl_msg(TH_ENC, a, 2u); }
static void t_press(uint32_t b) { t_buttons(1u << b); run_ms(40); t_buttons(0); run_ms(160); }

int main(void)
{
    uint32_t i, p0, p1, n0;
    printf("-- MENU > USB TABLET: the panel (the tablet's buttons, keys, encoders in; the LEDs out)\n");
    web_nor_erase();
    web_boot();
    for (i = 0; i < 4000u; i++)
        web_ms();

    printf(" A: the LEDs\n");
    tb_live = 1;
    tab_pnl_hello();
    run_ms(1);
    check("after HELLO one LEDS, as the browser page reads them", leds_n == 1u && leds_match() && !bad_msgs);
    n0 = leds_n;
    run_ms(200);
    check("nothing changed: no more LEDS", leds_n == n0);

    printf(" B: the tablet's buttons, encoders and keys\n");
    checking = 1;
    t_press(B_FX);
    check("FX from the tablet: the FX page", fx_on());
    t_press(B_HOME);
    check("HOME from the tablet: HOME", ui.home);
    t_press(B_SCL);                                     /* (2026-11-13: FX, ENV, LFO, EDIT, ARP are row lists: SELECT scrolls rows; SCL has several pages) */
    p0 = ui.page;
    t_enc(EN_SELECT, 1);
    run_ms(100);
    p1 = ui.page;
    web_enc(EN_SELECT, -1);
    run_ms(100);
    check("SELECT +1 from the tablet moves the page, the panel's -1 brings it back", p1 != p0 && ui.page == p0);
    t_keys(1u << 7);
    run_ms(40);
    check("a key held from the tablet: held", fm1_in.notes == 1u << 7);
    t_keys(0);
    run_ms(40);
    check("let go: not held", fm1_in.notes == 0u);
    t_press(B_PLAY);
    run_ms(2000);
    t_press(B_PLAY);
    t_press(B_SEQ);
    run_ms(500);
    checking = 0;
    check("through the session the last LEDS = the LEDs after every ms", ms_wrong == 0u);
    printf("       %u ms checked, %u LEDS messages, %u malformed\n", ms_checked, leds_n, bad_msgs);

    printf(" C: both panels\n");
    web_buttons(1u << B_SAVE);
    t_buttons(1u << B_SAVE);
    run_ms(40);
    t_buttons(0);
    run_ms(40);
    check("the panel still holds SAVE: held", (fm1_in.buttons >> panel.btn[B_SAVE]) & 1u);
    t_buttons(1u << B_SAVE);
    web_buttons(0);
    run_ms(40);
    check("the tablet still holds it: held", (fm1_in.buttons >> panel.btn[B_SAVE]) & 1u);

    printf(" D: the tablet gone\n");
    t_keys(1u << 3 | 1u << 20);
    run_ms(40);
    tb_live = 0;
    run_ms(40);
    check("what it held let go", fm1_in.notes == 0u && !((fm1_in.buttons >> panel.btn[B_SAVE]) & 1u));
    run_ms(200);

    printf(" E: the ring full\n");
    tb_live = 1;
    link_on = 0;
    while (tab_open(0x7Fu, 10u)) {
        for (i = 0; i < 10u; i++)
            tab_byte(i);
        tab_close();
    }                                    /* (the ring filled with something else) */
    n0 = leds_n;
    tab_pnl_hello();
    run_ms(50);
    link_on = 1;
    drain();
    check("refused while full", leds_n == n0);
    bad_msgs = 0;
    run_ms(1);
    check("sent once there is room, right", leds_n == n0 + 1u && leds_match());

    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
