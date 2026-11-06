/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.9.2) The navigation, as the approved mock-up (navegacion.html): six rules, the same in every section.
 *   1 SELECT walks the section's pages as one chain (no levels, no zoom), stopping at its first and last page.
 *   2 The section's button: its chain's first page; already inside, HOME's first page, TRACKS (2026-11-02, the user's
 *     held notes a and b; HOME again on HOME: TRACKS).
 *   3 PRESETS loads presets: the sounds everywhere, the patterns in SEQ (the PHRASES loader), the genre in GEN; never a
 *     page, a track or a module (ui_input.c presets_turn).
 *   4 A page that picks something picks it with KNOB 1 (the step, the bar, the slot, the kit's sound ..); the track is
 *     ALGORITHM's.
 *   5 A button held is its layer, as before (ui_layer.c); held + SELECT: GLO the tempo (2026-11-02: the others' went
 *     with the tabs).
 *   6 The screen says where: (2026-11-02, the redesign) the page row at the foot, the section's pages one row that
 *     scrolls, the page shown in the middle (ui_draw.c page_row); no tabs, no breadcrumb.
 * Each section's chain is a table (CE_*): the page (its PAGES title; HOME's are its own), the name shown (each standing
 * on its own: NAV-LAYOUT.md's renames; the short forms beside it: ui_draw.c PG_SHORT), its group (nav_cur: which twin a
 * page put by code is) and the level its pictures were drawn for (0.8.5's lenses: the panels, the cards and the knobs
 * as they were, read through lens_level / save_level / pd_level / home_level). Each section remembers the entry shown (ui.nmem, EDIT
 * one per kind of track); a page put by code brings its entry (nav_cur). Included by ui.c. */
enum { LZ_SEQ, LZ_SAVE, LZ_FX, LZ_HOME, LZ_EDIT, LZ_ENV, LZ_MOD, LZ_KEY, LZ_GLO, LZ_LAYER, LZ_DLG, LZ_NONE };
enum { PD_EDIT, PD_ENV, PD_MOD, PD_KEY, PD_GLO, PD_N };
static uint32_t pd_of_fam(uint32_t f)
{
    return f == FAM_EDIT ? PD_EDIT : f == FAM_ENV ? PD_ENV : f == FAM_LFO ? PD_MOD : f == FAM_SCL || f == FAM_ARP ? PD_KEY :
           f == FAM_GLO || f == FAM_TRK ? PD_GLO : PD_N;
}
static uint32_t edit_kind(void)                          /* 0 a synth, 1 FM6, 2 DRUM (2026-11-02: the DIGITAL chain gone) */
{
    uint32_t e = trk_eng(TSEL);
    return e == ENGI_FM6 ? 1u : e == ENGI_DRUM ? 2u : 0u;
}
static int pd_on(void) { return !ui.home && !page_over && pd_of_fam(PAGES[ui.page].fam) < PD_N; }
static uint32_t pd_cur(void) { return pd_of_fam(PAGES[ui.page].fam); }

/* ------------------------------------------------------------------------------------------- the chains --- */
enum { SEC_HOME, SEC_EDIT, SEC_SEQ, SEC_FX, SEC_GLO, SEC_ENV, SEC_LFO, SEC_SCL, SEC_ARP, SEC_SAVE, SEC_N };
static const char *const SEC_NAME[SEC_N] = {"HOME", "EDIT", "SEQ", "FX", "GLO", "ENV", "LFO", "SCL", "ARP", "SAVE"};
static const uint8_t SEC_FAM[SEC_N] = {FAM_HOME, FAM_EDIT, FAM_SEQ, FAM_FX, FAM_GLO, FAM_ENV, FAM_LFO, FAM_SCL, FAM_ARP,
                                       FAM_SAVE};
typedef struct { const char *t, *nm; uint8_t g, lv; } nv_t;   /* PAGES title (HOME: 0), the name shown (0: t), group, level */
typedef struct { const nv_t *e; uint8_t n; } ch_t;            /* the entries, how many */
#define NV(t, nm, g, lv) {t, nm, g, lv}
enum { SV_LIB, SV_PROJ, SV_SOUND };
/* (2026-11-02, NAV-LAYOUT.md) HOME: TRACKS, PRESETS MORPH (from SAVE), MIXER SENDS (from GLO), MACRO (ROUTES' screen;
 * the old MACRO and ENGINE gone), the IDEAS; TRACKS, MACRO and IDEAS are HOME's own (t 0), the others real pages */
enum { HOME_MACRO = 5, HOME_IDEA = 6 };
static const nv_t CE_HOME[] = {NV(0, "TRACKS", 0, 0), NV("PRESETS", 0, 1, SV_SOUND), NV("MORPH", 0, 1, SV_SOUND),
                               NV("MIXER", 0, 2, 0), NV("SENDS", 0, 2, 0), NV(0, "MACRO", 2, 1),
                               NV(0, "IDEA TRK", 3, 2), NV(0, "IDEA SND", 3, 2), NV(0, "IDEA LFO", 3, 2),   /* (chord-harp fork) */
                               NV(0, "IDEA FX", 3, 2), NV(0, "IDEA TEX", 3, 2)};   /* IDEAS (ideas.c; was Yehnah's GEN) */
static const nv_t CE_SYN[] = {NV("EDIT 1", 0, 0, 0), NV("EDIT 2", 0, 0, 0), NV("FILTER", 0, 1, 1), NV("DRIFT", 0, 1, 1),
                              NV("VOICE", 0, 1, 1), NV("VOICE 2", 0, 1, 1),   /* (2026-10-30: MORPH now DRIFT; RETRIG,
                                                         * ROTOR gone with our LOFI and WHEEL) */
                              NV("FOCUS", 0, 2, 2)};
static const nv_t CE_FM6[] = {NV("EDIT 1", 0, 0, 0), NV("EDIT 2", 0, 0, 0), NV("FILTER", 0, 1, 1), NV("DRIFT", 0, 1, 1),
                              NV("VOICE", 0, 1, 1), NV("VOICE 2", 0, 1, 1), NV("OPERATOR", "OP 1", 2, 2), NV("OP ENV", 0, 2, 2),
                              NV("OPERATOR 2", "OP 2", 2, 2)};
static const nv_t CE_DRM[] = {NV("EDIT 1", "KIT", 0, 0), NV("EDIT 2", "KIT 2", 0, 0), NV("KIT 3", 0, 0, 0),
                              NV("SOUND", 0, 1, 2), NV("SOUND 2", 0, 1, 2), NV("SOUND 3", 0, 1, 2), NV("SOUND 4", 0, 1, 2),
                              NV("LEVELS", 0, 2, 1), NV("LEVELS 2", 0, 2, 1), NV("X-MOD", 0, 3, 1), NV("FILTER", 0, 3, 1),
                              NV("STEREO", 0, 3, 1), NV("VOICE", 0, 3, 1)};
enum { LV_PAT, LV_BAR, LV_STEP, LV_N };                 /* SEQ's pictures: the bars, the roll, the step large */
static const nv_t CE_SEQ[] = {NV("STEP", 0, 2, LV_BAR), NV("NOTE", 0, 2, LV_BAR), NV("COMP", 0, 3, LV_STEP),   /* (2026-11-02:
                                                         * the main page first, the rest in their wrapped order) */
                              NV("COMP 2", 0, 3, LV_STEP), NV("AUTOMATION", "AUTO", 4, LV_BAR), NV("AUTO LIST", 0, 4, LV_BAR),
                              NV("PATTERN", 0, 0, LV_PAT), NV("STRETCH", 0, 0, LV_PAT), NV("PLAYHEADS", "PLAYHEAD", 0, LV_PAT),
                              NV("TRACK", 0, 1, LV_PAT), NV("MUTATE", 0, 1, LV_PAT),
                              NV("SQ TOOLS", "TOOLS", 5, LV_PAT),   /* (2026-11-03: SEQ held's tools; BAR stays left of STEP) */
                              NV("BAR", 0, 2, LV_PAT)};
enum { FXL_TRACK, FXL_BUSES, FXL_EFFECT };
static const nv_t CE_FX[] = {NV("FX", "SENDS", 0, FXL_TRACK), NV("DIST", 0, 0, FXL_TRACK), NV("AUTO PAN", 0, 0, FXL_TRACK),
                             NV("PHASER", 0, 0, FXL_TRACK),   /* (chord-harp fork) */
                             NV("BUSES", 0, 1, FXL_BUSES),
#if FELUCCA_FXSCENE
                             NV("SCENE", 0, 1, FXL_BUSES),   /* (2026-11-02: the FX scenes) */
#endif
                             NV("DLY", "DELAY", 2, FXL_EFFECT), NV("DLY 2", "DELAY 2", 2, FXL_EFFECT),
                             NV("DLY 3", "DELAY 3", 2, FXL_EFFECT), NV("REVERB", 0, 3, FXL_EFFECT), NV("REVERB 2", 0, 3, FXL_EFFECT),
                             NV("CHORUS", 0, 4, FXL_EFFECT), NV("SLICER", 0, 5, FXL_EFFECT), NV("SL BUS", 0, 5, FXL_EFFECT),
                             NV("SL BUS 2", 0, 5, FXL_EFFECT), NV("SL PITCH", 0, 5, FXL_EFFECT),
                             NV("MASTER", 0, 6, FXL_EFFECT), NV("PUNCH", 0, 7, FXL_EFFECT)};
static const nv_t CE_GLO[] = {NV("TEMPO", 0, 0, 1),   /* (2026-11-02: the MENU's rows its pages, NAV-LAYOUT.md;
                                                         * MIXER, SENDS to HOME) */
                              NV("SCREEN", 0, 1, 2), NV("LEDS", 0, 1, 2), NV("CONTROL", 0, 2, 2),
                              NV("MIDI", 0, 2, 2), NV("AUDIO", 0, 3, 2), NV("CLICK", 0, 3, 2), NV("SYSTEM", 0, 4, 2),
                              NV("SETUP", "TOOLS", 4, 2)};   /* (SETUP: SAVE has a TOOLS) */
static const nv_t CE_ENV[] = {NV("ENV", 0, 1, 1), NV("ENV DEST", 0, 1, 1), NV("STAGE", 0, 2, 2),   /* (this track's, then
                                                         * all six tracks') */
                              NV("ENV", "ALL ENV", 0, 0), NV("ENV DEST", "ALL DEST", 0, 0)};
static const nv_t CE_LFO[] = {NV("LFO", 0, 1, 1), NV("LFO DEST", 0, 1, 1), NV("MSEQ", 0, 2, 2), NV("MSEQ 2", 0, 2, 2),
                              NV("MOD", 0, 0, 0)};
static const nv_t CE_SCL[] = {NV("SCL", "SCALE", 0, 0), NV("CHORD", 0, 1, 1), NV("HARP", 0, 2, 1), NV("OMNI", 0, 3, 1),
                              NV("PROG", 0, 4, 1)};
static const nv_t CE_ARP[] = {NV("ARP", 0, 0, 2), NV("ARP 2", 0, 0, 2), NV("ARP 3", 0, 0, 2),
#if FELUCCA_AMBIENT
                              NV("AMBIENT", 0, 0, 2),
#endif
};
static const nv_t CE_SAVE[] = {NV("PROJECT", 0, 2, SV_LIB), NV("ARRANGE", "STRUCT", 1, SV_PROJ),   /* (2026-11-06: TOOLS, ERASE, USER went to the
                                                         * SAVE held layer; PRESETS, MORPH are HOME's) */
                               NV("SONG EDIT", "TRIM", 1, SV_PROJ)};
#define CH(e, ng) {e, (uint8_t)NELEM(e)}
/* (chord-harp fork) a chain's pages, at most NAV_MAX (nav_list's buffers): FX's 17th page (PHASER) overran a 16-byte
 * buffer on the stack, a crash on the device only */
#define NAV_MAX 24u
#define CH_OK(e) _Static_assert(NELEM(e) <= NAV_MAX, "lens chain " #e ": NAV_MAX")
CH_OK(CE_HOME); CH_OK(CE_SYN); CH_OK(CE_SEQ); CH_OK(CE_FX); CH_OK(CE_GLO); CH_OK(CE_ENV); CH_OK(CE_LFO); CH_OK(CE_SCL);
CH_OK(CE_ARP); CH_OK(CE_SAVE); CH_OK(CE_FM6); CH_OK(CE_DRM);
#define NAV_KEYS (SEC_N + 2u)                           /* the sections' memories: EDIT's synth one, then FM6 DRUM */
static const ch_t CHAINS[NAV_KEYS] = {CH(CE_HOME, 4), CH(CE_SYN, 3), CH(CE_SEQ, 5), CH(CE_FX, 8), CH(CE_GLO, 5),
                                      CH(CE_ENV, 3), CH(CE_LFO, 3), CH(CE_SCL, 5), CH(CE_ARP, 1), CH(CE_SAVE, 2),
                                      CH(CE_FM6, 3), CH(CE_DRM, 4)};
static uint32_t nav_key(uint32_t sec) { uint32_t k = sec == SEC_EDIT ? edit_kind() : 0u; return k ? SEC_N - 1u + k : sec; }
static const ch_t *nav_ch(uint32_t sec) { return &CHAINS[nav_key(sec)]; }
static uint32_t nav_mem(uint32_t key) { return ui.nmem[key]; }
static void nav_mem_put(uint32_t key, uint32_t e) { ui.nmem[key] = (uint8_t)e; }
static uint32_t sec_of_fam(uint32_t f)
{
    uint32_t s;
    for (s = SEC_EDIT; s < SEC_N; s++)
        if (SEC_FAM[s] == f)
            return s;
    return SEC_HOME;                                    /* (FAM_TRK: HOME's MIXER, SENDS) */
}
static int home_page(uint32_t i)                        /* a page of HOME's chain: PRESETS MORPH MIXER SENDS */
{
    uint32_t k;
    if (PAGES[i].fam != FAM_SAVE && PAGES[i].fam != FAM_TRK)
        return 0;
    for (k = 0; k < NELEM(CE_HOME); k++)
        if (CE_HOME[k].t && str_eq(PAGES[i].title, CE_HOME[k].t))
            return 1;
    return 0;
}
static int gen_on(void) { return ui.confirm == CF_DICE_ALL; }   /* GEN: HOME's last group (the generator's screen) */
static int dlg_on(void) { return ui.confirm && ui.confirm != CF_DICE_ALL; }   /* a dialog over the page */
static uint32_t gen_page(void);                         /* (gen.c) */
static uint32_t nav_sec(void)
{
    return ui.home || gen_on() || home_page(ui.page) ? SEC_HOME : sec_of_fam(PAGES[ui.page].fam);
}
/* entry e of section sec: its PAGES index shown now (NPAGES: not on this track, LOAD ECO's, HOME's own) */
static uint32_t nv_page(uint32_t sec, uint32_t e)
{
    const ch_t *c = nav_ch(sec);
    uint32_t i, f = SEC_FAM[sec];
    if (e >= c->n || !c->e[e].t)
        return NPAGES;
    for (i = 0; i < NPAGES; i++)
        if ((sec == SEC_HOME ? home_page(i) : PAGES[i].fam == f) && str_eq(PAGES[i].title, c->e[e].t) && page_visible(i))
            return i;
    return NPAGES;
}
static int nv_ok(uint32_t sec, uint32_t e)
{
    return sec == SEC_HOME && e < NELEM(CE_HOME) && !CE_HOME[e].t ? 1 : nv_page(sec, e) < NPAGES;
}
/* the entry shown: HOME's from its state (ui.hlv, ui.hset, GEN's page); the others the one remembered when it is the page
 * shown, else the page's own (put by code: in the group left, ENV's own, then the chain's first), else
 * (PHRASES, CHANCE: out of the chain) the one left */
static uint32_t nav_cur(void)
{
    uint32_t sec = nav_sec(), key = nav_key(sec), e = nav_mem(key), k, pass, s;
    const ch_t *c = &CHAINS[key];
    if (sec == SEC_HOME && (ui.home || gen_on()))
        return gen_on() ? HOME_IDEA + gen_page() : ui.hlv ? HOME_MACRO : 0u;
    s = ((((ui.page * 31u + key) * 131u + e) * 257u + TSEL->eng_req) * 8191u + (uint32_t)(uint16_t)TSEL->p[P_E1]) * 7u +
        sys_load + 1u;
    if (s == ui.nav_sig)
        return ui.nav_at;
    if (e >= c->n || nv_page(sec, e) != ui.page) {     /* (the group left, then ENV's own, then any: ENV's ENV page in */
        uint32_t g0 = c->e[e < c->n ? e : 0].g, g1 = sec == SEC_ENV ? 1u : 0u;   /* ENV, not TRACKS) */
        for (pass = 0; pass < 3u; pass++)
            for (k = 0; k < c->n; k++)
                if ((pass == 2u || c->e[k].g == (pass ? g1 : g0)) && nv_page(sec, k) == ui.page) {
                    nav_mem_put(key, k);
                    e = k;
                    pass = 3u;
                    break;
                }
        if (e >= c->n)
            e = 0;
    }
    ui.nav_sig = ((((ui.page * 31u + key) * 131u + e) * 257u + TSEL->eng_req) * 8191u + (uint32_t)(uint16_t)TSEL->p[P_E1]) * 7u +
                 sys_load + 1u;
    ui.nav_at = e;
    return e;
}
static uint32_t nav_lv(void) { return nav_ch(nav_sec())->e[nav_cur()].lv; }   /* the 0.8.5 lens level it is drawn as */
static uint32_t pd_level(void) { return nav_lv(); }
static uint32_t save_level(void) { return nav_lv(); }
static uint32_t home_level(void) { return gen_on() ? 2u : ui.hlv ? 1u : 0u; }
static uint32_t home_set(void) { return ui.hset[home_level() & 1u] & 1u; }
static int home_macros(void) { return ui.home && !ui.menu && !gen_on() && home_set(); }   /* MACROS / ROUTES: M1..M4 */
/* the entries shown now (on this track, at this LOAD), in order: their indices in v; how many */
static uint32_t nav_list(uint32_t sec, uint8_t *v)
{
    const ch_t *c = nav_ch(sec);
    uint32_t k, n = 0;
    for (k = 0; k < c->n && n < NAV_MAX; k++)
        if (nv_ok(sec, k))
            v[n++] = (uint8_t)k;
    return n;
}
static void go_home(void);
static void page_entered(void);
static void confirm_open(uint32_t kind, uint32_t trk);
static void gen_open(void);
static void gen_page_put(uint32_t p);
static uint32_t fx_mod_of(uint32_t i);
/* entry e of section sec shown (one that is not here: the nearest after it, else before); outside: from another
 * section (SAVE: USER and SECTIONS come with their SAVE armed, SAVE . OCT+ . OCT+ as before) */
static void nav_go(uint32_t sec, uint32_t e, int outside)
{
    const ch_t *c = nav_ch(sec);
    uint32_t key = nav_key(sec), k, i;
    if (e >= c->n)
        e = 0;
    for (k = e; k < c->n && !nv_ok(sec, k); k++) {}
    if (k >= c->n)
        for (k = e; k-- > 0u && !nv_ok(sec, k);) {}
    if (k >= c->n)
        return;
    e = k;
    nav_mem_put(key, e);
    if (sec == SEC_HOME && !c->e[e].t) {
        if (e >= HOME_IDEA) {                           /* IDEAS (ideas.c): HOME's last group */
            if (!gen_on()) {
                confirm_open(CF_DICE_ALL, song.sel);
                gen_open();
            }
            gen_page_put(e - HOME_IDEA);
        } else {                                        /* TRACKS (level 0), MACRO (ROUTES' screen: level 1's set 1) */
            if (gen_on())
                ui.confirm = 0;
            ui.hlv = (uint8_t)(e == HOME_MACRO);
            ui.hset[0] = 0;
            ui.hset[1] = 1;
        }
        if (!ui.home)
            go_home();
        ui.force = 1;
        return;
    }
    if (gen_on())
        ui.confirm = 0;
    i = nv_page(sec, e);
    ui.page = (uint8_t)i;
    ui.home = 0;
    page_entered();
    if (PAGES[i].graph == GR_USER || PAGES[i].graph == GR_SLOTS)
        ui.act = outside && (PAGES[i].graph == GR_USER || c->e[e].lv == SV_PROJ) ? 4u : 0u;
    if (PAGES[i].graph == GR_SONG)                       /* (2026-11-05) ARRANGE opens on its NEW row: pick, OCT+ */
        ui.song_row = (uint8_t)(chain_config.count ? chain_config.count - 1u : 0u);
}
/* rule 2: a section's button: its chain's first page; already inside, HOME's first (TRACKS) */
static void nav_button(uint32_t sec)
{
    if (nav_sec() == sec && (sec == SEC_HOME || !ui.home))
        sec = SEC_HOME;
    nav_go(sec, 0, 1);
}
/* rule 1: SELECT, s detents along the chain; it wraps (the last page, then the first: the page row) */
static void nav_select(int32_t s)
{
    uint8_t v[NAV_MAX];
    uint32_t sec = nav_sec(), n = nav_list(sec, v), e = nav_cur(), i;
    int32_t at = -1, to;
    for (i = 0; i < n; i++)
        if (v[i] == e)
            at = (int32_t)i;
    if (at < 0) {                                       /* (the entry left is not here: the nearest that is) */
        for (i = 0; i < n && v[i] < e; i++) {}
        at = (int32_t)i - (s > 0 ? 1 : 0);
    }
    if (!n)
        return;
    to = (at + s) % (int32_t)n;
    if (to < 0)
        to += (int32_t)n;
    if (to != at || v[to] != e)
        nav_go(sec, v[to], 0);
}
/* the page's name in the chain: EDIT 1 . OSC (the engine's page name), KIT, ALL DEST, SENDS .. */
static __attribute__((noinline)) const char *nav_page_name(void)
{
    static char b[24];
    uint32_t sec = nav_sec(), e = nav_cur();
    const nv_t *v = &nav_ch(sec)->e[e];
    if (sec != SEC_HOME && v->t && !str_eq(PAGES[ui.page].title, v->t)) {   /* (PHRASES, CHANCE: their own) */
        str_cpy(b, PAGES[ui.page].title, 24);
        return b;
    }
    str_cpy(b, v->nm ? v->nm : v->t, 24);
    if (sec == SEC_EDIT && v->g == 0u && !v->nm && PAGES[ui.page].scope == SC_ENGINE) {   /* (EDIT 1 . OSC) */
        const char *pt = trk_engp(TSEL)->page_title[PAGES[ui.page].id[0] != P_E0];
        if (pt && pt[0] && pt[0] != '-') {
            str_cpy(b + str_len(b), " \xB7 ", 4);
            str_cpy(b + str_len(b), pt, 24 - str_len(b));
        }
    }
    return b;
}
/* (code, the tests, the renders) the page of section sec named nm in its chain */
static int nav_put(uint32_t sec, const char *nm)
{
    const ch_t *c = nav_ch(sec);
    uint32_t k;
    for (k = 0; k < c->n; k++)
        if (str_eq(c->e[k].nm ? c->e[k].nm : c->e[k].t, nm) && nv_ok(sec, k)) {
            nav_go(sec, k, 0);
            return 1;
        }
    return 0;
}

/* EDIT > FOCUS (a synth's detail): the parameter shown (E1..E8 that has a name: 0..7), the MOD slot aimed at it (its
 * source set; -1 none) */
static int focus_named(uint32_t k)
{
    const param_desc_t *d = track_desc(TSEL, P_E0 + (k & 7u));
    return d && d->label && d->label[0] != '-' && d->max > d->min;
}
static uint32_t focus_k(void)
{
    uint32_t k = ui.focus & 7u, n;
    for (n = 0; n < 8u && !focus_named(k); n++)
        k = (k + 1u) & 7u;
    return k;
}
static int32_t focus_slot(uint32_t k)
{
    uint32_t j;
    for (j = 0; j < 4u; j++)
        if (TSEL->p[P_M1SRC + 3u * j] && TSEL->p[P_M1DST + 3u * j] == (int16_t)(MD_E1 + k))
            return (int32_t)j;
    return -1;
}
