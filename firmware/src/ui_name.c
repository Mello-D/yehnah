/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: NAME, naming a user preset or a project on the device (included by ui_input.c).
 *
 * Where: SAVE > USER, SAVE (KNOB 4, OCT+) and SAVE > PROJECT, SAVE: after the OVERWRITE? dialog when the slot is
 * used, the NAME screen opens before anything is written; OCT+ writes, OCT- cancels (back to the page, nothing
 * written). EDIT on those pages renames the selected slot (only its name is written). The name is prefilled:
 * USER the sound's own (the user preset it came from) or the automatic one ("DIGITAL 14"), PROJECT the current
 * project's (as loaded or last saved; none: "PROJECT A"), so SAVE, OCT+, OCT+ saves with the default name.
 * Writing needs the transport stopped (STOP TO SAVE; the screen stays).
 *
 * The keys type and never sound, record or send MIDI (seq.c keyboard_block: song.grid 2). Upper case, at most 12:
 *   white keys   ABC (letters): AB CD EF GH IJK LM NO PQ RS TU VW XYZ, then 123 456 789 0-. ; phone style: a tap
 *                types the group's first character, another tap of the same key within 0.8 s the next one
 *                (cycling); another key or 0.8 s without a tap keeps it and moves on.
 *                123 (digits and symbols): 1 2 3 4 5 6 7 8 9 0 - . _ / # + , one tap each.
 *   black keys   by name, in both octaves: F# cursor left, G# SPACE, A# cursor right, C# DELETE (the character
 *                before the cursor; held: repeats, as the arrows do), D# ABC / 123.
 *   KNOB 1       the cursor; KNOB 2 the character at the cursor (SPACE A..Z 0..9 - . _ / # +; at the end: a new one).
 *   LEDs         every key that types or edits is lit; the key whose letters are being cycled blinks.
 * Spaces at either end are dropped when it is written; an empty name: USER the automatic one, PROJECT none.
 * (0.8.5's LYRIC mode gone with VOICE's words: the chord-harp fork, 2026-11-02.) */
enum { NK_NONE, NK_USER_SAVE, NK_USER_RENAME, NK_PROJ_SAVE, NK_PROJ_RENAME };
#define NM_LEN 12u
#define NM_TAP_MS 800u                                 /* multi-tap: the next letter within this */
#define NM_REP_MS 450u                                 /* a held arrow / DELETE repeats after this, */
#define NM_RATE_MS 90u                                 /* .. every this */
enum { NB_LEFT, NB_SPACE, NB_RIGHT, NB_DEL, NB_MODE, NB_NONE };
static const char *const NM_ABC[16] = {"AB", "CD", "EF", "GH", "IJK", "LM", "NO", "PQ", "RS", "TU", "VW", "XYZ",
                                       "123", "456", "789", "0-."};
static const char NM_NUM[17] = "1234567890-._/#+";
static const char NM_SET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._/#+";   /* KNOB 2 */
static struct {
    uint8_t kind;                                      /* NK_*: NK_NONE = closed */
    uint8_t slot;
    uint8_t len, cur;                                  /* the name's length; the cursor 0..len */
    uint8_t num;                                       /* the white keys: 0 ABC, 1 123 */
    uint8_t key;                                       /* the white key (place + 1) of the letter being cycled, 0 none */
    uint8_t tap;                                       /* .. that letter's index in its group */
    uint8_t rep;                                       /* the key (+ 1) of a held arrow / DELETE, 0 none */
    uint32_t t, rep_t;                                 /* fm1_ms of the last tap; of the next repeat */
    uint32_t sig;                                      /* drawn-state cache */
    char s[NM_LEN + 1u];
    char ph[NM_LEN + 1u];                              /* what an empty name saves as / shows ("PROJECT A") */
} nm __attribute__((section(".pool")));                /* (zero-initialised; main loop only: off the audio code's .bss) */

static int name_on(void) { return nm.kind != NK_NONE; }
static void name_close(void)
{
    if (nm.kind)
        ui.force = 1;
    nm.kind = NK_NONE;
}
/* the keys: 2 = NAME's (silent), 1 = the DRUM grid, 0 = notes (seq.c keyboard_block reads song.grid) */
static uint32_t keys_mode(void) { return name_on() ? 2u : (uint32_t)grid_on(); }

static const char *nm_group(uint32_t p)                /* white key place p's characters */
{
    static char one[2];
    if (!nm.num)
        return NM_ABC[p & 15u];
    one[0] = NM_NUM[p & 15u];
    return one;
}
static uint32_t nm_black(uint32_t k)                   /* black key k's function: by its name (F# .. D#) */
{
    switch ((k + 5u) % 12u) {
    case 6: return NB_LEFT;
    case 8: return NB_SPACE;
    case 10: return NB_RIGHT;
    case 1: return NB_DEL;
    case 3: return NB_MODE;
    default: return NB_NONE;
    }
}

static void name_open(uint32_t kind, uint32_t slot)
{
    char b[16];
    nm.kind = (uint8_t)kind;
    nm.slot = (uint8_t)slot;
    nm.num = nm.key = nm.rep = 0;
    b[0] = 0;
    if (kind == NK_USER_SAVE || kind == NK_USER_RENAME) {
        uint32_t e = kind == NK_USER_RENAME ? up_engine(slot) : TSEL->eng_req;
        up_auto_name(nm.ph, e, slot);
        if (kind == NK_USER_RENAME)
            up_name(slot, b);
        else if (user_of(TSEL) < UP_SLOTS)             /* the sound's own name */
            up_name(user_of(TSEL), b);
        else
            str_cpy(b, nm.ph, sizeof b);
    } else {
        str_cpy(nm.ph, "PROJECT A", sizeof nm.ph);
        nm.ph[8] = (char)('A' + (slot & 3u));
        if (kind == NK_PROJ_RENAME)
            project_name(slot, b);
        else
            project_cur_name(b);
    }
    str_cpy(nm.s, b, sizeof nm.s);
    nm.len = nm.cur = (uint8_t)str_len(nm.s);
    ui.force = 1;
}

/* SAVE > USER / PROJECT, EDIT: rename the selected slot */
static void name_rename(void)
{
    int user = cur_page()->graph == GR_USER;
    uint32_t k = user ? ui.uslot : (uint32_t)song.g[G_SLOT] - 1u;
    if (!(user ? up_used(k) : project_used(k)))
        ui_message("EMPTY SLOT");
    else if (transport_busy())
        ui_message("STOP TO SAVE");
    else
        name_open(user ? NK_USER_RENAME : NK_PROJ_RENAME, k);
}

static void nm_commit(void)                            /* the letter being cycled is kept: the cursor past it */
{
    if (nm.key) {
        nm.key = 0;
        nm.cur++;
    }
}
static int nm_insert(char c)                           /* at the cursor (it stays on it); 0 = full */
{
    uint32_t i;
    if (nm.len >= NM_LEN) {
        ui_message("NAME FULL");
        return 0;
    }
    for (i = nm.len; i > nm.cur; i--)
        nm.s[i] = nm.s[i - 1u];
    nm.s[nm.cur] = c;
    nm.s[++nm.len] = 0;
    return 1;
}
static void nm_white(uint32_t p)
{
    const char *g = nm_group(p);
    uint32_t n = str_len(g);
    if (nm.key == p + 1u && fm1_ms - nm.t < NM_TAP_MS) {   /* the same key again: its next character */
        nm.tap = (uint8_t)((nm.tap + 1u) % n);
        nm.s[nm.cur] = g[nm.tap];
        nm.t = fm1_ms;
        return;
    }
    nm_commit();
    if (!nm_insert(g[0]))
        return;
    if (n > 1u) {
        nm.key = (uint8_t)(p + 1u);
        nm.tap = 0;
        nm.t = fm1_ms;
    } else {
        nm.cur++;
    }
}
static void nm_do(uint32_t f)                          /* a black key's function */
{
    uint32_t i;
    nm_commit();
    switch (f) {
    case NB_LEFT:
        if (nm.cur)
            nm.cur--;
        break;
    case NB_RIGHT:
        if (nm.cur < nm.len)
            nm.cur++;
        break;
    case NB_SPACE:
        if (nm_insert(' '))
            nm.cur++;
        break;
    case NB_DEL:
        if (!nm.cur)
            break;
        for (i = --nm.cur; i < nm.len; i++)
            nm.s[i] = nm.s[i + 1u];
        nm.len--;
        break;
    case NB_MODE:
        nm.num ^= 1u;
        break;
    default:
        break;
    }
}
static void nm_knob(uint32_t k, int32_t s)             /* KNOB 1 the cursor, KNOB 2 the character there */
{
    int32_t i, n = (int32_t)sizeof NM_SET - 1;
    nm_commit();
    if (k == 0u) {
        nm.cur = (uint8_t)clamp((int32_t)nm.cur + s, 0, nm.len);
        return;
    }
    if (nm.cur == nm.len && !nm_insert(' '))           /* at the end: a new character (from SPACE) */
        return;
    for (i = 0; i < n && NM_SET[i] != nm.s[nm.cur]; i++)
        ;
    i = i < n ? i : 0;
    nm.s[nm.cur] = NM_SET[((i + s) % n + n) % n];
}

/* OCT+: the name (spaces at its ends dropped) is written; the screen stays while that is refused (playing) */
static void name_ok(void)
{
    char b[NM_LEN + 1u];
    uint32_t a = 0, z;
    nm_commit();
    for (z = nm.len; z && nm.s[z - 1u] == ' '; z--)
        ;
    while (a < z && nm.s[a] == ' ')
        a++;
    for (z -= a, b[z] = 0; z--;)
        b[z] = nm.s[a + z];
    if (transport_busy()) {
        ui_message("STOP TO SAVE");
        return;
    }
    switch (nm.kind) {
    case NK_USER_SAVE:
        up_ui_named(2, nm.slot, b);
        break;
    case NK_USER_RENAME:
        up_ui_named(3, nm.slot, b);
        break;
    case NK_PROJ_SAVE:
        project_save_as(nm.slot, b);
        break;
    default:
        project_rename(nm.slot, b);
        break;
    }
    name_close();
}

/* one UI frame of NAME (ui_input.c): note edges, OCT taps (bit 0 OCT-, bit 1 OCT+) */
static void name_input(uint32_t notes, uint32_t oct)
{
    uint32_t k;
    int32_t s;
    song.grid = 2;                                     /* (already: keys_mode) */
    if (nm.key && fm1_ms - nm.t >= NM_TAP_MS)          /* 0.8 s without a tap: the letter is kept */
        nm_commit();
    for (k = 0; k < 27u; k++) {
        if (!((notes >> k) & 1u))
            continue;
        if (!key_black(k)) {
            nm_white(key_place(k));
            continue;
        }
        nm_do(nm_black(k));
        if (nm_black(k) == NB_LEFT || nm_black(k) == NB_RIGHT || nm_black(k) == NB_DEL) {
            nm.rep = (uint8_t)(k + 1u);
            nm.rep_t = fm1_ms + NM_REP_MS;
        }
    }
    if (nm.rep && !((fm1_in.notes >> (nm.rep - 1u)) & 1u))
        nm.rep = 0;
    else if (nm.rep && (int32_t)(fm1_ms - nm.rep_t) >= 0) {
        nm_do(nm_black(nm.rep - 1u));
        nm.rep_t = fm1_ms + NM_RATE_MS;
    }
    for (k = 0; k < 2u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0)
            nm_knob(k, s);
    enc_drop();                                        /* the other knobs do nothing here */
    if (oct & 2u)
        name_ok();
    else if (oct & 1u)
        name_close();                                  /* cancel: nothing written, back to the page */
}

/* the key LEDs: every key that does something lit, the key whose letters are cycling blinks */
static uint32_t name_leds(void)
{
    uint32_t k, m = 0, blink = ((fm1_ms / 250u) & 1u) == 0u;
    for (k = 0; k < 27u; k++) {
        uint32_t on = key_black(k) ? nm_black(k) != NB_NONE : nm.key != key_place(k) + 1u || blink;
        m |= on << k;
    }
    return m;
}

/* ----------------------------------------------------------- drawing --- */
/* (2026-11-02) NAME in the new layout (mockups/name_layout.py A): the header stays; in the panel a RAISE card under a THEME
 * rule: "SAVE U07" / "NAME PROJECT A" (a lyric: "LYRIC T1" and its length) and ABC / 123, the 12 cells (M: the letter being
 * cycled a THEME cell (INK), the cursor an ACCENT bar, an empty name its placeholder in DIM), the cycling key's letters
 * (the one typed THEME, the next ACCENT) or a hint, the white keys as two rows of 8 (the left octave, then the right one;
 * the cycling key THEME), the black keys' functions as five KEY cells under their names. The knob cells: CURSOR, CHAR,
 * LEN, OCT + with SAVE / WRITE in the accent (playing: STOP TO SAVE); the page row one pill, NAME (ui_draw.c
 * page_row). No icons */
#define NM_CX(i) (12 + 18 * (int32_t)(i))              /* cell i: 17 x 24 at y 22 of the panel */
static void nm_title(char *b)
{
    str_cpy(b, nm.kind == NK_USER_SAVE || nm.kind == NK_PROJ_SAVE ? "SAVE " : "NAME ", 8);
    if (nm.kind <= NK_USER_RENAME)
        up_slot_label(b + 5, nm.slot);
    else
        str_cpy(b + 5, nm.ph, 12);                     /* "PROJECT A" */
}
static void nm_draw_field(void)                        /* the knob cells (every frame: their values roll) */
{
    static kn_t k[4];
    uint32_t c, i = 0, busy = transport_busy();
    char ch = nm.cur < nm.len ? nm.s[nm.cur] : ' ';
    for (c = 0; c < 4u; c++) {
        k[c].l[0] = k[c].u[0] = k[c].v[0] = 0;
        k[c].r = -1;
        k[c].m = k[c].d = k[c].pn = k[c].pi = 0;
        k[c].n = 1;                                     /* (a name: snaps, never rolls) */
    }
    str_cpy(k[0].l, "CURSOR", sizeof k[0].l);
    fmt_int(k[0].v, (int32_t)nm.cur + 1);
    k[0].r = nm.len ? (int32_t)(nm.cur * 1000u / nm.len) : 0;
    k[0].n = 0;
    while (NM_SET[i] && NM_SET[i] != ch)
        i++;
    str_cpy(k[1].l, "CHAR", sizeof k[1].l);
    str_cpy(k[1].v, ch == ' ' ? "SPACE" : (char[2]){ch, 0}, sizeof k[1].v);
    k[1].r = (int32_t)(i * 1000u / (sizeof NM_SET - 2u));
    str_cpy(k[2].l, "LEN", sizeof k[2].l);
    fmt_int(k[2].v, nm.len);
    str_cpy(k[2].u, "/12", sizeof k[2].u);
    k[2].r = (int32_t)(nm.len * 1000u / NM_LEN);
    k[2].n = 0;
    str_cpy(k[3].l, "OCT +", sizeof k[3].l);
    k[3].d = (uint8_t)busy;                             /* (no value: d only redraws the row when STOP TO SAVE comes and goes) */
    if (kn_row(k)) {                                    /* OCT +'s word in its pixels' place */
        int32_t vy = KN_FILL_H + CAP_IN(S, TF_KN_H - KN_NAME_H - KN_FILL_H);
        cv_begin(KN_W, TF_KN_H - KN_NAME_H, T_BG);
        cv_text_in(0, CAP_IN(M, KN_FILL_H), KN_W, &AF_M, busy ? "STOP" : nm.kind == NK_USER_SAVE || nm.kind == NK_PROJ_SAVE ? "SAVE" : "WRITE", busy ? T_TEXT : T_ACCENT, T_BG);
        if (busy)
            cv_text_in(0, vy, KN_W, &AF_S, "TO SAVE", T_MID, T_BG);
        cv_blit(1 + 3 * (KN_W + 2), TF_KN_Y + KN_NAME_H);
    }
}
static void nm_draw_panel(void)
{
    static const char *const FN[5] = {"F#", "G#", "A#", "C#", "D#"};
    uint32_t i, empty = nm.len == 0u;
    char b[24];
    cv_begin(240, TF_KN_Y - TF_SC_Y, T_BG);
    cv_rrect(2, 2, 236, TF_SC_H - 4, 5, T_RAISE, T_BG);
    cv_rect(6, 2, 228, 2, T_THEME);
    cv_bg = T_RAISE;
    nm_title(b);
    cv_text(10, 9 - AF_S_CAP_Y, &AF_S, b, T_THEME);
    cv_text_r(229, 9 - AF_S_CAP_Y, &AF_S, nm.num ? "123" : "ABC", T_ACCENT, T_RAISE);
    for (i = 0; i < NM_LEN; i++) {                     /* the name: 12 cells */
        int32_t x = NM_CX(i);
        char c[2] = {empty ? nm.ph[i] : i < nm.len ? nm.s[i] : 0, 0};
        uint16_t bg = T_SURF, fg = empty ? T_DIM : T_TEXT;
        if (i == nm.cur && nm.key) {                   /* the letter being cycled */
            bg = T_THEME;
            fg = T_INK;
        }
        cv_rect(x, 22, 17, 24, bg);
        if (i == nm.cur && !nm.key)                    /* the cursor: a bar under its cell */
            cv_rect(x + 2, 42, 13, 2, T_ACCENT);
        if (c[0] && c[0] != ' ') {
            GFX_HOOK_ALIGN(x, 22, x + 17, 46, AL_HV, "name field letter in its cell");
            cv_text_in(x, 22 + CAP_IN(M, 24), 17, &AF_M, c, fg, bg);
        }
    }
    if (nm.cur >= NM_LEN)                         /* full, the cursor past the end: a bar */
        cv_rect(NM_CX(NM_LEN) - 1, 22, 2, 24, T_ACCENT);
    if (nm.key) {                                      /* the cycling key's characters: typed THEME, next ACCENT */
        const char *g = nm_group(nm.key - 1u);
        uint32_t n = str_len(g);
        for (i = 0; i < n; i++) {
            int32_t x = 12 + 14 * (int32_t)i;
            char c[2] = {g[i], 0};
            int typed = i == nm.tap, next = i == (nm.tap + 1u) % n;
            uint16_t f = typed ? T_THEME : T_SURF;
            cv_rect(x, 50, 12, 12, f);
            cv_text_in(x, 50 + CAP_IN(X, 12), 12, &AF_X, c, typed ? T_INK : next ? T_ACCENT : T_TEXT, f);
        }
        cv_text(16 + 14 * (int32_t)n, 50 + CAP_IN(S, 12), &AF_S, "TAP AGAIN: NEXT", T_MID);
    } else
        cv_text(12, 50 + CAP_IN(S, 12), &AF_S, nm.num ? "ONE TAP, ONE CHARACTER" : "TAP AGAIN: NEXT LETTER", T_MID);
    GFX_HOOK_ALIGN(4, 0, 236, 0, AL_H | AL_CELLS | AL_N(8), "name keys' row in the panel");
    for (i = 0; i < 16u; i++) {                        /* the white keys: the left octave, then the right one */
        int32_t x = 9 + 28 * (int32_t)(i % 8u), y = i < 8u ? 66 : 85;
        int act = nm.key == i + 1u;
        uint16_t f = act ? T_THEME : T_SURF;
        cv_rect(x, y, 26, 17, f);
        GFX_HOOK_ALIGN(x, y, x + 26, y + 17, AL_HV, "name white key letters centred");
        cv_text_in(x, y + CAP_IN(S, 17), 26, &AF_S, nm_group(i), act ? T_INK : T_TEXT, f);
    }
    for (i = 0; i < 5u; i++) {                         /* the black keys, by name */
        int32_t x = 9 + 45 * (int32_t)i, y = 106;
        if (!i)
            GFX_HOOK_ALIGN(4, 0, 236, 0, AL_H | AL_CELLS | AL_N(5), "name keys' row in the panel");
        cv_rect(x, y, 42, 26, T_KEY);
        GFX_HOOK_ALIGN(x, 0, x + 42, 0, AL_H | AL_PASS, "name black key name / use centred across");
        cv_text_in(x, y + 4 - AF_X_CAP_Y, 42, &AF_X, FN[i], T_INK, T_KEY);
        GFX_HOOK_ALIGN(x, 0, x + 42, 0, AL_H | AL_PASS, "name black key name / use centred across");
        cv_text_in(x, y + 14 - AF_S_CAP_Y, 42, &AF_S, i == NB_LEFT ? "<" : i == NB_RIGHT ? ">" : i == NB_SPACE ? "SPACE" :
                   i == NB_DEL ? "DEL" : nm.num ? "ABC" : "123", T_INK, T_KEY);
    }
    cv_blit(0, TF_SC_Y);
}
static void name_draw(void)
{
    uint8_t st[7] = {nm.kind, nm.slot, nm.len, nm.cur, nm.num, nm.key, nm.tap};
    uint32_t sig = fnv(fnv(2166136261u, st, sizeof st), nm.s, sizeof nm.s) + ux.gen * 7919u;
    if (ui.force) {
        cv_begin(240, TF_PG_Y - TF_KN_Y - TF_KN_H, T_BG);   /* (the rows between the knob cells and the page row) */
        cv_blit(0, TF_KN_Y + TF_KN_H);
        nm.sig = ~sig;
    }
    draw_head();
    if (sig != nm.sig) {
        nm.sig = sig;
        nm_draw_panel();
    }
    nm_draw_field();
    page_row();
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    ui.force = 0;
}
