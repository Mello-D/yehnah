/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) EDIT's lens panels (ui_lens.c), as the approved mock-up's; included by ui_graph.c. The canvas is the
 * lens's panel (240 x 140, x 4..236).
 *   SOUND   (EDIT 1 / EDIT 2 of a synth) the engine's picture (chord-harp fork: pixel art, alive with the sound) on the
 *           whole panel and nothing else (2026-11-10: the page name and the sound's name gone). DRUM's KIT keeps its colony
 *   FILTER  (2026-11-10, Yehnah's look: blocks, no words) the response of TYPE at CUT with RES (LP HP BP COMB), CUT a dotted
 *           column with a ball on it, DRIVE a row of blocks
 *   VOICE   the track's voices one lane each as full as its envelope now, GLIDE's curve under them (blocks, no words)
 * The others (MORPH, RETRIG, ROTOR, the kit's, the operators') keep their panels. */
#define ED_CH 17                                        /* a chip's height (two lines of AF_X) */
static void ed_cap(int32_t x, int32_t y, const char *s, uint16_t c)   /* a caption, its capitals from y */
{
    cv_text(x, y - AF_X_CAP_Y, &AF_X, s, c);
}
static void graph_sound_lz(const track_t *t)
{
    /* (2026-11-10, Yehnah's look) the engine's picture on the whole panel and nothing else: its page, the sound's name and
     * the four values are the header's and the knob cards' */
    (void)vx_draw(t, 4, 2, 232, (int32_t)cv_h - 4, -1);
}
/* FILTER: y of the response at x (px, 10..226), the cut at xc, res 0..1000; the unity line at y 62 */
static int32_t ed_resp(uint32_t ty, int32_t x, int32_t xc, int32_t res)
{
    int32_t d = x - xc, a = d < 0 ? -d : d, pk = res * 46 / 1000, y;
    int32_t bump = pk * 64 / (64 + d * d / 2);
    if (ty == 3u)                                       /* COMB: teeth CUT apart, as deep as RES */
        return 62 - (int32_t)((og_cos((uint32_t)x * (uint32_t)(4096 + xc * 160)) * (8 + pk / 2)) >> 15);
    y = ty == 0u ? (d > 0 ? 2 * d : 0) : ty == 1u ? (d < 0 ? 2 * a : 0) : 2 * a / 3;   /* LP HP BP */
    return clamp(62 + y - bump - (ty == 2u ? pk / 3 : 0), 8, 104);
}
/* (2026-11-10, Yehnah's look) FILTER as chunky blocks, as FX's pictures (4 px blocks, 58 x 35): the response filled in
 * the heat ramp with a dithered top, CUT a dotted column with a ball on the curve, DRIVE a row of blocks along the foot.
 * No words: TYPE, CUT, RES and DRIVE are the knob cards' */
static void graph_filter_lz(const track_t *t, const page_t *pg)
{
    int16_t *vp;
    const param_desc_t *d;
    int32_t r[4], c, cc, k, nd, y0 = 0;
    uint32_t ty = (uint32_t)clamp(t->p[P_FTYPE], 0, 3);
    for (k = 0; k < 4; k++) {
        d = page_desc(pg, (uint32_t)k, &vp);
        r[k] = d ? clamp(RATIO(d, enum_rank(d, *vp)), 0, 1000) : 0;
    }
    FP = 4;
    cc = r[1] * 57 / 1000;                              /* the cut's column */
    for (c = 0; c < 58; c++) {
        int32_t px = 10 + c * 216 / 57, ry = 3 + (ed_resp(ty, px, 10 + cc * 216 / 57, r[2]) - 8) * 26 / 96, row;
        for (row = ry; row < 31; row++) {
            int32_t dp = row - ry;
            if (dp < 2 && ((c + row) & 1))              /* (the top edge dithered) */
                continue;
            fxb(4, y0, c, row, 1, 1, dp < 2 ? FXP_H(255 - dp * 60) : ux_mix(T_SURF, T_THEME, 30 + (31 - row) * 3));
        }
        if (c && c != cc) {                             /* (a steep slope: the blocks joined) */
            int32_t pr = 3 + (ed_resp(ty, 10 + (c - 1) * 216 / 57, 10 + cc * 216 / 57, r[2]) - 8) * 26 / 96;
            int32_t a = pr < ry ? pr : ry, b = pr < ry ? ry : pr;
            if (b - a > 1)
                fxb(4, y0, c, a, 1, b - a, FXP_H(255));
        }
    }
    for (k = 1; k < 31; k += 2)                         /* the cut: a dotted column and a ball on the curve */
        fxb(4, y0, cc, k, 1, 1, T_ACCENT);
    fxb_ball(4, y0, cc - 2, 3 + (ed_resp(ty, 10 + cc * 216 / 57, 10 + cc * 216 / 57, r[2]) - 8) * 26 / 96 - 2, T_ACCENT);
    nd = r[3] * 30 / 1000;                              /* the drive: a row of blocks, the ramp along it */
    for (k = 0; k < 30; k++)
        fxb(4, y0, 14 + k, 32, 1, 2, k < nd ? FXP_H(60 + k * 7) : ux_mix(T_SURF, T_RAISE, 70));
}
/* (2026-11-10, Yehnah's look) VOICE: the eight voices as lanes of 4 px blocks, each as full as its envelope now (the ramp
 * along it), the ones the track does not use dotted; under them GLIDE's curve as a trace of blocks. No words (the
 * knob cards say VOICE, PRIO / ALLOC, GLIDE) */
static void graph_voice_lz(const track_t *t, const page_t *pg)
{
    uint32_t i, nv = trk_nvoice(t) ? trk_nvoice(t) : NVOICE;
    int32_t glide = t->p[P_GLIDE], py = 0, c, k;
    (void)pg;
    FP = 4;
    for (i = 0; i < NVOICE; i++) {
        int32_t row = 1 + (int32_t)i * 3, lv = 0;
        if (t->v[i].stage && t->v[i].stage < 4u)
            lv = (int32_t)(((int64_t)t->v[i].env * 56) >> 24);
        for (c = 0; c < 56; c++) {
            if (c < lv)
                fxb(4, 0, 1 + c, row, 1, 2, FXP_H(40 + c * 4));
            else if (i < nv)
                fxb(4, 0, 1 + c, row, 1, 2, ux_mix(T_SURF, T_RAISE, 85));
            else if (!((c + (int32_t)i) & 1))
                fxb(4, 0, 1 + c, row, 1, 1, T_LINE);
        }
    }
    for (c = 0; c < 56; c++) {                          /* the glide: low note to high, as slow as GLIDE */
        int32_t x = c * 104 / 55, tm = glide ? x * 127 * 6 / (glide * 4 + 1) : 1000, kk = tm > 255 ? 255 : tm;
        int32_t y = x < 16 ? 33 : 33 - 7 * (kk * (255 - (255 - kk) * (255 - kk) / 255)) / 65025;
        fxb_trace(8, 0, c, y, py, 1, FXP_H(200));
        py = y;
    }
    (void)k;
}
/* the lens's panel for EDIT's page pg (1), else 0 (its own) */
static int graph_edit_lz(const track_t *t, const page_t *pg)
{
    uint32_t e = trk_eng(t);
    if (pg->scope == SC_ENGINE && e != ENGI_DRUM && !(YEHNAH_FM4 && e == ENGI_DIGITAL))
        graph_sound_lz(t);
    else if (pg->id[0] == P_FTYPE)
        graph_filter_lz(t, pg);
    else if (pg->id[0] == P_VOICE || pg->id[0] == P_ALLOC)
        graph_voice_lz(t, pg);
    else
        return 0;
    return 1;
}
