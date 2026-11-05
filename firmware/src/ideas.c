/* SPDX-License-Identifier: GPL-3.0-only
 * (chord-harp fork) HOME > IDEAS: ideas for the selected track, one page each (HOME's last group, where Yehnah's GEN
 * was: ui_lens.c, the gen_* hooks): TRACK its sequence, SOUND, LFO, FX, TEXTURE. A knob turned (once a turn burst) throws an
 * idea, heard at once; OCT+ KEEP keeps it, OCT- BACK puts back what was there before the page's first throw (OCT- with
 * nothing thrown: back to TRACKS). A throw on another page or track, or leaving, keeps the one before (UNDO, REC + D4,
 * still brings it back). Included by ui_input.c after song_dice.c (the genres, the roles' figures).
 *
 * TRACK: GENRE (RANDOM: one thrown each time; else the genre's progression, voicing and figures), RHYTHM (RANDOM: the
 * genre's figure or a cell; GENRE the role's figure as the old dice wrote it; or one of Yehnah GEN's rhythm cells, a
 * bar the notes fall on, held to the next or short; the last one on over the loop's end), ROLE (AUTO: the sound's type, a kit drums, keys and pads chords,
 * a voice or lead a melody, a bass a bass line; a sound with no type by its notes) and IDEA (a turn: a throw). Only the
 * track's length and key are kept (a CHR track takes the genre's scale); the sound, the level and the mix stay.
 * SOUND: PRESET, VARY, WILD (ui_input.c sound_idea), ENGINE (SAME; ANY: PRESET one of any engine's presets of the
 * track's role, ix_any_preset; VARY and WILD on the engine it has). LFO: ALL, RATE, WAVE, TARGET; FX: ALL, DRIVE, SPACE, MOVE, each
 * knob its own throw (lfo_dice, fx_dice).
 * TEXTURE: what the track plays made ambient, one touch (K4) or a knob at a time, each turn heard at once and worked
 * from BEFORE (so a knob turned back is as it was): SOUND (KEEP, PAD, CHOIR, HISS, SEA, CRACKLE; a kit keeps its sound),
 * BLUR (KEEP, SOFT, LONG, HAZE: softer, then each note held to the next, then softer still; never a note taken out) and
 * SPACE (KEEP, NEAR, WIDE, VAST: the level down, the pan aside, the reverb up); BACK puts back all three */

static struct {
    uint8_t page, hot;                                  /* IX_TRACK .. IX_FX; the last knob turned (0xFF none) */
    uint8_t genre, rhythm, role;                        /* TRACK's settings: 0 RANDOM / RANDOM / AUTO */
    uint8_t tsnd, tblr, tspc;                           /* TEXTURE's: PAD / KEEP (the notes as they are) / WIDE */
    uint8_t seng;                                       /* SOUND's ENGINE: 0 SAME, 1 ANY */
    uint8_t pend, trk;                                  /* a throw not kept yet: its page + 1, its track */
    uint16_t n;                                         /* throws (the panel's redraw) */
    char last[32];                                      /* the last idea */
} ix = {IX_TRACK, 0xFF, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, ""};

/* Yehnah GEN's rhythm cells (gen.c GL_CELL: figures that hold together), named */
#define IX_NCELL 12u
static const char *const IX_CELL[IX_NCELL] = {"x..x..x...x.....", "x.x.x...x.x.....", "x...x.x.x.......",
                                              "..x...x...x...x.", "x..x.x..x..x.x..", "x.......x.x.x...",
                                              "x...x...x...x...", "x.x...x.x...x...", "..x.x...x.x.....",
                                              "x..x..x..x..x...", "x.....x...x.x...", "x...x.x.......x."};
static const char *const IX_CELLN[IX_NCELL] = {"TRESILLO", "PAIRS", "LONG SHORT", "OFFBEAT", "SKIP", "HOLD RUN",
                                               "QUARTERS", "PUSH", "ANSWER", "THREES", "CALL", "PICKUP"};
#define IX_NROLE 5u
static const char *const IX_ROLEN[IX_NROLE] = {"AUTO", "BASS", "CHORDS", "MELODY", "ARP"};
static const uint8_t IX_ROLE[IX_NROLE] = {RL_N, RL_BASS, RL_PAD, RL_LEAD, RL_ARP};
static const char *const IX_RN[RL_N] = {"DRUMS", "BASS", "CHORDS", "MELODY", "ARP", "TEXTURE"};

/* AUTO: the role of t's sound. Its preset's type (ui.c PTYPE "BLPKIOUGHEMSRWVADXN": a bass, a lead, chords for pads,
 * keys, pianos, organs, guitars and strings, an arp for plucks, harps, bells and mallets; brass, wind and voices a
 * melody); no type (FX, INIT, a user preset): low notes a bass, chords chords, else a melody or an arp */
static uint32_t ix_type_role(uint32_t ty)              /* a preset type's role (RL_N: none) */
{
    static const char BY_TYPE[] = "1322224244423334099";
    return ty >= 1u && ty < sizeof BY_TYPE && BY_TYPE[ty - 1u] != '9' ? (uint32_t)(BY_TYPE[ty - 1u] - '0') : RL_N;
}
static uint32_t ix_role(const track_t *t)
{
    uint32_t i, sum = 0, cnt = 0, chord = 0, ty;
    if (drum_track(t)) return RL_DRUM;
    ty = ix_type_role(t->user ? 0u : preset_type(trk_eng(t), t->preset));
    if (ty != RL_N) return ty;
    for (i = 0; i < NSTEP; i++)
        if (t->step[i].time == ST_NOTE && t->step[i].n) { sum += t->step[i].note[0]; cnt++; chord += t->step[i].n > 1u; }
    if (cnt && sum / cnt < 48u) return RL_BASS;
    if (cnt && chord * 3u > cnt) return RL_PAD;
    return chance(50) ? RL_ARP : RL_LEAD;
}
/* a cell's idea: the cell's hits each bar (now and then one left out or one added past the first bar), a motif of
 * degrees over them repeated on the progression's chords (a note in five moved a step), each note held to the next
 * hit (the last one round the loop's end, as the sequencer plays TIEs: no gap at the loop's top), or short, or half
 * way; a bass low on roots and fifths, chords the genre's voicing, an arp the chord's tones */
static void ix_cell(track_t *t, uint32_t r, const char *cell, uint32_t len)
{
    uint32_t i, h = 0, oct = r == RL_BASS ? 2u : r == RL_PAD ? 3u : 4u;
    int32_t base = 12 * (int32_t)(oct + 1u) + t->p[P_ROOT] % 12, deg[16], d = 0, nt[4];
    uint8_t at[16];
    for (i = 0; i < 16u; i++) {                         /* (the motif: one degree a hit of the cell) */
        at[i] = (uint8_t)h;
        if (cell[i] != 'x') continue;
        if (r == RL_BASS) d = chance(60) ? 0 : chance(60) ? 4 : chance(50) ? 7 : 2;
        else if (r == RL_ARP) d = (int32_t)(h * 2u % 6u) + (chance(30) ? 7 : 0);
        else if (r == RL_LEAD) d = clamp(d + (chance(15) ? rr(-4, 4) : rr(-2, 2)), -3, 9);
        deg[h++] = d;
    }
    for (i = 0; i < len; i++) {
        uint32_t pos = i % 16u, hit = cell[pos] == 'x';
        if (i >= 16u && (hit ? chance(12) : !(pos & 1u) && chance(6)))
            hit = !hit;
        if (!hit) continue;
        d = chord_at(i) + (h ? deg[at[pos] % h] : 0);
        if (r == RL_LEAD && i >= 16u && chance(20)) d += rr(-1, 1);
        if (r == RL_PAD)
            dstep_note(&t->step[i], nt, dchord(t, base, chord_at(i), dh.voicing, nt), pos % 4u ? 88 : 100, 0);
        else {
            nt[0] = r == RL_LEAD ? dlead_note(t, base, d) : scale_steps(t, base, d);
            dstep_note(&t->step[i], nt, 1, pos % 4u ? 90 : 105, 0);
        }
    }
    for (i = 0; i < len; i++) {                         /* (the lengths: to the next note, short or half way) */
        uint32_t gap;
        if (t->step[i].time != ST_NOTE) continue;
        for (gap = 1u; gap < len && t->step[(i + gap) % len].time != ST_NOTE; gap++) {}
        dhold(t, i, r == RL_PAD || chance(50) ? gap - 1u : chance(60) ? 0u : gap / 2u, len);
    }
}
/* TRACK's throw: its message */
static const char *ix_track(track_t *t)
{
    if (!RES_OK())                                      /* (chord-harp fork) the genres in the resource region (res.c) */
        return "SOUNDS MISSING";
    const dstyle_t *st = &DSTYLE[ix.genre ? (ix.genre - 1u) % NDSTYLE : dstyle_pick()];
    uint32_t len = dlen(t), i, r = IX_ROLE[ix.role % IX_NROLE], rh = ix.rhythm, pr = rng() % 3u;
    uint64_t was = tl_used(t, len);
    if (drum_track(t) || r == RL_N) r = ix_role(t);
    if (!rh) rh = r == RL_DRUM || chance(40) ? 1u : 2u + rng() % IX_NCELL;
    if (r == RL_DRUM) rh = 1u;
    if (st->prog[pr][0] == 127) pr = 0;                 /* (the genre's harmony, as the dice) */
    dh.prog = st->prog[pr];
    for (dh.n = 0; dh.n < 4u && dh.prog[dh.n] != 127; dh.n++) {}
    dh.steps = 16u * st->bars;
    if (dh.steps * dh.n > NSTEP) dh.steps = NSTEP / dh.n;
    dh.voicing = st->voicing; dh.arp = st->arp; dh.plain = st->plain;
    dh.energy = (uint32_t)rr(st->e_lo, st->e_hi);
    dh.nlast = 0; dh.twist = TW_NONE;
    if (!t->p[P_SCALE] && r != RL_DRUM) t->p[P_SCALE] = st->scale[rng() % 4u];   /* (CHR: the genre's) */
    load_begin(t, LOAD_PAT);
    latch_drop_req |= (uint8_t)(1u << trk_index(t));
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
    if (rh == 1u)
        switch (r) {
        case RL_DRUM: dice_drums(t, st, len); break;
        case RL_BASS: dice_bass(t, st, len); break;
        case RL_PAD: dice_pads(t, st, len); break;
        case RL_LEAD: dice_lead(t, st, len); break;
        default: dice_arp(t, st, len); break;
        }
    else
        ix_cell(t, r, IX_CELL[(rh - 2u) % IX_NCELL], len);
    tl_unlock(t, was, len);
    load_end(t);
    str_cpy(ix.last, IX_RN[r], sizeof ix.last);
    str_cpy(ix.last + str_len(ix.last), " ", 2);
    str_cpy(ix.last + str_len(ix.last), st->name, sizeof ix.last - str_len(ix.last));
    if (r != RL_DRUM) {
        str_cpy(ix.last + str_len(ix.last), " ", 2);
        str_cpy(ix.last + str_len(ix.last), rh == 1u ? "FIGURE" : IX_CELLN[(rh - 2u) % IX_NCELL],
                sizeof ix.last - str_len(ix.last));
    }
    return ix.last;
}
/* TEXTURE's sounds: an engine and a word of its presets' names (NOISE not built: a pad) */
#define IX_NTSND 5u
static const char *const IX_TSNDN[IX_NTSND + 1u] = {"KEEP", "PAD", "CHOIR", "HISS", "SEA", "CRACKLE"};
static const struct { uint8_t e; const char *w; } IX_TSND[IX_NTSND] = {
    {EG_A, "PAD"}, {EG_VO, "CHOIR"}, {EG_NO, "HISS"}, {EG_NO, "SEA"}, {EG_NO, "CRACKLE"}};
static const char *const IX_TBLRN[4] = {"KEEP", "SOFT", "LONG", "HAZE"};
static const char *const IX_TSPCN[4] = {"KEEP", "NEAR", "WIDE", "VAST"};
/* TEXTURE's turn (how: its knob; 3 all): the sound (K1, K4), then the steps and the mix from BEFORE (dk) */
static const char *ix_texture(track_t *t, uint32_t how)
{
    uint32_t s = ix.tsnd % (IX_NTSND + 1u), b = ix.tblr % 4u, k = ix.tspc % 4u, len = dlen(t), e, i, n, m = 0;
    uint64_t was = tl_used(t, len);
    uint8_t at[32];
    if ((how == 0u || how == 3u) && !drum_track(t)) {
        if (!s)
            (void)undo_sound(song.sel);                 /* (KEEP: the sound BEFORE) */
        else {
            e = IX_TSND[s - 1u].e;
            if (!eng_ok(e)) { s = 1u; e = EG_A; }
            set_engine_of(t, e);
            for (n = ENG_NP(ENGINES[e]), i = 0; i < n; i++)
                if (dname_has(ENGINES[e]->presets[i].name, IX_TSND[s - 1u].w) && m < sizeof at) at[m++] = (uint8_t)i;
            if (m) apply_preset_to(t, at[rng() % m]);
        }
    }
    load_begin(t, LOAD_PAT);                            /* (the steps BEFORE, blurred: the notes all stay) */
    latch_drop_req |= (uint8_t)(1u << trk_index(t));
    for (i = 0; i < NSTEP; i++)
        t->step[i] = dk.step[i];
    for (i = 0; b && i < len; i++)
        if (t->step[i].time == ST_NOTE)
            t->step[i].vel = (uint8_t)clamp((int32_t)t->step[i].vel * (b == 3u ? 70 : 85) / 100, 30, 127);
    for (i = 0; b >= 2u && i < len; i++)
        if (t->step[i].time == ST_NOTE) dhold(t, i, len - 1u, len);
    tl_unlock(t, was, len);
    load_end(t);
    if (k) {                                            /* (the old GEN's texture: quiet, aside, in a big room) */
        int32_t side = chance(50) ? 1 : -1, nz = trk_eng(t) == EG_NO;
        t->p[P_LEVEL] = (int16_t)(k == 1u ? 72 : k == 2u ? (nz ? 44 : 54) : (nz ? 38 : 46));
        t->p[P_PAN] = (int16_t)(side * (k == 1u ? rr(0, 10) : k == 2u ? rr(10, 30) : rr(25, 45)));
        t->p[P_REV] = (int16_t)(k == 1u ? rr(20, 40) : k == 2u ? rr(45, 85) : rr(90, 120));
    } else
        for (i = 0; i < 3u; i++) t->p[DK_TMIX[i]] = dk.v[i];
    str_cpy(ix.last, "TEXTURE ", sizeof ix.last);
    str_cpy(ix.last + 8, drum_track(t) ? "KIT" : IX_TSNDN[s], sizeof ix.last - 8u);
    str_cpy(ix.last + str_len(ix.last), " ", 2);
    str_cpy(ix.last + str_len(ix.last), IX_TBLRN[b], sizeof ix.last - str_len(ix.last));
    str_cpy(ix.last + str_len(ix.last), " ", 2);
    str_cpy(ix.last + str_len(ix.last), IX_TSPCN[k], sizeof ix.last - str_len(ix.last));
    return ix.last;
}
/* SOUND's PRESET, ENGINE ANY: one of every engine's presets (not DRUM's) of the track's role, not the one it has
 * (none: the engine's own, sound_idea) */
static const char *ix_any_preset(track_t *t)
{
    uint32_t r = ix_role(t), e, i, n = 0, k, pass;
    for (pass = 0; pass < 2u; pass++) {                 /* (count them, then take the k-th) */
        k = pass ? rng() % n : 0u;
        for (e = 0; e < NENGINES; e++) {
            if (!eng_ok(e) || e == ENGI_DRUM) continue;
            for (i = 0; i < ENG_NP(ENGINES[e]); i++) {
                if (ix_type_role(preset_type(e, i)) != r || (e == trk_eng(t) && i == t->preset && !t->user)) continue;
                if (pass && !k--) {
                    if (e != trk_eng(t)) set_engine_of(t, e);
                    apply_preset_to(t, i);
                    si_base_take(t);
                    str_cpy(ix.last, ENGINES[e]->presets[i].name, sizeof ix.last);
                    str_cpy(ix.last + str_len(ix.last), " ", 2);
                    str_cpy(ix.last + str_len(ix.last), ENGINES[e]->name, sizeof ix.last - str_len(ix.last));
                    return ix.last;
                }
                n += !pass;
            }
        }
        if (!n) break;
    }
    return sound_idea(t, SI_PRESET);
}
static void ix_settle(void) { ix.pend = 0; }            /* (what is there now stays) */
/* a throw on page p (how: SOUND's knob), once a turn burst */
static void ix_throw(uint32_t p, uint32_t how)
{
    static uint32_t last_ms;
    track_t *t = TSEL;
    const char *m;
    uint32_t again = last_ms && fm1_ms - last_ms <= 400u;
    last_ms = fm1_ms | 1u;
    if (again && (p != IX_TEX || how == 3u))           /* (TEXTURE's settings: every detent, from BEFORE) */
        return;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    if (ix.pend && (ix.pend != p + 1u || ix.trk != song.sel))
        ix_settle();
    if (!ix.pend) {
        rng_state ^= fm1_ticks() * 2654435761u;
        if (!rng_state) rng_state = 0x1234567u;
        undo_mark();
        dk_keep(p);
        ix.pend = (uint8_t)(p + 1u);
        ix.trk = (uint8_t)song.sel;
    }
    if (p == IX_TRACK) m = ix_track(t);
    else if (p == IX_TEX) m = ix_texture(t, how);
    else if (p == IX_SOUND && how == SI_PRESET && ix.seng && !drum_track(t)) m = ix_any_preset(t);
    else if (p == IX_SOUND) m = sound_idea(t, how);
    else if (p == IX_LFO) { static const char *const M[4] = {"NEW LFO", "NEW LFO RATE", "NEW LFO WAVE", "NEW LFO TARGET"};
        lfo_dice(t, how % 4u); m = M[how % 4u]; }
    else { static const char *const M[4] = {"NEW FX", "NEW DRIVE", "NEW SPACE", "NEW MOVE"};
        fx_dice(t, how % 4u); m = M[how % 4u]; }
    if (p != IX_TRACK && p != IX_TEX && m != ix.last) str_cpy(ix.last, m, sizeof ix.last);
    ix.n++;
    ui_message(m);
    ui.force = 1;
}

/* ---- the hooks (ui_lens.c, ui_input.c, ui_draw.c, ui_graph.c: Yehnah GEN's) */
static void gen_open(void)
{
    ix.hot = 0xFF;
    ix.pend = 0;
    ix.last[0] = 0;
}
static uint32_t gen_page(void) { return ix.page % IX_N; }
static void gen_page_put(uint32_t p)                     /* IDEAS' pages (ui_lens.c nav_go) */
{
    if (ix.page != p % IX_N) {
        ix.hot = 0xFF;
        ix_settle();
        ix.last[0] = 0;
    }
    ix.page = (uint8_t)(p % IX_N);
    ui.force = 1;
}
static void gen_knob(uint32_t k, int32_t s)
{
    uint32_t p = ix.page % IX_N;
    ix.hot = (uint8_t)k;
    if (p == IX_TRACK && k == 0u)                       /* GENRE: RANDOM, the genres */
        ix.genre = (uint8_t)((ix.genre + (s > 0 ? 1u : NDSTYLE)) % (NDSTYLE + 1u));
    else if (p == IX_TRACK && k == 1u)                  /* RHYTHM: RANDOM, GENRE, the cells */
        ix.rhythm = (uint8_t)((ix.rhythm + (s > 0 ? 1u : IX_NCELL + 1u)) % (IX_NCELL + 2u));
    else if (p == IX_TRACK && k == 2u)
        ix.role = (uint8_t)wrap_add((int32_t)ix.role, s > 0 ? 1 : -1, (int32_t)IX_NROLE);
    else if (p == IX_TEX && k < 3u) {                   /* SOUND, BLUR, SPACE: the setting, heard at once */
        static const uint8_t TOP[3] = {IX_NTSND, 3, 3};
        uint8_t *v = k == 0u ? &ix.tsnd : k == 1u ? &ix.tblr : &ix.tspc;
        *v = (uint8_t)wrap_add((int32_t)*v, s > 0 ? 1 : -1, TOP[k] + 1);
        ix_throw(p, k);
    } else if (p == IX_SOUND && k == 3u)                /* ENGINE: SAME, ANY (PRESET's) */
        ix.seng = !ix.seng;
    else
        ix_throw(p, k);
    ui.force = 1;
}
static void gen_press(void)                              /* OCT+ KEEP */
{
    ui_message(ix.pend ? "KEPT" : "TURN A KNOB: AN IDEA");
    ix_settle();
    ui.force = 1;
}
static int gen_back(void)                                /* OCT- BACK: 0 nothing to put back */
{
    uint32_t p = ix.pend;
    if (!p)
        return 0;
    ix.pend = 0;
    ui_message(dk_back(p - 1u, ix.trk) ? "CAN'T UNDO" : "BACK");
    ix.last[0] = 0;
    ui.force = 1;
    return 1;
}
static void gen_cards(void)
{
    static const char *const LBL[IX_N][4] = {{"GENRE", "RHYTHM", "ROLE", "IDEA"}, {"PRESET", "VARY", "WILD", "ENGINE"},
                                              {"ALL", "RATE", "WAVE", "TARGET"}, {"ALL", "DRIVE", "SPACE", "MOVE"},
                                              {"SOUND", "BLUR", "SPACE", "TEXTURE"}};
    uint32_t p = ix.page % IX_N, k, drum = drum_track(TSEL);
    for (k = 0; k < 4u; k++) {
        const char *v = "TURN";
        int32_t r = -1;
        uint16_t c = VAL(k);
        if (p == IX_TRACK && k == 0u) {
            v = !RES_OK() ? "-" : ix.genre ? DSTYLE[(ix.genre - 1u) % NDSTYLE].name : "RANDOM";
            r = pick_of(NDSTYLE + 1u, ix.genre);
        } else if (p == IX_TRACK && k == 1u) {
            v = ix.rhythm == 0u ? "RANDOM" : ix.rhythm == 1u ? "GENRE" : IX_CELLN[(ix.rhythm - 2u) % IX_NCELL];
            r = pick_of(IX_NCELL + 2u, ix.rhythm);
            if (drum) c = T_DIM;
        } else if (p == IX_TRACK && k == 2u) {
            v = drum ? "DRUMS" : IX_ROLEN[ix.role % IX_NROLE];
            r = drum ? -1 : pick_of(IX_NROLE, ix.role % IX_NROLE);
            if (drum) c = T_DIM;
        } else if (p == IX_TEX && k < 3u) {
            uint32_t v3 = k == 0u ? ix.tsnd : k == 1u ? ix.tblr : ix.tspc, n3 = k == 0u ? IX_NTSND : 3u;
            v = k == 0u ? (drum ? "KIT" : IX_TSNDN[v3 % (IX_NTSND + 1u)]) : k == 1u ? IX_TBLRN[v3 % 4u] : IX_TSPCN[v3 % 4u];
            r = pick_of(n3 + 1u, v3);
            if (k == 0u && drum) c = T_DIM;
        } else if (p == IX_SOUND && k == 3u) {
            v = drum ? "KIT" : ix.seng ? "ANY" : "SAME";
            r = drum ? -1 : pick_of(2u, ix.seng != 0);
            if (drum) c = T_DIM;
        }
        draw_column(k, LBL[p][k], v, "", c, r, ICON_NONE);
    }
}
static uint32_t gen_draw_sig(void)
{
    return ((((uint32_t)ix.page * 31u + ix.hot) * 31u + ix.genre) * 31u + ix.rhythm) * 31u + ix.role +
           ((uint32_t)ix.tsnd * 7u + ix.tblr * 3u + ix.tspc + ix.seng * 211u) * 7919u +
           ((uint32_t)ix.n << 12) + ((uint32_t)ix.pend << 28) + song.sel * 977u + str_hash(0, ix.last) + ux.gen;
}
static void gen_panel(void)
{
    uint32_t p = ix.page % IX_N;
    if (ix.pend && ix.trk != song.sel)                  /* (another track chosen: the idea kept, BEFORE that track's) */
        ix_settle();
    if (!ix.pend)                                       /* (nothing thrown: BEFORE is now) */
        dk_keep(p);
    dice_side(p, 0, 4);
    cv_rect(119, 8, 1, 108, T_LINE);
    dice_side(p, 1, 124);
    cv_clip_text(6, 126, &AF_X, ix.last[0] ? ix.last : "TURN A KNOB", ix.last[0] ? T_TEXT : T_DIM, T_SURF, 130);
    cv_text_r(234, 126, &AF_X, ix.pend ? "OCT+ KEEP" : "OCT- DONE", ix.pend ? T_ACCENT : T_DIM, T_SURF);
}
