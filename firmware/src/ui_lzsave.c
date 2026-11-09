/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) SAVE's lens (ui.c save_*): the panels of its three levels, drawn as the approved HTML mock-up (SAVE, the
 * proposal "LENTE"), in the lens's panel (240 x 140 at y 84; the mock-up's coordinates less 84). Included by
 * ui_graph.c.
 *   LIBRARY  PROJECT: the eight projects as tiles (4 x 2, 56 x 66): the number, the name (its section A's), its four
 *            sections as chips A..D (stored: the section's colour; empty: dark), the project shown TINT with a THEME
 *            frame, its section picked framed TEXT. TOOLS: the six tools as rows (the one picked TINT).
 *   PROJECT  the project's four sections large (stored: their colour, the one picked full with a TEXT frame, the
 *            others half; the music's: NOW), ARRANGE and the song's rows as chips ("A16"; a scene: a dot; ARRANGE's
 *            row picked full and framed), the song as a strip (its rows by their bars, the one picked full; playing:
 *            a TEXT line where it is) and its length and END under it.
 *   SOUND    the track's engine picture (chord-harp fork: pixel art, was its being), moving with its sound, with the
 *            track and its sound under it; USER: the 32 slots (4 x 8; a stored one its engine's picture small and its name, the one
 *            picked TINT with a THEME frame); PRESETS: under a header, the sounds around the one loaded (TINT): their
 *            number, name, type, a favourite's star, their engine at the right (the full width, no picture).
 * The other projects' sections and names come from their flash headers (project.c lib_scan), one project a frame,
 * cached here until a project is written, loaded or renamed (lib_gen). */
/* a section's chip colour: stored, its colour (half: mixed with BG); empty: BG */
static uint16_t lz_sec(uint32_t s, int used, int half)
{
    uint16_t c = used ? sec_col(s) : half < 0 ? ux_mix(T_BG, T_RAISE, 50) : T_BG;
    return used && half ? ux_mix(T_BG, c, 50) : c;
}
#define LZ_TX(c) (4 + (int32_t)(c) * 117 / 2)          /* four columns of 56 px, 58 or 59 apart (the mock-up's 58.5) */
/* (2026-11-06) PROJECT: one project only: a banner with its name, four squares under it for its sections (the one
 * picked framed, NOW on the section playing) */
static void graph_lib(void)
{
    uint32_t i, k = (uint32_t)song.g[G_SLOT] - 1u;
    char b[16], nm[13];
    uint16_t bc = T_TINT;
    fmt_int(b, (int32_t)song_cur + 1);
    str_cpy(nm, graph_project_used(0) ? graph_project_name(0) : "", sizeof nm);
    if (!nm[0]) str_cpy(nm, graph_project_used(0) ? "USED" : "EMPTY", sizeof nm);
    cv_rect(4, 2, 232, 40, bc);
    GFX_HOOK_ALIGN(0, 6, 0, 17, AL_V | AL_PASS, "project banner number");
    cv_text_on(12, 7, &AF_X, "PROJECT", T_DIM, bc);
    cv_text_r(228, 6, &AF_M, b, T_THEME, bc);
    cv_free_text(12, 22, &AF_M, nm, graph_project_used(0) ? T_TEXT : T_DIM, bc, 200);
    for (i = 0; i < 4u; i++) {
        int32_t x = LZ_TX(i), y = 50;
        int used = graph_project_used(i), pick = i == (k & 3u);
        uint16_t c = lz_sec(i, used, used ? !pick : -1), ink = used ? T_INK : T_DIM;
        b[0] = (char)('A' + i); b[1] = 0;
        cv_rect(x, y, 56, 56, c);
        GFX_HOOK_ALIGN(0, y + 8, 0, y + 19, AL_V | AL_PASS, "project section letter");
        cv_text_on(x + 6, y + 4, &AF_M, b, ink, c);
        cv_free_text(x + 6, y + 30, &AF_X, !used ? "EMPTY" : "USED", ink, c, 47);
        if (live.cur == (int8_t)i && used)
            cv_text_r(x + 52, y + 42, &AF_X, "NOW", ink, c);
        if (pick) {
            cv_frame(x - 2, y - 2, 60, 60, T_TEXT);
            cv_frame(x - 1, y - 1, 58, 58, T_TEXT);
        }
    }
}
/* (2026-11-11, topic D) ARRANGE and SONG EDIT share one view: a lane per track, the song's bars left to right (SV_BARS on the screen,
 * scrolled to keep the picked part in view), each part in its section's colour with a gap between, the bars a track sits out of
 * dim. ARRANGE (trim 0): the picked part framed across every lane, the next INSERT a ghost frame at the end, a header strip with the
 * sections' letters. SONG EDIT (trim 1): only the picked part of the picked track framed. The song playing: a line where it is. */
#define SV_BARS 32
static uint32_t sv_scroll;                            /* the first bar on the screen */
static void graph_song_view(int trim)
{
    uint32_t i, q, n = chain_config.count, total = 0, nt = trk_live, r = ui.song_row, st = ui.sed_trk < nt ? ui.sed_trk : 0u;
    uint32_t pa = 0, pb = 0, acc = 0, end;
    int32_t h = 100 / (int32_t)(nt ? nt : 1u), top = 10, bw = 6;
    int ghost = !trim && n && r >= n && n < CHAIN_ROWS;
    char b[24];
    if (!n) {
        cv_text_in(4, 56, 232, &AF_X, trim ? "NO PARTS: ADD ONE IN ARRANGE" : "OCT+ INSERTS A SECTION", trim ? T_MID : T_THEME, T_SURF);
        return;
    }
    if (h > 18) h = 18;
    for (i = 0; i < n; i++) {
        if (i == r) pa = total;
        total += chain_config.row[i].bars;
    }
    if (r >= n) { r = n - 1u; pa = total - chain_config.row[r].bars; }
    pb = pa + chain_config.row[r].bars;
    if (ghost) { pa = total; pb = total + (uint32_t)arr_bars_get(); }
    if (pa < sv_scroll) sv_scroll = pa;
    else if (pb > sv_scroll + SV_BARS) sv_scroll = pb - pa > SV_BARS ? pa : pb - SV_BARS;
    if (sv_scroll + SV_BARS > total + (ghost ? arr_bars_get() : 0u) && total + (ghost ? arr_bars_get() : 0u) > SV_BARS)
        sv_scroll = total + (ghost ? arr_bars_get() : 0u) - SV_BARS;
    if (total + (ghost ? arr_bars_get() : 0u) <= SV_BARS) sv_scroll = 0;
    for (i = 0; i < SV_BARS / 4u + 1u; i++)            /* a tick every 4 bars */
        if ((sv_scroll + i * 4u) % 4u == 0u && sv_scroll + i * 4u <= total)
            cv_rect(20 + (int32_t)(i * 4u) * bw, top - 3, 1, 2, T_DIM);
    for (i = 0; i < n; i++) {
        uint32_t bars = chain_config.row[i].bars, a = acc, e = acc + bars;
        acc = e;
        if (e <= sv_scroll || a >= sv_scroll + SV_BARS) continue;
        if (!trim && a >= sv_scroll) {                   /* the section's letter over its first bar */
            b[0] = (char)('A' + chain_config.row[i].slot); b[1] = 0;
            cv_text_on(20 + (int32_t)(a - sv_scroll) * bw + 1, 0, &AF_X, b, i == r ? T_TEXT : T_DIM, T_SURF);
        }
    }
    for (q = 0; q < nt; q++) {
        int32_t y = top + (int32_t)q * h;
        acc = 0;
        if (h >= 10) {
            b[0] = 'T'; b[1] = (char)('1' + q); b[2] = 0;
            cv_text_on(4, y + CAP_IN(X, h - 2), &AF_X, b, trim && q == st ? T_TEXT : T_DIM, T_SURF);
        }
        for (i = 0; i < n; i++) {
            uint32_t bars = chain_config.row[i].bars, ta, tb, a = acc, e = acc + bars, va, ve;
            int pick = trim ? (q == st && i == r) : i == r && !ghost;
            uint16_t c = lz_sec(chain_config.row[i].slot, 1, !pick);
            int32_t x0, x1, k0, k1;
            acc = e;
            if (e <= sv_scroll || a >= sv_scroll + SV_BARS) continue;
            va = a < sv_scroll ? sv_scroll : a;
            ve = e > sv_scroll + SV_BARS ? sv_scroll + SV_BARS : e;
            x0 = 20 + (int32_t)(va - sv_scroll) * bw;
            x1 = 20 + (int32_t)(ve - sv_scroll) * bw - (ve == e ? 1 : 0);
            row_trims(&chain_config, song_cur, i, q, &ta, &tb);
            cv_rect(x0, y, x1 - x0, h - 2, ux_mix(T_BG, c, 22));                      /* the whole part, dim: what is cut */
            k0 = 20 + (int32_t)((a + ta > va ? a + ta : va) - sv_scroll) * bw;
            k1 = 20 + (int32_t)((e - tb < ve ? e - tb : ve) - sv_scroll) * bw - (ve == e && tb == 0u ? 1 : 0);
            if (k1 > k0) cv_rect(k0, y, k1 - k0, h - 2, c);
            if (pick && trim)
                cv_frame(x0 - 1, y - 1, x1 - x0 + 2, h, T_TEXT);
            if (chain.running && i == chain.row) {
                uint32_t at = a + bars - chain.remaining;
                if (at >= sv_scroll && at < sv_scroll + SV_BARS)
                    cv_rect(20 + (int32_t)(at - sv_scroll) * bw, y - 1, 1, h, T_ACCENT);
            }
        }
        if (ghost) {                                       /* the next INSERT, framed after the last part */
            int32_t x0 = 20 + (int32_t)(total - sv_scroll) * bw, w = (int32_t)arr_bars_get() * bw - 1;
            if (x0 + w > 236) w = 236 - x0;
            if (w > 0 && q < nt) cv_frame(x0, y, w, h - 2, T_THEME);
        }
    }
    {                                                      /* the whole song as a strip under the lanes, the window on it */
        int32_t oy = top + (int32_t)nt * h + 3, tot = (int32_t)total + (ghost ? (int32_t)arr_bars_get() : 0);
        acc = 0;
        for (i = 0; i < n; i++) {
            int32_t o0 = 20 + (int32_t)acc * 212 / tot, o1 = 20 + (int32_t)(acc + chain_config.row[i].bars) * 212 / tot;
            acc += chain_config.row[i].bars;
            cv_rect(o0, oy, o1 - o0 > 1 ? o1 - o0 - 1 : 1, 3, lz_sec(chain_config.row[i].slot, 1, i != r));
        }
        if (tot > SV_BARS)
            cv_frame(19 + (int32_t)sv_scroll * 212 / tot, oy - 1, SV_BARS * 212 / tot + 2, 5, T_TEXT);
    }
    if (!trim && !ghost) {                                 /* the picked part framed across every lane */
        uint32_t va = pa < sv_scroll ? sv_scroll : pa, ve = pb > sv_scroll + SV_BARS ? sv_scroll + SV_BARS : pb;
        cv_frame(19 + (int32_t)(va - sv_scroll) * bw, top - 1, (int32_t)(ve - va) * bw + 1, (int32_t)nt * h, T_TEXT);
    }
    if (trim) {                                            /* the picked cell in words: PART 2 · BARS 5-12 · CUT 2 / 1 */
        uint32_t k;
        str_cpy(b, "PART ", sizeof b);
        fmt_int(b + 5, (int32_t)r + 1);
        k = str_len(b);
        str_cpy(b + k, " \xB7 CUT ", 9);
        row_trims(&chain_config, song_cur, r, st, &pa, &pb);
        fmt_int(b + str_len(b), (int32_t)pa);
        k = str_len(b); b[k] = '/'; b[k + 1u] = 0;
        fmt_int(b + k + 1u, (int32_t)pb);
        cv_text_r(236, 128, &AF_X, b, T_DIM, T_SURF);
        return;
    }
    end = SONG_END(&chain_config);
    fmt_int(b, (int32_t)total);
    str_cpy(b + str_len(b), " BARS \xB7 ", 9);
    if (end == SONG_FOREVER) str_cpy(b + str_len(b), "LOOP", 6);
    else if (end >= 2u) { uint32_t k = str_len(b); b[k] = 'x'; fmt_int(b + k + 1u, (int32_t)end); }
    else str_cpy(b + str_len(b), "STOP", 6);
    GFX_HOOK_ALIGN(0, 128, 0, 137, AL_V | AL_PASS, "song structure length");
    cv_text_r(236, 128, &AF_X, b, r >= n ? T_THEME : T_DIM, T_SURF);
}
static void graph_song(void) { graph_song_view(0); }
static void graph_sedit(void) { graph_song_view(1); }
static void graph_lz_tools(void)
{
    static const char *const WHAT[4] = {"CLEAR SEQ", "DELETE ROW", "CLEAR SONG", "NEW PROJECT"};
    static const char *const ON[4] = {"STEPS + AUTO", "", "ALL ROWS", "EVERYTHING"};
    uint32_t c;
    char b[12];
    for (c = 0; c < 4u; c++) {
        int32_t y = 1 + (int32_t)c * 23;
        int sel = ui.act == c + 1u;
        uint16_t bg = sel ? T_TINT : T_RAISE;
        cv_rect(4, y, 232, 21, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 21, AL_V, "tools row text centred up/down");
        cv_text_on(12, y + CAP_IN(M, 21), &AF_M, WHAT[c], sel ? T_TEXT : T_MID, bg);
        if (c == 1u) {                                   /* (the row ARRANGE has picked) */
            str_cpy(b, "ROW ", sizeof b);
            fmt_int(b + 4, (int32_t)ui.song_row + 1);
        }
        GFX_HOOK_ALIGN(0, y, 0, y + 21, AL_V, "tools row text centred up/down");
        cv_text_r(230, y + CAP_IN(X, 21), &AF_X, c == 1u ? b : ON[c], sel ? T_MID : T_DIM, bg);
    }
}
/* PROJECT: the sections, the arrangement's chips, the song's strip (ar: ARRANGE, its rows picked) */
static void graph_proj(int ar)
{
    uint32_t i, total = 0, played = 0, sel = (uint32_t)song.g[G_SLOT] - 1u, n = chain_config.count, end;
    char b[24];
    for (i = 0; i < 4u; i++) {
        int32_t x = LZ_TX(i);
        int used = graph_project_used(i), pick = !ar && i == (sel & 3u);
        uint16_t c = lz_sec(i, used, used ? !pick : -1), ink = used ? T_INK : T_DIM;
        const char *nm = graph_project_name(i);
        b[0] = (char)('A' + i); b[1] = 0;
        cv_rect(x, 2, 56, 49, c);
        GFX_HOOK_ALIGN(0, 8, 0, 19, AL_V | AL_PASS, "project section letter");
        cv_text_on(x + 6, 4, &AF_M, b, ink, c);
        cv_free_text(x + 6, 25, &AF_X, !used ? "EMPTY" : nm[0] ? nm : "USED", ink, c, 47);
        if (live.cur == (int8_t)i && used)
            cv_text_r(x + 52, 38, &AF_X, "NOW", ink, c);
        if (pick) {                                      /* (2 px, round it) */
            cv_frame(x - 2, 0, 60, 53, T_TEXT);
            cv_frame(x - 1, 1, 58, 51, T_TEXT);
        }
    }
    GFX_HOOK_ALIGN(0, 58, 0, 67, AL_V | AL_PASS, "project arrange title");
    cv_text(6, 58, &AF_X, "ARRANGE", ar ? T_THEME : T_DIM);
    for (i = 0; i < n && i < CHAIN_ROWS; i++) {
        const chain_row_t *r = &chain_config.row[i];
        int32_t x = 4 + (int32_t)(i % 8u) * 293 / 10, y = 72 + (int32_t)(i / 8u) * 22;
        int pick = ar && i == ui.song_row, used = graph_project_used(r->slot);
        uint16_t c = lz_sec(r->slot, 1, !pick);
        b[0] = (char)('A' + (r->slot & 3u));
        fmt_int(b + 1, r->bars);
        cv_rect(x, y, 27, 20, c);
        GFX_HOOK_ALIGN(x, y, x + 27, y + 20, AL_HV, "arrange chip centred");
        cv_text_in(x, y + CAP_IN(X, 20), 27, &AF_X, b, used ? T_INK : T_DIM, c);
        if (pick)
            cv_frame(x - 1, y - 1, 29, 22, T_TEXT);
        if (song_idx.scene[song_cur][i].on)              /* a scene: a dot */
            cv_rect(x + 22, y + 3, 2, 2, T_TEXT);
        total += r->bars;
        if (chain.running && i < chain.row) played += r->bars;
    }
    if (ar && ui.song_row == n && n < CHAIN_ROWS) {      /* (2026-11-05) NEW: the row OCT+ would add, framed */
        int32_t x = 4 + (int32_t)(n % 8u) * 293 / 10, y = 72 + (int32_t)(n / 8u) * 22;
        uint16_t c = lz_sec(ui.arr_sect & 3u, 1, 0);
        b[0] = (char)('A' + (ui.arr_sect & 3u));
        fmt_int(b + 1, (int32_t)arr_bars_get());
        cv_rect(x, y, 27, 20, c);
        cv_text_in(x, y + CAP_IN(X, 20), 27, &AF_X, b, T_INK, c);
        cv_frame(x - 1, y - 1, 29, 22, T_TEXT);
    }
    if (chain.running && chain.row < n)
        played += chain_config.row[chain.row].bars - chain.remaining;
    if (total) {                                         /* the song: its rows by their bars */
        int32_t acc = 0;
        for (i = 0; i < n; i++) {
            int32_t x0 = 4 + acc * 232 / (int32_t)total, x1;
            acc += chain_config.row[i].bars;
            x1 = 4 + acc * 232 / (int32_t)total;
            cv_rect(x0, 118, x1 - x0 > 1 ? x1 - x0 - 1 : 1, 6, lz_sec(chain_config.row[i].slot, 1, !(ar && i == ui.song_row)));
            if (song_idx.scene[song_cur][i].on)
                cv_rect(x0 + 2, 120, 2, 2, T_TEXT);
        }
        if (chain.running)
            cv_rect(4 + (int32_t)(played * 232u / total), 116, 1, 10, T_TEXT);
    }
    end = SONG_END(&chain_config);
    fmt_int(b, (int32_t)total);
    str_cpy(b + str_len(b), " BARS \xB7 ", 9);
    if (end == SONG_FOREVER) str_cpy(b + str_len(b), "LOOP", 6);
    else if (end >= 2u) { uint32_t k = str_len(b); b[k] = 'x'; fmt_int(b + k + 1u, (int32_t)end); }
    else str_cpy(b + str_len(b), "STOP", 6);
    GFX_HOOK_ALIGN(0, 128, 0, 137, AL_V | AL_PASS, "project song length");
    cv_text_r(236, 128, &AF_X, n ? b : "NO ROWS: OCT+ ADDS ONE", ar && ui.song_row >= song_end_row() ? T_THEME : T_DIM,
              T_SURF);
}
/* SOUND: the track's engine drawn (chord-harp fork: its pixel art, ui_pixart.c; was Yehnah's being), alive with its
 * sound, the track and its sound under it; centred on cx, w px wide (USER 42 / 78; chord-harp fork: PRESETS 32 / 56, its
 * list wider) */
static void lz_track_art(int32_t cx, int32_t w)
{
    const track_t *t = TSEL;
    char b[20];
    (void)vx_draw(t, cx - w / 2, 4, w, 76, -1);
    b[0] = 'T'; b[1] = (char)('1' + song.sel % NTRK); b[2] = 0;
    GFX_HOOK_ALIGN(cx - w / 2, 0, cx + w / 2, 0, AL_H | AL_PASS, "sound track centred");
    cv_text_in(cx - w / 2, 85, w, &AF_X, b, T_THEME, T_SURF);
    sound_name(t, b);
    cv_free_text(cx - HALF_UP(text_w(&AF_X, b) < w - 2 ? text_w(&AF_X, b) : w - 2), 99, &AF_X, b, T_TEXT, T_SURF, w - 2);
}
static void graph_sound(void)
{
    uint32_t k;
    lz_track_art(42, 78);
    for (k = 0; k < UP_SLOTS; k++) {
        int32_t x = 88 + (int32_t)(k % 4u) * 37, y = 2 + (int32_t)(k / 4u) * 17;
        int sel = k == ui.uslot, used = up_used(k);
        uint16_t bg = sel ? T_TINT : T_RAISE;
        char b[13];
        cv_rect(x, y, 35, 15, bg);
        if (used) {
            (void)vx_draw_e(TSEL, up_engine(k), x + 1, y + 4, 10, 7, 0);   /* (chord-harp fork: its engine's picture, a
                                                         * thumbnail; was a tiny being) */
            up_name(k, b);
            cv_clip_text(x + 12, y + CAP_IN(X, 15), &AF_X, b, T_TEXT, bg, 22);
        } else {
            up_slot_label(b, k);
            GFX_HOOK_ALIGN(x, y, x + 35, y + 15, AL_HV, "sound slot label centred");
            cv_text_in(x, y + CAP_IN(X, 15), 35, &AF_X, b, T_DIM, bg);
        }
        if (sel)
            cv_frame(x, y, 35, 15, T_THEME);
    }
}
/* PRESETS (chord-harp fork): a row a sound, its number among all of them (the same whatever TYPE / LIST show), its
 * name, its type, a favourite's star, its engine; (2026-11-03, the user's) no engine picture: the columns the full
 * width under a header that stays as the list scrolls */
#define LP_HDR 11                                      /* the header's band; eight rows of 15 under it */
static void graph_lz_presets(void)
{
    uint32_t total, cur = preset_pos(&total), e, k, i, g, first;
    cv_rect(4, 0, 232, LP_HDR, T_RAISE);
    GFX_HOOK_ALIGN(0, 0, 0, LP_HDR, AL_V, "presets header centred up/down");
    cv_text_r(22, CAP_IN(X, LP_HDR), &AF_X, "#", T_MID, T_RAISE);
    cv_text_on(28, CAP_IN(X, LP_HDR), &AF_X, "NAME", T_MID, T_RAISE);
    cv_text_on(136, CAP_IN(X, LP_HDR), &AF_X, "TYPE", T_MID, T_RAISE);
    cv_text_r(232, CAP_IN(X, LP_HDR), &AF_X, "ENGINE", T_MID, T_RAISE);
    if (!total) {
        cv_text_in(4, LP_HDR + 52, 232, &AF_X, favorites.filter ? "NO FAVORITES" : "NO SOUNDS", T_MID, T_SURF);
        return;
    }
    first = cur >= total || cur < 3u ? 0u : cur - 3u;   /* (the sound loaded not in the list: its start) */
    if (total > 8u && first + 8u > total)
        first = total - 8u;
    for (i = first; i < total && i < first + 8u; i++) {
        int32_t y = LP_HDR + 2 + (int32_t)(i - first) * 15;
        int sel = i == cur;
        uint16_t bg = sel ? T_TINT : T_SURF;
        char nm[13], no[6];
        const char *en;
        e = preset_at_g(i, &k, &g);
        if (e == NENGINES) {
            up_name(k, nm);
            en = "USER";
        } else {
            str_cpy(nm, ENGINES[e]->presets[k].name, sizeof nm);
            en = ENGINES[e]->name;
        }
        if (sel)
            cv_rect(4, y, 232, 14, bg);
        fmt_int(no, (int32_t)g + 1);
        GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset row centred up/down");
        cv_text_r(22, y + CAP_IN(X, 14), &AF_X, no, sel ? T_MID : T_DIM, bg);
        GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset row centred up/down");
        cv_free_text(28, y + CAP_IN(S, 14), &AF_S, nm, sel ? T_TEXT : T_MID, bg, 104);
        if (e != NENGINES) {
            GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset row centred up/down");
            cv_text_on(136, y + CAP_IN(X, 14), &AF_X, PTYPE_NAME[preset_type(e, k)], sel ? T_MID : T_DIM, bg);
        }
        if (favorite_has(e, k)) {
            GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset star centred up/down");
            cv_text_on(180, y + CAP_IN(S, 14), &AF_S, "*", T_ACCENT, bg);   /* (a favourite; 0.8.5 no icons) */
        }
        GFX_HOOK_ALIGN(0, y, 0, y + 14, AL_V, "sound preset row centred up/down");
        cv_text_r(232, y + CAP_IN(X, 14), &AF_X, en, sel ? T_MID : T_DIM, bg);
    }
}
/* the lens's panel for the SAVE page shown */
static void graph_save_lz(const page_t *pg)
{
    if (pg->graph == GR_TOOLS) graph_lz_tools();
    else if (pg->graph == GR_SONG) graph_song();
    else if (pg->graph == GR_SEDIT) graph_sedit();
    else if (pg->graph == GR_ERASE) graph_proj(0);
    else if (pg->graph == GR_SLOTS) graph_lib();
    else if (pg->graph == GR_USER) graph_sound();
    else if (pg->graph == GR_BLEND) graph_blend();       /* (chord-harp fork) MORPH: TRACK A | B, the divider at the blend */
    else graph_lz_presets();
}
