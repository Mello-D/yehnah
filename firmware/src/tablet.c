/* SPDX-License-Identifier: GPL-3.0-only */
/* (chord-harp: the big FM-1, docs/plans/BIG-FM1-PLAN.md) the tablet app's messages, main loop side: MENU > SYSTEM > USB
 * TABLET's vendor bulk pipe (usb.c: tab_q out, tab_rx in). Every message is `type, len lo, len hi, payload`; the
 * tablet's arrive whole in one packet (<= 64 bytes), the FM-1's are a byte stream the tablet reassembles.
 *   tablet -> FM-1  HELLO (1)          the app is there: INFO back, the screen in full (tab_scr_hello)
 *                   BUTTONS (4), KEYS (5), ENC (6)  the tablet's panel (tab_panel.c)
 *   FM-1 -> tablet  INFO (1)           protocol 1, usb_out, the ring's size (2 bytes), "Yehnah v.."
 *                   TILE (3)           a piece of the screen (tab_screen.c)
 *                   ROAD (5)           (step 4) HOME's scene as its inputs, drawn by the tablet (tab_screen.c ts_road)
 *                   FRAME (6) whole    the tiles and roads before it make a picture: the tablet shows it (tab_screen.c)
 *                   LEDS (4)           the LEDs (tab_panel.c)
 * (2026-11-04: the link tests STATS (2) and FLOOD (3, 0x7F) removed, for the flash: the app's buttons get no answer) */
#if YEHNAH_TABLET
enum { TB_INFO = 1 };
enum { TH_HELLO = 1 };
#define TAB_SCR_LIVE() (tab.live && !usb.suspended)   /* (suspended: unplugged or the host
                                                         * asleep, tab.live kept: no repaints for no one, the panel's keys let go) */
#include "tab_screen.c"
#include "tab_panel.c"

static void tab_msg(uint32_t type, const uint8_t *a, uint32_t n)
{
    uint8_t o[64];
    uint32_t k;
    switch (type) {
    case TH_HELLO: {
        static const char v[] = "Yehnah " YEHNAH_VERSION;
        tab.live = 1;
        o[0] = 1;
        o[1] = usb_out;
        o[2] = (uint8_t)TQ_N;
        o[3] = (uint8_t)(TQ_N >> 8);
        for (k = 0; k + 1u < sizeof v && k < sizeof o - 4u; k++)
            o[4u + k] = (uint8_t)v[k];
        tab_put(TB_INFO, o, 4u + k);
        tab_scr_hello();
        tab_pnl_hello();
        break;
    }
    default:
        tab_pnl_msg(type, a, n);
        break;
    }
}

static void tab_service(void)                           /* main loop: the tablet's packets */
{
    uint32_t n = tab_rx_n, i = 0, len;
    if (n) {
        while (i + 3u <= n) {
            len = tab_rx[i + 1u] | (uint32_t)tab_rx[i + 2u] << 8;
            if (i + 3u + len > n)
                break;                                  /* (cut short: the rest dropped) */
            tab_msg(tab_rx[i], tab_rx + i + 3u, len);
            i += 3u + len;
        }
        RING_PUBLISH();
        tab_rx_n = 0;                                   /* (usb_poll takes the next packet) */
    }
    {   /* live again (a resume): what was drawn meanwhile never went, so the screen in full */
        static uint8_t was;
        if (TAB_SCR_LIVE() && !was)
            tab_scr_hello();
        was = (uint8_t)TAB_SCR_LIVE();
    }
    tab_scr_service();
    tab_pnl_service();
    fm1_irq_off();                                      /* (page changes b) a packet now if the SIE took the last */
    tab_ep_more();
    fm1_irq_on();
}
#else
static void tab_service(void) {}
#endif
