/* (2026-11-19) SCL > USER: the player's own chord sets (HARP's USER genre, harp.c harp_usr: 6 sets of 12 chords, each a root, a
 * shape and a bass; the flash tail of the second user preset object, upreset.c) written from a LIST of the chords there are
 * on a root, as the preset browser lists the sounds (it replaces the USER and COPY pages of 2026-11-18). The last page of SCL
 * (left of HARP).
 *   the list  one row a chord: its number, the chord, the set it comes from. Each chord once (the first set that has it; J-6
 *             chords as the nearest of the 35 shapes).
 *   KNOB 1 the row, KNOB 2 the root (C .. B: only the chords on it show), KNOB 3 the user set written to (U1..U6; the track
 *   plays it, so the keys sound it). KNOB 4 BASS: the slash bass of the chord picked, LIST (as the set has it), NONE or a note
 *   (C .. B): the preview, the rows and the write take it (a bass no set has, or a listed one stripped).
 *   the top F# (the LATCH key) previews the row's chord, always lit (seq.c harp_pv); scrolling plays nothing.
 *   OCT+   arms the write: "HIT A KEY", the twelve chord keys lit: a key that holds a chord solid, an empty one breathing
 *          (harp_usr's bit 14); the key hit takes the row's chord, the prompt ends. OCT-, a knob or leaving the page cancels.
 * KNOB 4 changes nothing in the list. The other keys' LEDs are dark here (the harp's map is not needed). The sets go to flash when the page is left (the transport
 * stopped). State (ui.c): uc_r, uc_row, uc_tot, uc_w. Included by yehnah.c before ui_draw.c. */
static char uc_b[4][2][16];                              /* the four cards' value and unit texts (kept: the cards are drawn after) */
typedef struct { uint8_t set, idx; } uc_ref_t;           /* a row: chord idx of set */
#define UC_SEEN ((HQ_N * 13u + 7u) / 8u)                 /* the root's shapes x basses, a bit each (the rows' once-only) */

static uint32_t uc_slot(void)
{
    uint32_t s = harp_set(TSEL);
    return s >= HARP_U0 && s < HARP_NALL ? s - HARP_U0 : 0u;
}
/* the list: rows from .. from + n - 1 into out (n 0: none), the whole count back (and kept in uc_tot). The sets in order (not the
 * user sets: they are the target), the root alone looked at (harp_root_of) until one matches; the rows are never kept */
static uint32_t uc_scan(uint32_t from, uint32_t n, uc_ref_t *out)
{
    uint8_t seen[UC_SEEN];
    uint32_t s1 = RES_OK() ? HARP_U0 : HARP_NSET, s, i, cnt = 0;
    memset(seen, 0, sizeof seen);
    for (s = 0; s < s1; s++)
        for (i = 0; i < HARP_NCH; i++) {
            harp_ch_t c;
            uint32_t b;
            if (harp_root_of(s, i) != uc_r)
                continue;
            c = harp_user_from(s, i);
            b = c.q * 13u + (c.bass < 12u ? c.bass : 12u);
            if ((seen[b >> 3] >> (b & 7u)) & 1u)
                continue;
            seen[b >> 3] |= (uint8_t)(1u << (b & 7u));
            if (cnt >= from && cnt - from < n)
                out[cnt - from] = (uc_ref_t){(uint8_t)s, (uint8_t)i};
            cnt++;
        }
    uc_tot = (uint16_t)cnt;
    return cnt;
}
static uint32_t uc_total(void) { return uc_tot == 0xFFFFu ? uc_scan(0, 0, 0) : uc_tot; }
/* the top F# plays the row's chord (seq.c harp_pv); none when there is no row */
static harp_ch_t uc_apply(harp_ch_t c)                   /* the row's chord with KNOB 4's bass */
{
    if (uc_bs == 1u) c.bass = HB_NONE;
    else if (uc_bs >= 2u) c.bass = (uint8_t)(uc_bs - 2u);
    return c;
}
static void uc_pv_sync(void)
{
    uc_ref_t r;
    uint32_t n = uc_total();
    if (!n || !RES_OK()) {
        harp_pv_on = 0;
        return;
    }
    if (uc_row >= n)
        uc_row = (uint16_t)(n - 1u);
    uc_scan(uc_row, 1, &r);
    harp_pv = uc_apply(harp_user_from(r.set, r.idx));
    harp_pv_on = 1;
}
static void uc_cname(harp_ch_t c, char *buf)             /* "Cmaj7", "C/E": the chord as it is written */
{
    str_cpy(buf, HARP_PC[c.root % 12u], 4);
    str_cpy(buf + str_len(buf), HQ_NAME[c.q % HQ_N], 8);
    if (c.bass < 12u) {
        str_cpy(buf + str_len(buf), "/", 2);
        str_cpy(buf + str_len(buf), HARP_PC[c.bass], 4);
    }
}
static void uc_setname(uint32_t set, char *buf)          /* "MAJ3", "POP 2", "DARK 1" */
{
    if (set < HARP_NSET) {
        str_cpy(buf, HARP_SET_NAME[set], 12);
        return;
    }
    str_cpy(buf, harp_gname(harp_genre(set)), 9);
    str_cpy(buf + str_len(buf), " ", 2);
    fmt_int(buf + str_len(buf), (int32_t)(set - harp_genre_set(harp_genre(set))) + 1);
}
static void uc_arm(void)                                 /* OCT+: the write prompt */
{
    if (!RES_OK()) { ui_message("NEEDS RES"); return; }
    if (!uc_total()) { ui_message("NO CHORD"); return; }
    uc_w = 1;
}
static void uc_key(uint32_t k)                           /* chord key k hit during the prompt: the row's chord goes there */
{
    static char m[16];
    uc_ref_t r;
    uc_w = 0;
    if (uc_row >= uc_scan(uc_row, 1, &r))
        return;
    harp_usr_put(uc_slot(), k, uc_apply(harp_user_from(r.set, r.idx)));
    uc_dirty = 1;
    str_cpy(m, "WRITTEN K", 12);
    fmt_int(m + str_len(m), (int32_t)k + 1);
    ui_message(m);
}
/* the page's key LEDs (bit k = key k): the preview key always lit; in the write prompt the chord keys too: lit ones hold a
 * chord, *empty breathe */
static uint32_t uc_leds(uint32_t *empty)
{
    uint32_t k, sl = uc_slot(), lit = 1u << HARP_KLATCH;
    *empty = 0;
    for (k = 0; uc_w && k < HARP_NCH; k++)
        if (harp_usr_has(sl, k)) lit |= 1u << k;
        else *empty |= 1u << k;
    return lit;
}
static void uc_knob(uint32_t slot, int32_t steps)
{
    if (!RES_OK()) { ui_message("NEEDS RES"); return; }
    uc_w = 0;                                           /* (a knob cancels the prompt) */
    if (slot == 0u) {                                   /* the row */
        uc_total();
        uc_row = (uint16_t)(uc_tot ? clamp((int32_t)uc_row + steps, 0, (int32_t)uc_tot - 1) : 0);
    } else if (slot == 1u) {                            /* the root */
        uc_r = (uint8_t)wrap_add(uc_r, steps, 12);
        uc_row = 0;
        uc_tot = 0xFFFFu;
    } else if (slot == 2u) {                            /* the user set */
        TSEL->p[P_CVOI] = (int16_t)(HARP_U0 + (uint32_t)clamp((int32_t)uc_slot() + steps, 0, (int32_t)HARP_NUSR - 1));
    } else {                                            /* the bass */
        uc_bs = (uint8_t)wrap_add(uc_bs, steps, 14);
    }
    uc_pv_sync();
}
/* card c of the page: its label, value and unit (the three texts), the pick it is one of (0: none) and which */
static void uc_col(uint32_t c, const char **lab, const char **val, const char **unit, uint32_t *pn, uint32_t *pi)
{
    uint32_t sl = uc_slot(), k, n = 0;
    char *v = uc_b[c & 3u][0], *u = uc_b[c & 3u][1];
    *lab = "";
    *val = v;
    *unit = u;
    v[0] = u[0] = 0;
    *pn = *pi = 0;
    if (c == 0u) {
        n = uc_total();
        *lab = "CHORD";
        fmt_int(v, n ? (int32_t)uc_row + 1 : 0);
        u[0] = '/'; fmt_int(u + 1, (int32_t)n);
        *pn = n; *pi = uc_row;
    } else if (c == 1u) {
        *lab = "ROOT";
        str_cpy(v, HARP_PC[uc_r % 12u], 8);
        *pn = 12u; *pi = uc_r;
    } else if (c == 2u) {
        for (k = 0; k < HARP_NCH; k++)
            n += (uint32_t)harp_usr_has(sl, k);
        *lab = "SET";
        v[0] = 'U'; fmt_int(v + 1, (int32_t)sl + 1);
        fmt_int(u, (int32_t)n); str_cpy(u + str_len(u), "/12", 4);   /* (the keys written) */
        *pn = HARP_NUSR; *pi = sl;
    } else {
        *lab = "BASS";
        str_cpy(v, uc_bs == 0u ? "LIST" : uc_bs == 1u ? "NONE" : HARP_PC[(uc_bs - 2u) % 12u], 8);
        *pn = 14u; *pi = uc_bs;
    }
}
/* the list: as PRESETS' (ui_lzsave.c graph_lz_presets): a header, eight rows of 15 px, the one picked tinted */
static void graph_uchord(void)
{
    uc_ref_t rw[8];
    uint32_t tot = uc_total(), cur = uc_row, first, i;
    char tx[16], nm[16], sn[16], no[6];
    if (tot && cur >= tot)
        uc_row = (uint16_t)(cur = tot - 1u);
    first = cur < 3u ? 0u : cur - 3u;
    if (tot > 8u && first + 8u > tot)
        first = tot - 8u;
    if (tot)
        uc_scan(first, 8, rw);
    cv_rect(4, 0, 232, LP_HDR, T_RAISE);
    if (uc_w && tot) {                                   /* the prompt: the chord, the set, the keys lit */
        uc_cname(uc_apply(harp_user_from(rw[cur - first].set, rw[cur - first].idx)), nm);
        str_cpy(tx, "HIT A KEY: ", 12);
        cv_text_on(8, CAP_IN(X, LP_HDR), &AF_X, tx, T_ACCENT, T_RAISE);
        cv_text_on(78, CAP_IN(X, LP_HDR), &AF_X, nm, T_TEXT, T_RAISE);
        str_cpy(tx, "TO U", 5);
        fmt_int(tx + 4, (int32_t)uc_slot() + 1);
        cv_text_r(232, CAP_IN(X, LP_HDR), &AF_X, tx, T_ACCENT, T_RAISE);
    } else {
        cv_text_r(22, CAP_IN(X, LP_HDR), &AF_X, "#", T_MID, T_RAISE);
        cv_text_on(28, CAP_IN(X, LP_HDR), &AF_X, "CHORD", T_MID, T_RAISE);
        cv_text_on(136, CAP_IN(X, LP_HDR), &AF_X, "SET", T_MID, T_RAISE);
        fmt_int(tx, (int32_t)tot);
        cv_text_r(232, CAP_IN(X, LP_HDR), &AF_X, tx, T_MID, T_RAISE);
    }
    if (!tot) {
        cv_text_in(4, LP_HDR + 52, 232, &AF_X, "NO CHORDS", T_MID, T_SURF);
        return;
    }
    for (i = first; i < tot && i < first + 8u; i++) {
        int32_t y = LP_HDR + 2 + (int32_t)(i - first) * 15;
        int sel = i == cur;
        uint16_t bg = sel ? T_TINT : T_SURF;
        uc_ref_t *r = &rw[i - first];
        uc_cname(uc_apply(harp_user_from(r->set, r->idx)), nm);
        uc_setname(r->set, sn);
        if (sel)
            cv_rect(4, y, 232, 14, bg);
        fmt_int(no, (int32_t)i + 1);
        cv_text_r(22, y + CAP_IN(X, 14), &AF_X, no, sel ? T_MID : T_DIM, bg);
        cv_free_text(28, y + CAP_IN(S, 14), &AF_S, nm, sel ? T_TEXT : T_MID, bg, 104);
        cv_text_on(136, y + CAP_IN(X, 14), &AF_X, sn, sel ? T_MID : T_DIM, bg);
    }
}
