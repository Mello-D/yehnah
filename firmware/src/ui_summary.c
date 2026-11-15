/* SPDX-License-Identifier: GPL-3.0-only */
/* TRACKS (2026-11-15, the user's; HOME's second page, after DANCE: the old TRACKS page of meters and the road): one row a track,
 * what each holds: its sound and engine, the length, the key, the pattern written to it (a small roll of its steps, a kit's
 * three lanes) with the playhead on it, ARP / MUTE / REC, the level. K1 picks the track (the other pages then follow it).
 * The panel runs to the page row, drawn in two passes as SWARM's. Included after ui_swarm.c. */
static int summary_page_on(void)
{
    return !ui.home && !page_over && !ui.layer && !ui.menu && PAGES[ui.page].graph == GR_SUMMARY;
}
static void summary_knob(uint32_t slot, int32_t steps)
{
    uint32_t n = trk_live ? trk_live : NTRK;
    (void)slot;
    track_select((song.sel + n + (steps > 0 ? 1u : n - 1u)) % n);
    ui.force = 1;
}
/* a signature of what the rows show (a redraw only when it changes; the playhead moves it while playing) */
static uint32_t summary_sig(void)
{
    uint32_t c, i, h = 0x811C9DC5u;
    for (c = 0; c < NTRK; c++) {
        const track_t *t = &trk[c];
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        h = (h ^ (t->preset | (uint32_t)t->user << 8 | (uint32_t)t->eng_req << 16 | (uint32_t)(t->p[P_MUTE] != 0) << 24)) * 16777619u;
        h = (h ^ ((uint32_t)len | (uint32_t)(t->p[P_LEVEL] & 127) << 8 | (uint32_t)t->p[P_AMODE] << 16 | (uint32_t)t->p[P_ROOT] << 20 |
                  (uint32_t)t->p[P_SCALE] << 24)) * 16777619u;
        for (i = 0; i < len; i++) {
            const step_t *s = &t->step[i];
            h = (h ^ ((uint32_t)s->note[0] | (uint32_t)s->time << 8 | (uint32_t)s->n << 12 | (uint32_t)s->hit << 16)) * 16777619u;
        }
        if (song.playing)
            h = (h ^ (uint32_t)t->seq_idx) * 16777619u;
    }
    return (h ^ (uint32_t)song.sel << 3 ^ (uint32_t)song.rec << 8 ^ (uint32_t)trk_live << 20 ^ ux.gen ^ (uint32_t)settings.palette << 12) * 16777619u;
}
/* one track's pattern as a roll in the box x y w h: a note a 2 px bar at its pitch, a TIE its line carried on, a rest a dim dot;
 * a kit three lanes; the playhead a line */
static void summary_roll(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t col, int dim)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, j, drum = drum_track(t), any = 0;
    int32_t lo, hi, cw = w * 16 / (int32_t)len, py = 0;      /* (a step's width, x16) */
    uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu;
    uint16_t dc = ux_mix(T_SURF, T_MID, 90);
    cv_rect(x, y, w, h, ux_mix(T_SURF, T_BG, 110));
    pat_range(t, &lo, &hi);
    if (hi - lo < 11)                                       /* (an octave at least: a narrow tune does not fill the box) */
        lo -= (11 - (hi - lo)) / 2, hi = lo + 11;
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        int32_t sx = x + (int32_t)i * cw / 16, sw = (int32_t)(i + 1u) * cw / 16 - (int32_t)i * cw / 16;
        sw = sw > 2 ? sw - 1 : sw;
        if (drum) {
            for (j = 0; j < NLANE; j++)
                if (s->hit >> j & 1u) {
                    uint32_t band = j < 2u ? j : 2u;
                    cv_rect(sx, y + 1 + (int32_t)band * (h - 2) / 3, sw, (h - 2) / 3 > 1 ? (h - 2) / 3 - 1 : 1, dim ? dc : col);
                    any = 1;
                }
        } else if (s->time == ST_NOTE && s->n) {
            int32_t p = pr_note(s, 0);
            py = y + h - 3 - (p - lo) * (h - 4) / (hi - lo > 0 ? hi - lo : 1);
            py = clamp(py, y + 1, y + h - 3);
            cv_rect(sx, py, sw, 2, dim ? dc : col);
            any = 1;
        } else if (s->time == ST_TIE && py)
            cv_rect(sx, py, sw, 1, dim ? dc : ux_mix(T_SURF, col, 150));
        else if (!drum)
            cv_rect(sx, y + h - 2, 1, 1, dc);
    }
    if (!any)
        cv_text_in(x, y + h / 2 - 4, w, &AF_X, "EMPTY", T_DIM, ux_mix(T_SURF, T_BG, 110));
    if (ph < len)
        cv_rect(x + (int32_t)ph * cw / 16, y, 1, h, T_TEXT);
}
static void summary_graph(void)
{
    const int32_t gh = (int32_t)graph_h();
    uint32_t n = trk_live && trk_live <= NTRK ? trk_live : NTRK, c, k;
    int32_t rh = (gh - 5) / (int32_t)n;
    for (c = 0; c < n; c++) {
        const track_t *t = &trk[c];
        int32_t y = 3 + (int32_t)c * rh, ch = rh - 2, cy = y + (ch - 14) / 2, tx = 31;
        uint32_t sel = c == song.sel, mute = t->p[P_MUTE] != 0, arp = t->p[P_AMODE] != 0, armed = (song.rec >> c) & 1u, lvl = trk_level(c);
        uint16_t col = ux.k[c & 3u], fg = mute ? T_DIM : T_TEXT, fm = mute ? T_DIM : T_MID, bg = sel ? T_RAISE : ux_mix(T_SURF, T_BG, 60);
        char b[28];
        const engine_t *e = trk_engp(t);
        cv_rrect(7, y, 226, ch, 4, bg, T_SURF);
        if (sel)
            cv_frame(7, y, 226, ch, T_ACCENT);
        cv_rrect(11, cy, 14, 14, 3, mute ? ux_mix(T_BG, col, 90) : col, bg);
        b[0] = (char)('1' + c);
        b[1] = 0;
        cv_text_in(11, cy + 3, 14, &AF_X, b, T_BG, mute ? ux_mix(T_BG, col, 90) : col);
        sound_name(t, b);
        cv_clip_text(tx, y + 3, &AF_S, b[0] ? b : "-", fg, bg, 86);
        str_cpy(b, e->name, 10);                        /* the engine and the length */
        k = str_len(b);
        b[k] = ' ';
        fmt_int(b + k + 1u, (int32_t)clamp(t->p[P_SLEN], 1, NSTEP));
        str_cpy(b + str_len(b), " ST", 4);
        if (armed)                                      /* REC, or the ARP, after the length */
            str_cpy(b + str_len(b), " REC", 5);
        else if (arp)
            str_cpy(b + str_len(b), " ARP", 5);
        cv_clip_text(tx, y + ch - 11, &AF_X, b, fm, bg, 86);
        summary_roll(t, 120, y + 4, 66, ch - 8, col, (int)mute);
        if (mute)                                       /* MUTE, or the key (a kit has none), at the top right */
            cv_text_r(229, y + 3, &AF_X, "MUTE", T_DIM, bg);
        else if (!drum_track(t) && !e->oneshot) {
            str_cpy(b, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
            str_cpy(b + str_len(b), " ", 2);
            str_cpy(b + str_len(b), N_SCALE[(uint32_t)t->p[P_SCALE] % 8u], 5);
            cv_text_r(229, y + 3, &AF_X, b, fm, bg);
        }
        if (lvl) {
            if (LEVEL_DB_X10[lvl])
                fmt_fix(b, LEVEL_DB_X10[lvl], 1);
            else
                str_cpy(b, "0.0", 4);
            str_cpy(b + str_len(b), " dB", 5);
        } else
            str_cpy(b, "OFF", 5);
        cv_text_r(229, y + ch - 11, &AF_X, b, fm, bg);
    }
}
static void summary_draw(void)
{
    static uint32_t last_sig;
    uint32_t pass, h0 = graph_h() / 2u, sg = summary_sig();
    if (!ui.force && sg == last_sig)
        return;
    last_sig = sg;
    ui.graph_sig = 0;
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, pass ? graph_h() - h0 : h0, T_BG);
        cv_oy = pass ? -(int32_t)h0 : 0;
        cv_scroll = 1;
        cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);
        cv_bg = T_SURF;
        summary_graph();
        cv_scroll = 0;
        cv_oy = 0;
        cv_blit(0, graph_y() + pass * h0);
    }
}
