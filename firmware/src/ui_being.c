/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.5) the beings' helpers that outlived them: a line's heat colour, a Q15 sine, the output's level now (HOME,
 * the lenses, the engines' pictures read them). (chord-harp fork, 2026-10-26) The beings themselves (their genomes,
 * isotherms, nuclei, graph_being, the DRUM-X colony) are gone: every engine has its pixel art (ui_pixart.c), drawn
 * wherever a being was (EDIT, HOME's ENGINE and ROUTES, SAVE, the USER slots, the DICE) */
/* a line's colour: heat v */
static uint16_t bg_col(int32_t v)
{
    return heat_col(v);
}
static int32_t bg_sin(uint32_t ang) { return sine_i(ang << 16); }   /* Q15 */
/* the output's level now, 0..256, slewed (fast up, slow down) */
static int32_t bg_energy(void)
{
    static int32_t e;
    static uint32_t fr = 0xFFFFFFFFu;
    uint32_t w = scope_w, i;
    int32_t pk = 0, v;
    if (fr == ui.frame)                                 /* (once a frame: a redraw in the same frame draws the same) */
        return e;
    fr = ui.frame;
    for (i = 1; i <= 256u; i += 2u) {
        v = scope_buf[(w - i) & (SCOPE_N - 1u)];
        if (v < 0) v = -v;
        if (v > pk) pk = v;
    }
    v = pk * 256 / 12000;
    if (v > 256) v = 256;
    e += v > e ? (v - e) * 3 / 4 : (v - e) / 6;
    return e;
}
