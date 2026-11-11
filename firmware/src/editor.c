/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Editor protocol: SysEx for the web editor (web/EDITOR_PROTOCOL.md; v2 = user presets + live sync,
 * v3 = four tracks: the v1 / v2 commands act on the selected track, cmds 27-30 reach any track;
 * v4 = TRACK_PARAM (31) and the TRACK_CHANGED push (32), enabled by WATCH bit 1).
 *   F0 7D 46 4C cmd args.. F7     (7D = non-commercial ID, "FL")
 * Values are 14 bit, two 7-bit bytes LSB first, offset by 8192 (so -8192..8191).
 * Every request gets a reply with the same cmd; 23/24/26 are also pushed
 * while watched. Frames arrive through sx_frame (usb.c), replies leave
 * through ota_wire_send(). */
#define ED_HDR0 0x7D
#define ED_HDR1 0x46
#define ED_HDR2 0x4C
enum { ED_INFO = 1, ED_GET, ED_SET, ED_DUMP, ED_DESC, ED_STEP_GET, ED_STEP_SET, ED_PRESET, ED_PROJECT, ED_NAMES,
       ED_UP_LIST = 16, ED_UP_GET, ED_UP_PUT, ED_UP_STORE, ED_UP_LOAD, ED_UP_ERASE,   /* v2: user presets */
       ED_WATCH, ED_CHANGED, ED_RELOAD, ED_PING, ED_STEP_CHANGED,              /* v2: live sync */
       ED_TRACK, ED_TRACK_MIX, ED_TRACK_DUMP, ED_TRACK_STEP,                    /* v3: tracks */
       ED_TRACK_PARAM, ED_TRACK_CHANGED, ED_SONG,
       ED_UI_STATE, ED_UI_SET, ED_UI_PALETTES, ED_FAV_GET, ED_FAV_SET,
       ED_LYRIC,                                                                /* (0.8.5) VOICE's lyric: gone, its number kept */
       ED_MOTION = 64, ED_BACKUP_LIST, ED_BACKUP_GET, ED_BACKUP_PUT,                               /* v6: song chain */
       ED_PROBE = 100 };                                                        /* YEHNAH_PROBE builds (usb.c pr) */

static uint8_t ed_out[600];
static uint32_t ed_n;

static void ed_begin(uint32_t cmd)
{
    ed_out[0] = 0xF0;
    ed_out[1] = ED_HDR0;
    ed_out[2] = ED_HDR1;
    ed_out[3] = ED_HDR2;
    ed_out[4] = (uint8_t)cmd;
    ed_n = 5;
}
static void ed_b(uint32_t v)
{
    if (ed_n < sizeof ed_out - 1u)
        ed_out[ed_n++] = (uint8_t)(v & 0x7Fu);
}
static void ed_v(int32_t v)
{
    uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192);
    ed_b(u);
    ed_b(u >> 7);
}
static void ed_str(const char *s, uint32_t max)   /* ASCII, 0-terminated */
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i] & 0x7Fu);
    ed_b(0);
}
static void ed_send(void)
{
    ed_out[ed_n++] = 0xF7;
    ota_wire_send(ed_out, ed_n);
}
static int32_t ed_rv(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }
/* (chord-harp fork) a parameter id on the wire: one byte (ids 0..127), or wide (INFO 'W') two, id & 127 then id >> 7.
 * A request says which by its length, its reply the same; the pushes are wide once WATCH asked for it (bit 2) */
static uint32_t ed_id(const uint8_t *p, uint32_t w) { return w ? p[0] | (uint32_t)p[1] << 7 : p[0]; }
static void ed_id_out(uint32_t id, uint32_t w)
{
    ed_b(id);
    if (w) ed_b(id >> 7);
}

/* 7-bit packing (the backup's PUT): groups of a msb byte, then up to 7 bytes */
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;                                 /* groups: msb byte, then up to 7 bytes */
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;                              /* incomplete group, overflow, or unused mask bits */
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
/* the engine byte of DUMP / RELOAD / TRACK */
static uint32_t ed_eng(const track_t *t) { return trk_eng(t); }

/* ---- live sync (v2): while the editor WATCHes, device-side changes are pushed.
 * Shadows of the selected track's p[] + song.g[] and of its steps are kept in step
 * with what the editor knows (its own SET / STEP_SET update them); the main loop
 * compares and pushes CHANGED / STEP_CHANGED, or RELOAD after a load or when another
 * track was selected (v3: RELOAD and STEP_CHANGED carry the selected track). Pushes
 * go out only into a half-empty SysEx ring, so they never wait. */
#define ED_PUSH_MAX 4u                                   /* frames per pass */
#define ED_NV (P_COUNT + G_COUNT)
/* v4: the parameters of the tracks that are not selected which TRACK_CHANGED follows (the mixer) */
static const uint8_t ED_TIDS[3] = {P_LEVEL, P_PAN, P_MUTE};
#define ED_NT (NTRK * 3u)
static struct {
    uint8_t on, eng, preset, sel;
    uint8_t v4;                                          /* WATCH bit 1: TRACK_CHANGED pushes too */
    uint8_t wide;                                        /* WATCH bit 2: the pushes' ids wide (ed_id) */
    uint32_t pos, last_ms, run_ms, resets;
    int16_t v[ED_NV];                                    /* TSEL->p[], then song.g[] */
    uint16_t t[ED_NV];                                   /* ms (low 16 bits) of the last push */
    uint32_t st[NSTEP];                                  /* step signatures */
    int16_t tv[ED_NT];                                   /* v4: trk[k].p[ED_TIDS[j]] at k * 3 + j */
    uint16_t tt[ED_NT];
    uint32_t tpos;
} ed_w;

static int16_t *ed_val(uint32_t i) { return i < P_COUNT ? &TSEL->p[i] : &song.g[i - P_COUNT]; }
static uint32_t ed_step_sig(const step_t *s)
{
    return ((uint32_t)s->note[0] | (uint32_t)s->note[1] << 7 | (uint32_t)s->note[2] << 14 | (uint32_t)s->note[3] << 21) ^
           ((uint32_t)s->n << 28) ^ ((uint32_t)s->time * 0x9E3779B1u) ^ ((uint32_t)s->flags * 0x85EBCA6Bu) ^
           ((uint32_t)s->vel * 0xC2B2AE35u) ^ ((uint32_t)(s->hit | s->acc << 8) * 0x27D4EB2Fu) ^ ((uint32_t)s->probability * 0x165667B1u);
}
static void ed_shadow(void)                              /* the editor is in sync */
{
    uint32_t i;
    for (i = 0; i < ED_NV; i++)
        ed_w.v[i] = *ed_val(i);
    for (i = 0; i < NSTEP; i++)
        ed_w.st[i] = ed_step_sig(&seq_steps(TSEL)[i]);
    for (i = 0; i < ED_NT; i++)
        ed_w.tv[i] = trk[i / 3u].p[ED_TIDS[i % 3u]];
    ed_w.eng = (uint8_t)ed_eng(TSEL);
    ed_w.preset = TSEL->preset;
    ed_w.sel = song.sel;
    sync_reload = 0;
}
/* the editor's own sound load on the selected track (PRESET, SET of G_ENGSEL): the editor re-reads DUMP after
 * the reply, so the shadow takes the load (engine, preset, the parameters it changed) and no RELOAD echoes back
 * (INFO 53 01 bit 1). A RELOAD already due before it (a load or selection on the device) still goes out. */
typedef struct { int16_t p[P_COUNT]; uint8_t eng, preset, due; } ed_load_t;
static void ed_load_before(ed_load_t *b)
{
    memcpy(b->p, TSEL->p, sizeof b->p);
    b->eng = (uint8_t)ed_eng(TSEL);
    b->preset = TSEL->preset;
    b->due = sync_reload || b->eng != ed_w.eng || b->preset != ed_w.preset || song.sel != ed_w.sel;
}
static void ed_load_after(const ed_load_t *b)
{
    uint32_t i;
    if (!ed_w.on || b->due)
        return;
    sync_reload = 0;
    ed_w.eng = (uint8_t)ed_eng(TSEL);
    ed_w.preset = TSEL->preset;
    for (i = 0; i < P_COUNT; i++)                        /* the load's values; a pending push of another stays */
        if (TSEL->p[i] != b->p[i])
            ed_w.v[i] = TSEL->p[i];
}
static int ed_room(void) { return so_w - so_r + 8u <= SXQ / 2u; }
static void ed_known(uint32_t k, uint32_t id)            /* the editor's own change of trk[k].p[id]: no push */
{
    uint32_t j;
    if (k == song.sel) {
        ed_w.v[id] = trk[k].p[id];
        return;
    }
    for (j = 0; j < 3u; j++)
        if (ED_TIDS[j] == id)
            ed_w.tv[k * 3u + j] = trk[k].p[id];
}

static void ed_sync(void)                                /* main loop */
{
    uint32_t i, n = 0, now = fm1_ms;
    if (!ed_w.on || now - ed_w.run_ms < 5u)
        return;
    ed_w.run_ms = now;
    if (!usb.config || usb.resets != ed_w.resets || now - ed_w.last_ms > 3000u) {
        ed_w.on = 0;                                     /* no host, USB reset, or 3 s without a request */
        return;
    }
    if (sync_reload || ed_eng(TSEL) != ed_w.eng || TSEL->preset != ed_w.preset || song.sel != ed_w.sel) {
        if (!ed_room())
            return;
        ed_shadow();
        ed_begin(ED_RELOAD);
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        ed_b(song.sel);
        ed_send();
        return;
    }
    for (i = 0; i < NSTEP && n < ED_PUSH_MAX; i++) {
        uint32_t h = ed_step_sig(&seq_steps(TSEL)[i]);
        if (h == ed_w.st[i])
            continue;
        if (!ed_room())
            return;
        ed_w.st[i] = h;
        ed_begin(ED_STEP_CHANGED);
        ed_b(i);
        ed_b(song.sel);
        ed_send();
        n++;
    }
    for (i = 0; i < ED_NV && n < ED_PUSH_MAX; i++) {    /* round robin: nothing starves */
        uint32_t k = (ed_w.pos + i) % ED_NV;
        int16_t v = *ed_val(k);
        if (v == ed_w.v[k] || (uint16_t)(now - ed_w.t[k]) < 20u)
            continue;                                    /* coalesced: the latest value goes out later */
        if (!ed_room())
            return;
        ed_w.v[k] = v;
        ed_w.t[k] = (uint16_t)now;
        ed_w.pos = k + 1u;
        if (!ed_w.wide && (k < P_COUNT ? k : k - P_COUNT) > 127u)
            continue;                                    /* (no byte for it: an editor of one-byte ids has not asked) */
        ed_begin(ED_CHANGED);
        ed_b(k < P_COUNT ? 0u : 1u);
        ed_id_out(k < P_COUNT ? k : k - P_COUNT, ed_w.wide);
        ed_v(v);
        ed_send();
        n++;
    }
    for (i = 0; ed_w.v4 && i < ED_NT && n < ED_PUSH_MAX; i++) {   /* v4: the other tracks' mix */
        uint32_t k = (ed_w.tpos + i) % ED_NT, tr = k / 3u, id = ED_TIDS[k % 3u];
        int16_t v = trk[tr].p[id];
        if (tr == song.sel || v == ed_w.tv[k] || (uint16_t)(now - ed_w.tt[k]) < 20u)
            continue;                                    /* (the selected track: CHANGED above) */
        if (!ed_room())
            return;
        ed_w.tv[k] = v;
        ed_w.tt[k] = (uint16_t)now;
        ed_begin(ED_TRACK_CHANGED);
        ed_b(tr);
        ed_id_out(id, ed_w.wide);
        ed_v(v);
        ed_send();
        n++;
        ed_w.tpos = k + 1u;
    }
}

/* descriptor of parameter id of track t: the engine parameters of the engine it asked for
 * (t->engine follows in the audio ISR, after a short fade) */
static const param_desc_t *ed_tdesc(const track_t *t, uint32_t id) { return param_desc_of(trk_eng(t), id); }
/* descriptor and value slot of (scope, id): scope 0 = the selected track, 1 = global */
static int16_t ed_fmx_v;         /* (2026-11-11) the virtual id P_COUNT + k: FMINI's patch setting k (core.h fmx_get), INFO 'X' */
static const param_desc_t *ed_desc(uint32_t scope, uint32_t id, int16_t **vp)
{
    if (scope == 0 && id >= P_COUNT && id < P_COUNT + NFMX && ed_eng(TSEL) == ENGI_MINI) {
        ed_fmx_v = (int16_t)fmx_get(TSEL->p, id - P_COUNT);
        *vp = &ed_fmx_v;
        return &FMX_DESC[id - P_COUNT];
    }
    if (scope == 0 && id < P_COUNT) {
        *vp = &TSEL->p[id];
        return ed_tdesc(TSEL, id);
    }
    if (scope == 1 && id < G_COUNT) {
        *vp = &song.g[id];
        return &GP[id];
    }
    return 0;
}

/* a step as STEP_SET / TRACK_STEP send it (na bytes): n, 4 notes, time, flags, vel, then (v5) the lane hits
 * and their accents: hit & 127, acc & 127, bit 7 of each (bit 0 hit, bit 1 acc), (v7) the chance, (RATCH, INFO
 * 52 01) the hits 1..4; each value made valid. 8 bytes (an editor of before the grid): the hits stay; without
 * the chance or the ratchet the step keeps its own (flags carry accent and slide only) */
static void ed_step_put(step_t *st, const uint8_t *a, uint32_t na)
{
    uint32_t i;
    st->n = (uint8_t)(a[0] > 4u ? 4u : a[0]);
    for (i = 0; i < 4u; i++)
        st->note[i] = a[1 + i] & 0x7Fu;
    st->time = (uint8_t)(a[5] > ST_REST ? ST_REST : a[5]);
    st->flags = (uint8_t)((a[6] & (SF_ACCENT | SF_SLIDE)) | (st->flags & (SF_RATCH | SF_MFINE)));   /* (0.9.1: its MICRO kept) */
    st->vel = a[7] & 0x7Fu;
    if (na >= 11u) {
        st->hit = (uint8_t)((a[8] & 0x7Fu) | (a[10] & 1u) << 7);
        st->acc = (uint8_t)(((a[9] & 0x7Fu) | (a[10] & 2u) << 6) & st->hit);
    }
    if (na >= 12u) step_set_chance(st, a[11] <= 100u ? a[11] : 100u);
    if (na >= 13u) step_set_ratchet(st, a[12]);
    if (na >= 21u)                                        /* (0.8.5, INFO 4F 01 07) each lane's mode, 0..6 (core.h LM_*) */
        for (i = 0; i < NLANE; i++)
            step_set_lmode(st, i, a[13 + i] < LM_N ? a[13 + i] : LM_NONE);
    ui.force = 1;
}
static void ed_step_reply(const step_t *st, int lanes)   /* the same 13 bytes; (0.8.5) then the 8 lanes' modes, when the
                                                         * request asked for them (a write with them, a read with its
                                                         * flag 1): every reply of before as it was */
{
    uint32_t i;
    ed_b(st->n);
    for (i = 0; i < 4u; i++)
        ed_b(st->note[i]);
    ed_b(st->time);
    ed_b(st->flags & (SF_ACCENT | SF_SLIDE));
    ed_b(st->vel);
    ed_b(st->hit & 0x7Fu);
    ed_b(st->acc & 0x7Fu);
    ed_b((uint32_t)(st->hit >> 7) | (uint32_t)(st->acc >> 7) << 1);
    ed_b(step_chance(st));
    ed_b(step_ratchet(st));
    for (i = 0; lanes && i < NLANE; i++)
        ed_b(step_lmode(st, i));
}

/* a flash write from the editor while the transport runs: stop it first (the erase silences the audio and
 * stalls the sequencer; the device's own SAVE refuses with "STOP TO SAVE", the editor's confirmed save stops) */
static int ed_flash_stop(void)
{
    uint32_t t0 = fm1_ms;
    if (!transport_busy())
        return 0;
    transport_req = 2;
    while ((song.playing || chain_busy()) && fm1_ms - t0 < 100u)          /* the audio ISR stops it at its next block */
        fm1_wdt_feed();
    return song.playing || chain_busy();
}

#include "editor_preferences.c"
#include "editor_backup.c"
#include "editor_fm6.c"
#include "editor_menu.c"
#if YEHNAH_RESREGION
#include "editor_res.c"           /* (chord-harp fork) the resource region, cmd 82 */
#endif

/* (chord-harp fork) a motion id on the wire: one byte, the DJ FILTER's 127 (MOT_DJF_V4) as before FUND widened the ids,
 * or wide (op + 16: ed_id, the DJ FILTER's MOT_DJF itself); one byte has none for the ids 127 .. P_COUNT - 1 */
static uint32_t ed_mot_in(const uint8_t *p, uint32_t w) { return w ? ed_id(p, 1) : *p == MOT_DJF_V4 ? MOT_DJF : *p; }
static uint32_t ed_mot_fits(const motion_event_t *e, uint32_t k, uint32_t w)
{
    return MOTION_TRK(e) == k && (w || MOTION_ID(e) < MOT_DJF_V4 || MOTION_ID(e) == MOT_DJF);
}
/* MOTION's reply: the records of track k as (step, id, v14), a lock's id without its MOTION_LOCK bit; one-byte ids
 * leave out a record they cannot name (the count is of those sent); kinds (1.1: the ops 5..7): then one byte per
 * record, in the same order, 0 automation, 1 lock. The query and the ops 1..4 reply as before 1.1 (an editor of
 * before reads a lock as an automation event) */
static void ed_motion_reply(uint32_t k, uint32_t rc, uint32_t kinds, uint32_t w)
{
    track_t *t = &trk[k];
    uint32_t i, c = 0;
    for (i = 0; i < motion.count; i++)
        c += ed_mot_fits(&motion.event[i], k, w);
    ed_b(k); ed_b(rc); ed_b(motion_enabled(t)); ed_b(c); ed_b(MOTION_MAX);
    for (i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if (!ed_mot_fits(e, k, w)) continue;
        ed_b(MOTION_STEP(e));
        ed_id_out(w || MOTION_ID(e) != MOT_DJF ? MOTION_ID(e) : MOT_DJF_V4, w);
        ed_v(MOTION_VAL(e));
    }
    for (i = 0; kinds && i < motion.count; i++)
        if (ed_mot_fits(&motion.event[i], k, w))
            ed_b((motion.event[i].param & MOTION_LOCK) != 0u);
}
static int ed_lm_ok(const uint8_t *a)                  /* (0.8.5) eight lane modes, each 0..6 */
{
    uint32_t i;
    for (i = 0; i < NLANE; i++)
        if (a[i] >= LM_N) return 0;
    return 1;
}
static int ed_args_ok(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    switch (cmd) {
    case ED_INFO: case ED_DUMP: case ED_PING:
    case ED_UI_STATE: case ED_UI_PALETTES:
        return !n;
    case ED_PRESET: case ED_PROJECT: case ED_UP_LIST:
        return n == 2u;
    case ED_GET: case ED_DESC:                             /* (chord-harp fork: 3, a wide id) */
        return n == 2u || n == 3u;
    case ED_SET:
        return n == 4u || n == 5u;
    case ED_TRACK_PARAM:
        return n >= 2u && n <= 5u;
    case ED_STEP_GET:
        return n == 1u || (n == 2u && a[1] == 1u);         /* (0.8.5: 1, the lanes' modes too) */
    case ED_NAMES:                                         /* engine; (chord-harp fork) engine, from (2 bytes) */
        return n == 1u || n == 3u;
    case ED_UP_GET: case ED_UP_LOAD: case ED_UP_ERASE: case ED_WATCH: case ED_TRACK_DUMP: case ED_MENU_DESC:
        return n == 1u;
    case ED_MENU_SET:
        return n == 3u;
    case ED_STEP_SET:
        return n == 9u || n == 12u || (n == 13u && a[12] <= 100u) ||               /* notes only, or the complete grid extension, */
               (n == 14u && a[12] <= 100u && a[13] >= 1u && a[13] <= 4u) ||     /* its chance, its ratchet, */
               (n == 22u && a[12] <= 100u && a[13] >= 1u && a[13] <= 4u && ed_lm_ok(a + 14));   /* (0.8.5) the lanes' modes */
    case ED_TRACK:
        return n <= 1u;
    case ED_TRACK_MIX:
        return n == 1u || n == 4u;
    case ED_TRACK_STEP:
        return n == 2u || (n == 3u && a[2] == 1u) || n == 10u || n == 13u || (n == 14u && a[13] <= 100u) ||
               (n == 15u && a[13] <= 100u && a[14] >= 1u && a[14] <= 4u) ||
               (n == 23u && a[13] <= 100u && a[14] >= 1u && a[14] <= 4u && ed_lm_ok(a + 15));
    case ED_MOTION: {                                      /* (chord-harp fork) op + 16: wide ids (one more byte in 3..5) */
        uint32_t w = n > 1u && a[1] >= 16u, op = n > 1u ? a[1] & 15u : 0u, m = n - (w && op >= 3u && op <= 5u);
        if (n > 1u && (a[1] > 23u || op > 7u || (!w && !op)))
            return 0;
        return (m == 1u || (m == 2u && (op == 2u || op == 7u || (w && !op))) || (m == 3u && op == 1u && a[2] <= 1u) ||
                (m == 6u && (op == 3u || op == 5u)) || (m == 4u && op == 4u) || (m == 3u && op == 6u)) &&
               a[0] < NTRK;
    }
    case ED_SONG:
        return n && (a[0] == 1u || n == 1u);
    default:
        return 1;                                  /* variable payloads validate in their handler */
    }
}

static void ed_handle(const uint8_t *f, uint32_t n)   /* f: the bytes between F0 and F7 */
{
    if (n < 4u)
        return;
    uint32_t cmd = f[3], i;
    const uint8_t *a = f + 4;
    uint32_t na = n - 4u;
    int16_t *vp;
    const param_desc_t *d;
    if (!ed_args_ok(cmd, a, na))
        return;
    if (cmd >= ED_BACKUP_LIST && cmd <= ED_BACKUP_PUT)
        autosave_hold();                                /* (a transfer: no autosave meanwhile, project.c) */
    ed_begin(cmd);
    if (ed_ui_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_backup_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_fm6_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_menu_handle(cmd, a, na)) { ed_send(); return; }
#if YEHNAH_RESREGION
    if (ed_res_handle(cmd, a, na)) { ed_send(); return; }
#endif
    switch (cmd) {
    case ED_MOTION: {
        track_t *t = &trk[a[0]];
        uint32_t rc = 0, w = na > 1u && a[1] >= 16u, op = na > 1u ? a[1] & 15u : 0u;
        uint32_t id = op >= 3u && op <= 5u ? ed_mot_in(a + 3, w) : 0u;
        const uint8_t *vv = a + 4 + w;                     /* (ops 3, 5: the value) */
        if (op == 0u || op == 7u) {
            /* (the query; 7 with the kinds) */
        } else if (chain_busy()) rc = 3;
        else {
            if (op == 1u) motion_set_enabled(t, a[2]);
            else if (op == 2u) { load_begin(t, LOAD_PAT); motion_clear(t); load_end(t); }
            else if (op == 3u) rc = motion_set_event(t, a[2], id, (int16_t)ed_rv(vv));
            else if (op == 4u && a[2] < NSTEP && motion_param(id)) motion_delete_event(t, a[2], id);
            else if (op == 5u) rc = motion_set_lock(t, a[2], id, (int16_t)ed_rv(vv));
            else if (op == 6u && (a[2] < NSTEP || a[2] == 127u)) (void)motion_clear_locks(t, a[2]);
            else rc = 1;
            ui.force = 1;
        }
        ed_motion_reply(a[0], rc, op >= 5u, w);
        break;
    }
    case ED_INFO:
        ed_str("Yehnah " YEHNAH_VERSION, 24);   /* (chord-harp fork; 0.7.0: was "YEHNAH v..", then "Yehnah ": the web
                                                         * tools take all three) */
        ed_b(NENGINES);
        ed_b(P_COUNT);
        ed_b(G_COUNT);
        ed_b(NSTEP);
        ed_b(P_E0);
        for (i = 0; i < NENGINES; i++)
            ed_str(ENGINES[i]->name, 8);
        ed_b(NTRK);                                       /* v3 */
        ed_b(CHAIN_ROWS);                                  /* v6 */
        ed_b(0x55); ed_b(1); ed_b(ed_ui_caps());             /* tagged preferences v1: commands 34..38 */
        ed_b(0x4d); ed_b(1); ed_b(MOTION_MAX); ed_b(1); /* motion + chance v1 */
        ed_b(0x42); ed_b(1); ed_b(3); /* bounded full-backup read + restore */
        ed_b(0x46); ed_b(1); ed_b(FM6_NFACTORY); ed_b(0);   /* FM6 patches: cmds 68..71 (no bank since 1.0.3) */
        ed_b(0x53); ed_b(1); ed_b(3);   /* live sync: bit 0 WATCH while on keeps the shadow, bit 1 no RELOAD echo */
        ed_b(0x50); ed_b(1); ed_b(3);   /* FM6 patches v2: bit 0 no bank (SLOT F1..F8, 8 OWN), bit 1 user preset
                                         * patches (FM6 target 3, backup id 9) */
        ed_b(0x4E); ed_b(1); ed_b(ED_MENU_N);   /* MENU settings: cmds 72, 73; the items MENU_DESC offers */
        ed_b(0x52); ed_b(1); ed_b(4);   /* RATCH: a step's ratchet (1..4 hits) after its chance */
        ed_b(0x4C); ed_b(1); ed_b(1);   /* 1.1 parameter locks: MOTION ops 5..7, the kinds after the records */
        ed_b(0x56); ed_b(1); ed_b(FM6_NBANK);   /* (0.6.11, Felucca 1.4.1) FM6's voice bank: cmds 74..77, SLOT 9..40 */
        ed_b(0x4A); ed_b(1); ed_b(1);   /* (0.6.62) 'J': this is Yehnah (the web installer writes over it, web/index_pkg.html) */
        ed_b(0x4F); ed_b(1); ed_b(LM_N);   /* (0.8.5) 'O': the DRUM-X octaves, a step's lane modes 0..6 after its ratchet */
        ed_b(0x58); ed_b(1); ed_b(NFMX);   /* (2026-11-11) 'X': FMINI's patch settings as the ids P_COUNT .. P_COUNT + NFMX - 1 (GET / SET / DESC) */
        ed_b(0x57); ed_b(1); ed_b(P_COUNT >> 7); ed_b(P_E0 >> 7);   /* (chord-harp fork) 'W': wide parameter ids (ed_id);
                                                         * the bits of P_COUNT and P_E0 past the 7 above */
        break;
    case ED_GET:
    case ED_SET: {
        uint32_t w = na == 3u || na == 5u, id = ed_id(a + 1, w);
        if (!(d = ed_desc(a[0], id, &vp)))
            return;
        if (cmd == ED_SET && !(chain_busy() && !a[0] && id >= P_SLEN && id <= P_SGATE)) {
            if (a[0] == 1 && id == G_ENGSEL) {          /* engine change: the safe path (1 without YEHNAH_FM4:
                                                           * DIGITAL's first preset, as FM6) */
                ed_load_t b;
                ed_load_before(&b);
                set_engine((uint32_t)clamp(ed_rv(a + 2 + w), 0, NENGINES - 1));
                ed_load_after(&b);
            } else if (a[0] == 1 && id == G_DJF) {       /* (0.9.1) the DJ FILTER: recorded as its knob is */
                motion_djf_set(TSEL, (int16_t)clamp(ed_rv(a + 2 + w), d->min, d->max));
            } else if (d->max > d->min) {
                *vp = (int16_t)enum_orig(d, clamp(ed_rv(a + 2 + w), d->min, d->max));
                if (a[0] == 0 && id >= P_COUNT)               /* FMINI's patch setting: into its bit field */
                    fmx_set(TSEL->p, id - P_COUNT, *vp);
                if (a[0] == 0)
                    (void)motion_capture(TSEL, id, *vp);
            }
            ui.force = 1;
            if (a[0] || id < P_COUNT)
                ed_w.v[a[0] ? P_COUNT + id : id] = *vp;   /* the editor's own change: no push */
        }
        ed_b(a[0]);
        ed_id_out(id, w);
        ed_v(*vp);
        break;
    }
    case ED_DUMP:
        for (i = 0; i < ED_NV; i++)                       /* the editor gets them all here */
            ed_w.v[i] = *ed_val(i);
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        for (i = 0; i < P_COUNT; i++)
            ed_v(motion_base_value(TSEL, i));
        for (i = 0; i < G_COUNT; i++)
            ed_v(song.g[i]);
        break;
    case ED_DESC:
        if (!(d = ed_desc(a[0], ed_id(a + 1, na == 3u), &vp)))
            return;
        ed_b(a[0]);
        ed_id_out(ed_id(a + 1, na == 3u), na == 3u);
        ed_b(d->fmt);
        ed_v(d->min);
        ed_v(d->max);
        ed_v(d->def);
        ed_str(d->label, 8);
        ed_str(d->unit, 8);
        if (d->fmt == F_ENUM && d->names)                  /* up to 32 (0.7.0: the matrix DST has 30; was 24) */
            /* (HARP's SET: its 16 names; P_FMODEL's from 1, names[v]) */
            for (i = 0; i <= (uint32_t)(d->max - d->min) && i < 32u && d->names[d->min + i]; i++)
                ed_str(d->names[d->min + i], 8);
        break;
    case ED_STEP_GET:
    case ED_STEP_SET: {
        step_t *st;
        if (na < 1u || a[0] >= NSTEP)
            return;
        st = &TSEL->step[a[0]];
        if (cmd == ED_STEP_SET && na >= 9u && !chain_busy()) {
            ed_step_put(st, a + 1, na - 1u);
        }
        ed_w.st[a[0]] = ed_step_sig(&seq_steps(TSEL)[a[0]]);
        ed_b(a[0]);
        ed_step_reply(&seq_steps(TSEL)[a[0]], na == 22u || (cmd == ED_STEP_GET && na == 2u));
        break;
    }
    case ED_PRESET: {                                      /* engine, preset */
        ed_load_t b;
        if (na < 2u || a[0] >= NENGINES)
            return;
        ed_load_before(&b);
        {
            if (a[0] != TSEL->eng_req)
                set_engine(a[0]);
            apply_preset(a[1]);
        }
        ed_load_after(&b);
        ui.force = 1;
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        break;
    }
    case ED_PROJECT:                                       /* 0 = load, 1 = save, 2 = query; slot 0..3 */
        if (na < 2u || a[0] > 2u || a[1] >= 4u)
            return;
        if (a[0] == 1u) {
            if (ed_flash_stop() || project_save(a[1]))
                return;                              /* the legacy reply has no failure bit: do not confirm a failed save */
        }
        else if (a[0] == 0u)
            project_load(a[1] & 3u);
        ed_b(a[0]);
        ed_b(a[1] & 3u);
        ed_b(project_used(a[1] & 3u));
        break;
    case ED_NAMES: {                                       /* preset names of an engine. (chord-harp fork, 2026-10-27)
                                                            * as many as the reply holds from FROM (a[1..2], 0 without),
                                                            * their count, the two edit-page titles, then FROM and the
                                                            * engine's total (2 bytes each): the editor asks on from
                                                            * there (JUNO's 128 names are ~1.5 KB). An editor of before
                                                            * reads the first ones, as it read them all */
        const engine_t *e;
        uint32_t from, np, at, cnt = 0, tail = 6u;
        if (na < 1u || a[0] >= NENGINES)
            return;
        e = ENGINES[a[0]];
        from = na >= 3u ? a[1] | (uint32_t)a[2] << 7 : 0u;
        np = ENG_NP(e);
        for (i = 0; i < 2u; i++)
            tail += (uint32_t)(e->page_title[i] ? str_len(e->page_title[i]) : 0u) + 1u;
        ed_b(a[0]);
        at = ed_n;
        ed_b(0);
        for (i = from; i < np; i++) {
            uint32_t l = str_len(e->presets[i].name);
            if (ed_n + (l > 12u ? 12u : l) + 1u + tail > sizeof ed_out)
                break;
            ed_str(e->presets[i].name, 12);
            cnt++;
        }
        ed_out[at] = (uint8_t)cnt;
        for (i = 0; i < 2u; i++)                           /* then the two edit-page titles */
            ed_str(e->page_title[i], 8);
        ed_b(from & 127u);
        ed_b(from >> 7);
        ed_b(np & 127u);
        ed_b(np >> 7);
        break;
    }
    case ED_UP_LIST: {                                     /* start, count -> start, count, total, per slot: used, engine, name */
        uint32_t s0, cnt;
        if (na < 2u)
            return;
        s0 = a[0];
        cnt = a[1] > 16u ? 16u : a[1];
        if (s0 >= UP_SLOTS)
            cnt = 0;
        else if (s0 + cnt > UP_SLOTS)
            cnt = UP_SLOTS - s0;
        ed_b(s0);
        ed_b(cnt);
        ed_b(UP_SLOTS);
        for (i = s0; i < s0 + cnt; i++) {
            char nm[13] = {0};
            uint32_t j, u = (uint32_t)up_used(i);
            for (j = 0; u && j < 12u; j++)
                nm[j] = up_rec(i)->name[j];
            ed_b(u);
            ed_b(u ? up_rec(i)->engine : 0u);
            ed_str(nm, 12);
        }
        break;
    }
    case ED_UP_GET: {                                      /* slot -> slot, used, engine, name, P_COUNT x v14, 16 x (note,
                                                            * flags), (v5) kind [, 16 x hi] (upreset.c up_parse) */
        int16_t v[P_COUNT];
        char nm[13] = {0};
        const up_rec_t *r;
        uint32_t u;
        if (na < 1u || a[0] >= UP_SLOTS)
            return;
        r = up_rec(a[0]);
        u = (uint32_t)up_used(a[0]);
        if (u)
            up_values(r, v);                               /* today's P_* order, clamped */
        for (i = 0; u && i < 12u; i++)
            nm[i] = r->name[i];
        ed_b(a[0]);
        ed_b(u);
        ed_b(u ? r->engine : 0u);
        ed_str(nm, 12);
        for (i = 0; i < P_COUNT; i++)
            ed_v(u ? v[i] : 0);
        for (i = 0; i < 16u; i++) {
            ed_b(u ? r->note[i] & 0x7Fu : 0u);
            ed_b(u ? r->flags[i] & 0x7Fu : 0u);
        }
        ed_b(u && up_grid(r));                  /* kind: 1 = a drum grid, then bit 7 of each step's */
        for (i = 0; u && up_grid(r) && i < 16u; i++)   /* hits (bit 0) and accents (bit 1) */
            ed_b((uint32_t)(r->note[i] >> 7) | (uint32_t)(r->flags[i] >> 7) << 1);
        break;
    }
    case ED_UP_PUT: {                                      /* slot, engine, name, values, pattern -> slot, rc */
        static up_rec_t r;
        uint32_t slot = 0, rc;
        if (na < 1u)
            return;
        rc = (uint32_t)up_parse(a, na, &r, &slot);
        if (!rc && ed_flash_stop())
            rc = 2;
        if (!rc) {
            int16_t v[P_COUNT];
            up_values(&r, v);                              /* each value inside its range */
            for (i = 0; i < P_COUNT; i++)
                up_set_value(&r, i, v[i]);                 /* the record's own form (v4: a byte each) */
            rc = up_put(slot, &r) ? 2u : 0u;
        }
        ed_b(a[0]);
        ed_b(rc);
        break;
    }
    case ED_UP_STORE: {                                    /* slot, name -> slot, rc */
        char nm[13] = {0};
        uint32_t n0, rc = 1;
        if (na < 1u)
            return;
        for (n0 = 0; 1u + n0 < na && a[1 + n0] && n0 < 13u; n0++)
            ;
        if (a[0] < UP_SLOTS && (!n0 || up_name_ok(a + 1, n0)) &&
            (1u + n0 == na || (2u + n0 == na && !a[1u + n0]))) {   /* "" = automatic name */
            for (i = 0; i < n0; i++)
                nm[i] = (char)a[1 + i];
            int r;
            r = ed_flash_stop() ? 2 : up_store(a[0], nm);
            rc = r ? 2u : 0u;
        }
        ed_b(a[0]);
        ed_b(rc);
        break;
    }
    case ED_UP_LOAD:                                       /* slot -> slot, rc */
        if (na < 1u)
            return;
        ed_b(a[0]);
        ed_b(a[0] < UP_SLOTS && !up_load(a[0]) ? 0u : 1u);
        break;
    case ED_UP_ERASE:
        if (na < 1u)
            return;
        ed_b(a[0]);
        ed_b(a[0] >= UP_SLOTS ? 1u : ed_flash_stop() || up_put(a[0], 0) ? 2u : 0u);
        break;
    case ED_WATCH: {                                       /* on -> on */
        uint32_t keep, v4was = ed_w.v4;
        if (na < 1u)
            return;
        keep = ed_w.on && ed_w.resets == usb.resets && (a[0] & 1u);   /* already watching: the changes not yet */
        ed_w.on = a[0] & 1u;                                          /* pushed stay pending (INFO 53 01 bit 0) */
        ed_w.v4 = (uint8_t)(ed_w.on && (a[0] & 2u));     /* v4: also TRACK_CHANGED; the reply says it is known */
        ed_w.wide = (uint8_t)(ed_w.on && (a[0] & 4u));   /* (chord-harp fork) the pushes' ids wide */
        ed_w.resets = usb.resets;
        if (ed_w.on && !keep)
            ed_shadow();
        else if (keep && ed_w.v4 && !v4was)
            for (i = 0; i < ED_NT; i++)                    /* TRACK_CHANGED newly asked for: from the mix as it is */
                ed_w.tv[i] = trk[i / 3u].p[ED_TIDS[i % 3u]];
        ed_b(ed_w.on | ed_w.v4 << 1 | ed_w.wide << 2);
        break;
    }
    case ED_PING:
        ed_b(0);
        break;
    case ED_TRACK:                                         /* [track] -> selected, NTRK, per track: engine, preset, level, mute, armed */
        if (na >= 1u && a[0] < NTRK && a[0] != song.sel) {
            track_select(a[0]);
            if (ed_w.on)
                ed_shadow();                               /* the editor re-reads it: no RELOAD for this */
        }
        ed_b(song.sel);
        ed_b(NTRK);
        for (i = 0; i < NTRK; i++) {
            ed_b(ed_eng(&trk[i]));
            ed_b(trk[i].preset);
            ed_v(trk[i].p[P_LEVEL]);
            ed_b(trk[i].p[P_MUTE] != 0);
            ed_b((song.rec >> i) & 1u);
        }
        break;
    case ED_TRACK_MIX: {                                   /* track [, level v14, mute] -> track, level, mute */
        track_t *t;
        int16_t *lv;
        if (na < 1u || a[0] >= NTRK)
            return;
        t = &trk[a[0]];
        lv = &t->p[P_LEVEL];
        if (na >= 4u) {
            *lv = (int16_t)clamp(ed_rv(a + 1), 0, 127);
            t->p[P_MUTE] = (int16_t)(a[3] ? 1 : 0);
            ed_known(a[0], P_LEVEL);                       /* the editor's own change: no push */
            ed_known(a[0], P_MUTE);
            ui.force = 1;
        }
        ed_b(a[0]);
        ed_v(*lv);
        ed_b(t->p[P_MUTE] != 0);
        break;
    }
    case ED_TRACK_DUMP:                                    /* track -> track, engine, preset, P_COUNT x v14 */
        if (na < 1u || a[0] >= NTRK)
            return;
        ed_b(a[0]);
        ed_b(ed_eng(&trk[a[0]]));
        ed_b(trk[a[0]].preset);
        for (i = 0; i < P_COUNT; i++)
            ed_v(motion_base_value(&trk[a[0]], i));
        break;
    case ED_TRACK_STEP: {                                  /* track, index [, step] -> track, index, step (as STEP_GET) */
        step_t *st;
        if (na < 2u || a[0] >= NTRK || a[1] >= NSTEP)
            return;
        st = &trk[a[0]].step[a[1]];
        if (na >= 10u && !chain_busy())
            ed_step_put(st, a + 2, na - 2u);
        if (a[0] == song.sel)
            ed_w.st[a[1]] = ed_step_sig(&seq_steps(&trk[a[0]])[a[1]]);
        ed_b(a[0]);
        ed_b(a[1]);
        ed_step_reply(&seq_steps(&trk[a[0]])[a[1]], na == 23u || na == 3u);
        break;
    }
    case ED_TRACK_PARAM: {                                 /* track, id [, v14] -> track, id, v14 (3, 5 bytes: a wide id) */
        track_t *t;
        uint32_t w = na == 3u || na == 5u, id = ed_id(a + 1, w);
        if (a[0] >= NTRK || id >= P_COUNT)
            return;
        t = &trk[a[0]];
        d = ed_tdesc(t, id);
        if (na >= 4u + w && !(chain_busy() && id >= P_SLEN && id <= P_SGATE)) {
            if (d->max > d->min)                           /* as SET: clamped; a fixed value stays */
                t->p[id] = (int16_t)enum_orig(d, clamp(ed_rv(a + 2 + w), d->min, d->max));
            (void)motion_capture(t, id, t->p[id]);
            ed_known(a[0], id);
            ui.force = 1;
        }
        ed_b(a[0]);
        ed_id_out(id, w);
        ed_v(t->p[id]);
        break;
    }
    case ED_SONG: {
        uint32_t rc = 0, op;
        chain_config_t c;
        if (!na || a[0] > 3u) return;
        op = a[0];
        if (op == 1u) {
            chain_defaults(&c);
            if (na < 2u || a[1] > CHAIN_ROWS || na != 2u + 2u * a[1]) rc = 1;
            else {
                c.count = a[1];
                for (i = 0; i < c.count; i++) {
                    c.row[i].slot = a[2u + 2u * i];
                    c.row[i].bars = a[3u + 2u * i];
                }
                if (!chain_valid(&c)) rc = 1;
                else if (chain_busy()) rc = 2;
                else { chain_config = c; ui.song_row = 0; ui.force = 1; }
            }
        } else if (op == 2u) rc = chain_prepare();
        else if (op == 3u) transport_req = 2;
        ed_b(op); ed_b(rc); ed_b(chain_config.count);
        ed_b(chain.running); ed_b(chain.row); ed_b(chain.remaining);
        for (i = 0; i < chain_config.count; i++) {
            ed_b(chain_config.row[i].slot); ed_b(chain_config.row[i].bars);
        }
        break;
    }
#if YEHNAH_PROBE
    case ED_PROBE: {          /* sub: 0 INFO -> bulk; 1 FLOOD ep burst secs; 2 STATS -> 5 x 28 bits; 3 CLICK (5 = data) */
        uint32_t sub = na ? a[0] : 0u, j, st[5];
        ed_b(sub);
        if (sub == 0u) {
#if YEHNAH_CDC
            ed_b(usb_cdc_on);                               /* EP3 presented: the bulk flood possible */
#else
            ed_b(0);
#endif
        } else if (sub == 1u && na >= 4u && (a[1] == 1u || a[1] == 3u) && a[2] >= 1u && a[2] <= 8u && a[3]) {
            pr.ep = 0;
            pr.seq = pr.pkts = pr.waits = pr.polls = 0;
            pr.burst = a[2];
            pr.left = a[3] * 2000u;
            RING_PUBLISH();
            pr.ep = a[1];                                   /* (this reply still goes first: ep1_tx) */
            ed_b(1);
        } else if (sub == 2u) {
            st[0] = pr.ep; st[1] = pr.pkts; st[2] = pr.waits; st[3] = pr.polls; st[4] = pr.left;
            for (j = 0; j < 5u; j++) {
                ed_b(st[j]); ed_b(st[j] >> 7); ed_b(st[j] >> 14); ed_b(st[j] >> 21);
            }
        } else if (sub == 3u) {
            pr_click = 441u;                                /* 10 ms */
            ed_b(1);
        } else {
            ed_b(0);
        }
        break;
    }
#endif
    default:
        return;
    }
    ed_send();
}

/* main loop: editor frames first; anything else stays for ota_service() */
static void ed_service(void)
{
    const uint8_t *p;
    uint32_t n;
    ed_sync();                                             /* v2 pushes (while watched) */
    if (!ota_frame_get(&p, &n) || n < 4u || p[0] != ED_HDR0 || p[1] != ED_HDR1 || p[2] != ED_HDR2)
        return;
    ed_w.last_ms = fm1_ms;                                 /* any request keeps WATCH alive */
    if (p[3] >= ED_UP_LIST) {                              /* large frames: handled in place, then freed */
        ed_handle(p, n);
        ota_frame_done();
        return;
    }
    {
        static uint8_t f[64];
        uint32_t k = n > sizeof f ? sizeof f : n, i;
        for (i = 0; i < k; i++)
            f[i] = p[i];
        ota_frame_done();                                  /* free the frame buffer before replying */
        ed_handle(f, k);
    }
}
