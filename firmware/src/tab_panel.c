/* SPDX-License-Identifier: GPL-3.0-only */
/* (chord-harp: the big FM-1, docs/plans/BIG-FM1-PLAN.md step 3) the tablet's panel: its buttons, keys and encoders
 * into the FM-1's input as the panel's own, the LEDs back. Included by tablet.c (and tests/tablet_panel_test.c on the
 * browser emulator's firmware); the includer gives TAB_SCR_LIVE(), tab_open / tab_byte / tab_close.
 *   tablet -> FM-1  BUTTONS (4) u16    the buttons held, bit = label (panel.c B_FX .. B_OCTUP), the whole state
 *                   KEYS (5) u32       the note keys held, bit k = key k (F3 .. G5), the whole state
 *                   ENC (6) role, n    encoder role (EN_SELECT .. EN_K4) turned n detents (int8, + = clockwise)
 *   FM-1 -> tablet  LEDS (4) 23 B      as the browser page reads them (web/emu/yehnah_web.c web_lit_buttons ...):
 *                                      lit buttons u16 (bit 14 PLAY's green), lit keys u32, glowing buttons u16, keys
 *                                      u32, DIM LO (1 B), breathing buttons u16, keys u32, the mid level's keys u32
 * Held buttons and keys go through hal/fm1_input.h fm1_ext_hold as closed keys: they debounce and make edges as
 * a press on the panel does, and either the panel or the tablet holds a key. The tablet gone (no HELLO since the
 * last bus reset or mode change), whatever it held is let go. LEDS goes out when the LED arrays change (compared
 * once a pass, 56 bytes), again after HELLO, and again once there is room if the ring refused it. */
enum { TH_BUTTONS = 4, TH_KEYS = 5, TH_ENC = 6, TB_LEDS = 4 };
#ifndef TAB_DIM_LO
#define TAB_DIM_LO() (settings_leds == LEDS_DIM_LO ? 1u : settings_leds == LEDS_HIGH ? 2u : 0u)   /* the dim level: 0 HI, 1 LO, 2 HIGH (fm1_led_dim_level) */
#endif
static struct {
    uint8_t seen[4][FM1_NCOL + 1u];                     /* the LEDs last sent, each + DIM LO */
    uint8_t dirty, held;                                /* LEDS owed; the tablet holds something */
} tpn;

static void tab_pnl_hello(void) { tpn.dirty = 1; }

static void tab_pnl_msg(uint32_t type, const uint8_t *a, uint32_t n)
{
    static uint32_t lab, keys;
    uint32_t b, m = 0;
    if (type == TH_ENC) {
        if (n >= 2u && a[0] < NE)
            fm1_ext_turn(panel.enc[a[0]], (int32_t)(int8_t)a[1] * panel.dir[a[0]]);
        return;
    }
    if (type == TH_BUTTONS && n >= 2u)
        lab = a[0] | (uint32_t)a[1] << 8;
    else if (type == TH_KEYS && n >= 4u)
        keys = (a[0] | (uint32_t)a[1] << 8 | (uint32_t)a[2] << 16 | (uint32_t)a[3] << 24) & ((1u << 27) - 1u);
    else
        return;
    for (b = 0; b < NB; b++)
        if ((lab >> b) & 1u)
            m |= 1u << panel.btn[b];
    fm1_ext_hold(m, keys);
    tpn.held = m || keys;
}

static void tab_pnl_led_masks(const uint8_t *l, uint32_t *bm, uint32_t *km)
{
    uint32_t c, r, b, id;
    *bm = *km = 0;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++) {
            if (FM1_KEYMAP[r][c] < 0 || !((l[c] >> r) & 1u))
                continue;
            id = (uint32_t)FM1_KEYMAP[r][c];
            if (id >= 14u)
                *km |= 1u << (id - 14u);
            else
                for (b = 0; b < NB; b++)
                    if (panel.btn[b] == id)
                        *bm |= 1u << b;
        }
}

static void tab_pnl_service(void)                       /* main loop, every pass */
{
    const uint8_t *now[4] = {fm1_led, fm1_led_dim, fm1_led_breath, fm1_led_mid};
    uint32_t v[8], i, k, d;
    if (!TAB_SCR_LIVE()) {
        if (tpn.held) {
            static const uint8_t z[4];
            tab_pnl_msg(TH_BUTTONS, z, 2u);
            tab_pnl_msg(TH_KEYS, z, 4u);
        }
        return;
    }
    d = (uint32_t)TAB_DIM_LO();
    for (i = 0; i < 4u && !tpn.dirty; i++)
        tpn.dirty = memcmp(tpn.seen[i], now[i], FM1_NCOL) != 0 || tpn.seen[i][FM1_NCOL] != d;
    if (!tpn.dirty || !tab_open(TB_LEDS, 23u))
        return;
    for (i = 0; i < 4u; i++) {
        memcpy(tpn.seen[i], now[i], FM1_NCOL);
        tpn.seen[i][FM1_NCOL] = (uint8_t)d;
    }
    tab_pnl_led_masks(fm1_led, &v[0], &v[1]);
    v[0] |= ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u) << 14;
    tab_pnl_led_masks(fm1_led_dim, &v[2], &v[3]);
    v[4] = d;
    tab_pnl_led_masks(fm1_led_breath, &v[5], &v[6]);
    tab_pnl_led_masks(fm1_led_mid, &k, &v[7]);
    for (i = 0; i < 8u; i++)                            /* u16 u32 u16 u32 u8 u16 u32 u32 */
        for (k = 0; k < (i == 4u ? 1u : i == 0u || i == 2u || i == 5u ? 2u : 4u); k++)
            tab_byte(v[i] >> (8u * k));
    tab_close();
    tpn.dirty = 0;
}
