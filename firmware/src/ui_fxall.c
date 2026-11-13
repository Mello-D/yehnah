/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (2026-11-13) FX ALL: every effect's settings on ONE scrollable page (the FX button's only page). Each row is one of the old effect
 * pages (SENDS, DIST, DLY 1..3, REVERB 1..2, ..), up to four settings on KNOB 1..4; the rows are grouped under a header per effect. SELECT
 * scrolls the rows (cursor_row): a slow turn a row, a fast turn a section. cur_page() hands out a copy of the focused row's page, so
 * the knobs, the locks and the special cases (SL PITCH's step, the reverb's model, ..) are the old pages' own. Drawing: ui_lzfxall.c.
 * Included by ui.c before cur_page(). */
typedef struct { const char *name; uint8_t kind, n; const char *pg[3]; } fxs_t;
static const fxs_t FXS[] = {
    {"SENDS", 0, 1, {"FX"}},
    {"DIST", 1, 1, {"DIST"}},
    {"AUTO PAN", 2, 1, {"AUTO PAN"}},
    {"PHASER", 3, 1, {"PHASER"}},
#if FELUCCA_FXSCENE
    {"SCENE", 4, 1, {"SCENE"}},
#endif
    {"DELAY", 5, 3, {"DLY", "DLY 2", "DLY 3"}},
    {"REVERB", 6, 2, {"REVERB", "REVERB 2"}},
    {"CHORUS", 7, 1, {"CHORUS"}},
    {"SLICER", 8, 1, {"SLICER"}},
    {"SL BUS", 9, 2, {"SL BUS", "SL BUS 2"}},
    {"SL PITCH", 10, 1, {"SL PITCH"}},
    {"MASTER", 11, 1, {"MASTER"}},
    {"PUNCH", 12, 1, {"PUNCH"}},
};
#define FXS_N ((uint32_t)(sizeof FXS / sizeof FXS[0]))
/* (2026-11-13) the same page for the other lists: SOUND (PATCH), ENV LIST, LFO LIST, ARP LIST are kinds of it (the host page's first letter:
 * P E L A; FX ALL = F): a header per group, rows of up to four settings on KNOB 1..4, the same SELECT, the same drawing. Their rows are
 * tables of ids (arow_t; SOUND's from the engine's own rows, packed four to a row, then the generic FILTER / VOICE / DRIFT groups) */
enum { FXK_FX, FXK_SND, FXK_ENV, FXK_LFO, FXK_ARP, FXK_GLO, FXK_N };
#define SK_NONE 0xFFu                                   /* a header without a sketch */
#define SK_ENV 20u
#define SK_LFO 21u
#define SK_MSEQ 22u
typedef struct { const char *hdr; uint8_t sk, id[4]; } arow_t;   /* hdr: starts a group (its name; sk the sketch of its focused header) */
static const arow_t A_ENV[] = {
    {"ENV", SK_ENV, {P_ATK, P_DEC, P_SUS, P_REL}},
    {0, 0, {P_ELOOP, 0xFF, 0xFF, 0xFF}},
    {"TARGETS", SK_NONE, {P_ED_FLT, P_ED_PIT, P_ED_SHP, 0xFF}},
};
static const arow_t A_LFO[] = {
    {"LFO", SK_LFO, {P_LRATE, P_LWAVE, P_LPHASE, P_LFADE}},
    {"TARGETS", SK_NONE, {P_LD_AMP, P_LD_PIT, P_LD_FLT, P_LD_SHP}},
    {"MSEQ", SK_MSEQ, {P_MSLEN, P_MSDIV, P_MSSLW, 0xFF}},   /* (the old MSEQ / MSEQ 2: its steps four to a row) */
    {0, 0, {P_MS0, P_MS0 + 1, P_MS0 + 2, P_MS0 + 3}},
    {0, 0, {P_MS0 + 4, P_MS0 + 5, P_MS0 + 6, P_MS0 + 7}},
    {0, 0, {P_MS0 + 8, P_MS0 + 9, P_MS0 + 10, P_MS0 + 11}},
    {0, 0, {P_MS0 + 12, P_MS0 + 13, P_MS0 + 14, P_MS0 + 15}},
    {"MOD", SK_NONE, {P_M1SRC, P_M1DST, P_M1AMT, 0xFF}},   /* (the old MOD: a slot a row) */
    {0, 0, {P_M2SRC, P_M2DST, P_M2AMT, 0xFF}},
    {0, 0, {P_M3SRC, P_M3DST, P_M3AMT, 0xFF}},
    {0, 0, {P_M4SRC, P_M4DST, P_M4AMT, 0xFF}},
};
static const arow_t A_ARP[] = {
    {"ARP", SK_NONE, {P_AMODE, P_ARATE, P_AOCT, P_AORDER}},
    {"FEEL", SK_NONE, {P_AGATE, P_ASWING, P_APROB, P_AHOLD}},
    {"SPICE", SK_NONE, {P_SPICE, P_SLOOP, P_CSTR, 0xFF}},
#if FELUCCA_AMBIENT
    {"AMBIENT", SK_NONE, {P_AMB, P_ADEN, P_AMOT, P_ASPC}},
#endif
};
static const arow_t A_GLO[] = {                          /* GLO's four lists (params.c SETS), four to a row */
    {"MAIN", SK_NONE, {G_BPM, G_SWING, G_TRKS, MROW(MI_USB)}},
    {0, 0, {MROW(MI_CLICK), MROW(MI_CLKLVL), MROW(MI_COUNTIN), 0xFF}},
    {"DEVICE", SK_NONE, {MROW(MI_COLOR), MROW(MI_SCROFF), MROW(MI_FILL), MROW(MI_LEDS)}},
    {0, 0, {MROW(MI_SCLLED), 0xFF, 0xFF, 0xFF}},
    {"CONTROL", SK_NONE, {MROW(MI_HOLD), MROW(MI_ACCEL), MROW(MI_LATCH), G_CLOCK}},
    {0, 0, {G_ROUTE, MROW(MI_MIDIOUT), MROW(MI_LOWCUT), G_TUNE}},
    {"SYSTEM", SK_NONE, {G_LOADP, G_INFO, MROW(MI_SERIAL), MROW(MI_RESTORE)}},
    {0, 0, {MROW(MI_PANEL), MROW(MI_INIT), MROW(MI_ABOUT), 0xFF}},
};
static const arow_t A_ANALOG[] = {                       /* ANALOG's own groups: all 25 settings with the generic VOICE / DRIFT after */
    {"OSC", SK_NONE, {P_E0, P_E1, P_E2, VF(FX_A_RNG)}},
    {0, 0, {VF(FX_A_WAV), VF(FX_A_PW), VF(FX_A_SYN), P_E3}},
    {"FILTER", SK_NONE, {P_FMODEL, P_E4, P_E5, P_DIST}},
    {0, 0, {P_E6, P_E7, P_VELF, 0xFF}},
};
static const arow_t A_GFLT[] = {
    {"FILTER", SK_NONE, {P_FTYPE, P_FCUT, P_FRES, P_DIST}},
    {0, 0, {P_VELF, 0xFF, 0xFF, 0xFF}},
};
static const arow_t A_GVOI[] = {
    {"VOICE", SK_NONE, {P_VOICE, P_PRIO, P_ALLOC, P_GLIDE}},
    {0, 0, {P_GLMODE, P_DETUNE, P_KTRK, P_MWDST}},
    {"DRIFT", SK_NONE, {P_DRIFT, P_VINT, 0xFF, 0xFF}},
};
#define FXA_MAX 56u
#define FXA_SEC 24u
typedef struct { uint8_t pi, sec, first, aux, id[4]; } fxr_t;   /* the page (an index of PAGES; a list's: its own), its section, 1: the section's first row, the ids */
static fxr_t fxa_r[FXA_MAX] __attribute__((section(".pool")));   /* (RAM is full: the pool; aux: a lane / an operator the row's page shows, 0xFF none) */
static const char *fxa_sn[FXA_SEC];                    /* the sections' names and sketches */
static uint8_t fxa_sk[FXA_SEC];
static uint8_t fxa_cnt, fxa_cur, fxa_k, fxa_nsec, fxa_hpi, fxa_curs[FXK_N];
static uint32_t fxa_key = 0xFFFFFFFFu;
static uint8_t dx_ui_lane, fop_op;                      /* (ui.c, ui_fm6op.c: a row's lane / operator) */
static void nav_select(int32_t s);                      /* (ui_lens.c) */
static uint32_t nav_vis_n(void);
static uint32_t fxa_kind_of(const page_t *p)
{
    switch (p->title[0]) {                              /* (FX ALL, PATCH, ENV LIST, LFO LIST, ARP LIST) */
    case 'P': return FXK_SND;
    case 'E': return FXK_ENV;
    case 'L': return FXK_LFO;
    case 'A': return FXK_ARP;
    case 'G': return FXK_GLO;
    default: return FXK_FX;
    }
}
static void fxa_sync(uint32_t k)                       /* the active list: each keeps its cursor */
{
    if (k != fxa_k) {
        fxa_curs[fxa_k] = fxa_cur;
        fxa_k = (uint8_t)k;
        fxa_cur = fxa_curs[k];
        fxa_cnt = 0;
    }
}
static uint32_t fxa_find(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_FX && PAGES[i].graph != GR_FXALL && str_eq(PAGES[i].title, title))
            return i;
    return 0;
}
static void fxa_add(uint32_t pi, const char *hdr, uint32_t sk, const uint8_t *id)
{
    fxr_t *r;
    uint32_t k;
    if (fxa_cnt >= FXA_MAX)
        return;
    r = &fxa_r[fxa_cnt++];
    if (hdr) {
        if (fxa_nsec < FXA_SEC) {
            fxa_sn[fxa_nsec] = hdr;
            fxa_sk[fxa_nsec] = (uint8_t)sk;
            fxa_nsec++;
        }
    }
    r->first = hdr != 0;
    r->sec = (uint8_t)(fxa_nsec ? fxa_nsec - 1u : 0u);
    r->pi = (uint8_t)pi;
    r->aux = 0xFFu;
    for (k = 0; k < 4u; k++)
        r->id[k] = id[k];
}
static void fxa_tab(uint32_t pi, const arow_t *a, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        fxa_add(pi, a[i].hdr, a[i].sk, a[i].id);
}
static void fxa_addx(uint32_t pi, uint32_t aux, const char *hdr, const uint8_t *id)   /* a row of an old page, for lane / operator aux */
{
    uint32_t n = fxa_cnt;
    fxa_add(pi, hdr, SK_NONE, id);
    if (fxa_cnt != n)
        fxa_r[n].aux = (uint8_t)aux;
}
static uint32_t fxa_pt(const char *title)               /* an EDIT page of that title (the kit's and FM6's own pages) */
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_EDIT && str_eq(PAGES[i].title, title))
            return i;
    return 0;
}
static const arow_t A_DLV[] = {                          /* a kit's volumes (LEVELS, LEVELS 2) */
    {"LEVELS", SK_NONE, {P_LN0, P_LN1, P_LN2, P_LN3}},
    {0, 0, {P_LN4, P_LN5, P_LN6, P_LN7}},
};
static const char *const FXA_OPN[6] = {"OP 1", "OP 2", "OP 3", "OP 4", "OP 5", "OP 6"};
static const char *const FXA_LNN[8] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
static void fxa_fm6(void)                               /* FM6: an operator's three old pages (OPERATOR, OPERATOR 2, OP ENV) as three rows */
{
    static const uint8_t NO[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t o, a = fxa_pt("OPERATOR"), b = fxa_pt("OPERATOR 2"), c = fxa_pt("OP ENV");
    for (o = 0; o < 6u; o++) {
        fxa_addx(a, o, FXA_OPN[o], NO);
        fxa_addx(b, o, 0, NO);
        fxa_addx(c, o, 0, NO);
    }
}
static void fxa_drum(uint32_t hpi)                      /* a kit: its volumes, KIT 3, X-MOD, then every sound's four old pages as four rows */
{
    static const uint8_t NO[4] = {0xFF, 0xFF, 0xFF, 0xFF}, IMG[4] = {0xFF, 0xFF, 0xFF, P_DXIMG};
    static const char *const SN[4] = {"SOUND", "SOUND 2", "SOUND 3", "SOUND 4"};
    uint32_t l, k;
    fxa_tab(hpi, A_DLV, (uint32_t)NELEM(A_DLV));
    fxa_addx(fxa_pt("KIT 3"), 0xFFu, "KIT", IMG);
    fxa_addx(fxa_pt("X-MOD"), 0xFFu, "X-MOD", NO);
    for (l = 0; l < 8u; l++)
        for (k = 0; k < 4u; k++)
            fxa_addx(fxa_pt(SN[k]), l, k ? 0 : FXA_LNN[l], NO);
}
/* SOUND: the engine's own rows (core.h sound_row_t, up to three settings) packed into rows of four, a group a name every eight settings
 * or so (the first row's name); ANALOG's are its own table; then the generic FILTER (not ANALOG's: its own), VOICE and DRIFT (not a kit's) */
static void fxa_snd(uint32_t pi)
{
    const track_t *t = &trk[song.sel];
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t ei = t->eng_req % NENGINES, en = e->rows ? e->nrows : SR_GEN, r, k, n = 0, sn = 0;
    uint8_t id[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    const char *hdr = 0, *pend = 0;
    if (ei == ENGI_ANALOG) {
        fxa_tab(pi, A_ANALOG, (uint32_t)NELEM(A_ANALOG));
    } else {
        for (r = 0; r < en; r++) {
            const sound_row_t *sr = sound_row(t, r);
            uint32_t c = 0;
            for (k = 0; k < 3u; k++)
                c += sr->id[k] != 0xFFu;
            if (!c)
                continue;
            if (n && n + c > 4u) {                      /* a row is full */
                fxa_add(pi, hdr, SK_NONE, id);
                hdr = 0;
                n = 0;
                id[0] = id[1] = id[2] = id[3] = 0xFF;
            }
            if (!n && (!pend || sn >= 8u)) {
                hdr = sr->name;
                sn = 0;
            }
            for (k = 0; k < 3u; k++)
                if (sr->id[k] != 0xFFu)
                    id[n++] = sr->id[k];
            sn += c;
            pend = hdr ? hdr : pend;
        }
        if (n)
            fxa_add(pi, hdr, SK_NONE, id);
        if (ei == ENGI_FM6)
            fxa_fm6();
        else if (ei == ENGI_DRUM)
            fxa_drum(pi);
        fxa_tab(pi, A_GFLT, (uint32_t)NELEM(A_GFLT));
    }
    if (ei != ENGI_DRUM)
        fxa_tab(pi, A_GVOI, (uint32_t)NELEM(A_GVOI));
}
static void fxa_build(void)                            /* the rows shown (LOAD ECO: no CHORUS, SLICER, SL BUS, SL PITCH) */
{
    uint32_t s, k, key = (uint32_t)sys_load | (uint32_t)trk[song.sel].eng_req << 4 | (uint32_t)song.sel << 14 | (uint32_t)fxa_k << 20;
    if (fxa_cnt && fxa_key == key)
        return;
    fxa_cnt = 0;
    fxa_nsec = 0;
    fxa_key = key;
    if (fxa_k != FXK_FX) {
        for (fxa_hpi = 0; fxa_hpi < NPAGES && !(PAGES[fxa_hpi].graph == GR_FXALL && fxa_kind_of(&PAGES[fxa_hpi]) == fxa_k); fxa_hpi++) {}
        if (fxa_k == FXK_SND)
            fxa_snd(fxa_hpi);
        else if (fxa_k == FXK_ENV)
            fxa_tab(fxa_hpi, A_ENV, (uint32_t)NELEM(A_ENV));
        else if (fxa_k == FXK_GLO)
            fxa_tab(fxa_hpi, A_GLO, (uint32_t)NELEM(A_GLO));
        else if (fxa_k == FXK_LFO)
            fxa_tab(fxa_hpi, A_LFO, (uint32_t)NELEM(A_LFO));
        else
            fxa_tab(fxa_hpi, A_ARP, (uint32_t)NELEM(A_ARP));
    } else {
        for (s = 0; s < FXS_N; s++) {
            uint8_t first = 1;
            for (k = 0; k < FXS[s].n && fxa_cnt < FXA_MAX; k++) {
                uint32_t pi = fxa_find(FXS[s].pg[k]);
                if (sys_load == LOAD_ECO && (PAGES[pi].graph == GR_SLCR || PAGES[pi].graph == GR_SLPIT || PAGES[pi].id[0] == G_CRATE))
                    continue;
                fxa_add(pi, first ? FXS[s].name : 0, FXS[s].kind, PAGES[pi].id);
                first = 0;
            }
        }
    }
    if (fxa_cur >= fxa_cnt)
        fxa_cur = (uint8_t)(fxa_cnt ? fxa_cnt - 1u : 0u);
}
static void fxa_aux(void)                               /* the lane / operator the focused row shows (its page's KNOB 1, which no longer moves it) */
{
    uint32_t g = PAGES[fxa_r[fxa_cur].pi].graph;
    if (fxa_r[fxa_cur].aux == 0xFFu)
        return;
    if (g == GR_DXSND)
        dx_ui_lane = fxa_r[fxa_cur].aux;
    else if (g >= GR_FMOP && g <= GR_FMENV)
        fop_op = fxa_r[fxa_cur].aux;
}
static const page_t *fxa_page(const page_t *host)      /* the focused row's page (a copy: cur_page()) */
{
    static page_t sh;
    uint32_t k;
    fxa_sync(fxa_kind_of(host));
    fxa_build();
    if (fxa_cur >= fxa_cnt)
        fxa_cur = (uint8_t)(fxa_cnt - 1u);
    sh = PAGES[fxa_r[fxa_cur].pi];
    for (k = 0; k < 4u; k++)
        sh.id[k] = fxa_r[fxa_cur].id[k];
    if (fxa_k != FXK_FX && fxa_r[fxa_cur].pi == fxa_hpi)
        sh.graph = GR_NONE;                             /* (the list's own rows: settings; a kit's and FM6's old pages keep theirs) */
    fxa_aux();
    return &sh;
}
/* SELECT: a slow turn moves a row, a fast one (two detents in a read, or detents under 45 ms apart) a section: to the first row of the next
 * (the section's own first row when the cursor is further down it, going back). The list goes round; where the section's chain has other
 * pages (FM6's operators, ENV's STAGE, LFO's MSEQ, ..) the end of the list goes on to the next page of the chain instead */
static void fxa_turn(int32_t s)
{
    static uint32_t last_ms;
    static int32_t last_dir;
    int32_t dir = s > 0 ? 1 : -1, a = s < 0 ? -s : s, fast, i;
    uint32_t now = fm1_ms;
    fxa_sync(fxa_kind_of(&PAGES[ui.page]));
    fxa_build();
    fast = a >= 2 || (!(ui_prefs & PREF_ACCEL_OFF) && last_ms && dir == last_dir && now - last_ms < 45u);   /* (MENU > KNOB ACCEL OFF: only a multi-detent read) */
    last_ms = now | 1u;
    last_dir = dir;
    if (fxa_cur >= fxa_cnt)
        fxa_cur = (uint8_t)(fxa_cnt - 1u);
    if (!fast) {
        i = (int32_t)fxa_cur + dir;
        if ((i < 0 || i >= (int32_t)fxa_cnt) && nav_vis_n() > 1u) {
            nav_select(dir);
            return;
        }
        i = (i + (int32_t)fxa_cnt) % (int32_t)fxa_cnt;     /* (round: the last row, then the first) */
    } else if (dir > 0) {
        i = (int32_t)fxa_cur + 1;
        while (i < (int32_t)fxa_cnt && !fxa_r[i].first)
            i++;
        if (i >= (int32_t)fxa_cnt) {
            if (nav_vis_n() > 1u) {
                nav_select(1);
                return;
            }
            i = 0;
        }
    } else {
        i = (int32_t)fxa_cur;
        if (fxa_r[i].first) {
            if (!i && nav_vis_n() > 1u) {
                nav_select(-1);
                return;
            }
            i = i ? i - 1 : (int32_t)fxa_cnt - 1;
        }
        while (i > 0 && !fxa_r[i].first)
            i--;
    }
    fxa_cur = (uint8_t)clamp(i, 0, (int32_t)fxa_cnt - 1);   /* (no ui.force: the panel's signature sees the cursor; a force clears the frame's rows inside it) */
}
