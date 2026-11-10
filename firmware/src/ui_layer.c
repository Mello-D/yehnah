/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca quick layers: while a page button is held, the keys and KNOB 1..4
 * are its shortcuts and its map shows over the page. One table (LAYERS) drives the gesture, the keys, the knobs,
 * the LEDs and the overlay:
 *   FX   HOLD  the performance effects (perform.c) and the track mutes while held; KNOB 1..4 its macros
 *   (2026-11-02, NAV-LAYOUT.md: GLO's layer gone, held it is a tap: its mutes / solos are HOME > MIXER's KNOB 3, the
 *   levels MIXER's, TAP UNDO REVERT ALL the REC layer's, GLO + PLAY's restart REC + E4)
 *   SCL  SET   (chord-harp fork, 2026-10-27) the 12 keys C4..B4 (white and black) ROOT: the root lit, the scale's
 *              notes breathe; KNOB 1 STRUM (every chord mode's); KNOB 2..4 free (QUANT: the SCL page's KNOB 3)
 *   EDIT SET   (Yehnah: Felucca's engine keys are gone, PRESETS picks engines and sounds) F3 INIT: the track's sound
 *              to its engine's first preset, a DRUM track's DRUM-X kit to the factory one; G3 RECALL: the track's
 *              sound (and a DRUM track's kit) as the section playing stored it, the safe state (project.c
 *              project_recall_sound). Each asks first (the dialog closes the layer); the steps stay. KNOB 1..4 the
 *              EDIT page's four (the engine's first); on a DRUM track black keys 1..8 mute each DRUM-X sound
 *              (eng_drum.c DXM_LANE, saved with the section; lit = sounding)
 *   REC  SET   (1.1.5, on every page; REC's tap still arms / disarms) the white keys from F3: CLEAR (the selected
 *              track's steps and automation, no dialog), CLICK (OFF REC ON, each press the next value); KNOB 1 the
 *              same (TONIC: CLICK LEVEL only in MENU > AUDIO; chord-harp fork: C4 / KNOB 4 COUNT-IN). The settings
 *              are MENU > AUDIO's (ui.c rp_get / rp_put: one store), saved when the layer closes.
 *              (2026-11-02) D4 UNDO (the last big change, project.c undo_do; again: redo), E4 RESTART (from the
 *              top, playing on; stopped: PLAY), F4 REVERT ALL (every track back to MUTATE's anchor). Replaces the old REC held on SEQ (Discussion #91:
 *              the CLEAR Tn SEQUENCE? dialog and its "HOLD: CLEAR" hint). (2026-11-03, SAVE's song layer gone) G4..C5
 *              PLAY A..D (the next bar; two or more in one hold: a quick chain), D5 LOOP / SONG, E5 SONG REC, F5 NAME (USER's
 *              slot, PROJECT's project or section)
 * (2026-11-03, the user's: only FX, SCL and REC have a held state; every other button held past HOLD does nothing when
 * let go. SEQ's TOOLS are the SEQ > TOOLS page (ui_tools.c), SAVE's song layer went: STORE, RECALL, the SONG page and
 * the song knob were PROJECT's already, the rest is REC's)
 * The gesture: let go before HOLD (the menu: 0.3 .. 0.6 s) with nothing else touched: a tap, the button's page.
 * Held past HOLD alone: the map (a peek), letting go does nothing. A key, a knob or a button meanwhile: a combo,
 * the map at once, no tap. Keys pressed with the button down are the layer's (seq.c keyboard_block): silent, no
 * MIDI, never recorded; keys held before stay notes. Only one layer at a time: a second layer button is ignored.
 * PLAY and REC work in every layer; SAVE, HOME, SEQ and the other page buttons are swallowed. In SET
 * layers OCT± do not shift the octave (TONIC: OCT- no longer puts back what the layer changed: no undo). A HOLD key's effect lasts until the key is let go, the map with it. No layer in the menu, a dialog,
 * NAME or the UPDATE MODE countdown. A tap says nothing of the layer (0.9.5: the "HOLD [X] QUICK" hint is gone).
 * TONIC: no lock (Felucca's double tap, Discussion #83, is gone): a layer is open while its button is held. */
#include "ui_tools.c"                                   /* SEQ > TOOLS (the page) */
/* REC's song keys (song_main.c) */
static int section_jump(uint32_t s);
static void quick_chain(const uint8_t *s, uint32_t n);
static void song_rec_toggle(void);
static int project_used(uint32_t slot);
enum { LK_HOLD, LK_SET };
typedef struct {
    uint8_t btn, kind, fam;            /* the button, HOLD / SET, the family whose first page KNOB 1..4 edit */
    const char *head;                  /* (FAM_HOME: the layer's own knobs) the header over the map */
    khint_t foot[3];                   /* the footer's key hints (a third with no word: none) */
} layer_t;
static const layer_t LAYERS[LAYER_N] = {
    {0, 0, 0, "", {{0, 0}, {0, 0}, {0, 0}}},
    {B_FX, LK_HOLD, FAM_HOME, "[FX] HOLD", {{KC_KEYS, "EFFECTS"}, {KC_K14, "MACROS"}, {0, 0}}},   /* (LET GO: the header's HOLD) */
    {B_REC, LK_SET, FAM_HOME, "[REC] SET", {{KC_KEYS, "RECORDING"}, {KC_REC, "DONE"}, {0, 0}}},
    {B_SAVE, LK_SET, FAM_HOME, "[SAVE] SET", {{KC_KEYS, "CLEAR ERASE NEW"}, {0, 0}, {0, 0}}},   /* (2026-11-06) the generic save tools */
    {B_SEQ, LK_SET, FAM_HOME, "[SEQ] SET", {{KC_KEYS, "PATTERN TOOLS"}, {0, 0}, {0, 0}}},   /* (2026-11-10) the pattern tools */
};
#define LY_OPEN 2u                     /* ui.ly_t0: the map opened (no tap any more) */
#define LY_COMBO 4u                    /* .. by a combo */
#define LY_DEAD 8u                     /* .. pressed where there is no layer: does nothing */
static const khint_t FX_LATCH_FOOT[3] = {{KC_KEYS, "ON / OFF"}, {KC_K14, "MACROS"}, {KC_OCTDN, "ALL OFF"}};

static struct {
    uint8_t l, oct;                    /* the layer open; OCT- / OCT+ pressed in a SET layer (bits) */
    uint8_t qc[8], nqc;                /* REC: the sections tapped in this hold (two or more: a quick chain) */
    uint8_t rp_dirty;                  /* REC: a setting changed (settings_save when the layer lets go) */
    uint32_t tap[4];                   /* REC's TAP: the last taps (fm1_ms) */
    uint8_t ntap;
    uint8_t quiet;                     /* #39: a layer closed after it opened: KNOB 1..4 do nothing until quiet_t + */
    uint32_t quiet_t;                  /* LY_QUIET_MS (fm1_ms) */
    uint8_t used;                      /* (0.6.44) a key, a knob or OCT used in the layer since it was armed */
} lys;
#define LY_QUIET_MS 250u

/* (0.6.44) FX LATCH: FX held open and let go locks its layer open, as the macros latch: its keys, knobs and map stay
 * until FX is pressed again (or another layer is held over it, for as long as it is) */
static uint8_t fx_lock;
static int layer_allowed(void) { return !ui.menu && !dlg_on() && !ui.uboot && !name_on() && !ui.hm; }   /* (GEN: a page) */
static uint32_t ly_bit(uint32_t l) { return 1u << panel.btn[LAYERS[l].btn]; }
static uint32_t ly_down(uint32_t l) { return l && (fm1_in.buttons & ly_bit(l)) != 0u; }
static uint32_t layer_bits(void)
{
    uint32_t l, m = 0;
    for (l = LAYER_FX; l < LAYER_N; l++)
        m |= ly_bit(l);
    return m;
}
static uint32_t layer_btn(void) { return LAYERS[ui.layer % LAYER_N].btn; }
static uint32_t layer_open(void);
static const char *layer_head(void)                     /* FX: LATCH, and the MIDI effects' tracks (A#4) */
{
    static const char *const H[3][3] = {{"[FX] HOLD", "[FX] HOLD SYN", "[FX] HOLD DRM"},
                                        {"[FX] LATCH", "[FX] LATCH SYN", "[FX] LATCH DRM"},
                                        {"[FX] LOCKED", "[FX] LOCK SYN", "[FX] LOCK DRM"}};
    return ui.layer == LAYER_FX ? H[fx_lock && !ui.ly ? 2 : perf_latch_on ? 1 : 0][pfx_tgt % 3u] : LAYERS[ui.layer % LAYER_N].head;
}
static uint32_t layer_open(void)
{
    if (ui.ly && (ui.ly_t0 & LY_OPEN) && ly_down(ui.ly) && layer_allowed())
        return ui.ly;
    return fx_lock && !ui.ly && layer_allowed() ? LAYER_FX : 0u;
}
static int layer_set_open(void)                 /* (FX LATCH: FX is one too, OCT- turns all off) */
{
    return layer_open() && (LAYERS[layer_open()].kind == LK_SET || (layer_open() == LAYER_FX && perf_latch_on));
}
static uint32_t layer_held(void) { return ui.ly && ly_down(ui.ly) && layer_allowed(); }

/* each pass, before the keys: what the ISR (seq.c keyboard_block) gives to a layer. Armed: its button; none
 * armed: any layer button not already down (one pressed meanwhile is armed below; one held over from before
 * is ignored, its keys stay notes); armed but dead (pressed under a dialog, a menu or NAME that has closed
 * since): nothing, its keys stay notes and FX starts no effect with no map shown; FX's effects only with FX */
static void layer_masks(void)
{
    uint32_t m = !layer_allowed() || (ui.ly && (ui.ly_t0 & LY_DEAD)) ? 0u
               : ui.ly ? ly_bit(ui.ly) : layer_bits() & ~fm1_in.buttons;
    kb_mask = m;
    perf_mask = m & ly_bit(LAYER_FX);
    fx_lock_on = (uint8_t)(fx_lock && !ui.ly && layer_allowed());
}

/* a layer button pressed (its edge) while none is armed: it is now, a dead one where no layer opens */
static void layer_arm(uint32_t pressed, uint32_t now)
{
    uint32_t l;
    if (ui.ly)
        return;
    if (fx_lock && (pressed & ly_bit(LAYER_FX))) {      /* (0.6.44) FX pressed again: the lock off, nothing else */
        fx_lock = 0;
        ui.force = 1;
        ui.ly = LAYER_FX;
        ui.ly_t0 = (now & ~15u) | 1u | LY_DEAD;
        ui_message("FX UNLOCKED");
        return;
    }
    for (l = LAYER_FX; l < LAYER_N; l++)
        if (pressed & ly_bit(l)) {
            lys.used = 0;
            ui.ly = (uint8_t)l;
            ui.ly_t0 = (now & ~15u) | 1u | (layer_allowed() ? 0u : LY_DEAD);
            return;
        }
}

/* the armed layer lets go (its button up): FX's macros snap back (FX LATCH: they stay); one that
 * opened, or a combo: KNOB 1..4 quiet for a while (#39: the knob still turning is not the page's) */
static void layer_let_go(uint32_t quiet)
{
    if (ui.ly == LAYER_REC && lys.nqc >= 2u)            /* REC: two or more sections tapped: a quick chain */
        quick_chain(lys.qc, lys.nqc);
    lys.nqc = 0;
    if (ui.ly == LAYER_FX && !perf_latch_on)
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    if (ui.ly == LAYER_FX && (ui.ly_t0 & LY_OPEN) && !(ui.ly_t0 & (LY_DEAD | LY_COMBO)) &&
        !lys.used && !kb_layer && layer_allowed()) {
        fx_lock = 1;                                    /* (0.6.44) held open, nothing used, let go: locked open;
                                                         * (0.6.64) whatever FX LATCH (the effects' own: momentary by
                                                         * default) */
        ui_message("FX LOCKED: [FX] TO UNLOCK");
        quiet = 0;
        ui.force = 1;
    }
    if (lys.rp_dirty) {                                 /* REC: its settings kept, as MENU does when it closes */
        lys.rp_dirty = 0;
        settings_save();                                /* (deferred while playing) */
    }
    if (quiet) {
        lys.quiet = 1;
        lys.quiet_t = fm1_ms;
    }
    ui.ly_t0 = 0;
    ui.ly = 0;
}

/* the layer opened */
static void layer_opened(uint32_t l)
{
    lys.l = (uint8_t)l;
    lys.nqc = 0;
}

/* the armed button: 0, or the layer whose button was tapped (let go) */
static uint32_t layer_gesture(uint32_t now, uint32_t combo)
{
    uint32_t *t0 = &ui.ly_t0, l = ui.ly;
    if (!l)
        return 0;
    if (!layer_allowed())
        *t0 |= LY_DEAD;
    if (ly_down(l)) {
        if (!(*t0 & (LY_OPEN | LY_DEAD)) &&
            (combo || now - (*t0 & ~15u) >= (uint32_t)HOLD_MS[settings_hold % 4u] * 1000u * FM1_TICKS_PER_US)) {
            *t0 |= LY_OPEN | (combo ? LY_COMBO : 0u);
            layer_opened(l);
        }
        return 0;
    }
    l = *t0 & (LY_OPEN | LY_DEAD) || combo ? 0u : l;   /* (a knob turned as it was let go: a combo, no tap) */
    layer_let_go(*t0 & LY_OPEN || combo);
    return l;
}

/* #39: KNOB 1..4 belong to no page while a layer lets go: the frame its button is let go (the turns read then
 * were made with it held), while its map still shows (its keys held after it), and LY_QUIET_MS after it closed.
 * Their turns are dropped there (ui_input); before, they edited the page under the layer (an ARP page: ARP on) */
static int layer_knobs_quiet(void)
{
    if (lys.quiet && fm1_ms - lys.quiet_t >= LY_QUIET_MS)
        lys.quiet = 0;
    return layer_allowed() && (ui.ly || ui.layer || lys.quiet);
}

/* the map shows while the button is held open, and after it while its keys are still held */
static void layer_show(void)
{
    uint32_t show = layer_allowed() ? (layer_open() ? layer_open() : kb_layer ? ui.layer : 0u) : 0u;
    if (show != ui.layer) {
        ui.layer = (uint8_t)show;
        ui.force = 1;
    }
}

/* a tap: the button's page (0.9.5: no "HOLD [X] QUICK" hint any more: the layers show themselves when held) */
static void layer_tap(uint32_t l)
{
    if (l == LAYER_REC)                                 /* REC: arms / disarms (no page) */
        (void)rec_tap();
    else
        (void)page_tap(LAYERS[l].btn);
}
typedef char hk_room_t[HK_N <= 20 ? 1 : -1];   /* (bytes 20.. of engine 15's: other settings) */
/* (0.9.5) INIT SYSTEM: the teaching hints start over (none given) */
static void hints_reset(void)
{
    uint32_t k;
    for (k = 0; k < HK_N; k++)
        hint_given(k) = 0;
    ui.hint_k = 0;
    settings_save();
}
/* (0.9.5) a frame: the page's hint waiting (page_entered: MOTION, PUNCH, MUTATE) given once its page has been shown HINT_DWELL_MS (gone with
 * another page: walking past with SELECT says nothing), when nothing else is said or open over it, at most HINT_MAX times
 * ever (hint_given, kept with the settings); on its page only (ui.hint_pg) */
static void hint_tick(void)
{
    uint32_t k = ui.hint_k, at = ui.home ? 255u : ui.page + 1u;
    if (!k)
        return;
    if (ui.hint_at != at) {
        ui.hint_k = 0;
        return;
    }
    if (fm1_ms - ui.hint_ms < HINT_DWELL_MS || ui.msg_t || ui.menu || ui.confirm || ui.layer || name_on() || ui.uboot)
        return;
    ui.hint_k = 0;
    if (hint_given(k) >= HINT_MAX)
        return;
    hint_given(k)++;
    settings_save();                                    /* (the count kept: deferred while playing) */
    if (k == HK_MOTION)
        ui_message("REC + PLAY, TURN A KNOB");          /* (0.6.92) how it is recorded */
    else if (k == HK_PUNCH)
        ui_message("REC + PLAY, HOLD FX + A KEY");      /* (0.6.99) the lane's recording */
    ui.hint_pg = (uint8_t)at;                           /* (0.9.0: this page's only) */
}

static uint32_t snd_id(void) { return TSEL->eng_req | (uint32_t)TSEL->preset << 8 | (uint32_t)TSEL->user << 16; }
/* REC's TAP: from the third tap the tempo of the taps (the last 4); 2 s without one starts over. INT clock only */
static void tap_tempo(void)
{
    uint32_t n = lys.ntap, i;
    if (song.g[G_CLOCK]) {
        ui_message("TAP: CLK IS EXT");
        return;
    }
    if (n && fm1_ms - lys.tap[n - 1u] > 2000u)
        n = 0;
    if (n == 4u) {
        for (i = 0; i < 3u; i++)
            lys.tap[i] = lys.tap[i + 1u];
        n = 3;
    }
    lys.tap[n++] = fm1_ms;
    lys.ntap = (uint8_t)n;
    if (n >= 3u && lys.tap[n - 1u] != lys.tap[0]) {
        song.g[G_BPM] = (int16_t)clamp((int32_t)(60000u * (n - 1u) / (lys.tap[n - 1u] - lys.tap[0])),
                                       GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
    }
}

/* REC: CLEAR, the selected track's steps and automation (and ARP's latched chord), no dialog, as SEQ TOOLS' CLEAR
 * (up to 1.1.4 REC held on SEQ asked first, CF_CLEAR_SEQ) */
static void rec_clear(void)
{
    track_t *t = TSEL;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (seq_is_empty(t) && !motion_count(t)) {
        ui_message("NOTHING TO CLEAR");
        return;
    }
    undo_mark();                                        /* (0.6.21) UNDO: REC + D4 */
    load_begin(t, LOAD_PAT);
    track_defaults_steps(t);
    load_end(t);
    latch_drop_req |= (uint8_t)(1u << (t - trk));       /* (0.6.19) the latched chord too (events_block) */
    {   /* (0.6.28) which track: the others play on */
        char cb[16] = "T1 CLEARED";
        cb[1] = (char)('1' + (t - trk));
        ui_say(cb, "");
        str_cpy(ui.msg2, "REC+G5: CLEAR ALL", sizeof ui.msg2);
    }
    ui.force = 1;
}
/* REC's settings, row r (G3, KNOB 1): CLICK as MENU > AUDIO steps it (ui.c rp_*). TONIC: COUNT-IN and CLICK LEVEL
 * left the layer (MENU > AUDIO still has them). (chord-harp fork, 2026-10-26) and A3 / KNOB 2 MODE (ONE PASS / LOOP: what
 * a REC tap records, ui.c rec_loop), B3 TAP (the tempo), KNOB 3 BPM, C4 / KNOB 4 COUNT-IN (OFF 1 BAR 2 BARS,
 * MENU > AUDIO's); (2026-11-02, were GLO + G4, GLO + PLAY, GLO + B4) D4 UNDO, E4 RESTART, F4 REVERT ALL; (2026-11-03,
 * SAVE held's until then) G4..C5 PLAY A..D, D5 LOOP / SONG, E5 SONG REC, F5 NAME (song_key) */
#define RK_MODE 2u                                     /* the layer's white keys: F3 CLEAR, G3 CLICK, A3 MODE, B3 TAP, */
#define RK_TAP 3u                                      /* C4 COUNT-IN, D4 UNDO, E4 RESTART, F4 REVERT ALL */
#define RK_CIN 4u
#define RK_UNDO 5u
#define RK_RST 6u
#define RK_RVA 7u                                      /* (the key F4 was REVERT ALL: gone with MUTATE) */
#define RK_PLAY 8u                                     /* G4 A4 B4 C5: PLAY A..D */
#define RK_LOOP 12u                                    /* D5: LOOP / SONG (what PLAY plays) */
#define RK_SREC 13u                                    /* E5: SONG REC */
#define RK_ALL 15u                                     /* G5: CLEAR ALL (every track, the lane: it asks first) */
#define RK_NAME 14u                                    /* F5: NAME (USER's slot, PROJECT's project or section) */
static const char *const REC_MODE_N[2] = {"1 PASS", "LOOP"};
#define RL_N 2u                                        /* the menu's settings here: row 0 CLICK (KNOB 1), 1 COUNT-IN (KNOB 4) */
static const uint8_t RL_F[RL_N] = {RP_CLICK, RP_COUNTIN};
static const char *const RL_LABEL[RL_N] = {"CLICK", "COUNT-IN"};
static const char *rl_name(uint32_t r, uint32_t v) { return r ? COUNTIN_N[v % 3u] : CLICK_N[v % 3u]; }
static void rec_set(uint32_t r, uint32_t v)
{
    if (v == rp_get(RL_F[r]))
        return;
    rp_put(RL_F[r], v);
    lys.rp_dirty = 1;
}
static void rec_mode_set(uint32_t v)
{
    rec_loop = (uint8_t)(v != 0u);
    ui_say("REC MODE ", rec_loop ? "LOOP" : "ONE PASS");
}
static int restart_ok(void) { return !chain_busy() && !(song.g[G_CLOCK] && song.playing); }
static int name_here(void) { return !ui.home && cur_page()->graph == GR_SLOTS; }   /* (USER page gone) */
/* REC's song keys (SAVE held's until 2026-11-03), white key p: RK_PLAY.. the sections A..D (on the next bar; tapped in
 * a row in one hold: a quick chain, layer_let_go), LOOP / SONG, SONG REC, NAME */
static void song_key(uint32_t p)
{
    uint32_t k;
    if (p - RK_PLAY < 4u) {
        p -= RK_PLAY;
        if (lys.nqc < sizeof lys.qc)
            lys.qc[lys.nqc++] = (uint8_t)p;
        if (lys.nqc == 1u)
            (void)section_jump(p);                       /* (the first: at once, as a single tap) */
        else {
            char b[24] = "CHAIN";
            for (k = 0; k < lys.nqc && k < 8u; k++) {
                char c[3] = {' ', (char)('A' + lys.qc[k]), 0};
                str_cpy(b + str_len(b), c, sizeof b - str_len(b));
            }
            ui_message(b);
        }
    } else if (p == RK_LOOP) {
        live.mode ^= 1u;
        ui_message(live.mode ? "PLAY: THE SONG" : "PLAY: THE LOOP");
    } else if (p == RK_SREC) {
        song_rec_toggle();
    } else if (p == RK_NAME) {                          /* (0.9.2) USER's slot, PROJECT's project or section */
        if (name_here())
            name_rename();
        else
            ui_message("NAME: ON USER OR PROJECT");
    }
}
/* (2026-11-06) SAVE held: the generic save tools that were SAVE > TOOLS, ERASE and USER, white keys (each asks first, as before):
 * F3 CLEAR PAT, G3 CLEAR SONG, A3 DELETE ROW (SONG EDIT / ARRANGE's row), C4 ERASE SECTION (PROJECT's section), D4 ERASE
 * PROJECT, E4 NEW PROJECT, G4 SAVE SOUND (the user slot the track's sound came from, else the first free one), A4 ERASE SOUND */
enum { SK_CLRPAT = 0, SK_CLRSONG = 1, SK_DELROW = 2, SK_ERASEC = 4, SK_ERAPRJ = 5, SK_NEW = 6, SK_SAVSND = 8, SK_ERASND = 9 };
static const char *const SK_NAME[10] = {"CLEAR PAT", "CLEAR SONG", "DEL ROW", "", "ERASE SECT", "ERASE PROJ", "NEW PROJ", "",
                                        "SAVE SND", "ERASE SND"};
static uint32_t sv_free_slot(void)
{
    uint32_t k;
    for (k = 0; k < UP_SLOTS; k++)
        if (!up_used(k))
            return k;
    return UP_SLOTS;
}
static int sv_ok(uint32_t p)
{
    uint32_t k = (uint32_t)song.g[G_SLOT] - 1u;
    switch (p) {
    case SK_CLRPAT: return !chain_busy() && (!seq_is_empty(TSEL) || motion_count(TSEL) != 0u);
    case SK_CLRSONG: return !chain_busy() && chain_config.count != 0u;
    case SK_DELROW: return !chain_busy() && ui.song_row < chain_config.count;
    case SK_ERASEC: return !song.playing && project_used(k);
    case SK_ERAPRJ: return !song.playing && (project_used(0) || project_used(1) || project_used(2) || project_used(3) || chain_config.count);
    case SK_NEW: return !chain_busy();
    case SK_SAVSND: return !transport_busy();
    case SK_ERASND: return !transport_busy() && user_of(TSEL) < UP_SLOTS;
    default: return 0;
    }
}
static void sv_key(uint32_t p)
{
    uint32_t k = (uint32_t)song.g[G_SLOT] - 1u, u;
    if (p >= 10u || !SK_NAME[p][0])
        return;
    if (!sv_ok(p)) {
        ui_message(chain_busy() || song.playing ? "STOP TO EDIT" : "NOTHING TO DO");
        return;
    }
    switch (p) {
    case SK_CLRPAT: confirm_open(CF_CLEAR_SEQ, song.sel); break;
    case SK_CLRSONG: confirm_open(CF_CLEAR_SONG, song.sel); break;
    case SK_DELROW: confirm_open(CF_DEL_ROW, ui.song_row); break;
    case SK_ERASEC: confirm_open(CF_ERASE_SEC, k); break;
    case SK_ERAPRJ: confirm_open(CF_ERASE_PRJ, song_cur); break;
    case SK_NEW: confirm_open(CF_INIT_SYS, song.sel); break;
    case SK_SAVSND:
        u = user_of(TSEL);
        if (u < UP_SLOTS) confirm_open(CF_OVR_USER, u);
        else if ((u = sv_free_slot()) < UP_SLOTS) { ui.uslot = (uint8_t)u; name_open(NK_USER_SAVE, u); }
        else ui_message("NO FREE SOUND SLOT");
        break;
    default: confirm_open(CF_ERASE_USER, user_of(TSEL)); break;
    }
}
/* SEQ held (2026-11-10): the pattern tools (ui_tools.c) on the selected track, white keys: F3 REVERSE, G3 SHIFT <, A3 SHIFT >,
 * B3 BEAT (DRUM), G4 CLR LANE, A4 REV LANE, B4 FILL LANE (the lane: the DRUM black keys 1..8), G5 CLR STEP (the cursor step and its
 * locks, the cursor on to the next). Each at once, a message; none has an undo */
enum { LS_OFF, LS_SEL, LS_HELD, LS_WAIT, LS_DIM, LS_MUTE };
#define SQ_STEP 15u
static int sq_tool(uint32_t p)                          /* white key p -> TL_*, or 0xFF */
{
    return p == 0u ? TL_REV : p == 1u ? TL_LEFT : p == 2u ? TL_RIGHT : p == 3u ? TL_BEAT :
           p == 8u ? TL_LCLR : p == 9u ? TL_LREV : p == 10u ? TL_LFILL : 0xFF;
}
static int sq_ok(uint32_t k)
{
    uint32_t p = key_place(k);
    int tool;
    if (key_black(k))
        return drum_track(TSEL) && p < NLANE;
    tool = sq_tool(p);
    if (p == SQ_STEP)
        return !chain_busy();
    return tool != 0xFF && !chain_busy() && (tool < TL_BEAT || drum_track(TSEL));
}
static uint32_t sq_keyname(int blk, uint32_t p, char *nm)
{
    static const char *const N[16] = {"REVERSE", "SHIFT <", "SHIFT >", "BEAT", "", "", "", "", "CLR LN", "REV LN", "FILL LN",
                                      "", "", "", "", "CLR STP"};
    uint32_t busy = chain_busy() ? LS_DIM : LS_OFF;
    if (blk) {
        if (!drum_track(TSEL) || p >= NLANE)
            return LS_OFF;
        str_cpy(nm, drum_lane_abbr(TSEL, p), 12);
        return ui.lane % NLANE == p ? LS_SEL : LS_OFF;
    }
    if (p >= 16u || !N[p][0])
        return LS_OFF;
    str_cpy(nm, N[p], 12);
    if (p == SQ_STEP)
        return busy;
    return chain_busy() || (sq_tool(p) >= TL_BEAT && !drum_track(TSEL)) ? LS_DIM : LS_OFF;
}
static void sq_key(uint32_t k)
{
    uint32_t p = key_place(k);
    int tool;
    const char *m;
    if (key_black(k)) {                                 /* the DRUM lane the lane tools act on */
        if (drum_track(TSEL) && p < NLANE) {
            ui.lane = (uint8_t)p;
            ui_say("LANE ", drum_lane_abbr(TSEL, p));
            ui.force = 1;
        }
        return;
    }
    tool = sq_tool(p);
    if (p != SQ_STEP && tool == 0xFF)
        return;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (p == SQ_STEP) {
        track_t *t = TSEL;
        uint32_t len = tl_len(t);
        if (ui.cursor >= len)
            return;
        step_clear(&t->step[ui.cursor]);
        (void)motion_clear_locks(t, ui.cursor);
        ui.cursor = (uint8_t)((ui.cursor + 1u) % len);
        ui.bank = (uint8_t)(ui.cursor / 16u);
        ui_message("STEP CLEARED");
        ui.force = 1;
        return;
    }
    if (tool >= TL_BEAT && !drum_track(TSEL)) {
        ui_message("DRUM TRACKS ONLY");
        return;
    }
    if ((m = tl_do(TSEL, (uint32_t)tool, ui.lane)) != 0)
        ui_message(m);
    ui.force = 1;
}
static void rec_key(uint32_t p)
{
    if (p == RK_ALL) {                                  /* (2026-11-10, was SEQ > TOOLS' CLEAR ALL) */
        if (chain_busy()) ui_message("STOP TO EDIT");
        else confirm_open(CF_CLEAR_ALL, song.sel);
        return;
    }
    if (p >= RK_PLAY) {
        song_key(p);
        return;
    }
    if (p == 0u) {
        rec_clear();
        return;
    }
    if (p == RK_UNDO) {                                 /* (0.6.21) the last big change; again: redo */
        if (chain_busy())
            ui_message("STOP TO EDIT");
        else if (undo_do())
            ui_message("NOTHING TO UNDO");
        else
            ui_message("UNDONE (AGAIN: REDO)");
        return;
    }
    if (p == RK_RST) {                                  /* from the top, playing on; stopped: PLAY */
        if (chain_busy())
            ui_message("STOP TO RESTART");
        else if (song.g[G_CLOCK] && song.playing)
            ui_message("RESTART: CLK IS EXT");
        else
            transport_req = song.playing ? 3u : 1u;
        return;
    }
    if (p == RK_MODE) {
        rec_mode_set(!rec_loop);
        return;
    }
    if (p == RK_TAP) {
        if (song.g[G_CLOCK])
            ui_message("EXT CLOCK");
        else
            tap_tempo();
        return;
    }
    /* (2026-11-09) G3 CLICK and C4 COUNT-IN left the layer: GLO > MAIN has them (and BPM) */
}

/* a key pressed in layer l (k: 0 = F3 .. 26 = G5) */
/* (0.4) MACRO DICE: random routes from M1..M4 to what each track's sound has (its engine's parameters, CUT,
 * SHP, the sends, a little PITCH), two a track into its free MOD slots, the macros spread so each moves two tracks:
 * the instrument invites a turn from the start, the routes there to tune (MOD) or keep (saved with the project).
 * mode 0 (each power-on, project.c autosave_boot then main.c): only a track with no macro route yet, so a session
 * restored keeps its own; 3 (gen.c, DICE ALL): the macros' routes rolled again on every track not muted */
static void macro_dice(uint32_t mode)
{
    static const uint8_t DST[] = {MD_CUT, MD_SHP, MD_E1, MD_E1 + 1, MD_E1 + 2, MD_E1 + 3, MD_E1 + 4, MD_E1 + 5, MD_E1 + 6,
                                  MD_E1 + 7, MD_E1, MD_E1 + 1, MD_E1 + 2, MD_E1 + 3, MD_DLY, MD_REV, MD_PITCH, MD_DIST};   /* (the
                                                         * engine's, its first four twice as likely) */
    uint32_t k, j, n;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t has = 0, used = 0;
        for (j = 0; j < 4u; j++) {
            int16_t *m = &t->p[P_M1SRC + 3u * j];
            if (m[0] >= MS_M1 && m[0] < MS_M1 + 4) {
                if (mode) m[0] = m[1] = m[2] = 0;
                else has = 1;
            }
            if (m[0]) used |= 1u << m[1];
        }
        if (has || (mode && t->p[P_MUTE]))              /* (0.6.21) DICE ALL (mode 3): every track not muted */
            continue;
        for (j = 0, n = 0; j < 4u && n < 2u; j++) {
            int16_t *m = &t->p[P_M1SRC + 3u * j];
            uint32_t d, tries;
            if (m[0])
                continue;
            for (tries = 0; tries < 16u; tries++) {     /* a destination the sound has, not used on this track */
                const param_desc_t *pd;
                d = DST[rng() % sizeof DST];
                if ((used >> d) & 1u)
                    continue;
                if (d >= MD_E1 && d < MD_E1 + 8u &&
                    ((pd = track_desc(t, P_E0 + (d - MD_E1))) == 0 || pd->max <= pd->min || pd->label[0] == '-'))
                    continue;
                break;
            }
            if (tries == 16u)
                continue;
            used |= 1u << d;
            m[0] = (int16_t)(MS_M1 + (k + 2u * n) % 4u);
            m[1] = (int16_t)d;
            m[2] = (int16_t)((d == MD_PITCH ? 4 + (int32_t)(rng() % 9u) : 24 + (int32_t)(rng() % 40u)) * (rng() & 1u ? 1 : -1));
            n++;
        }
    }
}

/* (0.4) the session's macros (main.c after autosave_boot; the emulator too): the random numbers seeded by
 * something of this power-on, then the routes the tracks have none of */
static void session_dice(uint32_t seed)
{
    rng_state ^= seed * 2654435761u;
    if (!rng_state)
        rng_state = 0x1234567u;
    macro_dice(0);
}

static void layer_key(uint32_t l, uint32_t k)
{
    uint32_t p = key_place(k);
    if (l == LAYER_REC) {
        if (!key_black(k) && p <= RK_ALL)
            rec_key(p);
    } else if (l == LAYER_SAVE) {
        if (!key_black(k))
            sv_key(p);
    } else if (l == LAYER_SEQ) {
        sq_key(k);
    }
}
/* each pass: the keys the layer got now; the solo: HOME > MIXER's (perform.c) */
static void layer_keys(uint32_t keys)
{
    uint32_t k, l = layer_open() ? layer_open() : ui.layer;
    if (keys) lys.used = 1;
    for (k = 0; keys && k < 27u; k++)
        if ((keys >> k) & 1u)
            layer_key(l, k);
    perf_solo = ui.msolo;
}

/* KNOB 1..4 in the open layer: its own (FX macros, REC's), else the four of its page */
static void layer_knob(uint32_t k, int32_t s)
{
    uint32_t l = layer_open();
    lys.used = 1;
    if (l == LAYER_FX) {                                /* perform.c perf_k, not recorded */
        if (k == 3u)
            s = -s;                                     /* DEPTH (100 - perf_k[3]) rises to the right (#40: it fell) */
        perf_k[k] = (int8_t)clamp(perf_k[k] + accel(EN_K1 + k, s, k ? 100 : 200), k ? 0 : -100, 100);   /* (#126) */
    } else if (l == LAYER_REC) {                        /* MODE (2026-11-09: CLICK, BPM, COUNT-IN are GLO > MAIN's) */
        if (k == 1u && s)
            rec_mode_set(!rec_loop);
    } else if (l == LAYER_SAVE) {                       /* (no knobs) */
    }
}

/* OCT- / OCT+ pressed with a SET layer's button down (FX LATCH: FX's) before it opened: a combo, so it opens now,
 * before oct_taps and layer_oct read it (else the press was nobody's: no put back / ALL OFF, no octave) */
static void layer_oct_open(uint32_t pressed)
{
    uint32_t l = ui.ly, oct = 1u << panel.btn[B_OCTDN] | 1u << panel.btn[B_OCTUP];
    if ((pressed & oct) && l && !(ui.ly_t0 & (LY_OPEN | LY_DEAD)) && layer_held() &&
        (LAYERS[l].kind == LK_SET || (l == LAYER_FX && perf_latch_on))) {
        ui.ly_t0 |= LY_OPEN | LY_COMBO;
        layer_opened(l);
    }
}

/* OCT- / OCT+ in a SET layer: no octave (TONIC: no put back; FX LATCH: OCT- all off). Returns the taps left */
static uint32_t layer_oct(uint32_t pressed, uint32_t oct)
{
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP], mine;
    if (layer_set_open())
        lys.oct |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    if (ui.ly && (pressed & (1u << dn | 1u << up)))
        lys.used = 1;                                   /* (0.6.44: an OCT with it: no FX lock) */
    mine = oct & lys.oct;
    if ((mine & 1u) && lys.l == LAYER_FX) {             /* OCT- (FX LATCH): every effect and macro off */
        perf_latched = 0;
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
        ui_message("FX ALL OFF");
    }
    lys.oct &= (uint8_t)(((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1);
    return oct & ~mine;
}

/* --------------------------------------------------------- the LEDs --- */
/* lit = in effect now (held, the value, a track sounding), breathing = can be pressed (*br: dark .. ~60 % and
 * back, ~1.1 s, hal/fm1_input.h fm1_led_breath; was a hard 250 ms blink, #119), dark = nothing there */
static int rec_clearable(void) { return !seq_is_empty(TSEL) || motion_count(TSEL) != 0u; }
static uint32_t layer_leds(uint32_t *br)
{
    uint32_t k, m = 0, n = 0, l = ui.layer, held = perf_held | perf_latched, ok = perf_avail();
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), b = (uint32_t)key_black(k), on = 0, can = 0, e;
        if (l == LAYER_FX) {                            /* effects breathe, held lit, a too-long REPEAT dark */
            e = perf_key(k);
            can = e < PF_N && ((ok >> e) & 1u);
            on = can && ((held >> e) & 1u);
        } else if (l == LAYER_SAVE && !b && p < 10u && SK_NAME[p][0]) {   /* the tools that would do something breathe */
            can = (uint32_t)sv_ok(p);
        } else if (l == LAYER_SEQ) {                    /* the tools that would do something breathe; the lane picked lit */
            can = sq_ok(k);
            on = b && drum_track(TSEL) && p < NLANE && ui.lane % NLANE == p;
        } else if (l == LAYER_REC && !b && p == RK_ALL) {
            can = !chain_busy();
        } else if (l == LAYER_REC && !b && p >= RK_PLAY && p <= RK_NAME) {   /* the section playing lit, the saved
                                                         * ones and the actions breathe */
            on = (p - RK_PLAY < 4u && live.cur == (int8_t)(p - RK_PLAY)) || (p == RK_SREC && live.srec) ||
                 (p == RK_LOOP && live.mode);
            can = p - RK_PLAY < 4u ? project_used(p - RK_PLAY) : p == RK_NAME ? name_here() : 1;
        } else if (l == LAYER_REC && !b && p <= RK_RVA) {   /* the click / count-in on, LOOP: lit, else they breathe; CLEAR: */
            on = p == RK_MODE && rec_loop;   /* something to clear, not
                                                         * while a song plays; TAP breathes; UNDO: one to undo */
            can = p == RK_RVA ? 0u : p == RK_UNDO ? undo_ok && !chain_busy() : p == RK_RST ? restart_ok() : p == 1u || p == RK_CIN ? 0u : p ? 1u : !chain_busy() && rec_clearable();
        }
        m |= on << k;
        n |= (can & !on) << k;
    }
    if (br)
        *br = n;
    return m;
}

/* ------------------------------------------------------ the overlay --- */
/* (0.8.9) The layers as the approved LENTE mock-up's CAPAS (MAP . KNOBS . STATE) in the skins' language (the mock-up
 * opciones_skin.html): the header "[REC] SET" in plain words; the cards KNOB 1..4 in their colours (COLORS on top,
 * BOTTOM / HUD the knob row); MAP the keyboard itself, F3 .. G5, 16 white keys and 11 black, each named upright by what it
 * does there (none: an empty key): a key that can be pressed RAISE (white) or tinted (black), the value now THEME, held
 * the accent, a muted track KEY, unavailable its name DIM, its note under it; KNOBS the four as gauges in their knobs'
 * colours over the layer's keys (the keycaps' words); STATE what is on, as chips. (Until 0.8.8: cells with note names,
 * the Felucca map.) The keys and knobs do what they did: only the drawing is new */
static const char *const PF_NAME[PF_M1] = {"RPT 1/8", "RPT 1/16", "RPT 1/32", "LPF", "HPF",   /* the audio ones (perform.c) */
    "OCT-", "OCT+", "1/2 TEMPO", "DEC-", "DEC+", "STUT 1/8", "STUT 1/16", "ATK+", "ARP", "RANDOM"};   /* the MIDI ones (pfx.c) */
static const char W_NOTE[16] = {'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G'};
static const uint8_t PK_BLK[11] = {1, 2, 3, 5, 6, 8, 9, 10, 12, 13, 15};   /* a black key: left of white key n */
#define LMC_TW 140                                       /* MACRO's routes: their width (x 92 .. 232) */
/* key p (white, or black: blk) of layer l: its name into nm (12; "" none), its state */
static uint32_t ly_keyname(uint32_t l, int blk, uint32_t p, char *nm)
{
    uint32_t busy = chain_busy() ? LS_DIM : LS_OFF;
    nm[0] = 0;
    if (l == LAYER_FX) {
        uint32_t held = perf_kill ? 0u : perf_held | perf_latched, e = blk ? PF_M1 + p : p;
        static const char *const TG[3] = {"TRK ALL", "TRK SYN", "TRK DRM"};
        if (blk && p == 7u) {
            str_cpy(nm, TG[pfx_tgt % 3u], 12);
            return LS_OFF;
        }
        if (blk ? p >= NTRK : p >= PF_M1)
            return LS_OFF;
        if (blk) {
            str_cpy(nm, "MUTE T1", 12);
            nm[6] = (char)('1' + p);
            return (held >> e) & 1u ? LS_MUTE : LS_OFF;
        }
        str_cpy(nm, PF_NAME[p], 12);
        return !((perf_avail() >> e) & 1u) ? LS_DIM : !((held >> e) & 1u) ? LS_OFF :
               ((perf_act >> e) & 1u) || ((PF_MIDI >> e) & 1u) ? LS_HELD : LS_WAIT;
    }
    if (l == LAYER_SAVE) {
        if (blk || p >= 10u || !SK_NAME[p][0])
            return LS_OFF;
        str_cpy(nm, SK_NAME[p], 12);
        return sv_ok(p) ? LS_OFF : LS_DIM;
    }
    if (l == LAYER_SEQ)
        return sq_keyname(blk, p, nm);
    if (l == LAYER_REC) {
        if (blk || p > RK_ALL)
            return LS_OFF;
        if (p == RK_ALL) {
            str_cpy(nm, "CLEAR ALL", 12);
            return chain_busy() ? LS_DIM : LS_OFF;
        }
        if (p - RK_PLAY < 4u) {                         /* the song keys */
            p -= RK_PLAY;
            str_cpy(nm, "PLAY A", 12);
            nm[5] = (char)('A' + p);
            return !project_used(p) ? LS_DIM : live.req == (int8_t)p ? LS_WAIT : live.cur == (int8_t)p ? LS_SEL : busy;
        }
        if (p == RK_LOOP) {
            str_cpy(nm, live.mode ? "SONG" : "LOOP", 12);
            return live.mode ? LS_SEL : LS_OFF;
        }
        if (p == RK_SREC) {
            str_cpy(nm, "SONG REC", 12);
            return live.srec == 2u ? LS_HELD : live.srec ? LS_WAIT : LS_OFF;
        }
        if (p == RK_NAME) {
            str_cpy(nm, "NAME", 12);
            return name_here() ? LS_OFF : LS_DIM;
        }
        if (p == RK_UNDO) {
            str_cpy(nm, "UNDO", 12);
            return undo_ok && !chain_busy() ? LS_OFF : LS_DIM;
        }
        if (p == RK_RST) {
            str_cpy(nm, "RESTART", 12);
            return restart_ok() ? LS_OFF : LS_DIM;
        }
        if (p == RK_CIN || p == 1u)                     /* (CLICK, COUNT-IN: GLO > MAIN) */
            return LS_OFF;
        if (p == RK_MODE) {
            str_cpy(nm, REC_MODE_N[rec_loop & 1u], 12);
            return rec_loop ? LS_SEL : LS_OFF;
        }
        if (p == RK_TAP) {
            str_cpy(nm, "TAP", 12);
            return song.g[G_CLOCK] ? LS_DIM : LS_OFF;
        }
        str_cpy(nm, "CLEAR", 12);
        return chain_busy() || !rec_clearable() ? LS_DIM : LS_OFF;
    }
    return LS_OFF;
}
/* a key's fill and its name's ink by its state (named: n) */
static uint16_t pk_fill(uint32_t st, int blk, int n, uint16_t *ink)
{
    *ink = st == LS_DIM ? T_DIM : blk ? T_TEXT : T_TEXT;
    if (st == LS_HELD) { *ink = T_INK; return T_ACCENT; }
    if (st == LS_SEL || st == LS_WAIT) { *ink = T_INK; return st == LS_WAIT ? ux_mix(T_RAISE, T_THEME, 55) : T_THEME; }
    if (st == LS_MUTE) { *ink = T_INK; return T_KEY; }
    if (!n)
        return blk ? T_BG : ux_mix(T_BG, T_TEXT, 7);
    return blk ? ux_mix(T_BG, T_THEME, 34) : ux_mix(T_BG, T_TEXT, 17);
}
/* MAP (2026-11-09, held menus): a GRID of cells, one for each key that does something in the layer, in key order: its name, its
 * note in the corner, the state as the keys' fill (lit = on / held, DIM = unavailable). The keys' LEDs light with it */
static void ly_grid(uint32_t l)
{
    static const char NN[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    uint32_t k, n = 0, i = 0, cols, rows;
    int32_t cw, ch, ph = (int32_t)graph_h() - 8;
    char nm[12], nt[6];
    for (k = 0; k < 27u; k++) {
        (void)ly_keyname(l, key_black(k), key_place(k), nm);
        n += nm[0] != 0;
    }
    if (!n)
        return;
    cols = n <= 4u ? 2u : n <= 9u ? 3u : 4u;
    rows = (n + cols - 1u) / cols;
    cw = (224 - (int32_t)(cols - 1u) * 3) / (int32_t)cols;
    ch = (ph - (int32_t)(rows - 1u) * 3) / (int32_t)rows;
    if (ch > 36)
        ch = 36;
    for (k = 0; k < 27u; k++) {
        int blk = key_black(k);
        uint32_t st = ly_keyname(l, blk, key_place(k), nm), semi = 53u + k;
        uint16_t ink, fill;
        int32_t x, y;
        if (!nm[0])
            continue;
        x = 8 + (int32_t)(i % cols) * (cw + 3);
        y = 4 + (int32_t)(i / cols) * (ch + 3);
        i++;
        fill = pk_fill(st, blk, 1, &ink);
        cv_rrect(x, y, cw, ch, 4, fill, T_SURF);
        if (ch >= 28) {                                 /* roomy: the name above, the note in the corner */
            cv_clip_text(x + 5, y + 4 - (cols < 4u ? AF_S_CAP_Y : AF_X_CAP_Y), cols < 4u ? &AF_S : &AF_X, nm, ink, fill, cw - 8);
            str_cpy(nt, NN[semi % 12u], sizeof nt);
            {
                uint32_t e = str_len(nt);
                nt[e] = (char)('0' + (int32_t)(semi / 12u) - 1);
                nt[e + 1u] = 0;
            }
            cv_text_r(x + cw - 4, y + ch - 4 - AF_X_CAP_H - AF_X_CAP_Y, &AF_X, nt, ink == T_INK ? ink : T_DIM, fill);
        } else {                                        /* tight: the name alone, centred */
            cv_clip_text(x + 5, y + (cols < 4u ? CAP_IN(S, ch) : CAP_IN(X, ch)), cols < 4u ? &AF_S : &AF_X, nm, ink, fill, cw - 8);
        }
    }
}
static void ly_own_cards(void)                          /* FX REC: their own four (into the knob cells: knobs_of) */
{
    uint32_t l = ui.layer % LAYER_N, c;
    char val[12];
    if (l == LAYER_FX) {                                /* FILTER CRUSH THROW DEPTH */
        int32_t m = perf_k[0];
        fmt_int(val, m < 0 ? -m : m);
        draw_column(0, "FILTER", m ? val : "OFF", m < 0 ? "LP" : m > 0 ? "HP" : "", m ? VAL(0u) : T_DIM, (m + 100) * 5,
                    ICON_CUTOFF);
        fmt_int(val, perf_k[1]);
        draw_column(1, "CRUSH", perf_k[1] ? val : "OFF", perf_k[1] ? "%" : "", perf_k[1] ? VAL(1u) : T_DIM,
                    perf_k[1] * 10, ICON_BITS);
        fmt_int(val, perf_k[2]);
        draw_column(2, "THROW", perf_k[2] ? val : "OFF", perf_k[2] ? "%" : "", perf_k[2] ? VAL(2u) : T_DIM,
                    perf_k[2] * 10, ICON_DELAY);
        fmt_int(val, 100 - perf_k[3]);
        draw_column(3, "DEPTH", val, "%", VAL(3u), (100 - perf_k[3]) * 10, ICON_MIX);
    } else if (l == LAYER_SAVE || l == LAYER_SEQ) {     /* (no knobs: four empty cells) */
        for (c = 0; c < 4u; c++)
            draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
    } else if (l == LAYER_REC) {                        /* MODE (the others are GLO > MAIN's) */
        for (c = 0; c < 4u; c++)
            if (c == 1u)
                draw_column(c, "MODE", REC_MODE_N[rec_loop & 1u], "", VAL(c), pick_of(2u, rec_loop & 1u), ICON_LOOP);
            else
                draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
    }
}
static void layer_cards(uint32_t l)
{
    {
        knobs_of(ly_own_cards);
    }
}

/* (0.8.5) a layer as a lens (MAP . STATE, ALGORITHM while it is held; 2026-11-02 the redesign: the panel, the four in the
 * knob cells, MAP STATE in the page row; KNOBS gone): MAP the keys' map, STATE the keys' words and what is on. The
 * header "[FX] HELD" */
static const khint_t *layer_foot(uint32_t l)
{
    return l == LAYER_FX && perf_latch_on ? FX_LATCH_FOOT : LAYERS[l].foot;
}
/* STATE: "REC . HELD", the keys' words ("KEYS RECORDING . REC DONE": KNOBS' until 2026-11-02), then what is on as chips,
 * two a row (on: a THEME stripe, TEXT; off: DIM) */
static uint32_t lst_n __attribute__((section(".pool")));
static void lst_chip(const char *a, const char *v, int on)
{
    int32_t x = 4 + (int32_t)(lst_n & 1u) * 117, y = 25 + (int32_t)(lst_n >> 1) * 14;
    uint16_t bg = on ? T_TINT : T_RAISE;
    char b[24];
    if (lst_n >= 16u)
        return;
    lst_n++;
    str_cpy(b, a, sizeof b);
    if (v) {
        str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
        str_cpy(b + str_len(b), v, sizeof b - str_len(b));
    }
    cv_rrect(x, y, 114, 13, 3, bg, T_SURF);
    if (on)
        cv_rect(x, y + 2, 2, 9, T_THEME);
    cv_clip_text(x + 6, y + 3 - AF_X_CAP_Y, &AF_X, b, on ? T_TEXT : T_DIM, bg, 104);
}
static void layer_state(uint32_t l)
{
    static const char *const LN[LAYER_N] = {"", "FX", "REC", "SAVE", "SEQ"};
    const khint_t *ft = layer_foot(l);
    uint32_t k, e;
    char b[48], v[8];
    str_cpy(b, LN[l], sizeof b);
    str_cpy(b + str_len(b),  " \xB7 HELD", sizeof b - str_len(b));
    cv_text(6, 4 - AF_X_CAP_Y, &AF_X, b, T_THEME);
    if (l == LAYER_FX && fx_lock)
        cv_text_r(234, 4 - AF_X_CAP_Y, &AF_X, "[FX] LOCKED", T_ACCENT, T_SURF);
    b[0] = 0;
    for (k = 0; k < 3u; k++)
        if (ft[k].act) {
            if (b[0])
                str_cpy(b + str_len(b), " \xB7 ", sizeof b - str_len(b));
            str_cpy(b + str_len(b), KC[ft[k].key % KC_COUNT].label, sizeof b - str_len(b));
            str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
            str_cpy(b + str_len(b), ft[k].act, sizeof b - str_len(b));
        }
    cv_clip_text(6, 14 - AF_X_CAP_Y, &AF_X, b, T_MID, T_SURF, 228);
    lst_n = 0;
    if (l == LAYER_FX) {                                /* the lock, the tracks, the latch, the effects held */
        uint32_t held = perf_kill ? 0u : perf_held | perf_latched;
        static const char *const TG[3] = {"ALL", "SYN", "DRM"};
        lst_chip("LOCK", 0, fx_lock);
        lst_chip("TRACKS", TG[pfx_tgt % 3u], 1);
        lst_chip("FX LATCH", perf_latch_on ? "ON" : "OFF", perf_latch_on);
        for (e = 0; e < PF_M1; e++)
            lst_chip(PF_NAME[e], 0, (held >> e) & 1u);
    } else if (l == LAYER_REC) {
        b[0] = 'T'; b[1] = (char)('1' + song.sel); b[2] = 0;
        lst_chip("ARMED", b, (song.rec >> song.sel) & 1u);
        lst_chip("MODE", rec_loop ? "LOOP" : "ONE PASS", 1);
        lst_chip(b, rec_clearable() ? "SEQUENCE" : "EMPTY", rec_clearable());
        lst_chip("UNDO", undo_ok ? "READY" : "-", undo_ok);
        v[0] = live.cur >= 0 ? (char)('A' + live.cur) : '-'; v[1] = 0;   /* (the song keys) */
        lst_chip("SECTION", v, live.cur >= 0);
        str_cpy(b, "CHAIN ", sizeof b);
        for (k = 0; k < lys.nqc && k < 8u; k++) { char c[2] = {(char)('A' + lys.qc[k]), 0}; str_cpy(b + str_len(b), c, sizeof b - str_len(b)); }
        lst_chip(lys.nqc >= 2u ? b : "CHAIN -", 0, lys.nqc >= 2u);
        lst_chip(live.mode ? "SONG" : "LOOP", 0, 1);
        lst_chip("SONG REC", live.srec == 2u ? "ON" : live.srec ? "ARM" : "OFF", live.srec != 0u);
        if (chain.running) {
            fmt_int(v, (int32_t)chain.row + 1);
            lst_chip("ROW", v, 1);
            fmt_int(v, chain.remaining);
            lst_chip("BARS LEFT", v, 1);
        }
    } else if (l == LAYER_SEQ) {                        /* what the keys would act on */
        b[0] = 'T'; b[1] = (char)('1' + song.sel); b[2] = 0;
        lst_chip(b, seq_is_empty(TSEL) ? "EMPTY" : "SEQUENCE", !seq_is_empty(TSEL));
        fmt_int(v, (int32_t)tl_len(TSEL));
        lst_chip("LEN", v, 1);
        fmt_int(v, (int32_t)ui.cursor + 1);
        lst_chip("STEP", v, 1);
        if (drum_track(TSEL))
            lst_chip("LANE", drum_lane_abbr(TSEL, ui.lane % NLANE), 1);
    } else if (l == LAYER_SAVE) {                       /* what the keys would act on */
        v[0] = (char)('A' + (song.g[G_SLOT] - 1) % 4); v[1] = 0;
        lst_chip("SECTION", v, project_used((uint32_t)song.g[G_SLOT] - 1u));
        fmt_int(v, (int32_t)ui.song_row + 1);
        lst_chip("ROW", v, ui.song_row < chain_config.count);
        b[0] = 'T'; b[1] = (char)('1' + song.sel); b[2] = 0;
        lst_chip(b, sv_ok(SK_CLRPAT) ? "SEQUENCE" : "EMPTY", sv_ok(SK_CLRPAT));
        lst_chip("SONG", chain_config.count ? "ROWS" : "EMPTY", chain_config.count != 0u);
        lst_chip("SOUND", user_of(TSEL) < UP_SLOTS ? "USER" : "PRESET", user_of(TSEL) < UP_SLOTS);
    } else {                                            /* the four knobs' values; the track's sound; DRUM's sounds */
        for (k = 0; k < 4u; k++)
            if (ly_card[k].l[0])
                lst_chip(ly_card[k].l, ly_card[k].v, 1);
        b[0] = 'T'; b[1] = (char)('1' + song.sel); b[2] = ' ';
        str_cpy(b + 3, trk_engp(TSEL)->name, sizeof b - 3);
        lst_chip(b, 0, 1);
        sound_name(TSEL, b);
        if (b[0])
            lst_chip(b, 0, 1);
    }
}
static void draw_layer(void)
{
    uint32_t l = ui.layer % LAYER_N, sig = l * 7919u + ux.gen * 977u;
    layer_cards(l);
    if (l == LAYER_FX)
        sig += (perf_kill ? 0u : perf_held | perf_latched) * 31u + perf_latch_on * 11u + perf_act * 131u + perf_avail() * 7u;
    else if (l == LAYER_REC)
        sig += (uint32_t)ui_rec_prefs * 31u + (uint32_t)chain_busy() * 3u + (uint32_t)rec_clearable() * 5u + song.sel * 977u +
               rec_loop * 7919u + (uint32_t)song.g[G_BPM] * 65537u + undo_ok * 104729u + (uint32_t)song.g[G_CLOCK] * 13u +
               song.playing * 17u +   /* (the song keys) */
               (uint32_t)(live.cur + 1) * 23u + (uint32_t)(live.req + 1) * 131u + live.srec * 7u + live.mode * 11u +
               lys.nqc * 4099u + (chain.running ? chain.row * 13u + chain.remaining * 17u : 0u) + (uint32_t)name_here() * 29u +
               up_gen * 37u;
    else if (l == LAYER_SEQ)
        sig += (uint32_t)chain_busy() * 3u + (uint32_t)drum_track(TSEL) * 5u + ui.lane * 7u + ui.cursor * 11u + tl_len(TSEL) * 13u +
               (uint32_t)seq_is_empty(TSEL) * 17u + song.sel * 977u;
    else if (l == LAYER_SAVE)
        sig += (uint32_t)chain_busy() * 3u + song.playing * 5u + chain_config.count * 7u + ui.song_row * 11u + (uint32_t)rec_clearable() * 13u +
               (uint32_t)song.g[G_SLOT] * 17u + (user_of(TSEL) < UP_SLOTS) * 19u + up_gen * 37u + (uint32_t)project_used((uint32_t)song.g[G_SLOT] - 1u) * 23u;
    sig += (ui.lylv & 1u) * 0x9E3779B1u + (ui.lylv & 1u ? str_hash(7u, ly_card[0].v) + str_hash(9u, ly_card[1].v) * 3u +
                                     str_hash(11u, ly_card[2].v) * 5u + str_hash(13u, ly_card[3].v) * 7u : 0u);
    if (ui.force || sig != ui.layer_sig) {
        ui.layer_sig = sig;
        cv_begin(240, graph_h(), T_BG);
        cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);
        cv_bg = T_SURF;
        if (ui.lylv & 1u)                               /* STATE: what is on */
            layer_state(l);
        else                                            /* MAP: the keyboard */
            ly_grid(l);
        cv_blit(0, graph_y());
    }
    page_row();                                         /* (2026-11-02: the page row, MAP STATE) */
}
