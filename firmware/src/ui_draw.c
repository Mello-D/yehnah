/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: the header, the four knob cards, the footer (steps + engine / preset / page)
 * and the frame; the panel between the cards and the footer is ui_graph.c. The layout:
 * flat SURF cards and panels on BG,
 * rounded corners, no rules (MENU > STYLE LINE: no SURF, 1 px rules between the areas instead: draw_rules), colours from the theme tokens only (gfx.c T_*). Type: S (12 px) labels,
 * M (15 px) values and the header, L (28 px) big numerals. (0.6.56: S Chakra Petch, M IBM Plex Mono, L Michroma 18 px;
 * the Yehnah palette's boxes square with HUD corners, gfx.c cv_rrect). A card value too wide for M is set in S;
 * free text (names, messages) is ellipsised at its size. */
static void draw_menu(void);
static int name_on(void);                              /* NAME (ui_name.c) */
static uint64_t lock_held(void);                       /* parameter locks: the steps held on STEP (ui_input.c) */
static void name_draw(void);

/* --------------------------------------------------------- drawing --- */
#define COL_W CARD_W                                  /* a card: 57 px wide at x 3 + 59 c */

static int32_t batt_level(void)                         /* thresholds 531 / 561 / 591 on ADC ch3 */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* A USB host powers us; there is no separate charger status line.
 * 4 means external power (a steady bolt inside the battery), otherwise its level. */
static int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 4;
    return batt_level();
}

#define HEAD_MID 11                                    /* (0.8.5) the header's middle row: its ink centred on it (the
                                                        * rule at 22) */
/* REC: filled, the selected track armed; outlined, another track armed. (0.8.5, the mock-up's skin) a dot, 9 px, its
 * ink centred on the header's row 11 in a 16 px cell at x (x + 3 .. x + 11) */
static void draw_rec_mark(int32_t x, uint16_t bg)
{
    if (!song.rec)
        return;
    int ring = !((song.rec >> song.sel) & 1u);
    cv_rrect(x + 3, HEAD_MID - 4, 9, 9, 4, T_REC, bg);
    if (ring)
        cv_rrect(x + 5, HEAD_MID - 2, 5, 5, 2, bg, T_REC);
}

/* (0.5) the load: how much of the audio's time the sound takes now (audio.c song.cpu_q8, averaged); 0..5.
 * (0.8.5, the mock-up) five bars 3 x 7 side by side left of the battery, lit THEME from the left, the rest RAISE */
/* (chord-harp fork, the home page's header) the CPU load as GLO prints it, "38%", held 0.5 s (it moves every block);
 * over 70 % in the accent */
static uint32_t head_cpu, head_cpu_ms;
static uint32_t head_load(void)
{
    if (!head_cpu_ms || fm1_ms - head_cpu_ms >= 500u) {
        head_cpu = song.cpu_q8 * 100u / 256u;
        head_cpu_ms = fm1_ms | 1u;
    }
    return head_cpu > 99u ? 99u : head_cpu;
}
static void draw_battery(int32_t bx)
{
    int32_t lvl = batt_shown();
    uint16_t c = lvl == 4 ? T_THEME : lvl == 1 ? T_ACCENT : T_MID;
    cv_frame(bx, HEAD_MID - 4, 13, 9, c);
    if (lvl)
        cv_rect(bx + 2, HEAD_MID - 2, lvl >= 3 ? 9 : lvl * 3, 5, c);
}

/* ---------------------------------------------------- rolling digits --- */
/* A number that changes rolls its changed digits like a slot machine; only the drawing: the value, the gauge,
 * the hot colour and the sound change at once. Up: the old digit leaves upward and the new one comes from below;
 * down: the reverse. ROLL_FRAMES UI frames (ui.frame, ~135 ms) on an integer ease-out (ROLL_EASE: cubic, over
 * half-way in a quarter of the time, no overshoot); the ones digit first, each place one frame later, all ending
 * together on a frame equal to the static render. Clipped to the value strip, which alone is redrawn while it
 * rolls (a card: rows ROLL_Y.., the header: the BPM's columns). Characters other than digits never roll.
 * A change during a roll retargets it: it restarts from the value it was going to. Shown at once (no roll):
 * a change within ROLL_SNAP frames of the last one (a fast turn reads better as plain numbers), another
 * shape (length, sign, a non-digit, a name or an enum, a value set in S), another label or unit, a track /
 * engine / palette change (ui.roll[].sig), ui.force (a page change), a card's first draw; MENU > ANIM OFF (#46). */
#define ROLL_FRAMES 9u
#define ROLL_SNAP 3u                                    /* frames (~45 ms) */
#define ROLL_BPM 4u                                     /* ui.roll[]: the four cards, then the header BPM */
#define ROLL_Y (lz_on() ? 14 : 18)        /* a card's value strip: rows 18..34, its figures (21..31) in the middle, */
#define ROLL_H 17                                       /* the label (MONO's chip: to row 17) above it, the gauge (39) below;
                                                         * (0.8.5) a lens page's card (38 tall): rows 14..30, the figures
                                                         * 17..27, the gauge at 33; 0.8.9 SKIN BOTTOM / HUD's
                                                         * knob row (33 tall): rows 11..27, the figures 14..24, the
                                                         * label 4..10, the gauge 28..29) */
#define CARD_VY (ROLL_Y - 1)                            /* the value's line (M) */
#define CARD_XY 4                                       /* (0.8.5) a lens card's label (AF_X): its line, capitals rows 5..11 */
#define CARD_BG T_RAISE   /* (0.8.5, the mock-up) a card: RAISE (LINE: no fill; 0.8.9 BOTTOM,
                                                         * HUD: none, the knob row on the background) */
#define KN_NAME_H 13                                    /* (2026-11-02) a knob cell's bands: its name's, the pixels' (12 rows
                                                         * of 2), its value's (the rest: 15 rows, a roll's strip) */
#define KN_FILL_H 24
static int kn_cells(void);
static void menu_val(uint32_t row, char *b);         /* (ui_menu.c: GLO's MENU rows) */
static const char *menu_short(uint32_t row);
static const uint8_t ROLL_EASE[17] = {0, 45, 84, 118, 147, 172, 193, 210, 223, 234, 242, 247, 251, 253, 254, 255, 255};

static int roll_digit(char c) { return c >= '0' && c <= '9'; }
/* +1 / -1: b rolls in from a (same length, digits where a has digits, the rest equal and one of + - .),
 * the sign of b - a; 0: b is shown at once */
static int roll_dir(const char *a, const char *b)
{
    uint32_t i;
    int d = 0, neg = 0;
    for (i = 0; a[i] || b[i]; i++) {
        if (i + 1u >= sizeof ui.roll[0].from || roll_digit(a[i]) != roll_digit(b[i]))
            return 0;
        if (!roll_digit(a[i]) && (a[i] != b[i] || (a[i] != '-' && a[i] != '+' && a[i] != '.')))
            return 0;
        neg |= a[i] == '-';
        if (!d && a[i] != b[i])
            d = b[i] > a[i] ? 1 : -1;
    }
    return neg ? -d : d;
}
/* field k now shows b instead of a: roll (a retarget mid-roll), or snap */
static void roll_note(uint32_t k, const char *a, const char *b, int snap)
{
    uint32_t el = (uint8_t)(ui.frame - ui.roll[k].t0);
    int d = roll_dir(a, b);
    ui.roll[k].t0 = (uint8_t)ui.frame;
    ui.roll[k].from[0] = 0;
    if (snap || !d || el < ROLL_SNAP || ANIM_SNAP())
        return;
    str_cpy(ui.roll[k].from, a, sizeof ui.roll[k].from);
    ui.roll[k].dir = (int8_t)d;
}
/* s (M) at x, y with field k's roll: each character at its pen in s (the digits are tabular and never kerned,
 * so the old value has the same pens); a rolling digit has moved o of the strip's h rows (ROLL_EASE, one frame
 * later per place from the right), clipped to the strip. The roll's last frame clears it: the static text. */
static int32_t roll_text(uint32_t k, int32_t x, int32_t y, const char *s, uint16_t fg)
{
    int cell = k < ROLL_BPM && kn_cells();               /* (2026-11-02) a knob cell's value: S, its band */
    const aafont_t *f = k == ROLL_BPM || cell ? &AF_S : &AF_M;   /* (0.8.5: the header's BPM in S; 0.8.9, the skins'
                                                         * mock-up: in M; chord-harp fork, 2026-10-26: in S again, as the
                                                         * sound's name) */
    uint32_t e = (uint8_t)(ui.frame - ui.roll[k].t0) + 1u, i, p = 0;
    const char *a = ui.roll[k].from;
    int32_t cy = cell ? KN_NAME_H + KN_FILL_H : k < ROLL_BPM ? ROLL_Y : 0,
            h = cell ? TF_KN_H - KN_NAME_H - KN_FILL_H : k < ROLL_BPM ? ROLL_H : HEAD_RULE_Y;   /* (above the header's rule) */
    uint16_t bg = k < ROLL_BPM && !cell ? CARD_BG : T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + cv_oy);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + cv_oy + h);
    cv_scroll = 1;                                      /* (the lint: rolling digits are cut on purpose) */
    for (i = 0; s[i]; i++) {
        int32_t px, ny = y;
        t[i] = 0;
        px = x + text_w(f, t);                          /* (its pen: no digit kerns) */
        t[i] = s[i];
        p -= (uint32_t)roll_digit(s[i]);
        if (s[i] != a[i]) {                             /* p: the places right of it */
            int32_t o = 0, dir = ui.roll[k].dir;
            if (e > p)                                  /* (e < ROLL_FRAMES: the LUT index stays in 1..16) */
                o = (int32_t)((ROLL_EASE[((e - p) * 32u / (ROLL_FRAMES - p) + 1u) >> 1] * (uint32_t)h + 128u) >> 8);
            ch[0] = a[i];
            cv_text_on(px, y - dir * o, f, ch, fg, bg);
            ny = y + dir * (h - o);
        }
        ch[0] = s[i];
        cv_text_on(px, ny, f, ch, fg, bg);
    }
    cv_cy0 = 0;
    cv_cy1 = (int16_t)cv_h;
    cv_scroll = 0;
    return x + text_w(f, s);
}

/* the header (y 0..24). (0.8.5) The approved mock-up's skin: one quiet line over a LINE rule at row 22,
 *   left    (0.8.9, the skins' mock-up: the 0.6 colours) the selected track's number in an ACCENT box (4..16; chord-harp
 *           fork: wider with the key mode's letter, C H O P, its cell gray when the sound does not take the chords; the
 *           play state's square / triangle gone, the keys' LEDs show it) and the BPM in S (THEME; the knob turning it: the
 *           accent), then BPM LOCK's lock and the REC mark (a dot: filled the selected track armed, a ring another)
 *   centre  what the page is of, centred on x 120 (MID): "T1 . ANALOG ACID" (the track, its engine and sound), on the
 *           song's pages (SAVE, ARRANGE, TOOLS, GLOBAL) "P1 NAME" (the project), on FX "T1" (the accent)
 *   right   the octave ("OCT +2"), or the song row while a song plays (the disc and its number), ending at 196; the load
 *           (five bars 200..218) and the battery (222..234)
 * A message or a layer's label replaces the centre and the octave, centred on x 120, up to 158 px; while the BPM is
 * being changed (a SELECT turn: bpm_t) or in the GLO layer the BPM stays and the message goes right of it (over the
 * right slot). Every element's ink centred on row 11 (HEAD_MID; ui_test.c test_head_centres): S's capitals rows 7..15 */
#define HEAD_SY (HEAD_MID - AF_S_CAP_H / 2 - AF_S_CAP_Y)
#define HEAD_XY (HEAD_MID - AF_X_CAP_H / 2 - AF_X_CAP_Y)   /* (0.8.5) the centre's words in AF_X: capitals rows 8..14 */
#define HEAD_MY CAP_IN(M, H_HEAD)                      /* (the menu's and the calibration's own headers: M) */
#define HEAD_BMY HEAD_SY                                /* (0.8.9) the BPM in M; chord-harp fork: in S, as the centre */
#define HEAD_TK_X 4                                    /* (0.8.9) the track's box: 4 .. 16, rows 5 .. 17 (with a key mode's
                                                        * letter: 4 .. 29, one shape, the letter in its right half) */
#define HEAD_TK_W 13
#define HEAD_TK_KW 26                                  /* (two equal halves: the number's, the letter's) */
#define HEAD_BAT_X 222                                 /* the battery's outline: 222 .. 234 */
#define HEAD_GRP_R 196                                 /* the octave / song row ends here */
#define HEAD_MSG_W 158                                 /* a message centred on x 120: 41..199 */
#define HEAD_GAP 5                                     /* ink to ink: the BPM -> the lock -> the REC mark */
static const char *key_mode_tag(const track_t *t);
/* the track's box, its width: HEAD_TK_KW with a key mode's letter (chord-harp fork) */
static int32_t head_tk_w(void) { return !ui.menu && key_mode_tag(TSEL) ? HEAD_TK_KW : HEAD_TK_W; }
/* the BPM's pen and ink (b), its rolling strip (sx0 .. sx1): placed by as many 0s as the BPM has digits (tabular
 * figures), so nothing moves while the tempo changes; right of the track's box (chord-harp fork: the play state gone) */
typedef struct { int32_t bx, b[4], sx0, sx1; } head_geo_t;
static void head_geo(head_geo_t *g, const char *bpm)
{
    char z[8] = "0000000";
    z[str_len(bpm) < 7u ? str_len(bpm) : 7u] = 0;
    g->bx = HEAD_TK_X + head_tk_w() + 6;
    text_ink(&AF_S, z, g->b);
    g->sx0 = g->bx - 1;
    g->sx1 = g->bx + g->b[2] + 2;
}
static void sound_name(const track_t *t, char *b);
/* the centre's words (b holds 40) and their colour */
static uint8_t head_sound;                              /* (chord-harp fork) head_title gave the sound's name */
static uint16_t head_title(char *b)
{
    head_sound = 0;
    const page_t *pg = cur_page();
    int glob = !ui.home && !ui.layer && (pg->graph == GR_SLOTS || pg->graph == GR_SONG || pg->graph == GR_TOOLS || pg->graph == GR_ERASE ||
                                         pg->fam == FAM_GLO || pg->fam == FAM_SAVE);
    if (ui.menu) {                                      /* ABOUT (GLO > TOOLS) */
        str_cpy(b, "ABOUT / CREDITS", 40);
        return T_MID;
    }
    b[0] = 'T';
    b[1] = (char)('1' + song.sel % NTRK);
    b[2] = 0;
    if (!ui.home && !ui.layer && pg->fam == FAM_FX)
        return T_ACCENT;
    if (glob) {
        char nm[16];
        b[0] = 'P';
        fmt_int(b + 1, (int32_t)song_cur + 1);
        if (graph_project_used(0) && graph_project_name(0)[0])   /* (0.8.5) the project's name: its section A's (as the */
            str_cpy(nm, graph_project_name(0), sizeof nm);        /* library's), else the slot's last loaded / saved */
        else
            project_cur_name(nm);
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), nm[0] ? nm : "NO NAME", 16);
        return T_MID;
    }
    head_sound = 1;                                     /* (the sound's name: the key mode's gray may go under it) */
    sound_name(TSEL, b);                                /* (chord-harp fork, the home page's header: the sound alone, the
                                                         * track the chip's; the engine: the footer) */
    if (!b[0])
        str_cpy(b, trk_engp(TSEL)->name, 40);
    return T_MID;
}
/* a message / a layer's label at x, or centred on x 120 (x < 0), at most maxw px with its
 * icon (ui.c MSG_NOFILE: the accent's) */
static void head_msg(int32_t x, int32_t maxw)
{
    const char *m = ui.msg_t ? ui.msg : layer_head();
    int icon = m[0] == MSG_NOFILE[0];
    int32_t iw = icon ? text_w(&AF_X, "!") + KH_GAP : 0;
    /* (0.8.9) a message or a layer's name ("[GLO] SET", "[FX] LATCH") in plain words, AF_X as the header's centre
     * (was S with a keycap, Felucca's): a message TEXT, a layer THEME */
    if (x < 0)
        x = 120 - HALF_UP(iw + (text_w(&AF_X, m + icon) < maxw - iw ? text_w(&AF_X, m + icon) : maxw - iw));
    if (icon) {                                       /* (0.8.5: no icons; "!" in the accent) */
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_XY, &AF_X, "!", T_ACCENT) + KH_GAP;
    }
    GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
    cv_free_text(x, HEAD_XY, &AF_X, m + icon, ui.msg_t ? T_TEXT : T_THEME, T_BG, maxw - iw);
}
/* the octave, or the song row while a song plays, its right end at HEAD_GRP_R; its left end */
static int32_t head_group(void)
{
    char b[8];
    int32_t x, x0;
    if (chain.running) {
        fmt_int(b, (int32_t)chain.row + 1);
        x0 = x = HEAD_GRP_R - (12 + 3 + text_w(&AF_S, b));
        cv_rrect(x + 1, HEAD_MID - 5, 10, 10, -5, T_MID, T_BG);   /* SONG: the disc (drawn, 0.8.5) */
        cv_rect(x + 5, HEAD_MID - 1, 2, 2, T_BG);
        x += 12 + 3;
    } else {
        str_cpy(b, song.octave > 0 ? "+" : "", sizeof b);
        fmt_int(b + str_len(b), song.octave);
        x0 = x = HEAD_GRP_R - (text_w(&AF_S, "OCT") + 4 + text_w(&AF_S, b));
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_SY, &AF_S, "OCT", T_MID) + 4;
    }
    GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
    cv_text(x, HEAD_SY, &AF_S, b, T_THEME);
    return x0;
}
#define HEAD_LOAD_X 219                                /* the load's right end (its "%" before the battery) */
static void draw_load(int32_t x)
{
    char b[6];
    uint32_t n = head_load();
    fmt_int(b, (int32_t)n);
    str_cpy(b + str_len(b), "%", 2);
    GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
    cv_text_r(x, HEAD_SY, &AF_S, b, n > 70u ? T_ACCENT : T_MID, T_BG);
}
/* (chord-harp fork, 2026-10-26) the selected track's key mode, for the header's badge: CHORD (a CHORD type), HARP, OMNI,
 * PROG; 0 off */
static const char *key_mode_tag(const track_t *t)
{
    int32_t m = t->p[P_CHRD];
    return m == CD_HARP ? "HARP" : m == CD_OMNI ? "OMNI" : m == CD_PROG ? "PROG" : m > 0 && m < CD_HARP ? "CHORD" : 0;
}
/* .. and whether the sound plays it whole: not a kit nor an engine's own key map (the mode does not apply there), not
 * one voice (ACID) nor MONO / LEGATO / UNISON (the chord keys play the root alone). Else the badge and the sound's name
 * in the header go gray, so browsing on HARP shows which sounds take the chords */
static int key_mode_full(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return !e->oneshot && !(e->keys && e->keys(t, 0) >= 0) && e->poly != 1u && voice_poly(t->p[P_VOICE]);
}
static void draw_head(void)
{
    char b[16], ttl[40];
    head_geo_t g;
    uint16_t tc = head_title(ttl);
    uint32_t rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u;   /* 2 the selected track armed, 1 another */
    uint32_t sig = (uint32_t)song.playing * 3u + rec * 5u + (uint32_t)(song.octave + 8) * 11u + song.sel * 13131u +
                   (ui.msg_t ? str_hash(7u, ui.msg) : ui.layer * 7919u) + (uint32_t)song.g[G_BPM] * 101u + (ui.bpm_t != 0) * 31u +
                   (uint32_t)seq_counting() * 7u +
                   (uint32_t)batt_shown() * 7777u + (chain.running ? (chain.row + 1u) * 104729u : 0u) +
                   head_load() * 0x9E3779B1u + str_hash(tc, ttl) * 3u +
                   (uint32_t)TSEL->p[P_CHRD] * 7907u + TSEL->eng_req * 613u + (uint32_t)TSEL->p[P_VOICE] * 37u;   /* (the
                                                         * key mode's letter, gray or not) */
    int msg = ui.msg_t || ui.layer;
    int centre = !msg || ui.bpm_t;   /* the BPM being changed: it stays, the message right */
    int32_t lx, r = HEAD_LOAD_X - 30;                   /* (clear of "99%") */
    if (song.g[G_BPM] != ui.roll_bpm) {
        char a[8];
        fmt_int(a, ui.roll_bpm);
        fmt_int(b, song.g[G_BPM]);
        roll_note(ROLL_BPM, a, b, ui.force);
        ui.roll_bpm = song.g[G_BPM];
    } else if (ui.force) {
        ui.roll[ROLL_BPM].from[0] = 0;
    }
    fmt_int(b, song.g[G_BPM]);
    head_geo(&g, b);
    if (!ui.force && sig == ui.head_sig) {
        if (ui.roll[ROLL_BPM].from[0]) {               /* rolling: the BPM's strip only (the rule under it again) */
            cv_begin((uint32_t)(g.sx1 - g.sx0), H_HEAD, T_BG);
            roll_text(ROLL_BPM, g.bx - g.sx0, HEAD_BMY, b, ui.bpm_t ? T_ACCENT : T_THEME);
            cv_rect(0, HEAD_RULE_Y, g.sx1 - g.sx0, 1, T_LINE);
            cv_blit((uint32_t)g.sx0, Y_HEAD);
        }
        return;
    }
    ui.head_sig = sig;
    lz_begin(240, H_HEAD, 0, Y_HEAD);
    if (!ui.menu) {                                     /* (0.8.9) the track's box, as the 0.6 had it */
        char n[2] = {(char)('1' + song.sel % NTRK), 0};
        const char *km = key_mode_tag(TSEL);
        if (km) {                                       /* (chord-harp fork) the key mode's letter in a cell of its own,
                                                         * gray when the sound does not take the chords */
            char l[2] = {km[0], 0};
            uint16_t lf = key_mode_full(TSEL) ? T_ACCENT : T_DIM;
            cv_rrect(HEAD_TK_X, HEAD_MID - 6, HEAD_TK_KW, 13, 3, lf, T_BG);
            cv_rrect(HEAD_TK_X, HEAD_MID - 6, HEAD_TK_W, 13, 3, T_ACCENT, lf);
            cv_rect(HEAD_TK_X + HEAD_TK_W - 3, HEAD_MID - 6, 3, 13, T_ACCENT);
            GFX_HOOK_ALIGN(HEAD_TK_X + HEAD_TK_W, HEAD_MID - 6, HEAD_TK_X + HEAD_TK_KW, HEAD_MID + 7, AL_HV,
                           "header key mode letter in its cell");
            cv_text_in(HEAD_TK_X + HEAD_TK_W, HEAD_XY, HEAD_TK_KW - HEAD_TK_W, &AF_X, l, T_INK, lf);
        } else {
            cv_rrect(HEAD_TK_X, HEAD_MID - 6, HEAD_TK_W, 13, 3, T_ACCENT, T_BG);
        }
        GFX_HOOK_ALIGN(HEAD_TK_X, HEAD_MID - 6, HEAD_TK_X + HEAD_TK_W, HEAD_MID + 7, AL_HV, "header track number in its box");
        cv_text_in(HEAD_TK_X, HEAD_XY, HEAD_TK_W, &AF_X, n, T_INK, T_ACCENT);
    }
    if (!ui.roll[ROLL_BPM].from[0])
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
    roll_text(ROLL_BPM, g.bx, HEAD_BMY, b, ui.bpm_t ? T_ACCENT : T_THEME);
    lx = g.bx + g.b[2];                                /* the right end of the BPM's ink (and what follows it) */
    if (song.rec) {                                    /* REC: its dot (x + 3 .. x + 11 of its cell) */
        draw_rec_mark(lx + HEAD_GAP - 3, T_BG);
        lx += HEAD_GAP + 9;
    }
    if (!msg && (chain.running || song.octave))
        r = head_group() - 6;
    if (!msg) {                                        /* the centre's words on x 120, between the BPM and the right */
        int32_t w = text_w(&AF_S, ttl), x = 120 - HALF_UP(w), l = lx + 8;
        if (x + w > r) x = r - w;
        if (x < l) x = l;
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
        if (head_sound && !ui.menu && key_mode_tag(TSEL) && !key_mode_full(TSEL)) {   /* (chord-harp fork) the sound's
                                                         * name on gray: it does not take the key mode's chords */
            int32_t bw = (w < r - x ? w : r - x) + 8;
            cv_rrect(x - 4, HEAD_MID - 7, bw, 15, 4, T_RAISE, T_BG);
            cv_free_text(x, HEAD_SY, &AF_S, ttl, T_MID, T_RAISE, r - x);
        } else
            cv_free_text(x, HEAD_SY, &AF_S, ttl, tc, T_BG, r - x);   /* (chord-harp fork: AF_S, legible; was 0.8.5's AF_X) */
    } else if (centre) {                               /* right of the BPM, over the right slot */
        int32_t x = lx + 9 > 142 ? lx + 9 : 142;
        head_msg(x, 236 - x);
    } else {                                           /* centred on x 120, clear of the BPM and the REC mark */
        head_msg(-1, lx + 8 > 120 - HEAD_MSG_W / 2 ? 2 * (120 - (lx + 8)) : HEAD_MSG_W);
    }
    if (!(msg && centre)) {
        draw_battery(HEAD_BAT_X);
        draw_load(HEAD_LOAD_X);
    }
    cv_rect(0, HEAD_RULE_Y, 240, 1, T_LINE);            /* the rule */
    GFX_HOOK_RULE(0, HEAD_RULE_Y, 240, 1);
    lz_blit(0, Y_HEAD);
}
/* LINE: 1 px dividers in the BG gaps between the strips, not around them: under the header, above and
 * below the panel, and between the four columns over rows y .. y + h (the cards; the mixer's strips too) */
/* full redraw: the strips (header, panel, knob cells, page row) cover the rest; only the BG between them is filled.
 * (2026-11-02) every page in TRACKS' order (kn_cells) */
static void draw_frame(void)
{
    lcd_clear(0, TF_SC_Y + TF_SC_H, 240, TF_KN_Y - TF_SC_Y - TF_SC_H, T_BG);
    lcd_clear(0, TF_KN_Y + TF_KN_H, 240, TF_PG_Y - TF_KN_Y - TF_KN_H, T_BG);
}

/* MOTION on the cards (#63: values that move on their own): the selected track's motion-driven parameters
 * (motion.c motion_mask: PLAY ON and an event for the id), once per draw_columns; card_mot_next is set just
 * before a draw_column of a track parameter (card_mot_of) and taken by it; card_mot: the cards drawn with it. */
static uint32_t card_mot_mask[(P_COUNT + 31u) / 32u];
static uint8_t card_mot_next, card_mot;
static void card_mot_of(const int16_t *vp)
{
    uintptr_t id = ((uintptr_t)vp - (uintptr_t)TSEL->p) / sizeof *vp;   /* (not the track's: huge) */
    card_mot_next = (uint8_t)(id < P_COUNT && ((card_mot_mask[id / 32u] >> (id % 32u)) & 1u));
}

/* a card's gauge and value: (0.8.5, the mock-up) the gauge THEME, the value TEXT; only the knob just turned (T_ACCENT)
 * stands out */
static uint8_t card_bip;                                /* the card being drawn: a bipolar parameter (ratio_of) */
static uint16_t gauge_col(int32_t ratio) { (void)ratio; return T_THEME; }
static uint16_t value_col(uint16_t vc, int32_t ratio)
{
    (void)ratio;
    return vc != T_THEME ? vc : T_TEXT;
}
static uint8_t card_free_next;                          /* (0.8.5) the next draw_column's value is a name: free text */
static uint8_t card_arm_next;                           /* (0.8.5) the next draw_column is an action armed (OCT+ does it):
                                                         * the card TINT, its label THEME, as the mock-up's */
/* one card = one knob: [icon] LABEL / value unit / gauge on a SURF card, redrawn only when it changed.
 * The value is M, or S when M is too wide (engine names, long ENUMs); the unit S after it. The gauge is
 * a 3 px rounded bar: BG track, THEME fill. The knob just turned (hot): icon, label, value and gauge in
 * the accent. ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label;
 * a label too long to share the card with its icon goes without it. A parameter MOTION drives (card_mot_next):
 * the motion icon in place of its own, in the theme colour; a long label moves it left, up to the card's edge;
 * a label with no room for it at all (or YEHNAH_ICONS=0): the label in the theme colour.
 * vc: the value's colour (T_THEME, T_DIM inactive, T_ACCENT the knob just turned) */
/* (0.8.5) a held layer's cards as drawn (ui_layer.c: its KNOBS level draws them as gauges) */
/* (0.8.9, the skins' mock-up: the 0.6 cards' marks) a 7 x 7 mark before a card's label, by the label (a row a byte, bit 6
 * the left column); none for the others (or a label too long to sit after one) */
static const uint8_t CARD_ICO[14][7] = {
    {0x70, 0x08, 0x04, 0x04, 0x02, 0x02, 0x01}, {0x08, 0x14, 0x14, 0x62, 0x02, 0x01, 0x01},   /* cut res */
    {0x08, 0x14, 0x23, 0x20, 0x40, 0x40, 0x40}, {0x70, 0x08, 0x04, 0x04, 0x02, 0x01, 0x01},   /* atk rel */
    {0x04, 0x0C, 0x14, 0x24, 0x44, 0x04, 0x04}, {0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41},   /* wave detune */
    {0x22, 0x22, 0x72, 0x27, 0x22, 0x22, 0x22}, {0x20, 0x2A, 0x6B, 0x7F, 0x5D, 0x09, 0x08},   /* mix noise */
    {0x7F, 0x41, 0x5D, 0x5D, 0x41, 0x7F, 0x00}, {0x0C, 0x0A, 0x09, 0x08, 0x38, 0x78, 0x30},   /* step note */
    {0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, {0x08, 0x1C, 0x3E, 0x08, 0x08, 0x08, 0x08},   /* type accent */
    {0x77, 0x41, 0x41, 0x00, 0x41, 0x41, 0x77}, {0x08, 0x08, 0x08, 0x49, 0x2A, 0x1C, 0x08}};  /* size depth */
static const struct { char l[7]; uint8_t i; } CARD_ICO_OF[] = {
    {"CUT", 0}, {"FREQ", 0}, {"RES", 1}, {"PEAK", 1}, {"Q", 1}, {"ATK", 2}, {"REL", 3}, {"DEC", 3}, {"DECAY", 3},
    {"WAVE", 4}, {"WAVE2", 4}, {"SHAPE", 4}, {"DETUNE", 5}, {"MIX", 6}, {"LEVEL", 6}, {"NOISE", 7}, {"CRUSH", 7},
    {"STEP", 8}, {"LEN", 8}, {"NOTE", 9}, {"PITCH", 9}, {"TUNE", 9}, {"ROOT", 9}, {"TYPE", 10}, {"MODE", 10},
    {"TIME", 10}, {"ACCENT", 11}, {"FLAG", 11}, {"SIZE", 12}, {"DEPTH", 13}, {"AMT", 13}};
static int32_t card_ico(const char *label)            /* its mark, -1 none */
{
    uint32_t k;
    for (k = 0; k < NELEM(CARD_ICO_OF); k++)
        if (str_eq(CARD_ICO_OF[k].l, label))
            return CARD_ICO_OF[k].i;
    return -1;
}
static void card_ico_draw(int32_t x, int32_t y, uint32_t i, uint16_t c)
{
    uint32_t r, b;
    for (r = 0; r < 7u; r++)
        for (b = 0; b < 7u; b++)
            if ((CARD_ICO[i][r] >> (6u - b)) & 1u)
                cv_pset(x + (int32_t)b, y + (int32_t)r, c);
}
static struct { int16_t r; uint8_t b; char l[12], v[16]; } ly_card[4];   /* (b: bipolar) */
/* (0.9.1) GLO > MIXER KNOB 4, the DJ FILTER (fx.c djf_block): its value as "OFF", or the cutoff ("800", "1.2k", "12k")
 * with its side for unit ("LP" / "HP"); its card draws the response (djf_curve) where the gauge would be */
#define DJF_NOCURVE 0x7FFF
static int16_t card_curve_next __attribute__((section(".bss.djf")));   /* the next draw_column's card: the curve of G_DJF
                                                         * value card_curve_next - 256 (0: none; its own section: fx.c djf) */
static void djf_format(int32_t v, char *val, const char **unit)
{
    int32_t c = djf_cut(v), i = c >> 8, h;
    if (!v) {
        str_cpy(val, "OFF", 8);
        *unit = "";
        return;
    }
    h = CUTOFF_HZ[i] + (i < 127 ? ((CUTOFF_HZ[i + 1] - CUTOFF_HZ[i]) * (c & 255)) >> 8 : 0);
    if (h < 1000)
        fmt_int(val, h);
    else if (h < 10000)
        fmt_fix(val, h / 100, 1);
    else
        fmt_int(val, h / 1000);
    if (h >= 1000)
        str_cpy(val + str_len(val), "k", 2);
    *unit = v < 0 ? "LP" : "HP";
}
/* the response of G_DJF value v drawn w px wide from (x, y): rows rows (5 or 6), 0 dB on the second (the bump above it),
 * 12 dB / oct (2 px an octave of CUTOFF_HZ's scale) down to the last row; flat at 0 dB when off. The unity line under it
 * in tc */
static void djf_curve(int32_t x, int32_t y, int32_t w, int32_t rows, int32_t v, uint16_t vc, uint16_t tc)
{
    int32_t c = v ? djf_cut(v) : 0, py = y + 1, i;
    int32_t bump = v && djf_k(c, v > 0) < (DJF_KMAX + DJF_KMIN) / 2;   /* (Q past ~0.9: the peak a row up) */
    cv_rect(x, y + 1, w, 1, tc);
    for (i = 0; i < w; i++) {
        int32_t f = i * (127 << 8) / (w - 1), d = (v < 0 ? f - c : c - f) / 256, cy;   /* (octaves below / above: 14 idx) */
        cy = !v || d < -3 ? y + 1 : d > 0 ? y + 1 + clamp(d / 7, 0, rows - 2) : y + 1 - (bump && d >= -3);
        if (i)
            cv_rect(x + i, cy < py ? cy : py, 1, (cy < py ? py - cy : cy - py) + 1, vc);
        else
            cv_rect(x, cy, 1, 1, vc);
        py = cy;
    }
}
/* (2026-11-02) a knob cell's texts, its gauge (r: -1 none), MOTION moving it (m: its value THEME), dim (d) */
typedef struct { char l[12], u[8], v[12]; int32_t r; uint8_t m, d, n, pn, pi; } kn_t;   /* (n: a name, or a keycap: never
                                                         * rolls; pn: a pick of pn choices, pi the one chosen) */
static kn_t kn_buf[4];                                  /* (draw_column fills them on a kn_cells page) */
/* (2026-11-02, the user's) a knob of 2..32 named choices (an enum, a note) is a pick: its cell's pixels a grid (2: two
 * halves; 3..16: 4 across, 8: 4 x 2, 12: 4 x 3; 17..32: 8 across), the one chosen lit, the places past the last blank. gauge(): the ratio as before, the
 * pick noted for draw_column's next cell */
static uint8_t kn_pn, kn_pi;
static int32_t gauge(const param_desc_t *d, int32_t v)
{
    int32_t r = enum_rank(d, v);
    kn_pn = (uint8_t)param_pick(d);                     /* (2026-11-03: and a count of 2..16: OCT, SLOT, LOOP, PAT, LEN) */
    kn_pi = (uint8_t)(r - d->min);
    return RATIO(d, r);
}
static const char *sets_label(uint32_t id);               /* (ui_menu.c) */
static int32_t pick_of(uint32_t n, uint32_t i)          /* (2026-11-02, the user's: SAVE's SECT, PROJECT) a pick of n, i chosen */
{
    kn_pn = (uint8_t)(n >= 2u && n <= 32u ? n : 0u), kn_pi = (uint8_t)i;
    return (int32_t)(i * 1000u / (n > 1u ? n - 1u : 1u));
}
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                        int32_t ratio, uint32_t icon)
{
    card_bip = (uint8_t)(ratio >= 0 && ratio == ratio_last.r && ratio_last.bip);   /* (its ratio from RATIO: bipolar?) */
    ratio_last.bip = 0;
    if (ui.layer && c < 4u) {
        ly_card[c].r = (int16_t)ratio;
        ly_card[c].b = card_bip;
        str_cpy(ly_card[c].l, label, sizeof ly_card[c].l);
        str_cpy(ly_card[c].v, val, sizeof ly_card[c].v);
        str_cpy(ly_card[c].v + str_len(ly_card[c].v), unit, sizeof ly_card[c].v - str_len(ly_card[c].v));
    }
    char key[56];
    int hot = c == ui.hot_col && ui.hot_t, named = fmt_named, mot = card_mot_next && label[0], strip, snap;
    int arm = card_arm_next, free = card_free_next;
    int32_t curve = card_curve_next ? card_curve_next - 256 : DJF_NOCURVE;
    card_curve_next = 0;
    uint8_t sig;
    uint16_t lc = hot ? T_ACCENT : arm || mot ? T_THEME : T_MID, cb = arm ? T_TINT : CARD_BG, gk = 0;
    int32_t x, lx = 5, uw, room = COL_W - 8, ch = (int32_t)card_h(), ry = ROLL_Y;
    const aafont_t *vf = &AF_M;
    uint32_t n, kn;
    int32_t kid = kc_tag(val, &kn);
    /* (0.8.9, the skins: the 0.6 colours) a lens card's value and gauge in its knob's colour (the knob turned: its card
     * framed in it, COLORS; its label TEXT, BOTTOM / HUD); the label TEXT on COLORS' cards, MID in the knob row */
    if (lz_on()) {
        if (kid < 0 && (vc == T_THEME || vc == T_ACCENT)) {
            gk = vc = T_K(c);                           /* (the gauge's, the frame's) */
        }
        lc = arm || mot ? T_THEME : T_TEXT;
    }
    if (!gk)
        vc = value_col(vc, ratio);
    fmt_named = 0;                                      /* (params.c: a name, for this card only) */
    card_mot_next = 0;                                  /* (the same: MOTION, this card only) */
    card_arm_next = card_free_next = 0;
    card_mot = (uint8_t)((card_mot & ~(1u << c)) | (uint32_t)mot << c);
    if (str_eq(unit, label))
        unit = "";                                      /* "BPM 124 BPM", "USB OFF USB": the label says it */
    if (kn_cells() && c < 4u) {                         /* (2026-11-02, step 3) a knob cell: kn_row draws the four */
        kn_t *k = &kn_buf[c];
        str_cpy(k->l, label, sizeof k->l);
        str_cpy(k->v, val, sizeof k->v);
        str_cpy(k->u, unit, sizeof k->u);
        k->r = ratio;
        k->m = (uint8_t)mot;
        k->d = vc == T_DIM;
        k->n = (uint8_t)(named || kid >= 0);
        k->pn = ratio < 0 ? 0u : kn_pn, k->pi = kn_pi, kn_pn = 0;
        return;
    }
    (void)icon;                                         /* (0.8.5, the mock-up: no icons on the cards) */
    str_cpy(key, label, 12);                            /* cache key: texts + colour + gauge */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), val, 14);
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), unit, 8);
    n = str_len(key);
    /* every RGB565 bit: status colours can change with identical text */
    key[n] = (char)('A' + (vc & 15u));
    key[n + 1] = (char)('A' + ((vc >> 4) & 15u));
    key[n + 2] = (char)('A' + ((vc >> 8) & 15u));
    key[n + 3] = (char)('A' + (vc >> 12));
    key[n + 4] = (char)(' ' + (ratio < 0 ? 0 : 1 + clamp(ratio, 0, 1000) * (COL_W - 10) / 1000));   /* (the gauge's px) */
    key[n + 5] = (char)('!' + arm + 2 * (int)lz_on());
    key[n + 6] = (char)('0' + hot + 2 * mot);
    {   /* (0.6.10) the gauge's colour too */
        uint16_t gc = ratio < 0 ? 0u : gk ? gk : gauge_col(ratio);
        key[n + 7] = (char)('A' + (gc & 15u));
        key[n + 8] = (char)('A' + ((gc >> 4) & 15u));
        key[n + 9] = (char)('A' + ((gc >> 8) & 15u));
        key[n + 10] = (char)('A' + (gc >> 12));
    }
    key[n + 11] = 'A';
    key[n + 12] = (char)(curve == DJF_NOCURVE ? '#' : 0x80 | ((curve + 64) & 127));   /* (0.9.1) the DJ FILTER's curve */
    key[n + 13] = 0;
    uw = unit[0] ? text_w(&AF_S, unit) + 3 : 0;
    if (text_w(vf, val) + uw > room)
        vf = &AF_S;
    sig = (uint8_t)str_hash(str_hash(song.sel + TSEL->eng_req * 4u + ux.gen * 64u + lz_on() * 16u, label), unit);
    snap = ui.force || sig != ui.roll[c].sig;           /* what the value is of: label, unit, track, engine, palette */
    strip = !snap && str_eq(key, ui.col[c]);
    if (strip && !ui.roll[c].from[0])
        return;
    if (strip) {                                        /* rolling: the value strip only */
        cv_begin(COL_W, ROLL_H, cb);
        cv_oy = -ry;
        if (lz_on() && hot && vc != T_DIM) {    /* (0.8.9 COLORS: the hot card's frame, its sides) */
            uint16_t fc = ux_mix(cb, gk ? gk : T_K(c), 70);
            cv_rect(0, ry, 1, ROLL_H, fc);
            cv_rect(COL_W - 1, ry, 1, ROLL_H, fc);
        }
    } else {
        char ov[16];                                    /* the value drawn before (in the cache key) */
        const char *k = ui.col[c];
        uint32_t i = 0;
        while (*k && *k != '|')
            k++;
        while (*k && k[1] && k[1] != '|' && i + 1u < sizeof ov)
            ov[i++] = *++k;
        ov[i] = 0;
        ui.roll[c].sig = sig;
        if (!str_eq(ov, val))
            roll_note(c, ov, val, snap || named || vf != &AF_M || kid >= 0);
        else if (snap)
            ui.roll[c].from[0] = 0;
        str_cpy(ui.col[c], key, sizeof ui.col[c]);
        {
            cv_begin(COL_W, (uint32_t)ch, T_BG);
            if (!lz_on())
                cv_rect(0, 0, COL_W, ch, cb);           /* (0.8.5, the mock-up: a square box) */
            else if (hot && vc != T_DIM) {              /* (0.8.9) COLORS: a rounded card, the knob turned framed in its colour */
                uint16_t fc = ux_mix(cb, gk ? gk : T_K(c), 70);
                cv_rrect(0, 0, COL_W, ch, 5, fc, T_BG);
                cv_rrect(1, 1, COL_W - 2, ch - 2, 4, cb, fc);
            } else
                cv_rrect(0, 0, COL_W, ch, 5, cb, T_BG);
        }
    }
    if (label[0] || val[0]) {
        cv_scroll = (uint8_t)strip;                     /* (0.9.0) rolling: the label again where it reaches into the value's
                                                         * strip (MONO's chip), clipped to it */
        if (label[0]) {
            /* the label (S, MID) at the top left, its capitals rows 4..12 (a lens page: AF_X). A parameter MOTION
             * drives, an armed action: THEME */
            const aafont_t *lf = lz_on() ? &AF_X : &AF_S;
            int32_t ly = lf == &AF_M ? 0 : lf == &AF_X ? CARD_XY : 1;   /* (0.8.5 a lens card: AF_X, capitals rows 5..11;
                                                         * 0.8.9 the knob row: 4..10) */
            int32_t cy = ly + (lf == &AF_M ? AF_M_CAP_Y : lf == &AF_X ? AF_X_CAP_Y : AF_S_CAP_Y);
            {
                int32_t ic = lf == &AF_X && lz_on() &&
                             (ui.home || ui.menu || cur_page()->graph != GR_BROWSE) ? card_ico(label) : -1;   /* (chord-harp
                                                         * fork: none on PRESETS, its TYPE read "T TYPE") */
                if (ic >= 0 && 10 + text_w(lf, label) <= COL_W - 2 - 5) {   /* (0.8.9) its mark, then the label */
                    card_ico_draw(lx, cy, (uint32_t)ic, lc);
                    cv_text_fit(lx + 10, ly, lf, label, lc, cb, COL_W - 2 - 15);
                } else
                    cv_text_fit(lx, ly, lf, label, lc, cb, COL_W - 2 - 5);
            }
        }
        cv_scroll = 0;
        if (kid >= 0)                                   /* "[OCT+]": the keycap (accent: it would act; DIM: it would not) */
            x = cv_keycap(5, ry + 2, (uint32_t)kid, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, cb);
        else if (vf == &AF_M) {
            if (!ui.roll[c].from[0] && val[0])
                GFX_HOOK_ALIGN(0, ry, COL_W, ry + ROLL_H, AL_V, "card value in its roll window");
            x = roll_text(c, 5, ry - 1, val, vc);
        } else if (free)                                /* (a name: ellipsised where it must be) */
            x = cv_free_text(5, ry + 2, vf, val, vc, cb, room - uw);
        else
            x = cv_text_fit(5, ry + 2, vf, val, vc, cb, room - uw);
        if (unit[0]) {
            GFX_HOOK_ALIGN(0, 0, 0, vf == &AF_M ? ry - 1 + AF_M.asc : ry + 2 + AF_S.asc, AL_B, "card unit on its value's baseline");
            cv_text_on(x + 3, ry + 2, &AF_S, unit, T_MID, cb);   /* on the value's
                                                         * baseline (S) */
        }
        if (!strip && curve != DJF_NOCURVE) {   /* (0.9.1) the DJ FILTER: its response (BOTTOM, HUD:
                                                         * 5 rows, under the value's strip) */
            uint16_t gc = hot && !lz_on() ? T_ACCENT : vc == T_DIM ? T_DIM : lz_on() ? (gk ? gk : vc) : gauge_col(ratio);
            djf_curve(5, ch - 8, COL_W - 10, 6, curve, curve ? gc : T_MID, T_LINE);
        } else if (!strip && ratio >= 0) {              /* the gauge: a 2 px LINE track, the value THEME on it */
            int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
            uint16_t gc = hot && !lz_on() ? T_ACCENT : vc == T_DIM ? T_DIM : lz_on() ? (gk ? gk : vc) : gauge_col(ratio);
            if (lz_on()) {                      /* COLORS: 3 px, rounded, as the 0.6's */
                cv_rrect(5, ch - 6, gw, 3, 1, T_LINE, cb);
                cv_rrect(5, ch - 6, fx < 3 ? 3 : fx, 3, 1, gc, T_LINE);
            } else {
                cv_rect(5, ch - 5, gw, 2, T_LINE);
                cv_rect(5, ch - 5, fx < 2 ? 2 : fx, 2, gc);
            }
        }
    }
    cv_oy = 0;
    cv_blit((uint32_t)CARD_X(c), card_y() + (strip ? (uint32_t)ry : 0u));
}

/* an action's column (act_cols): picked, the OCT+ keycap (accent while it would do something); else "--" */
static void draw_act_column(uint32_t c, const char *label, uint16_t vc, uint32_t icon)   /* icon: ICON_AUTO = by label */
{
    int picked = act_col() == c + 1u;
    card_arm_next = (uint8_t)picked;                    /* (0.8.5, the mock-up: the armed card TINT, OCT+ its value) */
    draw_column(c, label, picked ? "OCT+" : "", "", picked ? (act_ready() ? T_TEXT : T_DIM) : vc, -1, icon);
}


/* the sound's name on the track: a user preset or the engine's preset (b holds 16) */
static void sound_name(const track_t *t, char *b)
{
    const engine_t *e = trk_engp(t);
    b[0] = 0;
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (ENG_NP(e))
        str_cpy(b, e->presets[t->preset % ENG_NP(e)].name, 16);
}

/* (chord-harp fork, 2026-11-02, the redesign: HANDOFF.md "The design") HOME . TRACKS (trk_face): the knob cells and the
 * page row. A knob cell (4 x 58 px, 2 apart, on BG): its name at the top in the knob's colour (the full word where it
 * fits: KN_FULL), its value small at the bottom (TEXT, the unit MID), between them a rectangle of 2 x 2 px pixels lit in
 * a fixed random order per knob, or GLO > SCREEN FILL SNAKE's (kn_px: none at 0 %, all at 100 %), the others BG mixed 14 % to the knob's colour */
static const char *const KN_FULL[][2] = {{"ATK", "ATTACK"}, {"CUT", "CUTOFF"}, {"DEC", "DECAY"}, {"DECY", "DECAY"},
    {"REL", "RELEASE"}, {"RES", "RESO"}, {"SUS", "SUSTAIN"}, {"DTN", "DETUNE"}, {"FB", "FEEDBACK"}, {"FDBK", "FEEDBACK"},
    {"FEEDBK", "FEEDBACK"}, {"DRV", "DRIVE"}, {"GLID", "GLIDE"}, {"CRSH", "CRUSH"}, {"NOIS", "NOISE"}, {"SWP", "SWEEP"},
    {"CHOR", "CHORUS"}, {"COLR", "COLOR"}, {"DRFT", "DRIFT"}, {"TIMB", "TIMBRE"}, {"CLIK", "CLICK"}, {"SHP", "SHAPE"},
    {"VINTG", "VINTAGE"}, {"ROTR", "ROTOR"}, {"MODL", "MODEL"}, {"FILT", "FILTER"}, {"DLY", "DELAY"}, {"REV", "REVERB"},
    {"DENS", "DENSITY"}};
#define KN_W 58
static const char *kn_name(const char *l)
{
    uint32_t k;
    for (k = 0; k < NELEM(KN_FULL); k++)
        if (str_eq(l, KN_FULL[k][0]))
            return text_w(&AF_S, KN_FULL[k][1]) <= KN_W - 2 ? KN_FULL[k][1] : l;
    return l;
}
static uint32_t kn_px(uint32_t c, int32_t bx, int32_t by)   /* knob c's pixel bx, by: its place in the order, 0 .. 65535
                                                         * (GLO > SCREEN FILL SNAKE: down the first column, up the next ..) */
{
    uint32_t h = (uint32_t)(by * (KN_W / 2) + bx + 1) * 2654435761u ^ (c + 1u) * 0x9E3779B9u;
    if (ui_prefs & PREF_SNAKE)
        return (uint32_t)(bx * (KN_FILL_H / 2) + (bx & 1 ? KN_FILL_H / 2 - 1 - by : by)) * 65536u /
               (uint32_t)(KN_W / 2 * (KN_FILL_H / 2));
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12, h *= 0x297A2D39u, h ^= h >> 15;
    return h & 0xFFFFu;
}
/* (ui.col[c]: what the cell says, "DEC|42|", as a card's key without its colours: never a card's; its old value rolls
 * to the new as on the cards (roll_note), a name or a keycap snaps; the knob just turned: its name on a chip of its
 * colour) */
static int kn_row(const kn_t *kn)                       /* 1: drawn */
{
    static uint32_t drawn;
    uint32_t c, roll = 0, sig = ux.gen * 977u + settings.palette * 131u + (ui_prefs & PREF_SNAKE) + 1u;
    for (c = 0; c < 4u; c++) {
        const char *o = ui.col[c];
        char ov[12];
        uint32_t i = 0;
        uint8_t ks = (uint8_t)str_hash(str_hash(song.sel + TSEL->eng_req * 4u + ux.gen * 64u + 32u, kn[c].l), kn[c].u);
        while (*o && *o != '|')
            o++;
        while (*o && o[1] && o[1] != '|' && i + 1u < sizeof ov)
            ov[i++] = *++o;
        ov[i] = 0;
        if (ks != ui.roll[c].sig || ui.force)
            ui.roll[c].from[0] = 0;
        else if (!str_eq(ov, kn[c].v))
            roll_note(c, ov, kn[c].v, kn[c].n);
        ui.roll[c].sig = ks;
        str_cpy(ui.col[c], kn[c].l, sizeof ui.col[c]);
        str_cpy(ui.col[c] + str_len(ui.col[c]), "|", sizeof ui.col[c] - str_len(ui.col[c]));
        str_cpy(ui.col[c] + str_len(ui.col[c]), kn[c].v, sizeof ui.col[c] - str_len(ui.col[c]));
        str_cpy(ui.col[c] + str_len(ui.col[c]), "|", sizeof ui.col[c] - str_len(ui.col[c]));
        str_cpy(ui.col[c] + str_len(ui.col[c]), kn[c].u, sizeof ui.col[c] - str_len(ui.col[c]));
        card_mot = (uint8_t)((card_mot & ~(1u << c)) | (uint32_t)kn[c].m << c);   /* (MOTION's marks) */
        roll |= ui.roll[c].from[0] != 0;
        sig = (str_hash(str_hash(sig ^ (uint32_t)kn[c].r ^ (uint32_t)kn[c].pn << 20 ^ (uint32_t)kn[c].pi << 25, kn[c].l), kn[c].v) ^ str_hash(c, kn[c].u) ^ kn[c].m ^
               (uint32_t)kn[c].d << 1 ^ (uint32_t)(c == ui.hot_col && ui.hot_t) << 2) * 16777619u;
    }
    if (!ui.force && sig == drawn && !roll)
        return 0;
    drawn = sig;
    cv_begin(240, TF_KN_H, T_BG);
    for (c = 0; c < 4u; c++) {
        int32_t x0 = 1 + (int32_t)c * (KN_W + 2), bx, by, k, vw, uw, vx, vy = KN_NAME_H + KN_FILL_H + CAP_IN(S, TF_KN_H - KN_NAME_H - KN_FILL_H);
        uint16_t kc = T_K(c), ec = ux_mix(T_BG, kc, 14), vc = kn[c].m ? T_THEME : kn[c].d ? T_DIM : T_TEXT;
        uint32_t t = kn[c].r <= 0 ? 0u : kn[c].r >= 1000 ? 65536u : (uint32_t)kn[c].r * 65536u / 1000u;
        const char *nm = kn_name(kn[c].l);
        if (nm[0])
            GFX_HOOK_ALIGN(x0, 0, x0 + KN_W, KN_NAME_H, AL_HV, "knob cell name centred");
        if (c == ui.hot_col && ui.hot_t && nm[0]) {     /* (the knob just turned: its name on a chip) */
            cv_rrect(x0, 0, KN_W, KN_NAME_H - 1, 3, kc, T_BG);
            cv_text_in(x0, CAP_IN(S, KN_NAME_H), KN_W, &AF_S, nm, T_BG, kc);
        } else
            cv_text_in(x0, CAP_IN(S, KN_NAME_H), KN_W, &AF_S, nm, kc, T_BG);
        if (kn[c].pn) {                                 /* a pick: the grid, the one chosen lit; a row short centred
                                                         * (the user's, 2026-11-02; 2026-11-03: two halves, 17..32 8 across) */
            int32_t cols = kn[c].pn <= 2u ? 2 : kn[c].pn <= 16u ? 4 : 8, rows = (kn[c].pn + cols - 1) / cols;
            int32_t ch = (KN_FILL_H - 2 * (rows - 1)) / rows, cw = (KN_W - 2 * (cols - 1)) / cols;
            int32_t gx = x0 + (KN_W - cols * cw - 2 * (cols - 1)) / 2;
            for (k = 0; k < kn[c].pn; k++) {
                int32_t in = k / cols < rows - 1 ? cols : kn[c].pn - (rows - 1) * cols;
                cv_rect(gx + (cols - in) * (cw + 2) / 2 + (k % cols) * (cw + 2), KN_NAME_H + (k / cols) * (ch + 2), cw, ch,
                        k == kn[c].pi ? kc : ec);
            }
        }
        for (by = 0; kn[c].r >= 0 && !kn[c].pn && by < KN_FILL_H / 2; by++)   /* (a row's runs of one colour as one rectangle;
                                                         * no gauge: none) */
            for (bx = 0; bx < KN_W / 2; bx = k) {
                int on = kn_px(c, bx, by) < t;
                for (k = bx + 1; k < KN_W / 2 && (kn_px(c, k, by) < t) == on; k++)
                    ;
                cv_rect(x0 + bx * 2, KN_NAME_H + by * 2, (k - bx) * 2, 2, on ? kc : ec);
            }
        vw = text_w(&AF_S, kn[c].v), uw = kn[c].u[0] ? text_w(&AF_S, kn[c].u) + 2 : 0;
        vx = vw + uw > KN_W - 2 ? x0 + 1 : x0 + HALF_UP(KN_W - vw - uw);
        if (ui.roll[c].from[0])                         /* (rolling: same length, it fits) */
            roll_text(c, vx, vy, kn[c].v, vc);
        else
            cv_text_fit(vx, vy, &AF_S, kn[c].v, vc, T_BG, KN_W - 2);
        if (uw && vw + uw <= KN_W - 2)
            cv_text(vx + vw + 2, vy, &AF_S, kn[c].u, T_MID);
    }
    cv_blit(0, TF_KN_Y);
    return 1;
}
/* the page row (no tabs): the button's pages one row, five pills: the page shown in the middle (68 px, THEME, its whole
 * name in B: bold), two pages either side (40 px, its short form in the pixel face X, 6 letters at most: pg_short); the row
 * wraps (the last pages before the first), a page shows at most once (a short chain leaves places empty). (2026-11-02)
 * A held layer: its views MAP STATE (ALGORITHM, no wrap: MAP's right STATE, STATE's left MAP) */
static const char *const PG_SHORT[][2] = {   /* (2026-11-02) every page name over 6 letters (else the spaces dropped:
                                                         * VOICE 2 VOICE2, DELAY 2 DELAY2) */
    {"IDEA TRK", "I-TRK"}, {"IDEA SND", "I-SND"}, {"IDEA LFO", "I-LFO"}, {"IDEA FX", "I-FX"}, {"IDEA TEX", "I-TEX"},
    {"LEVELS 2", "LEVEL2"}, {"PATTERN", "PATTRN"}, {"STRETCH", "STRTCH"}, {"PLAYHEAD", "PLAYHD"},
    {"AUTO LIST", "A LIST"}, {"AUTO PAN", "A PAN"}, {"REVERB 2", "REVRB2"},
    {"SL BUS 2", "SLBUS2"}, {"SL PITCH", "SL PCH"}, {"ALL ENV", "A ENV"}, {"ALL DEST", "A DEST"}, {"ENV DEST", "E DEST"},
    {"LFO DEST", "L DEST"}, {"PRESETS", "PRESET"}, {"SECTIONS", "SECTS"},
    {"ARRANGE", "ARRANG"}, {"SONG EDIT", "S EDIT"}, {"PROJECT", "PROJ"}, {"PROJECTS", "PROJS"}, {"CONTROL", "CTRL"}};   /* (CONTROL: GLO's, 2026-11-02) */
#define PG_SIDE_W 40
#define PG_MID_W 68
static const char *pg_short(const char *s)
{
    static char b[8];
    uint32_t k, j = 0;
    if (text_w(&AF_X, s) <= PG_SIDE_W - 4)
        return s;
    for (k = 0; k < NELEM(PG_SHORT); k++)
        if (str_eq(s, PG_SHORT[k][0]))
            return PG_SHORT[k][1];
    for (k = 0; s[k] && j + 1u < sizeof b; k++)          /* "VOICE 2": VOICE2 */
        if (s[k] != ' ')
            b[j++] = s[k];
    b[j] = 0;
    return b;
}
static void page_row(void)
{
    static uint32_t drawn;
    static const int8_t SLOT_D[4][2] = {{3, 1}, {1, -1}, {4, 2}, {0, -2}};   /* (the places filled in this order) */
    static const uint8_t SLOT_X[5] = {0, 43, 86, 157, 200};
    static const char *const LYV[2] = {"MAP", "STATE"};
    uint8_t v[NAV_MAX];
    const char *pn[5] = {0, 0, 0, 0, 0};
    uint32_t sec = nav_sec(), n = nav_list(sec, v), e = nav_cur(), i = 0, k, sig;
    const ch_t *ch = nav_ch(sec);
    if (dlg_on()) {                                     /* a NOTICE: its one pill */
        pn[2] = "NOTICE";
        sig = 0x40D1CEu;
    } else if (name_on()) {                             /* NAME: its one pill */
        pn[2] = "NAME";
        sig = 0x4A3Eu;
    } else if (ui.layer) {
        e = ui.lylv & 1u;
        pn[2] = LYV[e];
        pn[e ? 1 : 3] = LYV[e ^ 1u];
        sig = (e + 1u) * 2654435761u + 7u;
    } else {
        for (k = 0; k < n; k++)
            if (v[k] == e)
                i = k;
        pn[2] = ch->e[e].nm ? ch->e[e].nm : ch->e[e].t;
        for (k = 0; k < 4u && k + 1u < n; k++) {
            const nv_t *c = &ch->e[v[(i + n + (uint32_t)(int32_t)SLOT_D[k][1]) % n]];
            pn[SLOT_D[k][0]] = c->nm ? c->nm : c->t;
        }
        sig = ((e * 31u + n) * 131u + sec) * 977u + 1u;
    }
    sig += ux.gen * 7u + settings.palette * 8191u;
    if (!ui.force && sig == drawn)
        return;
    drawn = sig;
    cv_begin(240, TF_PG_H, T_BG);
    for (k = 0; k < 5u; k++) {
        const char *s = pn[k];
        int32_t x = SLOT_X[k];
        if (!s)
            continue;
        if (k == 2u) {                                  /* (every name in B, M's face bold and smaller: <= 62 px) */
            cv_rrect(x, 1, PG_MID_W, TF_PG_H - 2, (TF_PG_H - 2) / 2, T_THEME, T_BG);
            GFX_HOOK_ALIGN(x, 1, x + PG_MID_W, TF_PG_H - 1, AL_HV, "page row: the page shown centred in its pill");
            cv_text_in(x, 1 + CAP_IN(B, TF_PG_H - 2), PG_MID_W, &AF_B, s, T_INK, T_THEME);
        } else {
            cv_rrect(x, 4, PG_SIDE_W, 15, 7, T_RAISE, T_BG);
            GFX_HOOK_ALIGN(x, 4, x + PG_SIDE_W, 19, AL_HV, "page row: a page beside centred in its pill");
            cv_text_in(x, 4 + CAP_IN(X, 15), PG_SIDE_W, &AF_X, pg_short(s), T_TEXT, T_RAISE);
        }
    }
    cv_blit(0, TF_PG_Y);
}
static void draw_cards(void)
{
    uint32_t c;
    char val[12];
    const char *unit;
    (void)motion_mask(song.sel, card_mot_mask);       /* (PLAY OFF: no scan) */
    if (home_macros()) {                                /* (0.8.5) HOME's MACROS / ROUTES: M1..M4, 0..100 % */
        static const char *const M[4] = {"M1", "M2", "M3", "M4"};
        for (c = 0; c < 4u; c++) {
            fmt_int(val, macro_v[c] * 100 / 127);
            draw_column(c, M[c], val, "%", VAL(c), macro_v[c] * 1000 / 127, ICON_MIX);
        }
        return;
    }
    if (ui.home) {
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d = home_param(c, &vp);
            param_format(d, *vp, val, &unit);
            card_mot_of(vp);
            draw_column(c, d->label, val, unit, VAL(c), gauge(d, *vp), param_icon(d, *vp));
        }
        return;
    }
    if (cur_page()->graph == GR_FOCUS) {               /* (0.8.5) EDIT > FOCUS: PARAM, its value, SRC, AMT */
        uint32_t k = focus_k();
        int32_t j = focus_slot(k);
        int16_t *vp = &TSEL->p[P_E0 + k];
        const param_desc_t *d = track_desc(TSEL, P_E0 + k);
        param_format(d, *vp, val, &unit);
        draw_column(0, "PARAM", d->label, "", VAL(0u), pick_of(8u, k), ICON_NONE);
        card_mot_of(vp);
        draw_column(1, d->label, val, unit, VAL(1u), gauge(d, *vp), ICON_NONE);
        draw_column(2, "SRC", j < 0 ? "OFF" : N_MSRC[clamp(TSEL->p[P_M1SRC + 3u * (uint32_t)j], 0, MS_N - 1)], "",
                    j < 0 ? T_DIM : VAL(2u), j < 0 ? -1 : pick_of(MS_N, (uint32_t)clamp(TSEL->p[P_M1SRC + 3u * (uint32_t)j], 0, MS_N - 1)),
                    ICON_NONE);
        if (j < 0)
            draw_column(3, "AMT", "--", "", T_DIM, -1, ICON_NONE);
        else {
            const param_desc_t *da = track_desc(TSEL, P_M1AMT + 3u * (uint32_t)j);
            int16_t a = TSEL->p[P_M1AMT + 3u * (uint32_t)j];
            param_format(da, a, val, &unit);
            draw_column(3, "AMT", val, unit, a ? VAL(3u) : T_DIM, RATIO(da, a), ICON_NONE);
        }
        return;
    }
    if (cur_page()->graph == GR_STAGE) {               /* (0.8.5) ENV > STAGE: the stage, its value, LOOP */
        uint32_t id = P_ATK + (ui.stage & 3u);
        int16_t *vp = &TSEL->p[id];
        draw_column(0, "STAGE", TP[id].label, "", VAL(0u), pick_of(4u, ui.stage & 3u), ICON_NONE);
        param_format(&TP[id], *vp, val, &unit);
        card_mot_of(vp);
        draw_column(1, TP[id].label, val, unit, VAL(1u), gauge(&TP[id], *vp), ICON_NONE);
        param_format(&TP[P_ELOOP], TSEL->p[P_ELOOP], val, &unit);
        draw_column(2, TP[P_ELOOP].label, val, unit, VAL(2u), gauge(&TP[P_ELOOP], TSEL->p[P_ELOOP]), ICON_NONE);
        draw_column(3, "", "", "", T_DIM, -1, ICON_NONE);
        return;
    }
    if (cur_page()->graph == GR_DXSND) {               /* EDIT > SOUND (ui_dx.c) */
        dx_cards(cur_page());
        return;
    }
    if (DX_KIT3(cur_page())) {                         /* (0.6.54) EDIT > KIT 3 */
        dx_kit3_cards();
        return;
    }
    if (cur_page()->graph == GR_SEDIT) {                /* (2026-11-06) TRACK, PART, CUT START, CUT END */
        uint32_t n = chain_config.count, r = ui.song_row < n ? ui.song_row : 0u, t = ui.sed_trk < trk_live ? ui.sed_trk : 0u, a = 0, b = 0;
        if (n) row_trims(&chain_config, song_cur, r, t, &a, &b);
        val[0] = 'T'; fmt_int(val + 1, (int32_t)t + 1);
        draw_column(0, "TRACK", val, "", VAL(0u), pick_of(trk_live, t), ICON_X_SONG);
        if (n) { val[0] = (char)('A' + (chain_config.row[r].slot & 3u)); val[1] = ' '; fmt_int(val + 2, (int32_t)r + 1);
                 str_cpy(val + str_len(val), "/", 2); fmt_int(val + str_len(val), (int32_t)n); }
        else str_cpy(val, "-", sizeof val);
        draw_column(1, "PART", val, "", VAL(1u), n ? pick_of(n, r) : -1, ICON_AUTO);
        fmt_int(val, (int32_t)a);
        draw_column(2, "CUT START", val, "BAR", a ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        fmt_int(val, (int32_t)b);
        draw_column(3, "CUT END", val, "BAR", b ? VAL(3u) : T_DIM, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SONG) {                 /* (2026-11-06) PROJECT, SECT, TIMES, INSERT (OCT+ adds) */
        fmt_int(val, (int32_t)song_cur + 1);
        draw_column(0, "PROJECT", val, "", VAL(0u), pick_of(NSONG, song_cur), ICON_X_SONG);
        {
            uint32_t n = chain_config.count;
            if (n && ui.song_row >= n) {                /* END: K2 sets it */
                uint32_t e = SONG_END(&chain_config);
                if (e == SONG_FOREVER) str_cpy(val, "LOOP", sizeof val);
                else if (e >= 2u) { val[0] = 'x'; fmt_int(val + 1, (int32_t)e); }
                else str_cpy(val, "STOP", sizeof val);
                draw_column(1, "END", val, "", VAL(1u), -1, ICON_LOOP);
                draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
                draw_column(3, "ROW", "END", "", VAL(3u), -1, ICON_AUTO);
                return;
            }
            val[0] = (char)('A' + (ui.arr_sect & 3u)); val[1] = 0;
            draw_column(1, "SECT", val, "", VAL(1u), pick_of(4u, ui.arr_sect & 3u), ICON_X_PATTERN);
            val[0] = 'x';
            fmt_int(val + 1, (int32_t)arr_bars_get());
            draw_column(2, "TIMES", val, "", VAL(2u), -1, ICON_AUTO);
            if (n) {
                fmt_int(val, (int32_t)ui.song_row + 1);
                str_cpy(val + str_len(val), "/", 2);
                fmt_int(val + str_len(val), (int32_t)n);
            } else str_cpy(val, "0/0", sizeof val);
            draw_column(3, "ROW", val, "", VAL(3u), -1, ICON_AUTO);
        }
        return;
    }
    if (cur_page()->graph == GR_ERASE) {                 /* (2026-11-05) PROJECT, SECT, then ERASE SECTION and ERASE PROJECT */
        uint32_t k = (uint32_t)song.g[G_SLOT] - 1u;
        fmt_int(val, (int32_t)song_cur + 1);
        draw_column(0, "PROJECT", val, "", VAL(0u), pick_of(NSONG, song_cur), ICON_X_SONG);
        val[0] = (char)('A' + (k & 3u)); val[1] = 0;
        draw_column(1, "SECT", val, "", VAL(1u), pick_of(4u, k & 3u), ICON_AUTO);
        draw_act_column(2, "SECTION", graph_project_used(k) ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "PROJECT", T_THEME, ICON_X_SONG);
        return;
    }
    if (cur_page()->graph == GR_SHAPE) {               /* (0.6.85) STRETCH REPEAT NUDGE; (0.6.92) ROTATE, done on the turn */
        int32_t k = shp_k(), r = (int32_t)shp_rep(), ofs = TSEL->p[P_SOFS];
        val[0] = k > 0 ? 'x' : '/';
        fmt_int(val + 1, k > 0 ? k : -k);
        draw_column(0, "STRETCH", val, "", k != 1 ? VAL(0u) : T_DIM, pick_of(15u, shp_i), ICON_X_REPEAT);
        if (r == 1) str_cpy(val, "OFF", sizeof val);
        else if (r) { val[0] = 'x'; fmt_int(val + 1, r); }
        else str_cpy(val, "TO 64", sizeof val);
        draw_column(1, "REPEAT", val, "", r != 1 ? VAL(1u) : T_DIM, pick_of(9u, shp_r), ICON_X_PATTERN);
        draw_column(2, "NUDGE", "< >", "", VAL(2u), -1, ICON_ORDER);
        val[0] = ofs > 0 ? '+' : 0;
        fmt_int(val + (ofs > 0), ofs);
        card_mot_of(&TSEL->p[P_SOFS]);
        draw_column(3, "ROTATE", val, "ST", ofs ? VAL(3u) : T_DIM, (ofs + 32) * 1000 / 64, ICON_ORDER);
        return;
    }
    if (cur_page()->graph == GR_FUGUE) {               /* (0.6.52) HEAD RATE DIR INTERVAL */
        const fg_set_t *f = &fg_set[song.sel % NTRK][fg_ui % FG_HEADS];
        fmt_int(val, (int32_t)(fg_ui % FG_HEADS) + 2);
        draw_column(0, "HEAD", val, "", VAL(0u), pick_of(FG_HEADS, fg_ui % FG_HEADS), ICON_X_PATTERN);
        draw_column(1, "RATE", N_FGRATE[f->rate % FG_NRATE], "", f->rate ? VAL(1u) : T_DIM, pick_of(10u, f->rate),
                    ICON_RATE);
        draw_column(2, "DIR", N_FGDIR[f->dir & 3u], "", f->rate ? VAL(2u) : T_DIM, pick_of(4u, f->dir & 3u), ICON_ORDER);
        val[0] = f->ivl > 0 ? '+' : 0;
        fmt_int(val + (f->ivl > 0), f->ivl);
        draw_column(3, "INTVL", val, "DEG", f->rate ? VAL(3u) : T_DIM, (int32_t)(f->ivl + 14) * 1000 / 28, ICON_PITCH);
        return;
    }
    if (cur_page()->graph == GR_COMP) {                /* (0.6.39) STEP TYPE and the TYPE's two values */
        const step_t *cs = &TSEL->step[ui.cursor];
        uint32_t v = comp_view(), ca = comp_arg(cs), ct = comp_type(cs);
        static const char *const ORD[4] = {"UP", "DOWN", "UP-DN", "RND"};
        fmt_int(val, (int32_t)ui.cursor + 1);
        draw_column(0, "STEP", val, "", VAL(0u), -1, ICON_AUTO);
        draw_column(1, v >= COMP_ARPN && v <= COMP_ARPS ? "ARP" : "TYPE", COMP_NAMES[v], "", v ? VAL(1u) : T_DIM, pick_of(COMP_N, v),
                    v == COMP_CHANCE ? ICON_PROB : v == COMP_RATCH ? ICON_X_REPEAT : ICON_ARP);
        if (v == COMP_CHANCE) {
            fmt_int(val, (int32_t)step_chance(cs));
            draw_column(2, "CHANCE", val, "%", VAL(2u), -1, ICON_PROB);
        } else if (v == COMP_RATCH) {
            val[0] = 'x'; val[1] = (char)('0' + step_ratchet(cs)); val[2] = 0;
            draw_column(2, "RATCH", val, "", VAL(2u), -1, ICON_X_REPEAT);
        } else if (v == COMP_TRIG) {
            const uint8_t *c = CP_TRIGS[ca % CP_NTRIG];
            if (!c[0]) str_cpy(val, c[1] == 1u ? "FIRST" : "!1ST", sizeof val);
            else { val[0] = (char)('0' + c[0]); val[1] = ':'; val[2] = (char)('0' + c[1]); val[3] = 0; }
            draw_column(2, "COND", val, "", VAL(2u), -1, ICON_PROB);
        } else if (v > COMP_TRIG) {                    /* (0.6.42) the others' one value */
            static const char *const OCTN[6] = {"+1", "-1", "+1/2", "-1/2", "ALT", "RND"};
            int32_t a = (int32_t)ca;
            const char *lb = "", *un = "";
            val[0] = 0;
            switch (v) {
            case COMP_RAND: lb = "RANGE"; fmt_int(val, a); un = "DEG"; break;
            case COMP_OCT: lb = "MODE"; str_cpy(val, OCTN[a % 6], sizeof val); un = "OCT"; break;
            case COMP_TONAL: lb = "SHIFT"; a -= 7; val[0] = a < 0 ? '-' : '+'; fmt_int(val + 1, a < 0 ? -a : a); un = "DEG"; break;
            case COMP_BEND: lb = "BEND"; a -= 12; val[0] = a < 0 ? '-' : '+'; fmt_int(val + 1, a < 0 ? -a : a); un = "ST"; break;
            case COMP_ECHO: lb = "REPEAT"; val[0] = 'x'; fmt_int(val + 1, 2 + (a & 127) % 7); un = a & 128 ? "EVEN" : ""; break;
            case COMP_TOUCH: { static const char *const TN[3] = {"SHORT", "GHOST", "FLAM"}; lb = "HIT"; str_cpy(val, TN[a % 3], sizeof val); break; }   /* (0.8.5) */
            case COMP_CHORD: lb = a < (int32_t)HARP_NID ? HARP_SET_NAME[a / (int32_t)HARP_NCH] : "OMNI";   /* (OMNI: its name) */
                if (a < (int32_t)HARP_NID) fmt_int(val, a % (int32_t)HARP_NCH + 1); else hc_name((uint32_t)a % CHORD_NID, 0, val); break;   /* (chord-harp: the
                                                         * set and the chord's place; its name over the step, graph_bigstep) */
            case COMP_PULSE: lb = "EVERY"; fmt_int(val, 1 + a % 4); un = a ? "STEPS" : "STEP"; break;
            case COMP_JUMP: lb = "TO STEP"; fmt_int(val, (int32_t)CP_JTO(a) + 1); break;
            case COMP_WALK: case COMP_CLIMB: {          /* (0.6.98) MODE and RANGE in one: the range as the unit */
                static const char *const WM[4] = {"UP", "DOWN", "UP-DN", "RND"};
                char u[8];
                lb = "MODE"; str_cpy(val, WM[CP_WMODE(a)], sizeof val);
                fmt_int(u, v == COMP_WALK ? (int32_t)CP_WDEGS[CP_WRNG(a)] : (int32_t)CP_WRNG(a) + 1);
                str_cpy(u + str_len(u), v == COMP_WALK ? " DEG" : " OCT", 5);
                draw_column(2, lb, val, u, VAL(2u), -1, ICON_ORDER);
                lb = "";
                break;
            }
            default: break;
            }
            if (lb[0]) draw_column(2, lb, val, un, VAL(2u), -1, ICON_X_REPEAT);
            else if (v != COMP_WALK && v != COMP_CLIMB) draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
        } else if (v != COMP_OFF) {                    /* (0.6.92) an ARP: ORDER and RANGE in one (the range as the label) */
            char lb[10];
            fmt_int(lb, v == COMP_ARPS ? CP_DEGS[CP_RNG(ca)] : (int32_t)CP_RNG(ca) + 1);
            str_cpy(lb + str_len(lb), v == COMP_ARPS ? " DEG" : " OCT", 5);
            draw_column(2, lb, ORD[CP_ORD(ca)], "", VAL(2u), pick_of(4u, CP_ORD(ca)), ICON_ORDER);
        } else
            draw_column(2, "", "", "", T_THEME, -1, ICON_NONE);
        if (v == COMP_WALK || v == COMP_CLIMB) {       /* (0.6.98) RATE: a move every so many loops */
            uint32_t r = CP_WRATES[CP_WRATE(ca) % CP_NWRATE];
            fmt_int(val, (int32_t)r);
            draw_column(3, "EVERY", val, r == 1u ? "LOOP" : "LOOPS", VAL(3u), -1, ICON_RATE);
        } else if (v >= COMP_ARPN && v <= COMP_ARPS && comp_slot() == 0u) {   /* (0.6.92) RATE as a note value of the track's DIV */
            rate_name(TSEL, CP_RATES[CP_RATE(ca) % CP_NRATE], val);
            draw_column(3, "RATE", val, "", VAL(3u), -1, ICON_RATE);
        } else if ((v == COMP_RATCH || v == COMP_ECHO) && comp_slot() == 0u) {   /* (0.6.82) the parts' spacing */
            if (CT_DIV(cs)) rate_name(TSEL, CT_DIVS[CT_DIV(cs) % CT_NDIV], val); else str_cpy(val, "FIT", sizeof val);
            draw_column(3, "RATE", val, "", CT_DIV(cs) ? VAL(3u) : T_DIM, -1, ICON_RATE);
        } else {                                       /* (0.6.92) EVERY: the loops the step's components play on */
            uint32_t ev = ST_EVERY(cs) % EV_N, o;
            for (o = 0; o < EV_N - 1u && EV_ORD[o] != ev; o++)
                ;
            draw_column(3, "EVERY", EV_NAMES[ev], "", ev ? VAL(3u) : T_DIM, pick_of(EV_N, o), ICON_PROB);
        }
        (void)ct;
        return;
    }
    if (cur_page()->graph == GR_CHANCE) {              /* STEP CHANCE RATCH: the cursor step's, of all of it */
        const step_t *cs = &TSEL->step[ui.cursor];
        int rplays = cs->time == ST_NOTE && (cs->n || cs->hit);   /* RATCH does nothing on a REST, a TIE, empty: DIM */
        fmt_int(val, (int32_t)ui.cursor + 1);
        draw_column(0, "STEP", val, "", VAL(0u), -1, ICON_AUTO);
        fmt_int(val, (int32_t)step_chance(&TSEL->step[ui.cursor]));
        draw_column(1, "CHANCE", val, "%", VAL(1u), -1, ICON_PROB);   /* the die */
        val[0] = 'x';                                  /* x1 .. x4 */
        val[1] = (char)('0' + step_ratchet(&TSEL->step[ui.cursor]));
        val[2] = 0;
        draw_column(2, "RATCH", val, "", rplays ? VAL(2u) : T_DIM, -1, ICON_X_REPEAT);
        draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
        return;
    }
    if (cur_page()->graph == GR_EVENTS) {              /* AUTO LIST (ui_events.c): ROW STEP PARAM VALUE */
        const track_t *t = TSEL;
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx), r = ev_row(n), step, id;
        int16_t v;
        char nm[12];
        const param_desc_t *d;
        if (r < n) {
            const motion_event_t *e = &motion.event[idx[r]];
            fmt_int(val, (int32_t)r + 1);
            str_cpy(val + str_len(val), "/", 4);
            fmt_int(val + str_len(val), (int32_t)n);
            step = MOTION_STEP(e);
            id = MOTION_ID(e);
            v = MOTION_VAL(e);
        } else {
            ev_fix();
            str_cpy(val, "ADD", sizeof val);
            step = ui.ev_step;
            id = ui.ev_id;
            v = id < P_COUNT ? motion_base_value(t, id) : 0;
        }
        draw_column(0, "ROW", val, "", VAL(0u), -1, ICON_AUTO);
        fmt_int(val, (int32_t)step + 1);
        draw_column(1, "STEP", val, "", r < n || step < (uint32_t)t->p[P_SLEN] ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        if ((id >= P_COUNT && id != MOT_DJF) || !ev_id_ok(t, id)) {
            draw_column(2, "PARAM", "--", "", T_DIM, -1, ICON_AUTO);
            draw_column(3, "VALUE", "--", "", T_DIM, -1, ICON_AUTO);
            return;
        }
        ev_name(t, id, nm);
        draw_column(2, "PARAM", nm, "", VAL(2u), -1, ICON_AUTO);
        d = track_desc(t, id);
        param_format(d, v, val, &unit);
        draw_column(3, r == n ? "VALUE" : (motion.event[idx[r]].param & MOTION_LOCK) ? "LOCK" : "AUTO", val, unit,
                    r < n ? VAL(3u) : T_DIM, gauge(d, v), param_icon(d, v));
        return;
    }
    if (cur_page()->graph == GR_PUNCH) {               /* (0.6.99) the punch-in lane: LEN, RES, its state, CLEAR */
        uint32_t n = 0, i;
        for (i = 0; i < 64u; i++) n += pfx_lane_at(i) != 0u;
        fmt_int(val, song.g[G_PFXLEN]);
        draw_column(0, "LEN", val, "ST", VAL(0u), gauge(&GP[G_PFXLEN], song.g[G_PFXLEN]), ICON_AUTO);
        draw_column(1, "RES", N_PFXRES[(uint32_t)clamp(song.g[G_PFXRES], 0, 2)], "", VAL(1u), pick_of(3u, (uint32_t)clamp(song.g[G_PFXRES], 0, 2)), ICON_RATE);
        fmt_int(val, (int32_t)n);
        draw_column(2, pfx_rec_ok() ? "REC" : song.playing ? "PLAY" : "STEPS", pfx_rec_ok() ? "ON" : song.playing ? "ON" : val, "",
                    pfx_rec_ok() ? T_ACCENT : T_MID, -1, pfx_rec_ok() ? ICON_X_REC : ICON_NONE);
        draw_act_column(3, "CLEAR", n ? T_MID : T_DIM, ICON_X_MOTION_DEL);
        return;
    }
    if (cur_page()->graph == GR_MOTION) {
        draw_column(0, "PLAY", motion_enabled(TSEL) ? "ON" : "OFF", "", VAL(0u), pick_of(2u, motion_enabled(TSEL) != 0),
                    motion_icon());   /* (2026-11-02) on / off: all the pixels or none */
        fmt_int(val, (int32_t)(motion_count(TSEL) - motion_lock_count(TSEL)));
        draw_column(1, "EVENT", val, "", T_MID, -1, ICON_NONE);
        fmt_int(val, (int32_t)motion_lock_count(TSEL));   /* parameter locks (SEQ > STEP: a step held, a knob) */
        draw_column(2, "LOCK", val, "", T_MID, -1, ICON_NONE);
        draw_act_column(3, "CLEAR", T_MID, ICON_X_MOTION_DEL);
        return;
    }
    if (cur_page()->graph == GR_SQTOOL) {               /* (2026-11-03) SEQ > TOOLS (ui_tools.c) */
        sqt_cols();
        return;
    }
    if (cur_page()->graph == GR_TOOLS) {
        static const char *const labels[] = {"PAT", "ROW", "SONG", "NEW"};
        static const uint8_t icons[] = {ICON_AUTO, ICON_X_SONG, ICON_X_SONG, ICON_AUTO};   /* ROW, SONG: the song's */
        for (c = 0; c < 4u; c++) draw_act_column(c, labels[c], VAL(c), icons[c]);
        return;
    }
    if (cur_page()->scope == SC_TRK) {                 /* LEVEL PAN MUTE (OFF SOLO MUTE) of the selected track, the DJ FILTER */
        const track_t *t = TSEL;
        uint32_t lvl = trk_level(song.sel);
        param_format(&TP[P_LEVEL], (int32_t)lvl, val, &unit);   /* (0: OFF) */
        card_mot_of(&TSEL->p[P_LEVEL]);
        draw_column(0, "LEVEL", val, unit, lvl && !t->p[P_MUTE] ? VAL(0u) : T_DIM, (int32_t)lvl * 1000 / 127, ICON_AUTO);
        param_format(&TP[P_PAN], t->p[P_PAN], val, &unit);
        card_mot_of(&TSEL->p[P_PAN]);
        draw_column(1, "PAN", val, unit, VAL(1u), RATIO(&TP[P_PAN], t->p[P_PAN]), param_icon(&TP[P_PAN], t->p[P_PAN]));
        draw_column(2, "MUTE", MIX_ST[mix_state(song.sel)], "", VAL(2u), pick_of(3u, mix_state(song.sel)), ICON_AUTO);
        djf_format(song.g[G_DJF], val, &unit);          /* (0.9.1) KNOB 4: the master DJ FILTER (was the track's MUTE) */
        card_mot_next = (uint8_t)motion_djf_any();
        card_curve_next = (int16_t)(song.g[G_DJF] + 256);
        draw_column(3, "DJ FILT", val, unit, VAL(3u), RATIO(&GP[G_DJF], song.g[G_DJF]), ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_BROWSE) {                /* (chord-harp fork) KNOB 1 the TYPE; the numbers by the list */
        draw_column(0, "TYPE", PTYPE_NAME[ptype_sel % (PT_USER + 1u)], "", VAL(0u), pick_of(PT_USER + 1u, ptype_sel), ICON_NONE);
        draw_column(1, "ENG", ENGINES[TSEL->eng_req]->name, "", VAL(1u), pick_of(NENG_SHOWN, eng_rank(TSEL->eng_req)),
                    engine_icon(ENGINES[TSEL->eng_req]->name));   /* (2026-11-03, the user's: all four their grids) */
        draw_column(2, "FAV", preset_favorite() ? "ON" : "OFF", "", VAL(2u), pick_of(2u, (uint32_t)preset_favorite()), ICON_X_STAR);
        draw_column(3, "LIST", favorites.filter ? "FAV" : "ALL", "", VAL(3u), pick_of(2u, favorites.filter), ICON_X_FOLDER);
        return;
    }
    if (cur_page()->graph == GR_PATS) {                  /* PAT, then LOAD (a GO button) */
        uint32_t n = pat_count(), k = pat_pick();
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        draw_column(0, "PAT", tag, "", VAL(0u), pick_of(n, k), ICON_X_PATTERN);
        draw_act_column(1, "LOAD", T_THEME, ICON_AUTO);
        draw_column(2, "", "", "", T_THEME, -1, ICON_AUTO);
        draw_column(3, "", "", "", T_THEME, -1, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SLOTS) {                 /* (0.6.12) PROJECT, SECT A-D, then LOAD and SAVE */
        uint32_t k = (uint32_t)song.g[G_SLOT] - 1u;
        int used = graph_project_used(k);
        if (save_level() == SV_PROJ) {                   /* (0.8.5) SAVE > PROJECT's SECTIONS: SECT, its NAME */
            const char *nm;
            val[0] = (char)('A' + (k & 3u)); val[1] = 0;
            draw_column(0, "SECT", val, "", VAL(0u), pick_of(4u, k & 3u), ICON_AUTO);
            nm = used ? graph_project_name(k) : "";
            card_free_next = 1;
            draw_column(1, "NAME", !used ? "--" : nm[0] ? nm : "USED", "", used ? T_THEME : T_DIM, -1, ICON_NONE);
        } else {
            fmt_int(val, (int32_t)song_cur + 1);
            draw_column(0, "PROJECT", val, "", VAL(0u), pick_of(NSONG, song_cur), ICON_X_SONG);
            val[0] = (char)('A' + (k & 3u)); val[1] = 0;
            draw_column(1, "SECT", val, "", VAL(1u), pick_of(4u, k & 3u), ICON_AUTO);
        }
        draw_act_column(2, "LOAD", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_USER) {                  /* SLOT, then three GO buttons */
        int used = up_used(ui.uslot);
        up_slot_label(val, ui.uslot);
        draw_column(0, "SLOT", val, "", VAL(0u), pick_of(UP_SLOTS, ui.uslot), ICON_AUTO);
        draw_act_column(1, "LOAD", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(2, "ERASE", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SLPIT) {                 /* (0.6.3) STEP, its PIT, LEN, QNT */
        uint32_t id = G_SLP0 + (sl_ui_step & 15u);
        fmt_int(val, (int32_t)sl_ui_step + 1);
        draw_column(0, "STEP", val, "/16", sl_ui_step < (uint32_t)song.g[G_SLPLEN] ? VAL(0u) : T_DIM,
                    pick_of(16u, sl_ui_step), ICON_AUTO);
        param_format(&GP[id], song.g[id], val, &unit);
        draw_column(1, "PITCH", val, unit, song.g[id] ? VAL(1u) : T_DIM, gauge(&GP[id], song.g[id]), ICON_AUTO);
        param_format(&GP[G_SLPLEN], song.g[G_SLPLEN], val, &unit);
        draw_column(2, "LEN", val, unit, VAL(2u), gauge(&GP[G_SLPLEN], song.g[G_SLPLEN]), ICON_AUTO);
        param_format(&GP[G_SLPQ], song.g[G_SLPQ], val, &unit);
        draw_column(3, "QUANT", val, unit, VAL(3u), gauge(&GP[G_SLPQ], song.g[G_SLPQ]), ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_MSEQ) {                  /* (Yehnah) STEP, its LVL, LEN, SLEW */
        const track_t *t = TSEL;
        uint32_t id = P_MS0 + (ms_ui_step & 15u);
        fmt_int(val, (int32_t)ms_ui_step + 1);
        draw_column(0, "STEP", val, "/16", ms_ui_step < (uint32_t)t->p[P_MSLEN] ? VAL(0u) : T_DIM,
                    pick_of(16u, ms_ui_step), ICON_AUTO);
        param_format(&TP[id], t->p[id], val, &unit);
        draw_column(1, "LEVEL", val, unit, VAL(1u), gauge(&TP[id], t->p[id]), ICON_LEVEL);
        param_format(&TP[P_MSLEN], t->p[P_MSLEN], val, &unit);
        draw_column(2, "LEN", val, unit, VAL(2u), gauge(&TP[P_MSLEN], t->p[P_MSLEN]), ICON_AUTO);
        param_format(&TP[P_MSSLW], t->p[P_MSSLW], val, &unit);
        draw_column(3, "SLEW", val, unit, t->p[P_MSSLW] ? VAL(3u) : T_DIM, RATIO(&TP[P_MSSLW], t->p[P_MSSLW]), ICON_GLIDE);
        return;
    }
    if (fop_page(cur_page()->graph)) {                   /* (0.6.10) FM6: OP, then three of its bytes (ui_fm6op.c) */
        for (c = 0; c < 4u; c++) {
            const char *lb, *u;
            int32_t r;
            uint32_t ic;
            int on;
            if (!fop_ok()) {                             /* (the engine changed before ui_draw left the page) */
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
                continue;
            }
            on = fop_card(c, &lb, val, &u, &r, &ic);
            if (str_eq(lb, "OP"))                        /* (a pick: its grid; the amounts keep their fill) */
                r = pick_of(6u, fop_op);
            else if (str_eq(lb, "STAGE"))
                r = pick_of(4u, fop_stg);
            else if (str_eq(lb, "MODE"))
                r = pick_of(2u, str_eq(val, "FIXED"));
            draw_column(c, lb, val, u, on ? VAL(c) : T_DIM, r, ic);
        }
        return;
    }
    if (cur_page()->graph == GR_MOD) {                   /* SLOT, then that slot's SRC DST AMT */
        const track_t *t = TSEL;
        uint32_t id = P_M1SRC + 3u * mod_ui_slot;
        int32_t s = t->p[id], d = t->p[id + 1u], a = t->p[id + 2u];
        fmt_int(val, (int32_t)mod_ui_slot + 1);
        draw_column(0, "SLOT", val, "/4", VAL(0u), pick_of(4u, mod_ui_slot), mod_src_icon(MS_OFF));   /* (the mod icon) */
        draw_column(1, "SRC", N_MSRC[clamp(s, 0, MS_N - 1)], "", s ? VAL(1u) : T_DIM, pick_of(MS_N, (uint32_t)clamp(s, 0, MS_N - 1)), mod_src_icon(s));
        draw_column(2, "DEST", mod_dst_name(t, d), "", d ? VAL(2u) : T_DIM, -1, mod_dst_icon(t, d));
        param_format(&TP[id + 2u], a, val, &unit);
        draw_column(3, "AMT", val, unit, a ? VAL(3u) : T_DIM, RATIO(&TP[id + 2u], a), mod_src_icon(MS_OFF));
        return;
    }
    if (cur_page()->graph == GR_ROLL && lock_held()) {   /* a step held: what KNOB 1..4 lock (ui_input.c lock_turn) */
        uint64_t held = lock_held();
        uint32_t st = 0;
        while (!((held >> st) & 1u))
            st++;                                       /* (the first step held: its values) */
        for (c = 0; c < 4u; c++) {
            uint32_t id = lock_id(c);
            const param_desc_t *d;
            int16_t v;
            int on;
            if (id == 0xFFu) {
                draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
                continue;
            }
            d = track_desc(TSEL, id);
            on = motion_lock_get(TSEL, st, id, &v);
            if (!on)
                v = motion_base_value(TSEL, id);      /* not locked: the sound's own, DIM */
            param_format(d, v, val, &unit);
            draw_column(c, d->label, val, unit, on ? VAL(c) : T_DIM, gauge(d, v), param_icon(d, v));
        }
        return;
    }
    if (cur_page()->scope == SC_STEP && drum_track(TSEL)) {   /* the grid: STEP LANE HIT ACC */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t b = 1u << ui.lane, on = (step_lanes(st) & b) != 0u, ac = (step_accents(st) & b) != 0u;
        char sn[8], sl[8];
        fmt_int(sn, (int32_t)ui.cursor + 1);
        str_cpy(sl, "/", 8);
        fmt_int(sl + 1, TSEL->p[P_SLEN]);
        draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
        draw_column(1, "LANE", drum_lane_name(TSEL, ui.lane), "", VAL(1u), pick_of(NLANE, ui.lane), ICON_AUTO);
        draw_column(2, "HIT", GRID_HIT[grid_hit_of(st, ui.lane)], "", on ? VAL(2u) : T_DIM, pick_of(LM_N + 1u, grid_hit_of(st, ui.lane)), ICON_AUTO);   /* (0.8.5: its mode) */
        draw_column(3, "ACC", ac ? "ON" : "--", "", ac ? VAL(3u) : T_DIM, pick_of(2u, ac != 0), ICON_AUTO);
        return;
    }
    if (cur_page()->scope == SC_STEP && cur_page()->id[0] == 4u) {   /* (0.6.81) SEQ > NOTE: FOCUS PITCH MOVE; (0.6.92) MICRO
                                                         * (the step off the grid, 0.6.88's TIMING; on a kit: STEP and MICRO) */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t kit = drum_track(TSEL), on = st->time == ST_NOTE && (kit ? st->hit != 0u : st->n != 0u);
        uint32_t f = on && !kit && ui.pr_focus <= st->n ? ui.pr_focus : 0u;
        int32_t m = ST_MICRO(st);
        char fo[8], pn[8], sn[8], mo[10];
        if (!f) str_cpy(fo, on && !kit && st->n > 1u ? "CHORD" : "ALL", 8);
        else { str_cpy(fo, "NOTE ", 8); fmt_int(fo + 5, (int32_t)f); }
        if (on && !kit) note_name(pn, st->note[f ? f - 1u : 0u]); else str_cpy(pn, kit ? "--" : "NONE", 8);
        fmt_int(sn, (int32_t)ui.cursor + 1);
        if (!m) str_cpy(mo, on ? "GRID" : "--", sizeof mo);
        else { mo[0] = m < 0 ? '-' : '+'; fmt_int(mo + 1, m < 0 ? -m : m); str_cpy(mo + str_len(mo), "/64", 4); }   /* (0.9.1) */
        if (kit) draw_column(0, "STEP", sn, "", VAL(0u), -1, ICON_AUTO);
        else draw_column(0, "FOCUS", fo, "", on ? VAL(0u) : T_DIM, -1, ICON_AUTO);
        draw_column(1, "PITCH", pn, "", on && !kit ? VAL(1u) : T_DIM, -1, ICON_AUTO);
        draw_column(2, "MOVE", kit ? "--" : sn, kit ? "" : "<>", on && !kit ? VAL(2u) : T_DIM, -1, ICON_AUTO);
        draw_column(3, "MICRO", mo, "", on && m ? VAL(3u) : T_DIM, on ? (m - MICRO_MIN) * 1000 / (MICRO_MAX - MICRO_MIN) : -1,
                    ICON_ORDER);
        return;
    }
    if (cur_page()->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        char u[8];
        uint32_t cnt = st->n, first = st->note[0], l;
        for (l = NLANE; l-- > 0;)                         /* (lane hits count as their notes) */
            if ((st->hit >> l) & 1u) {
                cnt++;
                if (!st->n)
                    first = DRUM_LANE_NOTE[l];
            }
        if (cnt) {
            note_name(val, first);
            u[0] = 0;
            if (cnt > 1) {
                str_cpy(u, "+", 8);
                fmt_int(u + 1, (int32_t)cnt - 1);
            }
        } else {
            str_cpy(val, "NONE", 12);                   /* (2026-11-03: the NOTE knob's bottom, an empty step) */
            u[0] = 0;
        }
        {
            static const char *const FLAG_N[4] = {"-", "ACC", "SLD", "A+S"};
            char sn[8], sl[8], tv[8];
            uint32_t nl = st->time == ST_NOTE && st->n ? note_len(TSEL, ui.cursor, (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP)) : 1u;
            fmt_int(sn, (int32_t)ui.cursor + 1);
            str_cpy(sl, "/", 8);
            fmt_int(sl + 1, TSEL->p[P_SLEN]);
            if (nl > 1u) fmt_int(tv, (int32_t)nl);            /* (0.8.4) a NOTE's length: 3 ST (NOTE: one step) */
            else str_cpy(tv, TIME_N[st->time % 3u], sizeof tv);
            draw_column(0, "STEP", sn, sl, VAL(0u), -1, ICON_AUTO);
            draw_column(1, "NOTE", val, u, step_on(st) ? VAL(1u) : T_DIM, -1, ICON_AUTO);
            draw_column(2, "TIME", tv, nl > 1u ? "ST" : "", VAL(2u), nl > 1u ? -1 : pick_of(3u, st->time % 3u), ICON_AUTO);
            draw_column(3, "FLAG", FLAG_N[(st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)], "",
                        VAL(3u), pick_of(4u, (st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u)), ICON_AUTO);
        }
        return;
    }
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        if (cur_page()->graph == GR_PROG && c >= 1u && c <= 2u) {   /* (chord-harp fork) SCL > PROG: PROG (its number:
                                                         * the name in the breadcrumb), RATE; PLATE (HARP's) */
            if (c == 1u) {
                char u[8] = "/";
                fmt_int(val, TSEL->p[P_PROG] + 1);
                fmt_int(u + 1, (int32_t)PROG_N);
                draw_column(1, "PROG", val, u, VAL(1u), pick_of(PROG_N, (uint32_t)TSEL->p[P_PROG]), ICON_AUTO);
            } else
                draw_column(2, "RATE", N_PRATE[prog_rate % 3u], "", VAL(2u), pick_of(3u, prog_rate % 3u), ICON_RATE);
            continue;
        }
        if (cur_page()->graph == GR_SETS && c == 0u) {   /* (topic C) GLO's lists: KNOB 1 the row (its name; n/N) */
            uint32_t ls = sets_of(&PAGES[ui.page]) & 3u, n = sets_n(ls);
            char u[8];
            fmt_int(u, (int32_t)sets_cur[ls] + 1);
            str_cpy(u + str_len(u), "/", 2);
            fmt_int(u + str_len(u), (int32_t)n);
            draw_column(0, "ROW", sets_label(SETS[ls][sets_cur[ls]]), u, VAL(0u), pick_of(n, sets_cur[ls]), ICON_NONE);
            continue;
        }
        if (cur_page()->graph == GR_SETS && c >= 2u) {
            draw_column(c, "", "", "", T_DIM, -1, ICON_NONE);
            continue;
        }
        if (c == 1u && HARP_PAGE(cur_page())) {         /* (2026-10-27) SCL > HARP: KNOB 2 the set's genre */
            uint32_t g = harp_genre((uint32_t)clamp(TSEL->p[P_CVOI], 0, (int32_t)HARP_NALL - 1));
            draw_column(1, "GENRE", J6_GNAME[g], "", VAL(1u), pick_of(J6_NGEN, g), ICON_AUTO);
            continue;
        }
        if (c == 0u && BAR_PAGE(cur_page())) {          /* (0.9.2) SEQ > BAR: KNOB 1 the bar ("2/4") */
            uint32_t nb = ((uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP) + 15u) / 16u;
            char u[4] = "/1";
            fmt_int(val, (int32_t)ui.bank + 1);
            u[1] = (char)('0' + nb);
            draw_column(0, "BAR", val, u, VAL(0u), pick_of(nb, ui.bank), ICON_X_PATTERN);
            continue;
        }
        if (IS_MROW(cur_page()->id[c])) {               /* (2026-11-02) GLO: a MENU row, its value as the MENU reads it */
            uint32_t r = cur_page()->id[c] - ID_MENU;
            menu_val(r, val);
            if (r < MI_VALUES && menu_n(r) >= 2u && menu_n(r) <= 32u)   /* (a pick: its grid) */
                kn_pn = (uint8_t)menu_n(r), kn_pi = (uint8_t)menu_get(r);
            draw_column(c, menu_short(r), val, "", VAL(c), r < MI_VALUES && menu_n(r) > 1u ?
                        (int32_t)menu_get(r) * 1000 / (int32_t)(menu_n(r) - 1u) : -1, ICON_AUTO);
            continue;
        }
        if (!d || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_MIDI && cur_page()->scope == SC_GLOBAL) {
            str_cpy(val, !usb.up ? "OFF" : usb.config ? "MIDI" : usb.setups ? "ENUM" : usb.sof_seen ? "BUS" : "WAIT", 12);
            unit = "USB";
            draw_column(c, "USB", val, unit, T_THEME, -1, ICON_AUTO);
            continue;
        }
        if (cur_page()->graph != GR_PROG && (act_cols() >> c) & 1u) {
            draw_act_column(c, d->label, T_THEME, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_INFO && cur_page()->scope == SC_GLOBAL) {
            fmt_int(val, (int32_t)(song.cpu_q8 * 100u / 256u));
            unit = "%";
        } else {
            param_format(d, *vp, val, &unit);
            if (sys_load && cur_page()->scope == SC_GLOBAL &&   /* (0.6.29) LOAD LITE / ECO swaps it while it sounds */
                ((cur_page()->id[c] == G_RTYPE && REV_LITE[sys_load % 3u][*vp <= 4 ? *vp : 0] != *vp) ||
                 ((cur_page()->id[c] == G_DPIT || cur_page()->id[c] == G_DSPRY) && *vp)))
                unit = N_LOAD[LOAD_ECO - sys_load % 3u];
            if (sys_load == LOAD_ECO && cur_page()->scope == SC_TRACK && cur_page()->id[c] == P_CHOR && *vp)
                unit = "ECO";                           /* (0.6.30) no chorus: the send goes nowhere */
        }
        card_mot_of(vp);                                /* (a global: never) */
        draw_column(c, d->label, val, unit, VAL(c), d->fmt == F_ENUM && d->max < 2 && !kn_cells() ? -1 : gauge(d, *vp),
                    param_icon(d, *vp));
    }
}
/* the four knobs: cards, or (kn_cells: TRACKS' order on every button's page) the knob cells; f puts them (draw_cards,
 * IDEAS' gen_cards: 2026-11-02, IDEAS kept the cells of the page before) */
static void knobs_of(void (*f)(void))
{
    uint32_t c;
    if (!kn_cells()) {
        f();
        return;
    }
    for (c = 0; c < 4u; c++) {
        kn_buf[c].l[0] = kn_buf[c].u[0] = kn_buf[c].v[0] = 0;
        kn_buf[c].r = -1;
        kn_buf[c].m = kn_buf[c].d = kn_buf[c].n = 0;
    }
    f();
    kn_row(kn_buf);
}
static void draw_columns(void) { knobs_of(draw_cards); }


/* (chord-harp fork) The power-on splash (main.c fm1_main, web/emu yehnah_web.c), was Yehnah's signature and beings,
 * then robots dancing under a disco ball, then WSEQ's beach: (2026-11-02, the user's) HOME's road (ui_road.c rd_splash),
 * T1's robot walking it into the city; (2026-11-03, the user's: like a synthwave title) the scene the whole screen, its
 * sky carried up, and over it the name, "Yehnah", in an 80s arcade pixel font (ui_name_art.h: Press Start 2P), 4 x,
 * hot pink lit from above with a cyan edge and a drop shadow, written in as the knobs' SNAKE fills (down the first
 * column of font pixels, up the next ..) over SPL_WRITE ms, the version small at the top. The scene runs SPL_WRITE +
 * SPL_HOLD ms (splash_write, main.c's splash loop; MENU > ANIM OFF: still, the name whole). Its own colours.
 * (2026-11-03, the user's) The name at 30 degrees, rising left to right, written in more slowly */
#include "ui_name_art.h"
#define SPL_WRITE 1600u                             /* ms: the name written (the two together the splash's length) */
#define SPL_HOLD 900u
#define SPL_BAND 120                                /* the screen drawn in two bands (the canvas holds 240 x 164) */
#define SPL_NS 4                                    /* a font pixel's size */
#define SPL_CY 74                                   /* the name's middle (its left end lower, its right higher) */
static uint8_t spl_anim;                            /* main.c: 1 = the scene moves (splash_write) */
/* the name's font pixel under screen x y (turned 30 degrees about its middle, 120 SPL_CY): its SNAKE order + 1 when it
 * is lit and written (of n), else 0 */
static int32_t splash_at(int32_t x, int32_t y, int32_t n)
{
    float px = (float)x + 0.5f - 120.0f, py = (float)y + 0.5f - (float)SPL_CY;
    float u = (px * 0.8660254f - py * 0.5f) / (float)SPL_NS + (float)(NAME_N * 4), v = (px * 0.5f + py * 0.8660254f) / (float)SPL_NS + 4.0f;
    int32_t gx = (int32_t)(u + 8.0f) - 8, gy = (int32_t)(v + 8.0f) - 8, o;
    if (gx < 0 || gx >= NAME_N * 8 || gy < 0 || gy >= 8 || !(NAME_G[(gx >> 3) * 8 + gy] & (128 >> (gx & 7))))
        return 0;
    o = gx * 8 + (gx & 1 ? 7 - gy : gy);
    return o < n ? o + 1 : 0;
}
static void splash_name(uint32_t ms)                /* the name, ms after the start (in the band cv_oy picks) */
{
    static const uint32_t FILL[8] = {0xffb4e0, 0xff8ccc, 0xff64b4, 0xff3c9c, 0xf02c8c, 0xe02080, 0xc81870, 0xb01464};
    int32_t n = ms >= SPL_WRITE ? 1 << 30 : (int32_t)(ms * NAME_N * 64u / SPL_WRITE), pen = -1, x, y;
    if (n < NAME_N * 64)                                /* the pen: the pixel just written, white */
        for (pen = n - 1; pen >= 0; pen--) {
            int32_t gx = pen >> 3, gy = gx & 1 ? 7 - (pen & 7) : pen & 7;
            if (NAME_G[(gx >> 3) * 8 + gy] & (128 >> (gx & 7)))
                break;
        }
    for (y = SPL_CY - 64; y < SPL_CY + 66; y++) {
        uint16_t c0 = 0;
        int32_t x0 = -1;
        if (y < -cv_oy || y >= -cv_oy + SPL_BAND)
            continue;
        for (x = 26; x <= 214; x++) {                   /* (runs of a colour: 1 the cyan edge, 2 the shadow) */
            int32_t f = x < 214 ? splash_at(x, y, n) : 0;
            uint16_t c = 0;
            if (x < 214) {
                if (f) {
                    int32_t gx = (f - 1) >> 3, gy = gx & 1 ? 7 - ((f - 1) & 7) : (f - 1) & 7;
                    c = rb_565(f - 1 == pen ? 0xffffff : FILL[gy]);
                } else if (splash_at(x - 1, y, n) || splash_at(x + 1, y, n) || splash_at(x, y - 1, n) || splash_at(x, y + 1, n))
                    c = rb_565(0x40f0ff);
                else if (splash_at(x - 2, y - 3, n))
                    c = rb_565(0x2a0838);
            }
            if (c != c0 || x == 214) {
                if (c0)
                    cv_rect(x0, y, x - x0, 1, c0);
                c0 = c, x0 = x;
            }
        }
    }
}
/* the scene at ms after the start, the name over it */
static void splash_scene(uint32_t ms)
{
    int32_t b;
    for (b = 0; b < 240 / SPL_BAND; b++) {
        cv_begin(240, SPL_BAND, T_BG);
        cv_oy = -b * SPL_BAND;
        rd_splash(ms, 240);
        splash_name(spl_anim ? ms : SPL_WRITE);
        if (!b)
            cv_text_in(0, 4, 240, &AF_S, YEHNAH_VERSION, rb_565(0xb4b0e4), rb_565(rd.sky[0]));
        cv_oy = 0;
        cv_blit(0, b * SPL_BAND);
    }
}
static void draw_splash(void)
{
    splash_scene(0);
    lcd_sync();
}
/* main.c's splash loop: the scene ms after the start */
static void splash_write(uint32_t ms)
{
    if (!spl_anim)
        return;
    splash_scene(ms);
    lcd_sync();
}

/* UPDATE MODE countdown (main.c: OCT- + OCT+ held): over everything, the menu and the dialogs too */
static void draw_uboot(void)
{
    static uint8_t shown;
    char d[4] = {(char)('0' + ui.uboot % 10u), 0, 0, 0};
    if (!ui.force && shown == ui.uboot)
        return;
    shown = ui.uboot;
    lcd_clear(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    draw_text_box(0, 76, 240, &AF_M, "UPDATE MODE IN", T_TEXT, 1);
    draw_text_box(0, 102, 240, &AF_M, d, T_THEME, 1);
    {   /* the two keycaps held, and what letting go does */
        int32_t w = kc_w(KC_OCTDN) + 3 + kh_ink(KC_OCTUP, "LET GO TO CANCEL"), x = HALF_UP(240 - w);
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(3), "update-mode keys centred");
        x = cv_keycap(x, 1, KC_OCTDN, T_KEY, T_INK, T_BG) + 3;
        cv_key_hint(x, 1, KC_OCTUP, "LET GO TO CANCEL", 1, T_BG);
        cv_blit(0, 149);
        lcd_sync();
    }
    ui.force = 0;
}

/* the count-in (seq.c, MENU > COUNT-IN; 1.1, Discussion #131): over the page until the sequencer starts. Its bar's four
 * beats counted down, 4 3 2 1, in four cells (the beat now filled THEME, those gone dim), with 2 BARS which bar it is;
 * PLAY stops it. Drawn again on each beat (seq.c's state; the header stays live) */
#define CI_Y 88                                       /* the cells: 4 x 48 px, 8 px apart, 72 tall */
#define CI_H 72
static void draw_countin(void)
{
    static uint32_t shown;
    uint32_t total = cin_total, e = total - cin_left, j, sig = total * 64u + e + 1u;
    char sub[16];
    int full = ui.force || shown / 64u != total;       /* (0.9.0) a beat: its cells and the bar only (was the whole screen) */
    if (!ui.force && shown == sig)
        return;
    shown = sig;
    if (full) {                                       /* (round what it draws, not under it too) */
        lcd_clear(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
        ui.head_sig = ~0u;
        draw_text_box(0, 44, 240, &AF_M, "COUNT-IN", T_TEXT, 1);
    }
    if (total > 4u) {
        str_cpy(sub, "BAR 1 OF 2", sizeof sub);
        sub[4] = (char)('1' + e / 4u % 9u);
        sub[9] = (char)('0' + total / 4u % 10u);
    } else
        str_cpy(sub, "1 BAR", sizeof sub);
    draw_text_box(0, 66, 240, &AF_S, sub, T_MID, 1);
    cv_begin(216, CI_H, T_BG);
    for (j = 0; j < 4u; j++) {
        int32_t x = (int32_t)j * 56;
        char d[2] = {(char)('4' - j), 0};
        int now = j == e % 4u, gone = j < e % 4u;
        uint16_t fill = now ? T_THEME : T_SURF;
        cv_rrect(x, 0, 48, CI_H, 8, fill, T_BG);
        GFX_HOOK_ALIGN(x, 0, x + 48, CI_H, AL_HV, "count-in beat centred in its cell");
        cv_text_in(x, CAP_IN(M, CI_H), 48, &AF_M, d, now ? T_INK : gone ? T_DIM : T_TEXT, fill);
    }
    cv_blit(12, CI_Y);
    if (!full) {
        ui.force = 0;
        return;
    }
    draw_text_box(0, CI_Y + CI_H + 14, 240, &AF_S, "RECORDING STARTS AFTER 1", T_MID, 1);
    {   /* PLAY stops it */
        int32_t x = HALF_UP(240 - kh_ink(KC_PLAY, "STOP"));
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "count-in key centred");
        cv_key_hint(x, 1, KC_PLAY, "STOP", 1, T_BG);
        cv_blit(0, 206);
        lcd_sync();
    }
    ui.force = 0;
}

/* the OCT- / OCT+ dialog: what it does (ui.confirm, ui.confirm_trk) on two lines */
static void confirm_text(char *a, char *b)
{
    uint32_t k = ui.confirm_trk;
    b[0] = 0;
    switch (ui.confirm) {
    case CF_CLEAR_TRK:
        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_PROJ:
        str_cpy(a, "OVERWRITE PROJECT A?", 24);
        a[18] = (char)('A' + (k & 3u));
        if (!project_name(k & 3u, b) || !b[0])          /* the project's name, else what SONG plays from it */
            str_cpy(b, "SONG PATTERN CHANGES", 24);
        break;
    case CF_DEL_ROW:
        str_cpy(a, "DELETE SONG ROW?", 24);
        break;
    case CF_CLEAR_SONG:
        str_cpy(a, "CLEAR SONG ORDER?", 24);
        break;
    case CF_CLEAR_ALL:
        str_cpy(a, "CLEAR ALL SEQUENCES?", 24);
        break;
    case CF_MUT_BACK:                                /* (0.9.3) MUTATE turned OFF on track k (project.c mut_ask_poll) */
        str_cpy(a, "BACK TO BEFORE MUTATE?", 24);
        str_cpy(b, "T1: SOUND AND PATTERN", 24);
        b[1] = (char)('1' + k % NTRK);
        break;
    case CF_CALIB:                                   /* (2026-11-02) GLO > TOOLS */
        str_cpy(a, "OPEN CALIBRATION?", 24);
        break;
    case CF_ABOUT:
        str_cpy(a, "OPEN ABOUT?", 24);
        break;
    case CF_INIT_SYS:                                /* (0.6.40) SAVE > TOOLS 5 */
        str_cpy(a, "NEW PROJECT?", 24);
        str_cpy(b, "UNDO AFTER: REC + D4", 24);
        break;
    case CF_DICE_ALL:                                /* (0.6.21) HOME + SELECT */
        str_cpy(a, "DICE EVERYTHING?", 24);
        str_cpy(b, "UNDO AFTER: REC + D4", 24);
        break;
    case CF_CLEAR_MOTION:
        str_cpy(a, "CLEAR T1 AUTOMATION?", 24); a[7] = (char)('1' + k % NTRK);
        break;
    case CF_CLEAR_PFX:
        str_cpy(a, "CLEAR THE PUNCH LANE?", 24);
        break;
    case CF_OVR_USER:
        str_cpy(a, "OVERWRITE ", 24);
        up_slot_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        up_name(k, b);                               /* the sound stored there */
        break;
    case CF_ERASE_USER:
        str_cpy(a, "ERASE ", 24);
        up_slot_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        up_name(k, b);
        break;
    case CF_ERASE_SEC:                               /* (2026-11-05) SAVE > ERASE */
        str_cpy(a, "ERASE SECTION A?", 24);
        a[14] = (char)('A' + k % 4u);
        str_cpy(b, graph_project_name(k)[0] ? graph_project_name(k) : "NO UNDO", 24);
        break;
    case CF_ERASE_PRJ:
        str_cpy(a, "ERASE PROJECT 1?", 24);
        a[14] = (char)('1' + k % NSONG);
        str_cpy(b, "4 SECTS + SONG: NO UNDO", 24);
        break;
    case CF_LOAD_PAT: {
        char tag[4];
        str_cpy(a, "REPLACE T1 SEQUENCE?", 24);
        a[9] = (char)('1' + k % NTRK);
        pat_label(pat_pick(), tag, b);               /* with the pattern picked */
        break;
    }
    default:
        str_cpy(a, "CLEAR T1 SEQUENCE?", 24);     /* the header (T1..T4) is hidden */
        a[7] = (char)('1' + k % NTRK);
        break;
    }
}
static void gen_cards(void);                            /* (0.8.5) gen.c: GEN in HOME's lens, its cards */
/* (2026-11-02) a NOTICE (the OCT- / OCT+ questions) in the new layout (mockups/notice_layout.py A), the page gone: in the
 * panel a RAISE card under a THEME rule, "?" in the accent, the question, the detail; in the knob cells' row two buttons
 * two cells wide (OCT- NO, OCT+ YES in the accent, underlined); the page row one pill, NOTICE (page_row) */
#define DLG_BTN_W (2 * KN_W + 2)                        /* a button: two knob cells */
static void dlg_button(int32_t x, const char *k, const char *a, int yes)
{
    cv_rrect(x, 0, DLG_BTN_W, TF_KN_H, 4, T_RAISE, T_BG);
    cv_text_on(x + 7, 6 - AF_X_CAP_Y, &AF_X, k, T_DIM, T_RAISE);
    cv_text_on(x + 7, 22 - AF_M_CAP_Y, &AF_M, a, yes ? T_ACCENT : T_TEXT, T_RAISE);
    cv_rect(x + 7, TF_KN_H - 9, DLG_BTN_W - 14, yes ? 2 : 1, yes ? T_ACCENT : T_LINE);
}
static void draw_confirm(void)
{
    char a[24], b[24];
    const aafont_t *tf = &AF_M;
    confirm_text(a, b);
    ui.head_sig = ~0u;
    cv_begin(240, TF_KN_Y - TF_SC_Y, T_BG);            /* the panel: the card */
    cv_rrect(2, 2, 236, TF_SC_H - 4, 5, T_RAISE, T_BG);
    cv_rect(6, 2, 228, 2, T_THEME);
    cv_text_on(12, 14 - AF_M_CAP_Y, &AF_M, "?", T_ACCENT, T_RAISE);
    if (text_w(tf, a) > 220)
        tf = &AF_S;
    GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "dialog question centred");
    cv_text_in(0, 54, 240, tf, a, T_TEXT, T_RAISE);
    if (b[0]) {
        char f[32];
        text_fit(f, sizeof f, b, &AF_S, 220);          /* a name: free text */
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "dialog detail centred");
        cv_text_flags(ink_in(&AF_S, f, 240), 76, &AF_S, f, T_MID, T_RAISE, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    }
    cv_blit(0, TF_SC_Y);
    cv_begin(240, TF_PG_Y - TF_KN_Y, T_BG);            /* the knob cells' row: the buttons */
    dlg_button(1, "OCT -", ui.confirm == CF_DICE_ALL ? "CANCEL" : ui.confirm == CF_MUT_BACK ? "KEEP" : "NO", 0);
    dlg_button(1 + DLG_BTN_W + 2, "OCT +", ui.confirm == CF_MUT_BACK ? "BACK" : "YES", 1);
    cv_blit(0, TF_KN_Y);
    page_row();
}

/* (0.8.5) the end of a lens screen drawn outside the page path (GEN): the second message, the timers, the page row */
static void lz_frame_end(void)
{
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    if (pg_row())
        page_row();
    ui.force = 0;
}
static void ui_draw_page(uint32_t counting);
static void ui_draw(void)
{
    static uint8_t counting;
    ui.frame++;
    if (counting != (uint8_t)seq_counting()) {          /* a count-in began or ended: the page drawn again after it */
        counting = (uint8_t)seq_counting();
        ui.force = 1;
    }
    if (!scr_frame())                                   /* MENU > SCREEN OFF: dark (or waking), nothing drawn */
        return;
    lcd_quiet = lcd_tab_force && !ui.force;             /* (the tablet's repaint alone: gfx.c lcd_clear) */
    if (lcd_tab_force)
        ui.force = 1, lcd_tab_force = 0;
    ui_draw_page(counting);
    lcd_quiet = 0;
    scr_shown();
}
static void hint_tick(void);                            /* (ui_layer.c) */
static void ui_draw_body(uint32_t counting);
static void ui_draw_page(uint32_t counting)
{
    hint_tick();
    ui_draw_body(counting);
}
static void ui_draw_body(uint32_t counting)
{
    int skip = 0;
    if (ui.hint_pg && ui.hint_pg != (ui.home ? 255u : ui.page + 1u)) {   /* (0.9.0) a layer's hint: its own page's only */
        ui.hint_pg = 0;
        ui.msg_t = 0;
        ui.msg2[0] = 0;
    }
    if (ui.uboot) {
        draw_uboot();
        draw_head();
        return;
    }
    if (ui.menu) {
        draw_menu();
        ui.force = 0;
        return;
    }
    if (counting && !dlg_on() && !name_on()) {          /* the count-in: over the page (and a layer) */
        draw_countin();
        draw_head();
        return;
    }
    if (dlg_on()) {                                     /* the OCT- / OCT+ dialog (NOTICE: the page gone) */
        if (ui.force) {
            draw_confirm();
            ui.force = 0;
        }
        draw_head();                                    /* recording remains visible over confirmations */
        return;
    }
    if (name_on()) {                                    /* NAME: a user preset's or a project's (ui_name.c) */
        name_draw();
        return;
    }
    if (ui.layer) {                                     /* a layer's map over the page (ui_layer.c) */
        if (ui.force)
            draw_frame();
        draw_head();
        draw_layer();
        if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
            str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
            ui.msg2[0] = 0;
            ui.msg_t = 60;
        }
        if (ui.hot_t)
            ui.hot_t--;
        if (ui.bpm_t)
            ui.bpm_t--;
        ui.force = 0;
        return;
    }
    if (ui.confirm == CF_DICE_ALL) {                    /* (0.8.5) GEN: HOME's lens, its last level (0.9.2: under
                                                         * a layer's map, a page as the others) */
        if (ui.force)
            draw_frame();
        draw_head();
        knobs_of(gen_cards);
        draw_graph();
        lz_frame_end();
        return;
    }
    if (!ui.home && !page_visible(ui.page)) {          /* an OP page of a track that is not DIGITAL (without
                                                         * YEHNAH_FM4: any track): EDIT 1 */
        ui.page = (uint8_t)page_first(FAM_EDIT);
        page_entered();
    }
    cursor_fix();
    if (ui.force && !skip)
        draw_frame();
    yehnah_dbg.stage = 3;
    if (!skip) {
        draw_head();
        yehnah_dbg.stage = 4;
        draw_columns();
        yehnah_dbg.stage = 5;
        draw_graph();
    }
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {     /* the second message (ui_notices) */
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    yehnah_dbg.stage = 6;
    if (skip) {
        ui.force = 0;
        return;
    }
    if (pg_row())                                       /* (2026-11-02) a button's page: the page row at the foot */
        page_row();
    ui.force = 0;
}
