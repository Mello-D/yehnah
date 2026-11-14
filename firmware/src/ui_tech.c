/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * 0.5: the technical graphs of the pages that had none (they showed the scope): what the four knobs do, drawn
 * as an engineer would, live where the sound is (thermal colours: heat_col / heat_y).
 *   DIST       tk_dist_y: the drive TYPE's transfer (the FX lens draws it, ui_lzfx.c; 0.8.5: DIST, DLY, REVERB, CHORUS,
 *              MASTER and AUTOPAN live there, their old drawings gone)
 *   ENV DEST   the source now (the envelope's level) into four bipolar columns under the cards, each as long as its
 *              amount and lit by what it sends now (0.9.7: LFO DEST's and VOICE's, never drawn since the lenses, gone)
 *   GLOBAL     a bar of 16 steps with SWING moving the off-beats, the playhead, the beat */
#define TK_X0 PANEL_X0
#define TK_X1 (PANEL_X0 + PANEL_W)
#define TK_Y0 8
#define TK_Y1 112
/* the current page's knob c as 0..1000 (no knob: 500) */
static int32_t tk(uint32_t c)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), c, &vp);
    int32_t r = d && d->label && d->label[0] != '-' ? RATIO(d, enum_rank(d, *vp)) : -1;
    return r < 0 ? 500 : r;
}
/* .. as an amount -500..500: a bipolar knob about its middle, a unipolar one from 0 up */
static int32_t tk_amt(uint32_t c)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), c, &vp);
    return d && d->min >= 0 ? tk(c) / 2 : tk(c) - 500;
}

/* ENV DEST: the source now into four bipolar columns, one under each card (0.9.7: LFO DEST, VOICE and MSEQ 2 have
 * their lenses' pictures: ui_lzpage.c, ui_lzedit.c) */
static void tech_dest(const track_t *t)
{
    int32_t src = 0, c, mid = (TK_Y0 + TK_Y1) / 2, half = 44;
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].stage && t->v[i].stage < 4u && (t->v[i].env >> 9) > src)
            src = t->v[i].env >> 9;                     /* Q15 */
    for (c = 0; c < 4; c++) {
        int32_t x = CARD_X(c) + 22, amt = tk_amt((uint32_t)c), bh = amt * half / 500, live = bh * src / 32767;
        cv_rect(x - 6, mid, 25, 1, T_RAISE);
        cv_rect(x + 6, mid - half, 1, 2 * half, T_LINE);
        if (bh > 0) cv_rect(x, mid - bh, 13, bh, ux_mix(T_SURF, T_THEME, 35));
        else if (bh < 0) cv_rect(x, mid, 13, -bh, ux_mix(T_SURF, T_THEME, 35));
        if (live > 0) cv_rect(x, mid - live, 13, live, heat_col(80 + live * 170 / half));
        else if (live < 0) cv_rect(x, mid, 13, -live, heat_col(80 - live * 170 / half));
    }
    {   /* the source: its level now as a bar at the left edge */
        int32_t sh = (src < 0 ? -src : src) * half / 32767;
        cv_rect(TK_X0, src >= 0 ? mid - sh : mid, 3, sh ? sh : 1, T_TEXT);
    }
}

/* GLOBAL (0.5): the bar as 16 columns, one a sixteenth, in four beats; each as hot as the tempo (40 BPM cold ..
 * 240 white), the downbeats taller, the off-beats late by SWING, the one playing now white; under them CLK's three
 * sources (the one followed lit, its name over it) and TUNE as a bar from the middle (flat left, sharp right) */
static void tech_global(void)
{
    int32_t sw = song.g[G_SWING], bpm = song.g[G_BPM], tune = song.g[G_TUNE], i, x0 = TK_X0 + 6, w = PANEL_W - 12;
    int32_t step = w / 16, base = 60, hot = 40 + (bpm - 40) * 190 / 200, now = -1, cx, k;
    static const char *const CK[3] = {"INT", "USB", "TRS"};
    if (song.playing) {
        uint32_t bar = div_samples(2) * 16u;
        now = bar ? (int32_t)((ms_clock % bar) / div_samples(2)) : -1;
    }
    cv_rect(x0, base + 1, w, 1, T_RAISE);
    for (i = 0; i < 16; i++) {
        int32_t x = x0 + i * step + ((i & 1) ? sw * step / 200 : 0), hh = i % 4 == 0 ? 46 : i % 2 == 0 ? 30 : 20;
        uint16_t c = i == now ? T_TEXT : heat_col(hot - (i % 4 ? 30 : 0) + ((i & 1) ? sw / 2 : 0));
        cv_rect(x + 2, base - hh, step - 5, hh, c);
    }
    for (k = 0; k < 3; k++) {                           /* CLK: the sources */
        int32_t x = x0 + k * 40, on = song.g[G_CLOCK] == k;
        cv_rrect(x, 74, 34, 6, 3, on ? heat_col(200) : T_RAISE, T_SURF);
        cv_text_in(x, 82, 34, &AF_S, CK[k], on ? T_TEXT : T_DIM, T_SURF);
    }
    cx = x0 + 120 + (w - 120) / 2;                      /* TUNE: from the middle */
    cv_rect(x0 + 124, 76, w - 124, 2, T_RAISE);
    cv_rect(cx, 70, 1, 14, T_MID);
    if (tune)
        cv_rect(tune > 0 ? cx + 1 : cx + tune * ((w - 124) / 2) / 50, 74, (tune > 0 ? tune : -tune) * ((w - 124) / 2) / 50, 6,
                heat_col(60 + (tune > 0 ? tune : -tune) * 180 / 50));
}

/* the page's technical graph, if it has one (1) */
static int graph_tech(const track_t *t, const page_t *pg)
{
    if (pg->scope != SC_GLOBAL)
        switch (pg->id[0]) {
        case P_ED_FLT: tech_dest(t); return 1;
        default: return 0;
        }
    switch (pg->id[0]) {
    case G_BPM: tech_global(); return 1;
    default: return 0;
    }
}
