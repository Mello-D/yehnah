/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Keyboard, scale, arpeggiator, sequencer and transport.
 * Everything here runs in the audio ISR, once per CTL-sample block, and
 * ends in trk_note_on / trk_note_off: engines never see where a note came from.
 * Four tracks, one transport: every track's pattern loops on its own LEN / DIV /
 * SWING / GATE (polymeter). The keys play the selected track; MIDI IN by GLO > SYSTEM ROUT (G_ROUTE):
 * CH1-4 (0) channels 1..4 play parts 1..4, channels 5..16 are ignored (midi_control.c midi_event); SEL (1)
 * every channel the selected track. Their CC1 (mod wheel), CC11 (expression) and channel aftertouch go to
 * the same track's modulation matrix (mod.c; the selected track: the one selected then).
 * A note into an armed track (song.rec) while
 * the transport runs is recorded into its pattern, quantised to its (swung) steps, with its
 * held length as TIE steps (rec_note, rec_hold, rec_release). With the ARP on, the notes the arp
 * plays are recorded, not the keys held (what plays back is what was heard). A key or MIDI note plays a
 * chord when the track's CHRD is on (chord.c): its notes go through the same input as so many keys. */
static const uint16_t SCALE_MASK[] = {
    0xFFF,                                   /* CHR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MAJ */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* MIN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* DOR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* MIX */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                          /* PEN */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                         /* MPEN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),   /* HARM */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* PHRY */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11),   /* LYD */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10),   /* LOC */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MEL (ascending) */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10),              /* BLUES (minor) */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10),              /* WHOLE */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 10), /* DIMHW */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 11), /* DIMWH */
};

#define KB_SILENT 255u
static uint32_t kb_prev;
static volatile uint8_t kb_boot_hold;    /* #137: set from power-on to the end of the splash (main.c): the keys sound nothing,
                                          * and one held across its end stays silent until it is let go and pressed again */
static uint8_t kb_note[27], kb_trk[27];  /* per key: the note it started and on which track */
static int8_t kb_oct[27];                /* (0.8.5) .. on a DRUM track, the kit's octave it plays (OCT then: seq.c dxo_on) */
static volatile uint8_t kb_asleep;       /* MENU > SCREEN OFF (ui.c): the screen dark or waking, a key pressed now only
                                          * wakes it (silent, no layer, its release nothing) */
static uint8_t last_note = 60;
static volatile uint8_t transport_req;   /* 1 start, 2 stop, 3 restart (from the UI) */
/* (chord-harp fork) REC > ONE PASS (ui_input.c rec_arm): one track records its pattern once from step 1, then recording
 * and the transport stop. rp_trk: the track + 1; rp_state: RP_WAIT playing, armed at the next step 1; RP_PRE in the
 * last step before it (armed: what is played there goes into step 1, rec_note); RP_REC recording; rp_done: the pass
 * ended (the UI says so). seq.c rec_pass_step, rec_pass_end. RP_CIN: armed while playing with a COUNT-IN, ONE PASS
 * (rp_once) or LOOP: the track's pattern plays to its end, the transport stops, counts in, and records from step 1
 * (rec_cin_end) */
enum { RP_OFF, RP_WAIT, RP_PRE, RP_REC, RP_CIN };
static volatile uint8_t rp_trk, rp_state, rp_done, rp_once;
static volatile uint8_t panic_req;       /* bit per track: release every sounding note (preset / engine change) */
static volatile uint8_t midi_hint;       /* MIDI IN played track n - 1, which is not the selected one (the UI says so) */

static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }

static uint32_t trk_midi_ch(uint32_t i) { return i % NTRK; }   /* MIDI channel 0..15 of track i (keys -> MIDI out) */

static int drum_track(const track_t *t) { return ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM; }

/* (0.6.72) MENU > CONTROL > MIDI OUT (ui.c midi_out_mode, over USB): KEYS (as ever: the keys' notes), CLOCK (and
 * the clock, 24 a beat, with START / STOP while the tempo is the FM-1's own), ALL (and every note the FM-1 plays: the
 * sequencer, the arp, FUGUE, the punch-in repeats, OCT and RANDOM on them, each track on its channel; the keys then
 * only where no arp plays them), OFF. The notes sent are remembered (mo_out): each gets its note-off, all of them at
 * STOP, a panic or a change of setting */
enum { MO_KEYS, MO_CLOCK, MO_ALL, MO_OFF };
static volatile uint8_t mo_lvl;                    /* the setting (ui.c sets it every frame) */
static uint8_t mo_last;                            /* .. as the ISR last saw it */
static uint8_t mo_out[NTRK][128];                  /* a played note's note sent on, + 1 (0: none) */
static uint32_t mo_pulse_acc;                      /* (samples towards the next clock) */
static void mo_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_out_event((st >> 4) | st << 8 | d1 << 16 | d2 << 24);
}
static int mo_plays(const track_t *t, uint32_t note)   /* trk_note_on would sound it (voice.c: the same tests) */
{
    return trk_index(t) < trk_live && !t->p[P_MUTE] && !((trk_hush >> trk_index(t)) & 1u) &&
           !(t->engine == ENGI_DRUM && drum_muted(note));
}
/* a note the FM-1 plays (not a key's): sounded, and with ALL sent as it sounds: the punch-ins' OCT- / OCT+ and RANDOM
 * on it (pfx.c pfx_pit, pfx_rpit: drawn by trk_note_on) */
static void seq_note_on(track_t *t, uint32_t note, uint32_t vel)
{
    if ((chain.cut >> (trk_index(t) % NTRK)) & 1u)
        return;                                         /* (2026-11-06) SONG EDIT: the track sits this bar out */
    uint32_t i = trk_index(t) % NTRK, send = mo_lvl == MO_ALL && note < 128u && mo_plays(t, note) && !pfx_holds(t);
    trk_note_on(t, note, vel);
    if (send) {
        uint32_t x = (uint32_t)clamp((int32_t)note + (t->pfx_pit + t->pfx_rpit) / 16, 0, 127);
        if (mo_out[i][note])
            mo_send(0x80u | trk_midi_ch(i), mo_out[i][note] - 1u, 0);   /* (struck again: its last one ends first) */
        mo_out[i][note] = (uint8_t)(x + 1u);
        mo_send(0x90u | trk_midi_ch(i), x, vel ? vel : 1u);
    }
}
static void seq_note_off(track_t *t, uint32_t note)
{
    uint32_t i = trk_index(t) % NTRK;
    if (note < 128u && mo_out[i][note]) {
        mo_send(0x80u | trk_midi_ch(i), mo_out[i][note] - 1u, 0);
        mo_out[i][note] = 0;
    }
    trk_note_off(t, note);
}
static __attribute__((noinline)) void mo_flush_of(uint32_t mask)   /* the notes sent on, of these tracks: off */
{
    uint32_t i, n;
    for (i = 0; i < NTRK; i++)
        if ((mask >> i) & 1u)
            for (n = 0; n < 128u; n++)
                if (mo_out[i][n]) {
                    mo_send(0x80u | trk_midi_ch(i), mo_out[i][n] - 1u, 0);
                    mo_out[i][n] = 0;
                }
}
#define mo_flush() mo_flush_of(0xFFu)
static void mo_key(const track_t *t, uint32_t pkt)   /* a key's note: not with OFF; with ALL, an arp sends its own */
{
    if (mo_lvl != MO_OFF && !(mo_lvl == MO_ALL && t->p[P_AMODE]))
        midi_out_event(pkt);
}
static int mo_clock_on(void) { return (mo_lvl == MO_CLOCK || mo_lvl == MO_ALL) && !song.g[G_CLOCK]; }
static __attribute__((noinline)) void mo_block(uint32_t n)   /* events_block: the setting changed; the clock */
{
    if (mo_lvl != mo_last) {
        if (mo_last == MO_ALL)
            mo_flush();
        if ((mo_last == MO_CLOCK || mo_last == MO_ALL) && !(mo_lvl == MO_CLOCK || mo_lvl == MO_ALL) && song.playing &&
            !song.g[G_CLOCK])
            mo_send(0xFCu, 0, 0);                    /* (the clock leaves: whatever follows it stops) */
        mo_last = mo_lvl;
    }
    if (mo_clock_on()) {                             /* 24 a beat, also while stopped (a follower holds the tempo) */
        uint32_t len = beat_samples() / 24u;
        if (len < 64u)
            len = 64u;
        mo_pulse_acc += n;
        while (mo_pulse_acc >= len) {
            mo_pulse_acc -= len;
            midi_out_event(0x0Fu | 0xF8u << 8);
        }
    }
}

/* the 27 keys from F: black or white, and the key's place among the keys of its colour (white 0..15, black
 * 0..10). The DRUM grid: white keys are steps, black keys 1..8 lanes, 9 ACC, 10 / 11 the page */
static int key_black(uint32_t k) { return (int)((0x54Au >> ((k + 5u) % 12u)) & 1u); }
static uint32_t key_place(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < k; i++)
        n += (uint32_t)(key_black(i) == key_black(k));
    return n;
}
enum { GK_ACC = NLANE, GK_PGDN, GK_PGUP };       /* black keys 9..11 on the grid */

static uint32_t scale_mask(const track_t *t)
{
    return SCALE_MASK[clamp(t->p[P_SCALE], 0, sizeof SCALE_MASK / sizeof SCALE_MASK[0] - 1)];
}

/* note n moved down onto the track's scale (ROOT, SCALE): QNT SNAP's keys and QNT SEQ's sequenced notes */
static int32_t scale_snap(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t), guard = 12;
    while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
        n--;
    return n;
}

enum { QN_OFF, QN_SNAP, QN_WHITE, QN_SEQ };      /* P_QUANT */

/* (0.4) G_STRN: note n onto the track's scale (down), then d steps of the scale up (d > 0) or down */
static int32_t scale_steps(const track_t *t, int32_t n, int32_t d)
{
    uint32_t mask = scale_mask(t);
    int32_t r = t->p[P_ROOT];
    n = scale_snap(t, n);
    if (!(mask & 0xFFFu))
        return n;
    for (; d > 0; d--)
        do n++; while (!((mask >> (uint32_t)((n - r + 120) % 12)) & 1u));
    for (; d < 0; d++)
        do n--; while (!((mask >> (uint32_t)((n - r + 120) % 12)) & 1u));
    return n;
}

/* (chord-harp) CHORD HARP (P_CHRD 12; harp.c): the lowest 12 keys (F3..E4) chords 1..12 of the set VOICING picks
 * (SET), the 9 white keys above them (F4..G5) the tones of the last chord struck, one a key (the plate: strummed by a
 * sweep), the top F# LATCH (ARP 2's HOLD: with ARP OFF the chords keep sounding till the next one; the plate's notes
 * never latch), the other black keys above E4 silent. The chords move with ROOT and the octave; the root alone on
 * MONO / LEGATO / UNISON. Not on a kit nor an engine's own key map (the keys stay theirs) */
#define CD_HARP 12
#define HARP_KLATCH 25u
#define HARP_KWRITE 13u                 /* (2026-10-27) F#4: silent, on SCL > PROG its WRITE (ui_input.c) */
/* (chord-harp) CHORD OMNI (P_CHRD 13; harp.c): (chord-harp fork) HARP's keys, the chords the buttons SET picks, the
 * plate the last chord's strings (FoMni's), its bends and LATCH as HARP's */
#define CD_OMNI 13
/* (chord-harp fork) CHORD PROG (P_CHRD 14; harp.c PROGS): as HARP's, the chord keys the chords of the progression PROG
 * picks, in order (past its last: round again, every other time an octave up), in ROOT; recorded as PROG's chord ids
 * (the quality, the step's first note its root) */
#define CD_PROG 14
/* (2026-11-10) CHORD GUITAR (P_CHRD 15; harp.c gt_*): as HARP's on one of guitchordi's 50 chord styles (P_GSET), each key a
 * chord as a guitar plays it in standard tuning (up to six strings), moved with ROOT; recorded as its notes */
#define CD_GTR 15
enum { CD_OFF, CD_TRI, CD_SEV, CD_ADD9, CD_SUS2, CD_SUS4, CD_MAJ, CD_MIN, CD_DOM7, CD_MAJ7, CD_MIN7, CD_POW };
/* (2026-10-27) CHORD's own modes (TRIAD .. POWER) on HARP's keys: the lowest 7 keys (F..B) the scale's degrees I..VII
 * (I from G2..F#3, as HARP's chord 1), the 8th (C) I an octave up, each the chord the mode builds on it (chord_build,
 * VOICING); the 11 white keys from D the plate (the chord's tones up from the first above its top note); no chord id (recorded as its notes, as
 * before); LATCH and the bends as HARP's. No QUANT on the keys: the chords and the plate are in the scale */
#define HARP_NCM 8u                     /* CHORD's chord keys */
#define HARP_NCMP 11u                   /* .. its plate's: the white keys from key 9 (D) */
static int harp_cm(const track_t *t) { return t->p[P_CHRD] >= CD_TRI && t->p[P_CHRD] <= CD_POW; }
static uint32_t chord_build(const track_t *t, uint32_t root, uint8_t *out);
static uint32_t cm_chord(const track_t *t, uint32_t k, uint8_t *out);
static void cm_name(const track_t *t, uint32_t k, char *buf);
static void cm_plate(const track_t *t, uint32_t k, uint8_t *out);
static uint8_t harp_cur[NTRK];                  /* the chord key each track's plate follows */
/* (chord-harp) recording, a HARP chord key held (POLY): its chord + 1 (0 none), root, the step struck (org), the step the
 * hold reached (last), the steps since (span), the plate notes the hold's step sounds now (pn). seq.c harp_over, harp_hold */
static uint8_t hh_id[NTRK], hh_root[NTRK], hh_org[NTRK], hh_last[NTRK], hh_span[NTRK], hh_pn[NTRK][3], hh_pnn[NTRK];
static uint16_t hp_now[NTRK], hp_was[NTRK];     /* (chord-harp) playback: the CHORD (+ 1, its root << 8) of the step
                                                 * playing, and of the one before (harp_tie) */
static __attribute__((noinline)) int harp_on(const track_t *t)   /* (the harp out of RAM code: called, not inlined) */
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return (t->p[P_CHRD] == CD_HARP || t->p[P_CHRD] == CD_OMNI || t->p[P_CHRD] == CD_PROG || t->p[P_CHRD] == CD_GTR || harp_cm(t)) && !e->oneshot &&
           !(e->keys && e->keys(t, 0) >= 0);
}
static int harp_omni(const track_t *t) { return t->p[P_CHRD] == CD_OMNI; }
static int harp_prog(const track_t *t) { return t->p[P_CHRD] == CD_PROG; }
static uint32_t harp_pg(const track_t *t) { return (uint32_t)clamp(t->p[P_PROG], 0, (int32_t)PROG_N - 1); }   /* PROG */
static int harp_gtr(const track_t *t) { return t->p[P_CHRD] == CD_GTR; }
static uint32_t harp_set(const track_t *t)
{
    if (harp_gtr(t))
        return HARP_GT0 + (uint32_t)clamp(t->p[P_GSET], 0, (int32_t)GT_NSET - 1) +
               GT_NSET * ((uint32_t)clamp(t->p[P_GSHP], 0, (int32_t)GT_NSHP - 1) + GT_NSHP * (uint32_t)clamp(t->p[P_GTUN], 0, (int32_t)GT_NTUN - 1));   /* (harp.c's
                                                         * set numbers: the style, + GT_NSET * (the shape + 6 * the tuning)) */
    return (uint32_t)clamp(t->p[P_CVOI], 0, harp_omni(t) ? (int32_t)OMNI_NSET - 1 : RES_OK() ? (int32_t)HARP_NALL - 1 : (int32_t)HARP_NSET - 1);   /* (chord-harp
                                                         * fork: the J-6 sets in the resource region, res.c) */
}
static int harp_ck(const track_t *t, uint32_t k) { return k < (harp_cm(t) ? HARP_NCM : HARP_NCH); }   /* key k is a chord key:
                                                 * the lowest 12 (CHORD: 8) */
static uint32_t harp_p0(const track_t *t) { return harp_cm(t) ? 5u : 7u; }   /* the plate's first white key's place */
static uint32_t harp_np(const track_t *t) { return harp_cm(t) ? HARP_NCMP : HARP_NPLATE; }   /* .. its keys */
/* (2026-10-27) HARP on a J-6 set (harp.c j6_*): its chords have no id, they are played from the set and recorded as
 * their notes; harp_idk: chord key k records a chord id (a CHORD step) */
static int harp_j6(const track_t *t) { return !harp_omni(t) && !harp_prog(t) && harp_set(t) >= HARP_NSET; }
static int harp_idk(const track_t *t, uint32_t k) { return harp_ck(t, k) && !harp_j6(t) && !harp_cm(t); }
static uint32_t harp_lk(const track_t *t) { (void)t; return HARP_KLATCH; }   /* the LATCH key */
/* chord key k's chord id (harp.c) and the ROOT its notes take: HARP's chords move with it, an OMNI id holds it */
static uint32_t harp_id(const track_t *t, uint32_t k)
{
    int32_t rn;
    if (harp_prog(t))
        return prog_chord(harp_pg(t), k % HARP_NCH, t->p[P_ROOT], &rn);
    return harp_omni(t) ? omni_id(harp_set(t), k % HARP_NCH, t->p[P_ROOT]) : harp_set(t) * HARP_NCH + k % HARP_NCH;
}
static int32_t harp_rt(const track_t *t) { return harp_omni(t) || harp_prog(t) ? 0 : t->p[P_ROOT]; }
/* (chord-harp fork) HARP, OMNI and PROG's bend keys, the black keys over the plate: G#4 / A#4 an octave down / up, C#5 /
 * D#5 a whole tone (held: bent, let go: back, voice.c's glide; two held: both); F#4 stays silent.
 * The bend of key k (1/256 semitone), 0 none */
static int32_t harp_bend_k(uint32_t k) { return k == 15u ? -3072 : k == 17u ? 3072 : k == 20u ? -512 : k == 22u ? 512 : 0; }
static uint32_t kb_bend;                        /* the bend keys down (their bend in key_bend_goal) */
/* (chord-harp fork) how far chord key k's notes lie from its id's own: PROG's (its id on C3) its root's, else 0 */
static int32_t harp_dt(const track_t *t, uint32_t k)
{
    int32_t rn;
    if (!harp_prog(t))
        return 0;
    (void)prog_chord(harp_pg(t), k % HARP_NCH, t->p[P_ROOT], &rn);
    return rn - HARP_LO;
}
static uint32_t harp_oct(int32_t n) { return (uint32_t)clamp(n + 12 * song.octave, 0, 127); }
/* (chord-harp fork) the single-key mode: QUANT on (SNAP / WHITE / SEQ) keeps every note the chord keys and the plate
 * play (HARP, OMNI, PROG, CHORD's modes) in the track's key (ROOT, SCALE): a note outside it moves down onto the scale,
 * so the chords always sound good in it. QUANT off: the chords as they are */
static uint32_t harp_inkey(const track_t *t, uint32_t n)
{
    return t->p[P_QUANT] != QN_OFF ? (uint32_t)clamp(scale_snap(t, (int32_t)n), 0, 127) : n;
}
/* CHORD's key k: its degree's root (no octave); a scale of fewer than 7 notes round again an octave up, CHR major's;
 * the last key I an octave up */
static int32_t cm_root(const track_t *t, uint32_t k)
{
    uint32_t mask = scale_mask(t), i, c = 0;
    int32_t deg[12], rt = clamp(t->p[P_ROOT], 0, 11);
    if (k % HARP_NCM == HARP_NCM - 1u)
        return cm_root(t, 0) + 12;
    if ((mask & 0xFFFu) == 0xFFFu || !(mask & 0xFFFu))
        mask = SCALE_MASK[1];
    for (i = 0; i < 12u; i++)
        if ((mask >> i) & 1u)
            deg[c++] = (int32_t)i;
    return HARP_LO + rt - (rt > 6 ? 12 : 0) + deg[k % c] + 12 * (int32_t)(k / c);
}
static uint32_t harp_root(const track_t *t, uint32_t k)   /* chord key k's root note */
{
    if (harp_cm(t))
        return harp_oct(cm_root(t, k));
    return harp_oct((harp_j6(t) ? harp_lo(harp_set(t), k, t->p[P_ROOT]) : hc_lo(harp_id(t, k), harp_rt(t))) + harp_dt(t, k));
}
/* chord key k's name (buf: 16 bytes): HARP's set, OMNI's buttons, PROG's progression (ui_lzpage.c graph_harp) */
static void harp_key_name(const track_t *t, uint32_t k, char *buf)
{
    if (harp_cm(t))
        cm_name(t, k, buf);
    else if (harp_prog(t) || harp_omni(t))
        hc_name(harp_id(t, k), (int32_t)harp_root(t, k), buf);
    else
        harp_name(harp_set(t), k, t->p[P_ROOT], buf);
}
static uint32_t harp_map(const track_t *t, uint32_t k)   /* the note key k plays alone: the plate's; else silent */
{
    uint8_t pl[HARP_NCMP];
    if (harp_ck(t, k) || key_black(k))
        return KB_SILENT;
    if (harp_cm(t))
        cm_plate(t, harp_cur[trk_index(t) % NTRK], pl);
    else if (harp_j6(t))
        harp_plate(harp_set(t), harp_cur[trk_index(t) % NTRK], t->p[P_ROOT], pl);
    else
        hc_plate(harp_id(t, harp_cur[trk_index(t) % NTRK]), harp_rt(t), pl);
    return harp_inkey(t, harp_oct(pl[(key_place(k) - harp_p0(t)) % harp_np(t)] +
                    12 * (clamp(t->p[harp_omni(t) ? P_OSTR : P_HPLATE], 0, 2) - 1) +   /* (PLATE / STRINGS: an octave down, up) */
                    harp_dt(t, harp_cur[trk_index(t) % NTRK])));   /* (PROG: its chord's root) */
}
/* chord key k's notes into out (HARP_NMAX), low to high */
static __attribute__((noinline)) uint32_t harp_build(const track_t *t, uint32_t k, uint8_t *out)
{
    uint8_t nt[HARP_NMAX];
    uint32_t n, i, j = 0;
    if (!voice_poly(t->p[P_VOICE])) {
        out[0] = (uint8_t)harp_inkey(t, harp_root(t, k));
        return 1;
    }
    if (harp_cm(t)) {                               /* (LEAD: the track's last chord, as the keys always did) */
        n = chord_build(t, harp_root(t, k), out);
        for (i = 0; t->p[P_QUANT] != QN_OFF && i < n; i++)
            out[i] = (uint8_t)harp_inkey(t, out[i]);
        for (i = j = 0; i < n; i++)
            if (!j || out[j - 1u] != out[i])
                out[j++] = out[i];
        return j;
    }
    n = harp_j6(t) ? harp_chord(harp_set(t), k, t->p[P_ROOT], nt) : hc_chord(harp_id(t, k), harp_rt(t), nt);
    {
        int32_t dt = harp_dt(t, k) + 12 * song.octave;   /* (PROG: its root; the octave) */
        for (i = 0; i < n; i++)
            if (nt[i] + dt >= 0 && nt[i] + dt <= 127) {
                uint32_t x = harp_inkey(t, (uint32_t)(nt[i] + dt));
                if (!j || out[j - 1u] != x)             /* (two notes snapping together play once) */
                    out[j++] = (uint8_t)x;
            }
    }
    if (!j)
        out[j++] = (uint8_t)harp_inkey(t, harp_root(t, k));
    return j;
}

/* (chord-harp) step s gets chord id (set * 12 + its place) as its CHORD: in its first component's place when that is
 * free or a CHORD, else its second's */
static void step_chord_set(step_t *s, uint32_t id)
{
    if (s->comp2 == CP_CHORD || (s->comp && s->comp != CP_CHORD)) {
        s->comp2 = CP_CHORD;
        s->carg2 = (uint8_t)id;
    } else {
        s->comp = CP_CHORD;
        s->carg = (uint8_t)id;
    }
}

static uint32_t kb_map(const track_t *t, uint32_t k)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];   /* (the engine it switches to) */
    int32_t n;
    if (e->keys && (n = e->keys(t, k)) >= 0)           /* the engine's own key map (DRUM's GM map, slices) */
        return (uint32_t)n;
    if (harp_on(t))                                     /* (chord-harp) the plate's notes; the chord keys are key_on's */
        return harp_map(t, k);
    n = 53 + (int32_t)k;
    if (t->p[P_QUANT] == QN_SNAP || t->p[P_QUANT] == QN_SEQ)   /* SNAP (and SEQ): every key, rounded down */
        return (uint32_t)clamp(scale_snap(t, n + 12 * song.octave), 0, 127);   /* (0.6.81: TRANS as it sounds, voice.c) */
    if (t->p[P_QUANT] == QN_WHITE) {                    /* WHITE: white keys walk the scale, black keys are silent */
        uint32_t mask = scale_mask(t), i;
        int32_t count = 0, degree = DEGREE[n % 12], oct;
        if (degree < 0)
            return KB_SILENT;
        /* C4 is the root. Walk scale degrees on successive white keys, including
         * below C4; scales with 5, 6, 8 or 12 notes still have no duplicated degrees. */
        degree += (n / 12 - 5) * 7;
        for (i = 0; i < 12u; i++)
            count += (mask >> i) & 1u;
        oct = degree / count;
        degree %= count;
        if (degree < 0) {
            degree += count;
            oct--;
        }
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (!degree)
                    break;
                degree--;
            }
        n = 60 + t->p[P_ROOT] + 12 * oct + (int32_t)i;
    }
    return (uint32_t)clamp(n + 12 * song.octave, 0, 127);
}

/* the notes key k plays (kb_sn: how many, its chord; kb_on: how many of them started, the rest strummed in later) */
#define KB_CH 6u                                /* (chord-harp: 6, a HARP chord whole; was 4) */
static uint8_t kb_snd[27][8], kb_sn[27], kb_on[27];   /* (chord-harp: rows of 8, KB_CH used: a shift indexes them in RAM code) */
static uint16_t kb_sdue[27];                    /* (0.6.81) samples to the strum's next note */

/* (0.6.81) CHORD (SCL > CHORD; after the Omnichord and the Orchid, light): a key plays a chord on the track.
 * TRIAD 7TH ADD9 SUS2 SUS4 are built in the track's scale on the key (every tone in key; a key outside it takes the
 * scale note below; CHR: the major scale of ROOT; 0.8.3: a scale of five, six or eight notes takes each tone as the
 * interval the chord asks for where the scale has it, else the nearest it offers: sus chords and sixths in the
 * pentatonics, augmented in whole tone, diminished in the diminished scales), MAJ MIN 7 MAJ7 MIN7 POWER that shape on
 * any key. VOICING CLOSE
 * (root position), LEAD (each chord the inversion nearest the last one the track played: voice leading), OPEN (the
 * second tone an octave up), +BASS (the root an octave down added). STRUM: the notes one after the other, low to
 * high. The root alone on a kit and on MONO / LEGATO / UNISON. The keys only (MIDI notes play as they come); the
 * notes go to the sequencer's recording and the ARP as the key's own would */
enum { CV_CLOSE, CV_LEAD, CV_OPEN, CV_BASS };
static uint8_t chord_last[NTRK][KB_CH], chord_nlast[NTRK];   /* the last chord each track played (LEAD) */
static int32_t chord_r;                         /* (chord-harp fork) the root of the chord chord_build built last */
static uint8_t chord_any;                       /* (2026-10-27) 1: chord_build builds on any VOICE (cm_chord: names, plate) */
static uint8_t key_rpc[NTRK];                   /* .. of the chord (or note) each track's keys played last: its pitch class */
/* (0.8.3) the first of the intervals p (n of them) the scale (mask) has over a root rel semitones above ROOT; -1 none */
static int32_t chord_pick(uint32_t mask, int32_t rel, const int8_t *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if ((mask >> (uint32_t)((rel + p[i]) % 12)) & 1u)
            return p[i];
    return -1;
}
static uint32_t chord_build(const track_t *t, uint32_t root, uint8_t *out)
{
    static const int8_t DIA[5][4] = {{0, 2, 4, -1}, {0, 2, 4, 6}, {0, 2, 4, 8}, {0, 1, 4, -1}, {0, 3, 4, -1}};
    static const int8_t SHP[6][4] = {{0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10},
                                     {0, 7, 12, -1}};
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t mode = (uint32_t)t->p[P_CHRD], vc = (uint32_t)t->p[P_CVOI], n = 0, i, j, ti = trk_index(t) % NTRK;
    int32_t nt[KB_CH + 1], r = (int32_t)root;
    chord_r = r;
    if (!mode || mode > CD_POW || e->oneshot || (e->keys && e->keys(t, 0) >= 0) || (!voice_poly(t->p[P_VOICE]) && !chord_any)) {
        out[0] = (uint8_t)root;
        return 1;
    }
    if (mode >= CD_MAJ) {
        for (i = 0; i < 4u && SHP[mode - CD_MAJ][i] >= 0; i++)
            nt[n++] = r + SHP[mode - CD_MAJ][i];
    } else {                                    /* every other note of the scale from the key's */
        uint32_t mask = scale_mask(t), deg[12], c = 0, at = 0, guard = 12;
        int32_t rt = t->p[P_ROOT];
        if ((mask & 0xFFFu) == 0xFFFu || !(mask & 0xFFFu))
            mask = SCALE_MASK[1];               /* (CHR: the major scale of ROOT) */
        while (guard-- && !((mask >> (uint32_t)((r - rt + 120) % 12)) & 1u))
            r--;
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (i == (uint32_t)((r - rt + 120) % 12)) at = c;
                deg[c++] = i;
            }
        if (c == 7u) {                          /* a mode of seven: its own chord on the degree (diatonic) */
            for (i = 0; i < 4u && DIA[mode - CD_TRI][i] >= 0; i++) {
                uint32_t k = at + (uint32_t)DIA[mode - CD_TRI][i];
                nt[n++] = r + (int32_t)deg[k % c] + 12 * (int32_t)(k / c) - (int32_t)deg[at];
            }
        } else {                                /* (0.8.3) five, six or eight notes (PEN MPEN BLUES WHOLE DIMHW DIMWH):
                                                 * every other note of such a scale is a quartal or odd stack, so each
                                                 * tone is the interval the chord asks for where the scale has it, else
                                                 * the nearest it offers: a third major or minor (none: a suspension),
                                                 * the fifth perfect (else diminished over a minor third, augmented over
                                                 * a major), the seventh minor, major or sixth; pentatonic keys give
                                                 * triads, sus chords and sixths, whole tone augmented, diminished
                                                 * diminished and dominant sevenths: every tone in the scale */
            static const int8_t T3[4] = {4, 3, 5, 2}, F5[3] = {7, 8, 6}, F5M[3] = {7, 6, 8}, S7[3] = {10, 11, 9}, S7D[2] = {9, 10},
                                S9[2] = {14, 15}, S2[2] = {2, 1}, S4[2] = {5, 6};
            int32_t rel = (r - rt + 120) % 12, th, fi, x;
#define CPK(P) chord_pick(mask, rel, P, sizeof P)
            th = mode == CD_SUS2 ? CPK(S2) : mode == CD_SUS4 ? CPK(S4) : -1;
            if (th < 0) th = CPK(T3);
            fi = th == 3 ? CPK(F5M) : CPK(F5);
            nt[n++] = r;
            if (th >= 0) nt[n++] = r + th;
            if (fi >= 0) nt[n++] = r + fi;
            if (mode == CD_SEV && (x = th == 3 && fi == 6 ? CPK(S7D) : CPK(S7)) >= 0) nt[n++] = r + x;
            if (mode == CD_ADD9 && (x = CPK(S9)) >= 0) nt[n++] = r + x;
#undef CPK
        }
    }
    if (vc == CV_OPEN && n > 2u)
        nt[1] += 12;
    if (vc == CV_LEAD && chord_nlast[ti]) {     /* each tone the octave nearest the last chord's middle */
        int32_t mid = 0;
        for (i = 0; i < chord_nlast[ti]; i++) mid += chord_last[ti][i];
        mid /= (int32_t)chord_nlast[ti];
        for (i = 0; i < n; i++) {
            while (nt[i] - mid > 6 && nt[i] - 12 >= 24) nt[i] -= 12;
            while (mid - nt[i] > 6 && nt[i] + 12 <= 108) nt[i] += 12;
        }
    }
    if (vc == CV_BASS) {                        /* the root an octave down (a four-note chord leaves its fifth out) */
        if (n == 4u) { nt[2] = nt[3]; n = 3; }
        nt[n++] = r - 12;
    }
    for (i = 1; i < n; i++)                     /* low to high, no unisons */
        for (j = i; j > 0 && nt[j] < nt[j - 1]; j--) { int32_t x = nt[j]; nt[j] = nt[j - 1]; nt[j - 1] = x; }
    for (i = 1; i < n; i++)
        if (nt[i] <= nt[i - 1]) nt[i] = nt[i - 1] + 12;
    for (i = 0, j = 0; i < n && j < KB_CH; i++)
        if (nt[i] >= 0 && nt[i] <= 127) out[j++] = (uint8_t)nt[i];
    if (!j) out[j++] = (uint8_t)root;
    chord_r = r;
    for (i = 0; i < j; i++) chord_last[ti][i] = out[i];
    chord_nlast[ti] = (uint8_t)j;
    return j;
}
/* (2026-10-27) CHORD's key k: its chord (no octave), LEAD's memory kept, on any VOICE (cm_r: its root) */
static int32_t cm_r;
static uint32_t cm_chord(const track_t *t, uint32_t k, uint8_t *out)
{
    uint32_t ti = trk_index(t) % NTRK, nl = chord_nlast[ti], n, i;
    uint8_t last[KB_CH];
    int32_t r = chord_r;
    for (i = 0; i < KB_CH; i++) last[i] = chord_last[ti][i];
    chord_any = 1;
    n = chord_build(t, (uint32_t)cm_root(t, k % HARP_NCM), out);
    chord_any = 0;
    for (i = 0; i < KB_CH; i++) chord_last[ti][i] = last[i];
    chord_nlast[ti] = (uint8_t)nl;
    cm_r = chord_r;
    chord_r = r;
    return n;
}
/* its name (buf: 16 bytes): the root and HARP's shape of the same tones ("Cm", "G7", "Bdim"; POWER "C5"), else the root */
static void cm_name(const track_t *t, uint32_t k, char *buf)
{
    uint8_t nt[KB_CH];
    uint32_t n = cm_chord(t, k, nt), m = 0, i, q;
    str_cpy(buf, HARP_PC[(uint32_t)(cm_r + 120) % 12u], 16);
    for (i = 0; i < n; i++)
        m |= 1u << (uint32_t)((nt[i] - cm_r + 120) % 12);
    for (q = 0; q < HQ_N; q++) {
        uint32_t sh = 0;
        for (i = 0; i < HARP_NMAX && HQ_IV[q][i] >= 0; i++)
            sh |= 1u << HQ_IV[q][i] % 12;
        if (sh == m) {
            str_cpy(buf + str_len(buf), HQ_NAME[q], 16 - str_len(buf));
            return;
        }
    }
    if (m == 0x81u)
        str_cpy(buf + str_len(buf), "5", 16 - str_len(buf));
}
/* its plate (out: HARP_NCMP, no octave): the chord's tones up from the first above its top note */
static void cm_plate(const track_t *t, uint32_t k, uint8_t *out)
{
    uint8_t nt[KB_CH];
    uint32_t n = cm_chord(t, k, nt), m = 0, i, j = 0;
    int32_t x = nt[n - 1u];
    for (i = 0; i < n; i++)
        m |= 1u << nt[i] % 12u;
    while (j < HARP_NCMP)
        if (x >= 127 || (m >> (uint32_t)(++x % 12) & 1u))
            out[j++] = (uint8_t)(x < 127 ? x : 127);
}
static uint32_t chord_gap(const track_t *t) { return (uint32_t)clamp(t->p[P_CSTR], 0, 127) * (FS / 2000u); }   /* (~0.5 ms a step) */

/* ------------------------------------------------------------- arp --- */
static void arp_add(track_t *t, uint32_t note)
{
    uint32_t i;
    if (t->p[P_AHOLD] && t->arp_phys == 0u)
        t->nheld = 0;                               /* new chord replaces the latched one */
    t->arp_phys++;                                  /* every key-down: arp_remove counts every key-up */
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] == note)
            return;                                 /* repeated note-on: not a new note */
    if (t->nheld < 16u)
        t->held[t->nheld++] = (uint8_t)note;
    if (t->nheld == 1u) {
        t->arp_pos = 0xFFFFFFF;                     /* fire on this block */
        t->arp_idx = 0xFFFFFFFFu;
    }
}

static void arp_remove(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    if (t->arp_phys)
        t->arp_phys--;
    if (t->p[P_AHOLD])
        return;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] != note)
            t->held[k++] = t->held[i];
    t->nheld = (uint8_t)k;
}

static step_t *rec_note(track_t *t, uint32_t note, uint32_t vel);
static void rec_release(track_t *t, uint32_t note);
static int rec_on(const track_t *t) { return ((song.rec >> trk_index(t)) & 1u) && song.playing && !chain.running; }
static int pfx_rec_ok(void) { return song.rec && song.playing && !chain.running; }   /* (pfx.c: the punch-in lane) */

#include "motion.c"

/* the ARP's modes (P_AMODE). Append-only: a stored value keeps its meaning (ORD plays as UP, the ORD switch of
 * ARP 2 orders the notes). 1.2 (Discussion #95), over the note list (the held notes low to high, or as played,
 * across OCT octaves), C E G held: DNUP G E C E; UP+8 C E G C' (the first note again an octave above the top);
 * CONV C G E (outside in); DIVG E G C (inside out); PINKY C G E G (the top note between the others); THUMB
 * C E C G (the bottom one); WALK a random step up or down the list (bouncing off its ends); CHORD all the held
 * notes together (the first 4), an octave higher each step through OCT */
enum { AM_OFF, AM_UP, AM_DN, AM_UPDN, AM_RND, AM_ORD, AM_REPEAT, AM_DNUP, AM_UP8, AM_CONV, AM_DIVG, AM_PINKY,
       AM_THUMB, AM_WALK, AM_CHORD,
       AM_TRNS, AM_SKIP, AM_HOLD,
       AM_TURING,
       AM_STRDN, AM_STRUP, AM_STRALT, AM_BLOCK, AM_BSCH, AM_SHUF, AM_HALF, AM_TWO3, AM_FAST };   /* (strum-on-hold: the strokes) */                  /* (chord-harp fork, 2026-10-30: Clipper 0.9's TURNG, turing_step; its 15) */
/* (chord-harp fork) SKIP: UP, about a third of its notes left out at random;
                                       * HOLD: UP, now and then a note held 1 or 2 steps more (its count in arp_walk).
                                       * (Yehnah) TRNS: a key no note but the track's sequence transposed by its interval from C4 (t->trn, kept
                     * when let go: the next key moves it), MIDI in too; the steps' notes, never the lane hits */

static void arp_forget(track_t *t)               /* (its notes ended otherwise: trk_all_off) */
{
    t->arp_note = t->arp_ch[0] = t->arp_ch[1] = t->arp_ch[2] = 0;
}

/* CHORD's notes. Not inlined: they run once an arp step, and kept out of the audio ISR's loop over the tracks
 * (tests/target_budget.py) */
static __attribute__((noinline)) void arp_chord_off(track_t *t, int rec)   /* the other notes end */
{
    uint32_t i;
    for (i = 0; i < 3u; i++)
        if (t->arp_ch[i]) {
            if (rec)
                rec_release(t, t->arp_ch[i]);
            seq_note_off(t, t->arp_ch[i]);
            t->arp_ch[i] = 0;
        }
}

static __attribute__((noinline)) void arp_chord(track_t *t, const uint32_t *list, uint32_t n)
{
    uint32_t i, nt, top = 0, o = t->arp_idx % (uint32_t)(t->p[P_AOCT] > 0 ? t->p[P_AOCT] : 1);
    if (n > 4u)
        n = 4u;
    for (i = 0; i < n; i++) {                       /* the held notes (sorted or as played), o octaves up */
        nt = (uint32_t)clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
        if (nt == 127u && top++)
            continue;                               /* (notes clamped onto the top one: once) */
        if (!t->arp_note)
            t->arp_note = (uint8_t)nt;
        else
            t->arp_ch[i - 1u] = (uint8_t)nt;
        if (rec_on(t))
            rec_note(t, nt, 100);
        seq_note_on(t, nt, 100);
    }
}

static void arp_stroke_off(track_t *t, int rec)  /* a stroke's later notes that have come in end */
{
    uint32_t q;
    for (q = 0; q < t->as_i && q < t->as_n; q++) {
        if (rec)
            rec_release(t, t->as_nn[q]);
        seq_note_off(t, t->as_nn[q]);
    }
    t->as_n = t->as_i = 0;
}

static void arp_silence(track_t *t)              /* the sounding arp note(s) end */
{
    if (t->as_n)
        arp_stroke_off(t, 0);
    if (t->arp_ch[0] | t->arp_ch[1] | t->arp_ch[2])
        arp_chord_off(t, 0);
    seq_note_off(t, t->arp_note);
    t->arp_note = 0;
}

/* (2026-11-10) a stroke mode with GATE at its top is FREE, as guitchordi's gate: the strings ring on after their stroke, each
 * until it is struck again (or the chord goes), so the strokes overlap as a guitar's do (arp_stroke) */
static int arp_free(const track_t *t) { return t->p[P_AMODE] >= AM_STRDN && t->p[P_AGATE] >= 127; }
static void arp_off(track_t *t)                  /* the sounding arp note(s) end (a recorded one too) */
{
    if (t->as_n)
        arp_stroke_off(t, rec_on(t));
    if (t->arp_ch[0] | t->arp_ch[1] | t->arp_ch[2])
        arp_chord_off(t, rec_on(t));
    seq_note_off(t, t->arp_note);
    if (rec_on(t))
        rec_release(t, t->arp_note);
    t->arp_note = 0;
}

/* (chord-harp fork, 2026-10-30: Clipper's seq.c, fbd8434; the arp's, not yet the steps') Clipper 0.8, SPICE / DICE (after the MicroFreak's): the arp's notes and the sequencer's steps vary, as much as
 * SPICE says (0: as written), the same way each time round (the k-th note of 16, the pattern's step k) until DICE is
 * turned (another variation). A note may rest (up to 1 in 6), jump an octave (up to 1 in 4, mostly up), and its
 * velocity and gate move (up to +-63, x 0.05 .. 1.95). What is stored is never changed */
typedef struct { uint8_t skip; int8_t oct; uint8_t vel, gate; } spice_t;   /* gate: x / 64 */
static uint32_t spice_loop(const track_t *t)            /* (0.9) LOOP: the variation comes round every LOOP notes */
{
    return (uint32_t)clamp(t->p[P_SLOOP], 1, 16);
}
static uint32_t spice_gate = 64u;                    /* (the arp's last note: arp_tick's gate) */
static int spice_of(const track_t *t, uint32_t k, uint32_t vel, spice_t *o)
{
    uint32_t sp = (uint32_t)clamp(t->p[P_SPICE], 0, 127), h;
    int32_t v;
    o->skip = 0, o->oct = 0, o->vel = (uint8_t)vel, o->gate = 64;
    if (!sp)
        return 0;
    h = (k + 1u) * 0x9E3779B1u ^ 1u * 0x85EBCA77u;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12, h *= 0x297A2D39u, h ^= h >> 15;
    if ((h & 255u) < sp / 3u)
        o->skip = 1;
    else if (((h >> 8) & 255u) < sp / 2u)
        o->oct = (int8_t)((h >> 16) & 3u ? 12 : -12);
    v = (int32_t)vel + ((((int32_t)((h >> 18) & 127u) - 64) * (int32_t)sp) >> 7);
    o->vel = (uint8_t)clamp(v, 1, 127);
    o->gate = (uint8_t)clamp(64 + ((((int32_t)((h >> 25) & 63u) - 32) * (int32_t)sp) >> 6), 3, 125);
    return 1;
}

/* 0.9, the arp's TURNG (after Music Thing Modular's Turing Machine): a shift register of LOOP bits (SPICE page) turns
 * one step a note; the bit that comes round is flipped with the chance MUTAT (SPICE: 0 a locked loop, 127 nearly a
 * coin toss), and the register read as a number picks the note from the held notes over OCT octaves (low to high),
 * its velocity from two more bits. SEED (DICE) chooses the loop it starts from: the same SEED, the same loop. The
 * notes are always the held ones: in key whatever it plays */
static uint32_t turing_step(track_t *t, uint32_t len, uint32_t *vel)
{
    uint32_t loop = (uint32_t)clamp(t->p[P_SLOOP], 1, 16), mut = (uint32_t)clamp(t->p[P_SPICE], 0, 127);
    int32_t seed = 1;
    uint32_t mask = (1u << loop) - 1u, r = t->tm_reg, bit, v8;
    if (t->tm_seed != seed) {                           /* a new SEED: its loop */
        uint32_t h = (uint32_t)seed * 0x9E3779B1u;
        h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 13;
        t->tm_seed = (int16_t)seed;
        r = (h >> 8) & 0xFFFFu;
    }
    bit = (r >> (loop - 1u)) & 1u;
    if (mut && (rng() & 255u) < mut)
        bit ^= 1u;
    r = ((r << 1) | bit) & mask;
    t->tm_reg = (uint16_t)r;
    v8 = loop >= 8u ? r & 255u : (r << (8u - loop)) & 255u;
    *vel = 88u + ((r >> 1) & 3u) * 12u;                 /* 88 .. 124 */
    return (v8 * len) >> 8;
}

/* the place in the note list (len notes) the step arp_idx plays (not inlined, as arp_chord); UP+8 adds its note */
static __attribute__((noinline)) uint32_t arp_pick(track_t *t, uint32_t *list, uint32_t len)
{
    uint32_t i = t->arp_idx, m = (uint32_t)t->p[P_AMODE], cyc, k, w;
    if (m == AM_UP8 && len < 64u)                   /* UP+8: the first note again over the top */
        list[len++] = (uint32_t)clamp((int32_t)list[0] + 12 * t->p[P_AOCT], 0, 127);
    cyc = len > 1u ? 2u * len - 2u : 1u;
    k = i % cyc;
    switch (m) {
    case AM_DN:
        return len - 1u - i % len;
    case AM_UPDN:
        return k < len ? k : cyc - k;
    case AM_DNUP:
        return k < len ? len - 1u - k : k - (len - 1u);
    case AM_RND:
        return rng() % len;
    case AM_CONV:
    case AM_DIVG:
        k = i % len;
        if (m == AM_DIVG)
            k = len - 1u - k;
        return (k & 1u) ? len - 1u - k / 2u : k / 2u;
    case AM_PINKY:
        return len < 2u ? 0u : (k & 1u) ? len - 1u : k / 2u;
    case AM_THUMB:
        return len < 2u ? 0u : (k & 1u) ? 1u + k / 2u : 0u;
    case AM_WALK:
        w = t->arp_walk;
        if (!i)
            w = 0;                                  /* (from the bottom) */
        else if (w >= len)
            w = len - 1u;                           /* (notes let go) */
        else if (len > 1u)
            w = !w ? 1u : w == len - 1u ? w - 1u : (rng() >> 7) & 1u ? w + 1u : w - 1u;
        t->arp_walk = (uint8_t)w;
        return w;
    default:
        return i % len;
    }
}

/* n: samples the arp advances by (the external clock's while it runs, 0 stopped); gate_n: samples its
 * gate counts down by (real time when the external transport stops: a tapped or latched note still ends) */
static volatile uint8_t fx_lock_on;              /* (0.6.44) ui_layer.c: the FX layer locked open, no other layer armed:
                                                 * the keys are its effects without FX held */
static volatile uint32_t beat_pos, beat_n;      /* samples into the beat, the beat of the bar (0..3) */
static volatile uint32_t bar_n;                 /* (0.6.43) the bars since PLAY (MUTATE: project.c mutate_poll) */

/* the metronome (click.c) and the count-in (1.1, Discussion #131). The click follows the song tempo's quarter notes
 * (BPM, or the external clock's 24 pulses), 4 to the bar, the first accented, whatever a track's DIV or SWING: a beat is
 * four 1/16 steps as the sequencer counts them (click_beat_len: 4 x div_samples(1/16), which may be a few samples
 * shorter than beat_samples, so the click never drifts from the 1/16 tracks it is recorded against). clk_pos and
 * clk_step count the 1/16 steps as seq_tick counts a 1/16 track's (and midi_clock_pulse rescales clk_pos with them when
 * an external clock's tempo moves): CLK_START at seq_start clicks the first beat in the block step 0 starts in, and
 * beat k in the block a 1/16 track's step 4k starts in (every 4th step; step 0 of 16, the bar's first, accented).
 * The count-in: with MENU > COUNT-IN on, PLAY from stop with a track armed (REC while stopped starts PLAY too) on
 * the internal clock counts cin_total beats first (1 or 2 bars, clicked whatever CLICK is), then
 * starts the sequencer at step 1 exactly that many beats later (song.playing stays 0 until then: nothing plays or
 * records; the keys sound). A note into an armed track in the count-in's last eighth (the second half of its last
 * beat) is recorded onto step 1 when it starts (cin_keep, cin_flush), held on or let go as it was. An external clock
 * never counts in (its Start starts). STOP (seq_stop) cancels it. */
/* (clk_pos, clk_step, clk_n: mod.c, read by the SLICER and the FX layer before this file) */
static volatile uint8_t cin_left, cin_total;    /* the count-in's beats still to come (0: none), of how many */
static volatile uint32_t cin_pos;               /* samples into its beat playing */
static uint8_t cin_flush;                       /* the count-in ended in this block: record what was kept */
static uint8_t cin_note[NTRK][4], cin_vel[NTRK][4], cin_n[NTRK], cin_up[NTRK];   /* kept notes; cin_up bit k: let go */
static uint8_t cin_oct[NTRK][4];                /* (0.8.5) .. a kit's octave: its hits a step (bits 0-3), TOUCH (4-5) */
static int seq_counting(void) { return cin_left != 0u; }
static uint32_t click_beat_len(void) { return 4u * div_samples(2); }   /* DIV 2: 1/16 */
static int click_wanted(void)                  /* CLICK REC: while a track is armed (not in a song: no recording) */
{
    return click_mode == CLICK_ON || (click_mode == CLICK_REC && song.rec && !chain.running);
}
static void arp_fire(track_t *t, uint32_t period);
static uint32_t hum_amt(const track_t *t, uint32_t id);   /* (SEQ > HUMAN, below: the arp's too) */
static uint32_t hum_rnd(void);
static int32_t hum_pm(uint32_t r);
/* (2026-11-11) guitchordi's humanity (app.c rhythm_tick, its default 20 %): each note after the first is moved by up to
 * +-88 samples (of 44.1 kHz) x humanity / 10 = +-0.4 ms from its nominal place (not cumulative: the offset is the note's own,
 * here a hash of the repeat and the note, so no state), and its strength by up to +-0.02. Note i's offset, in samples */
static int32_t st_jit(const track_t *t, uint32_t i)
{
    uint32_t h = (t->arp_idx * 2654435761u + i * 40503u) >> 7;
    return i ? ((int32_t)(h % 177u) - 88) * 2 * (int32_t)(FS / 1000u) / (10 * 44) : 0;
}
/* the wait before overall note i (the first is 0) after note i - 1, never negative */
static uint32_t st_due(const track_t *t, uint32_t i)
{
    int32_t d = (int32_t)t->as_gap + st_jit(t, i) - st_jit(t, i - 1u);
    return t->as_gap && d > 0 ? (uint32_t)d : t->as_gap;
}
/* a stroke's later notes coming in, one gap apart (noinline, in the flash: arp_step is inlined into the RAM code) */
static __attribute__((noinline)) void arp_stroke_run(track_t *t, uint32_t n)
{
    if (t->as_due > n) {
        t->as_due -= n;
        return;
    }
    do {
        uint32_t nt = t->as_nn[t->as_i];
        if (rec_on(t))
            rec_note(t, nt, t->as_vv[t->as_i]);
        if (arp_free(t))
            seq_note_off(t, nt);                        /* (struck again: the old ring ends) */
        seq_note_on(t, nt, t->as_vv[t->as_i]);
        t->as_i++;
    } while (!t->as_gap && t->as_i < t->as_n);
    t->as_due = st_due(t, t->as_i + 1u);
}
static void arp_step(track_t *t, uint32_t n, uint32_t gate_n)
{
    uint32_t period;
    int32_t sw;
    if (t->arp_note) {
        if (t->arp_off <= gate_n)
            arp_off(t);
        else
            t->arp_off -= gate_n;
    }
    if (t->as_i < t->as_n)
        arp_stroke_run(t, n);
    if (!t->p[P_AMODE] || !t->nheld || t->p[P_AMODE] == AM_TRNS) {   /* (TRNS: no arpeggio) */
        if ((!t->nheld || t->p[P_AMODE] == AM_TRNS) && t->arp_note)
            arp_off(t);
        return;
    }
    period = t->p[P_ARATE] >= 10 ? div_samples((uint32_t)t->p[P_ARATE] - 10u) * 3u / 2u : div_samples((uint32_t)t->p[P_ARATE]);
    sw = t->p[P_ASWING] * (int32_t)period / 250;
    t->arp_pos += n;
    if (t->arp_pos < period + t->arp_hd + (uint32_t)((t->arp_idx & 1u) ? sw : -sw) && t->arp_pos != 0xFFFFFFF + n)
        return;
    t->arp_pos = 0;
    t->arp_hd = 0;
    arp_fire(t, period);
}
/* (strum-on-hold) the STR* modes: one stroke of the held chord (up to 6 notes, the lowest 6; FAST: up, down, up) each
 * repeat. The first note now, the rest by arp_stroke_run, one gap (SCL > GUITAR's STRUM, KNOB 4; FAST: that is the
 * sweep of one stroke, 30 .. 120 ms) apart; all inside 3/4 of the step. HUMAN moves each note's velocity */
#define ST_MAX 6u
static __attribute__((noinline)) void arp_stroke(track_t *t, const uint32_t *list, uint32_t cnt, uint32_t period, uint32_t vel)
{
    uint8_t o[ST_MAX * 3u];
    uint32_t m = (uint32_t)t->p[P_AMODE], p = t->arp_idx & 1u, c = cnt < ST_MAX ? cnt : ST_MAX, i, j, k = c, tot, gap, due, hv, rev = 0, span;
    /* (2026-11-10) guitchordi's strum, as its app.c rhythm_tick has it: the strings low to high is a DOWN stroke (UP, and
     * ALT's second, high to low); the knob (KNOB 4, STRUM) is the whole stroke's time, 0 or 5 .. 250 ms in 5 ms steps, the
     * notes evenly across it (a half or two-thirds: that many strings, chosen at random, in their physical order); BLOCK
     * all at once; BS+CH the bass, then the rest together 3 gaps later; FAST three strokes (up, down, up) in 30 .. 120 ms;
     * a stroke is never longer than its step. Every note the same velocity (HUMAN moves it) */
    for (i = 0; i < c; i++)
        o[i] = (uint8_t)i;                              /* (low to high) */
    switch (m) {
    case AM_STRUP: rev = 1; break;
    case AM_STRALT: rev = p; break;                     /* (down, up, ..) */
    case AM_SHUF: case AM_HALF: case AM_TWO3:
        for (i = c; i > 1u; i--) {                      /* a random order */
            j = rng() % i;
            k = o[i - 1u], o[i - 1u] = o[j], o[j] = (uint8_t)k;
        }
        k = m == AM_SHUF ? c : m == AM_HALF ? (c + 1u) / 2u : (2u * c + 2u) / 3u;
        if (m != AM_SHUF)                               /* a part: in the physical order, low to high */
            for (i = 1; i < k; i++)
                for (j = i; j > 0 && o[j - 1u] > o[j]; j--) {
                    uint32_t x = o[j];
                    o[j] = o[j - 1u];
                    o[j - 1u] = (uint8_t)x;
                }
        break;
    default:
        break;
    }
    if (rev)
        for (i = 0; i < k / 2u; i++) {
            uint8_t x = o[i];
            o[i] = o[k - 1u - i];
            o[k - 1u - i] = x;
        }
    if (m == AM_FAST) {                                 /* up (high to low), down, up */
        for (i = 0; i < c; i++) {
            o[i] = (uint8_t)(c - 1u - i);
            o[c + i] = (uint8_t)i;
            o[2u * c + i] = (uint8_t)(c - 1u - i);
        }
        k = 3u * c;
    }
    tot = k;
    span = (uint32_t)clamp(t->p[P_CSTR], 0, 127);
    span = ((span * 50u + 63u) / 127u) * 5u;            /* (ms) */
    if (m == AM_FAST)
        span = span < 30u ? 30u : span > 120u ? 120u : span;
    span *= FS / 1000u;
    if (span >= period)
        span = period - 1u;
    gap = m == AM_BLOCK || tot < 2u ? 0u : span / (tot - 1u);
    due = gap;
    if (m == AM_BSCH && c > 1u) {                       /* the bass, then the rest together */
        due = 3u * (span / (c - 1u));
        if (due >= period)
            due = period - 1u;
        gap = 0;
    }
    hv = hum_amt(t, P_HVEL);
    if (arp_free(t)) {                                  /* FREE: the last stroke's strings the new one does not strike end now */
        for (j = 0; j <= t->as_i && j <= t->as_n; j++) {
            uint32_t pv = j ? t->as_nn[j - 1u] : t->arp_note, keep = 0;
            if (!pv)
                continue;
            for (i = 0; i < tot; i++)
                keep |= (uint32_t)clamp((int32_t)list[o[i]], 1, 127) == pv;
            if (rec_on(t))
                rec_release(t, pv);
            if (!keep)
                seq_note_off(t, pv);
        }
    }
    t->as_n = t->as_i = 0;
    for (i = 0; i < tot; i++) {
        uint32_t nt = (uint32_t)clamp((int32_t)list[o[i]], 1, 127), v = vel;
        v = (uint32_t)clamp((int32_t)v + (int32_t)(rng() % 5u) - 2, 1, 127);   /* (guitchordi's +-0.02 strength) */
        if (hv)
            v = (uint32_t)clamp((int32_t)v + hum_pm(hv * 50u / 127u), 1, 127);
        if (!i) {
            t->arp_note = (uint8_t)nt;
            if (rec_on(t))
                rec_note(t, nt, v);
            if (arp_free(t))
                seq_note_off(t, nt);
            seq_note_on(t, nt, v);
        } else {
            t->as_nn[t->as_n] = (uint8_t)nt;
            t->as_vv[t->as_n++] = (uint8_t)v;
        }
    }
    t->as_gap = gap;
    t->as_due = due == gap ? st_due(t, 1u) : due;
    if (!due && t->as_n)
        arp_stroke_run(t, 0);
}
/* (chord-harp fork) an arp step: the next note. noinline, in the flash: arp_step is inlined into the RAM code
 * (mix_block), and this runs only once a step */
static __attribute__((noinline)) void arp_fire(track_t *t, uint32_t period)
{
    uint32_t cnt, list[64], len = 0, i, j, o, vel = 100, hv;
    spice_t sp = {0, 0, 100, 64};
    if (t->p[P_AMODE] == AM_HOLD && t->arp_note && t->arp_walk && t->arp_walk <= 2u) {   /* HOLD: this step the note held */
        t->arp_walk--;
        return;
    }
    /* build the note list: held notes (sorted or as played) over OCT octaves */
    for (i = 0; i < t->nheld; i++)
        list[i] = t->held[i];
    cnt = t->nheld;
    if (!t->p[P_AORDER])
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && list[j - 1] > list[j]; j--) {
                uint32_t x = list[j];
                list[j] = list[j - 1];
                list[j - 1] = x;
            }
    for (o = 0; o < (uint32_t)t->p[P_AOCT]; o++)
        for (i = 0; i < cnt && len < 64u; i++)
            list[len++] = clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    if (t->p[P_AMODE] == AM_REPEAT) { list[0] = t->held[t->nheld - 1u]; len = 1; } /* REPEAT: last played note, no octave traversal */
    t->arp_idx++;
    if (t->p[P_AMODE] == AM_TURING) {               /* (Clipper's TURNG) its register picks the note */
        spice_gate = 64u;
        j = turing_step(t, len, &vel);
        if (j >= len)
            j = len - 1u;
    } else {
        spice_of(t, t->arp_idx % spice_loop(t), 100u, &sp);   /* (Clipper's SPICE: this note's variation, every LOOP) */
        spice_gate = sp.gate;
        vel = sp.vel;
        j = t->p[P_AMODE] == AM_CHORD ? 0u : arp_pick(t, list, len);
    }
    if (t->arp_note && !arp_free(t))
        arp_off(t);
    hv = hum_amt(t, P_HVEL);
    if (hv) {                                       /* HUMAN: the next step up to 40 ms late (never early) */
        uint32_t d = hum_rnd() % ((FS * 40u / 1000u) * hv / 127u + 1u);
        t->arp_hd = d < period / 3u ? d : period / 3u;
    }
    if (t->p[P_AMODE] != AM_TURING && sp.skip)
        return;                                     /* (SPICE: a rest; the walk goes on) */
    if ((uint32_t)(rng() & 127u) <= (uint32_t)t->p[P_APROB] && !(t->p[P_AMODE] == AM_SKIP && rng() % 3u == 0u)) {
        t->arp_off = arp_free(t) ? 0x7FFFFFFFu : period * (uint32_t)t->p[P_AGATE] / 128u * spice_gate / 64u;
        if (t->p[P_AMODE] == AM_HOLD) {             /* HOLD: now and then this one on for 1 or 2 steps more */
            t->arp_walk = rng() % 10u < 3u ? (uint8_t)(1u + rng() % 2u) : 0u;
            t->arp_off += period * t->arp_walk;
        }
        if (t->p[P_AMODE] >= AM_STRDN) {            /* the strokes */
            arp_stroke(t, list, cnt, period, vel);
            return;
        }
        if (t->p[P_AMODE] == AM_CHORD) {            /* CHORD: the first 4 notes, an octave up each step */
            arp_chord(t, list, cnt);
            return;
        }
        t->arp_note = (uint8_t)clamp((int32_t)list[j] + (t->p[P_AMODE] == AM_TURING ? 0 : sp.oct), 1, 127);
        if (hv)
            vel = (uint32_t)clamp((int32_t)vel + hum_pm(hv * 50u / 127u), 1, 127);
        if (rec_on(t))
            rec_note(t, t->arp_note, vel);           /* recording: what the arp plays */
        seq_note_on(t, t->arp_note, vel);
    }
}

static void arp_tick(track_t *t, uint32_t n) { arp_step(t, n, n); }

/* -------------------------------------------------------- note input --- */
/* the length of step idx in samples: SWING (the track's + the global, at most 100) makes the even steps longer
 * and the odd ones shorter, so every odd step starts late */
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx)
{
    return swing_step_len(t, period, idx);   /* core.h: own + global, at most 100 */
}

/* (Yehnah) SHIFT OFS: the step whose notes play at place idx, the sequence OFS steps later (left with OFS < 0,
 * wrapping inside LEN). Live recording writes there too, so a note recorded plays where it was heard */
static uint32_t seq_src(const track_t *t, uint32_t idx)
{
    int32_t len = t->p[P_SLEN] > 0 ? t->p[P_SLEN] : 1, o = t->p[P_SOFS] % len;
    return (uint32_t)(((int32_t)(idx % (uint32_t)len) - o + len) % len);
}

/* (0.8.5) a recorded hit's octave (dx_in_rat / dx_in_tch: the key's or MIDI note's, eng_drum.c) as its lane's mode on
 * the step (core.h LM_*): x2 / x4 / x8 above the kit's octave, SHORT / GHOST / FLAM below, none in it; that lane only
 * (its hit and its notes), the rest of the step as it was. The last hit recorded on a lane sets it */
static uint32_t rec_lmode(void)
{
    if (dx_in_tch)
        return LM_SHORT + (dx_in_tch - 1u) % 3u;
    return dx_in_rat >= 8u ? LM_X8 : dx_in_rat >= 4u ? LM_X4 : dx_in_rat >= 2u ? LM_X2 : LM_NONE;
}
/* a hit (a GM note) into step s of a DRUM track (rec_note, a held ratchet's next step: dxo_rec_hold) */
static void rec_drum(track_t *t, step_t *s, uint32_t note, uint32_t vel, int32_t mic)
{
    uint32_t l = drum_lane(note), k;
    if (s->time != ST_NOTE || (!s->n && !s->hit)) {
        s->n = s->hit = s->acc = 0;
        s->flags = 0;
        s->vel = 0;
        ST_MICRO_SET(s, mic);                           /* (0.6.88) its place off the grid (REC Q) */
        if (s->comp == CP_TOUCH)                        /* (0.8.5: an empty step's TOUCH left over: not this hit's) */
            s->comp = s->carg = 0;
        if (s->comp2 == CP_TOUCH)
            s->comp2 = s->carg2 = 0;
        s->lm[0] = s->lm[1] = s->lm[2] = 0;
    }
    s->time = ST_NOTE;
    if (note == DRUM_LANE_NOTE[l]) {                    /* a lane's note: its hit */
        s->hit |= (uint8_t)(1u << l);
        if (vel > 110)
            s->acc |= (uint8_t)(1u << l);
    } else {                                            /* another GM drum (a low tom, a crash): a note */
        for (k = 0; k < s->n && s->note[k] != note; k++)
            ;
        if (k == s->n) {
            if (s->n < 4u)
                s->n++;
            s->note[s->n - 1u] = (uint8_t)note;
        }
        if (vel > 110)
            s->flags |= SF_ACCENT;
    }
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    step_set_lmode(s, l, rec_lmode());
    t->seq_active = 1;
}
/* (chord-harp) recording over a HARP chord held, or latched (hh_*): the chord goes on under the notes played over it
 * (the plate), in the step model: a step whose notes change is the chord again with the notes held over it, the step
 * before it sliding into it (SF_SLIDE: seq_step harp_tie keeps the chord's voices, starts and ends the others); the steps
 * where nothing changes TIEs. Up to the step it was struck in, round the pattern */
static int hh_hold(const track_t *t, uint32_t ti)   /* the hold is the chord's (a hold of other notes since: not) */
{
    uint32_t k;
    if (!hh_id[ti])
        return 0;
    for (k = 0; k < t->rh_n && t->rh_note[k] != hh_root[ti]; k++)
        ;
    if (k == t->rh_n)
        hh_id[ti] = 0;
    return hh_id[ti] != 0;
}
static int hh_mine(const step_t *s, uint32_t ti)   /* step s is the chord's: struck there, or a step of its hold */
{
    return s->time == ST_NOTE && s->n && s->note[0] == hh_root[ti] &&
           ((s->comp == CP_CHORD && s->carg == hh_id[ti] - 1u) || (s->comp2 == CP_CHORD && s->carg2 == hh_id[ti] - 1u));
}
static void hh_add(track_t *t, step_t *s, uint32_t ti)   /* the notes held over the chord into its step s */
{
    uint32_t i, k;
    for (i = 0; i < t->rh_n; i++) {
        for (k = 0; k < s->n && s->note[k] != t->rh_note[i]; k++)
            ;
        if (k == s->n && s->n < 4u)
            s->note[s->n++] = t->rh_note[i];
    }
    hh_pnn[ti] = (uint8_t)(s->n - 1u);
    for (i = 1; i < s->n; i++)
        hh_pn[ti][i - 1u] = s->note[i];
}
static void hh_step(track_t *t, step_t *s, uint32_t ti, uint32_t idx)   /* step idx the chord with the held notes */
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
    if (!hh_mine(s, ti)) {
        s->note[0] = hh_root[ti];
        s->n = 1;
        s->time = ST_NOTE;
        s->flags &= SF_MFINE;
        s->vel = 100;
        s->hit = s->acc = 0;
        s->lm[0] = s->lm[1] = s->lm[2] = 0;
        step_chord_set(s, hh_id[ti] - 1u);
    }
    hh_add(t, s, ti);
    if (idx != hh_org[ti] || hh_span[ti])           /* (the step before it the chord's too: it slides into this one) */
        t->step[seq_src(t, (idx + len - 1u) % len)].flags |= SF_SLIDE;
    hh_last[ti] = (uint8_t)idx;
    t->seq_active = 1;
}
/* rec_release: the chord's key let go: the hold ends, the notes held over it with it; 1: LATCH on, it goes on sounding,
 * so it goes on recording (till the next chord or LATCH off: latch_release, harp_unlatch) */
static int key_latch(const track_t *t);
static __attribute__((noinline)) int harp_release(track_t *t, uint32_t note)
{
    uint32_t ti = trk_index(t) % NTRK;
    if (hh_id[ti] && note == hh_root[ti]) {
        if (key_latch(t))
            return 1;
        hh_id[ti] = 0;
        t->rh_note[0] = (uint8_t)note;
        t->rh_n = 1;
    }
    return 0;
}
static __attribute__((noinline)) int harp_round(const track_t *t) { return hh_id[trk_index(t) % NTRK] != 0; }
/* rec_note: note played over the chord held, into step s (idx; next: the one after the playing one, it sounds now) */
static __attribute__((noinline)) int harp_over(track_t *t, step_t *s, uint32_t idx, uint32_t next, uint32_t note)
{
    uint32_t ti = trk_index(t) % NTRK, k;
    if (!hh_hold(t, ti) || note == hh_root[ti])
        return 0;
    for (k = 0; k < t->rh_n && t->rh_note[k] != note; k++)
        ;
    if (k == t->rh_n && t->rh_n < 4u)
        t->rh_note[t->rh_n++] = (uint8_t)note;
    hh_step(t, s, ti, idx);
    if (next) {                                     /* (it all sounds now: the step must not strike it again) */
        if (t->rskip_idx != idx)
            t->rskip_n = 0;
        t->rskip_idx = (uint8_t)idx;
        for (k = 0; k < s->n && t->rskip_n < NLANE; k++)
            t->rskip[t->rskip_n++] = s->note[k];
    }
    t->rh_start = (uint8_t)idx;
    t->rh_last = 0xFFu;                             /* (an early release puts no step back: this one is played) */
    return 1;
}
/* rec_hold: the sequencer enters step idx with the chord held: a TIE (0, rec_hold's) when the same notes go on, else the
 * chord with the notes held now (1) */
static __attribute__((noinline)) int harp_hold(track_t *t, uint32_t idx, uint32_t len)
{
    uint32_t ti = trk_index(t) % NTRK, i, k, np = 0, same;
    step_t *s = &t->step[seq_src(t, idx)];
    (void)len;
    if (!hh_hold(t, ti))
        return 0;
    if (idx == hh_org[ti]) {                        /* round to where it was struck: it ends there (as a note held */
        hh_id[ti] = 0;                              /* over the whole pattern; the steps after it, a later take's) */
        t->rh_n = 0;
        return 1;
    }
    if (hh_span[ti] < 255u)
        hh_span[ti]++;
    t->rh_start = (uint8_t)idx;                     /* (the hold's step now: round at the one struck, not passed by) */
    for (i = 0; i < t->rh_n; i++)                   /* the notes held over the chord now, the same as its step sounds? */
        if (t->rh_note[i] != hh_root[ti]) {
            for (k = 0; k < hh_pnn[ti] && hh_pn[ti][k] != t->rh_note[i]; k++)
                ;
            np++;
            if (k == hh_pnn[ti])
                np += 8u;
        }
    same = np == hh_pnn[ti];
    if (same && !hh_mine(s, ti)) {
        hh_last[ti] = (uint8_t)idx;
        return 0;
    }
    t->rh_bak = *s;
    t->rh_last = (uint8_t)seq_src(t, idx);
    t->rh_ties++;
    hh_step(t, s, ti, idx);
    return 1;
}
static uint8_t rec_at, rec_late;                        /* (0.8.5) the step (play index + 1) the last hit went into;
                                                         * 1: the next one (it was played late in the one before) */
/* live recording: the note goes into the nearest step, as swung (the one playing, or
 * the next one when it is past the middle of the playing one). Overdub: a step that
 * holds notes gets this one added (a chord of up to 4; when full, the last note is
 * replaced); MONO / LEGATO / UNISON parts keep one note per step, as step entry does.
 * Held on (synth parts): each further step the sequencer enters while the note is
 * held becomes a TIE (rec_hold), up to the pattern length; a release before the middle
 * of the last one puts that step back (rec_release), so a short note stays one step.
 * A note recorded into another step ends the hold before (the step model ties the
 * notes of one step only). */
static step_t *rec_note(track_t *t, uint32_t note, uint32_t vel)   /* (chord-harp: the step it went into) */
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, idx = t->seq_idx % len, k;
    uint32_t period = div_samples((uint32_t)t->p[P_SDIV]), sl = step_samples(t, period, t->seq_idx);
    int32_t gp = (int32_t)t->seq_pos - (int32_t)t->ext;   /* (0.6.88: from the step's place; an early one began before) */
    uint32_t next = t->seq_pos < 0x7FFFFFFFu && gp > (int32_t)(sl / 2u);
    int32_t mic = 0;
    step_t *s;
    if (rp_state == RP_PRE && rp_trk == trk_index(t) + 1u)
        next = 1;                                   /* (chord-harp fork) ONE PASS: what is played just before it, step 1's */
    if (next)
        idx = (idx + 1u) % len;
    s = &t->step[seq_src(t, idx)];
    if (t->seq_pos < 0x7FFFFFFFu && t->p[P_HRQ] < 127) {   /* (0.6.88) REC Q under 100 %: where it was played, kept */
        int32_t off = next ? gp - (int32_t)sl : gp, l = (int32_t)(next ? step_samples(t, period, idx) : sl);
        int32_t q = 127 - clamp(t->p[P_HRQ], 0, 127);
        mic = (off * 64 + (off < 0 ? -l / 2 : l / 2)) / (l ? l : 1);   /* (0.9.1: 64ths of the step, rounded; was 16ths) */
        mic = clamp((mic * q + (mic < 0 ? -63 : 63)) / 127, MICRO_MIN, MICRO_MAX);   /* (REC Q's share of it, rounded) */
    }
    if (drum_track(t)) {                            /* DRUM: into the grid, no holds (hits) */
        rec_drum(t, s, note, vel, mic);
        rec_at = (uint8_t)(idx + 1u);
        rec_late = (uint8_t)next;
        if (next) {
            if (t->rskip_idx != idx)
                t->rskip_n = 0;
            t->rskip_idx = (uint8_t)idx;
            if (t->rskip_n < NLANE)
                t->rskip[t->rskip_n++] = (uint8_t)note;
        }
        return s;
    }
    if (harp_over(t, s, idx, next, note))           /* (chord-harp) a note over a HARP chord held: the chord goes on */
        return s;
    if (s->time != ST_NOTE || !s->n || !voice_poly((int)trk_vmode(t))) {
        s->n = 0;                                   /* a fresh step */
        s->flags = 0;
        s->vel = 0;
        ST_MICRO_SET(s, mic);                       /* (0.6.88) its place off the grid (REC Q) */
        if (s->comp == CP_CHORD)                    /* (chord-harp) a chord left over: not this note's */
            s->comp = s->carg = 0;
        if (s->comp2 == CP_CHORD)
            s->comp2 = s->carg2 = 0;
    }
    for (k = 0; k < s->n && s->note[k] != note; k++)
        ;
    if (k == s->n) {
        if (s->n < 4u)
            s->n++;
        s->note[s->n - 1u] = (uint8_t)note;
    }
    s->time = ST_NOTE;
    if (vel > 110)
        s->flags |= SF_ACCENT;
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    t->seq_active = 1;
    if (next) {                                     /* it sounds now: the step must not trigger it again */
        if (t->rskip_idx != idx)
            t->rskip_n = 0;
        t->rskip_idx = (uint8_t)idx;
        if (t->rskip_n < 4u)
            t->rskip[t->rskip_n++] = (uint8_t)note;
    }
    if (!t->rh_n || t->rh_start != idx) {           /* a new hold (one in another step ends) */
        t->rh_n = 0;
        t->rh_start = (uint8_t)idx;
        t->rh_ties = 0;
    }
    for (k = 0; k < t->rh_n && t->rh_note[k] != note; k++)
        ;
    if (k == t->rh_n && t->rh_n < 4u)
        t->rh_note[t->rh_n++] = (uint8_t)note;      /* a chord: held until its last key is up */
    return s;
}

/* the sequencer enters step idx (before playing it): a recorded note still held ties into it. (chord-harp: not inlined,
 * out of mix_block's RAM code) */
/* (chord-harp fork) REC > ONE PASS: the sequencer enters step idx of track t: in the last step before step 1 a waiting
 * pass arms (RP_PRE), at step 1 it records (RP_REC) */
static __attribute__((noinline)) void rec_pass_step(track_t *t, uint32_t idx, uint32_t len)
{
    if (!rp_state || rp_trk != trk_index(t) + 1u)
        return;
    if (rp_state == RP_WAIT && idx + 1u >= len) {
        rp_state = RP_PRE;
        song.rec |= (uint8_t)(1u << trk_index(t));
    } else if (rp_state == RP_PRE && !idx) {
        rp_state = RP_REC;
    }
}
static int rec_pass_end(track_t *t, uint32_t len);   /* (below, after the clock) */
static int rec_cin_end(track_t *t, uint32_t len);    /* (below) */
static __attribute__((noinline)) void rec_hold(track_t *t, uint32_t idx, uint32_t len)
{
    step_t *s;
    uint32_t k;
    rec_pass_step(t, idx, len);                     /* (chord-harp fork) REC > ONE PASS */
    if (!t->rh_n)
        return;
    if (!((song.rec >> trk_index(t)) & 1u) || (t->rh_ties + 1u >= len && !harp_round(t))) {
        t->rh_n = 0;                                /* disarmed, or the whole pattern is this note (chord-harp: a HARP
                                                     * chord held goes round, harp_hold) */
        return;
    }
    if (idx == t->rh_start)
        return;                                     /* (recorded ahead into the step now starting) */
    if (harp_hold(t, idx, len))
        return;
    s = &t->step[seq_src(t, idx)];
    t->rh_bak = *s;
    t->rh_last = (uint8_t)seq_src(t, idx);
    t->rh_ties++;
    for (k = 0; k < 4u; k++)
        s->note[k] = 0;
    s->n = 0;
    s->time = ST_TIE;
    s->flags &= SF_MFINE;                           /* (0.9.1: MICRO kept whole, as its sixteenth in cwhen) */
    s->vel = 0;
    s->hit = s->acc = 0;
    s->lm[0] = s->lm[1] = s->lm[2] = 0;
}

/* a key of a recorded note is up: the hold ends with the last one */
static void rec_release(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    if (harp_release(t, note))                      /* (chord-harp) a HARP chord let go: the notes over it with it */
        return;
    for (i = 0; i < t->rh_n; i++)
        if (t->rh_note[i] != note)
            t->rh_note[k++] = t->rh_note[i];
    if (k == t->rh_n || (t->rh_n = (uint8_t)k))
        return;                                     /* not one of them, or others still held */
    if (t->rh_ties && seq_src(t, t->seq_idx) == t->rh_last &&
        t->seq_pos < step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / 2u)
        t->step[t->rh_last] = t->rh_bak;            /* released early in it: not held into this step */
}

/* a note into armed track t in the count-in's last eighth: kept for step 1 (cin_flush); once each, 4 per track */
static __attribute__((noinline)) void cin_keep(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t i = trk_index(t), k;
    if (cin_left != 1u || cin_pos < click_beat_len() / 2u || !((song.rec >> i) & 1u) || t->p[P_AMODE])
        return;
    for (k = 0; k < cin_n[i] && cin_note[i][k] != note; k++)
        ;
    if (k == cin_n[i]) {
        if (k == 4u)
            return;
        cin_n[i]++;
    }
    cin_note[i][k] = (uint8_t)note;
    cin_vel[i][k] = (uint8_t)vel;
    cin_oct[i][k] = (uint8_t)((dx_in_rat & 15u) | (dx_in_tch & 3u) << 4);
    cin_up[i] &= (uint8_t)~(1u << k);
}

static uint8_t in_norec;                            /* (0.6.84) input_on plays without recording (key_next) */
static void input_on(track_t *t, uint32_t note, uint32_t vel)
{
    if (bl_is_b(t))                                 /* (chord-harp fork) a blend's B: its keys and MIDI play A (both) */
        t = &trk[bl_a - 1u];
    last_note = (uint8_t)note;
    if (cin_left)
        cin_keep(t, note, vel);
    if (t->p[P_AMODE] == AM_TRNS) {                 /* ARP TRNS: the key transposes the sequence */
        t->trn = (int8_t)clamp((int32_t)note - 60, -36, 36);
        return;
    }
    if (rec_on(t) && !t->p[P_AMODE] && !in_norec)  /* (ARP on: arp_tick records its notes; 0.6.84: a key's chord was
                                                     * recorded whole when the key went down) */
        rec_note(t, note, vel);
    if (t->p[P_AMODE])
        arp_add(t, note);
    else
        trk_note_on(t, note, vel);
}

static int midi_note_held(const track_t *t, uint32_t note);
static int midi_local_held(const track_t *t, uint32_t note);
static void input_off(track_t *t, uint32_t note)
{
    if (bl_is_b(t))
        t = &trk[bl_a - 1u];
    if (midi_note_held(t, note))
        return;
    if (cin_left) {                                 /* a kept note let go before step 1: recorded, then let go */
        uint32_t i = trk_index(t), k;
        for (k = 0; k < cin_n[i]; k++)
            if (cin_note[i][k] == note)
                cin_up[i] |= (uint8_t)(1u << k);
    }
    rec_release(t, note);
    arp_remove(t, note);                            /* both: the note may have started in the */
    trk_note_off(t, note);                          /* other mode (ARP switched while held) */
}

/* ------------------------------------------- (0.8.5) the kit's octaves --- */
/* A DRUM track's note in another octave than the kit's (eng_drum.c DXO_*): a key at OCT +-n (kb_oct), a MIDI note of a
 * band around the GM map (drum_oct_of). Below: the hit with TOUCH n (dx_in_tch on its note-on: SHORT GHOST FLAM).
 * Above: the hit, then again x2 / x4 / x8 a step while held (dxo_tick): playing, on the grid the sequencer's lane
 * ratchets use (a part every step / n from each step's start, swung as the steps are: seq_lane_rat), stopped, every
 * step / n from the press. Recording, playing (rec_on), the step it goes into gets the hit and its lane's mode
 * (rec_lmode: that lane only); every step entered while it is held too, before it plays (dxo_rec_hold: the sequencer
 * plays those, the held note is silent there), so the pass after plays what was heard; let go before the middle of
 * such a step, that step is put back (dxo_bak), as a held synth note's TIE. The ARP on: the notes go to it plain. Each
 * held one: its key or MIDI note (id), up to DXO_N a track (one more replaces the last) */
#define DXO_N 4u
#define DXO_KEY 0x100u                                  /* id: a key (+ k), a MIDI note (+ ch << 7 + note) */
#define DXO_MIDI 0x1000u
typedef struct {
    uint16_t id, idx;                                   /* who holds it; the step (play index) part counts in */
    uint8_t note, vel, r, part;                         /* the hit, its hits a step, the part struck last (255 none) */
    uint8_t quiet, rec;                                 /* a step (+ 1) it is silent in (the sequencer plays it); the
                                                         * step (+ 1) its press was recorded into */
    uint32_t cnt;                                       /* stopped: samples since its last hit */
} dxo_t;
static dxo_t dxo[NPART][DXO_N];
static uint8_t dxo_n[NPART];
static step_t dxo_bak[NPART];                           /* the step a held one was last recorded into, as it was */
static uint8_t dxo_bak_at[NPART];                       /* .. which (its source step + 1; 0 none), entered at play idx */
static uint16_t dxo_bak_idx[NPART];
static uint32_t dxo_part(const track_t *t, uint32_t r)  /* the part of the playing step it is in */
{
    uint32_t sl = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / (r ? r : 1u), p;
    p = t->seq_pos / (sl ? sl : 1u);
    return p < r ? p : r - 1u;
}
static void dxo_on(track_t *t, uint32_t note, int32_t o, uint32_t vel, uint32_t id)
{
    uint32_t ti = trk_index(t), k;
    dxo_t *e;
    if (!o || t->p[P_AMODE] || ti >= NPART || !drum_track(t)) {
        input_on(t, note, vel);
        return;
    }
    if (o < 0) {
        dx_in_tch = (uint8_t)(o < -3 ? 3 : -o);
        input_on(t, note, vel);
        dx_in_tch = 0;
        return;
    }
    dx_in_rat = DXO_RAT[o > 3 ? 3 : o];
    rec_at = 0;
    input_on(t, note, vel);                             /* (its first hit; recorded with the RATCH) */
    for (k = 0; k < dxo_n[ti] && dxo[ti][k].id != id; k++)
        ;
    if (k == dxo_n[ti] && dxo_n[ti] < DXO_N)
        dxo_n[ti]++;
    e = &dxo[ti][k < DXO_N ? k : DXO_N - 1u];
    e->id = (uint16_t)id;
    e->note = (uint8_t)note;
    e->vel = (uint8_t)vel;
    e->r = dx_in_rat;
    e->cnt = 0;
    e->idx = t->seq_idx;
    e->part = song.playing && t->seq_pos < 0x7FFFFFFFu ? (uint8_t)dxo_part(t, e->r) : 255u;
    e->quiet = e->rec = 0;
    if (cin_left)                                       /* (the count-in keeps it for step 1) */
        e->rec = 1;
    else if (rec_at && rec_on(t)) {
        e->rec = rec_at;
        if (rec_late)                                   /* into the next step: silent to its end, as it plays back */
            e->quiet = (uint8_t)(t->seq_idx + 1u);
    }
    dx_in_rat = 0;
}
static void dxo_off(track_t *t, uint32_t id)
{
    uint32_t ti = trk_index(t), k, j = 0;
    if (ti >= NPART || !dxo_n[ti])
        return;
    for (k = 0; k < dxo_n[ti]; k++)
        if (dxo[ti][k].id != id)
            dxo[ti][j++] = dxo[ti][k];
    if (j == dxo_n[ti])
        return;
    dxo_n[ti] = (uint8_t)j;
    if (!j && dxo_bak_at[ti]) {                         /* let go early in a step it was held into: that step back */
        if (rec_on(t) && dxo_bak_idx[ti] == t->seq_idx && seq_src(t, t->seq_idx) == dxo_bak_at[ti] - 1u &&
            t->seq_pos < step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / 2u)
            t->step[dxo_bak_at[ti] - 1u] = dxo_bak[ti];
        dxo_bak_at[ti] = 0;
    }
}
/* the sequencer enters step idx of track t (before it plays it): recording, the held ones go into it */
static __attribute__((noinline)) void dxo_rec_hold(track_t *t, uint32_t idx)
{
    uint32_t ti = trk_index(t), k, src = seq_src(t, idx), bak = 0;
    for (k = 0; k < dxo_n[ti]; k++) {
        dxo_t *e = &dxo[ti][k];
        if (e->rec == idx + 1u) {                       /* (its press went into this one: it plays it) */
            e->quiet = (uint8_t)(idx + 1u);
            continue;
        }
        if (!rec_on(t) || !drum_track(t))
            continue;
        if (!bak) {
            dxo_bak[ti] = t->step[src];
            dxo_bak_at[ti] = (uint8_t)(src + 1u);
            dxo_bak_idx[ti] = (uint16_t)idx;
            bak = 1;
        }
        dx_in_rat = e->r;
        rec_drum(t, &t->step[src], e->note, e->vel, 0);
        dx_in_rat = 0;
        e->quiet = (uint8_t)(idx + 1u);
    }
}
static void dxo_hit(track_t *t, const dxo_t *e) { if (!((chain.cut >> (trk_index(t) % NTRK)) & 1u)) trk_note_on(t, e->note, e->vel); }
/* a block on (events_block, after the sequencer): the held ones' next hits */
static __attribute__((noinline)) void dxo_tick(track_t *t, uint32_t n)
{
    uint32_t ti = trk_index(t), k;
    if (!drum_track(t) || t->p[P_AMODE]) {
        dxo_n[ti] = 0;
        return;
    }
    for (k = 0; k < dxo_n[ti]; k++) {
        dxo_t *e = &dxo[ti][k];
        uint32_t r = e->r ? e->r : 1u, part;
        if (!song.playing || t->seq_pos >= 0x7FFFFFFFu) {   /* stopped: from the press */
            uint32_t per = div_samples((uint32_t)t->p[P_SDIV]) / r;
            e->part = 255u;
            e->idx = 0xFFFFu;
            e->cnt += n;
            if (per && e->cnt >= per) {
                e->cnt -= per;
                if (e->cnt >= per)
                    e->cnt = 0;
                dxo_hit(t, e);
            }
            continue;
        }
        e->cnt = 0;
        part = dxo_part(t, r);
        if (t->seq_idx != e->idx) {
            e->idx = t->seq_idx;
            e->part = 255u;
        }
        if (e->quiet == t->seq_idx + 1u) {
            e->part = (uint8_t)part;
            continue;
        }
        if (part != e->part) {
            e->part = (uint8_t)part;
            dxo_hit(t, e);
        }
    }
}

/* the effect of key k in the FX layer (PF_N none): the first PF_M1 white keys (F3 .. A4) in order, black keys
 * 1..4 (F#3 G#3 A#3 C#4) the track mutes */
static uint32_t perf_key(uint32_t k)
{
    uint32_t p = key_place(k);
    return !key_black(k) ? (p < PF_M1 ? p : p == PF_M1 ? PF_CLR : PF_N) : p < NTRK ? PF_M1 + p : p == 7u ? PF_TGT : PF_N;
}

/* (0.6.14) LATCH: ARP OFF and ARP 2's HOLD ON, the keys' notes keep sounding when let go; the first key of the next
 * chord (no key down) lets the latched ones go. In the arp's note list (held / nheld / arp_phys, unused with ARP OFF);
 * HOLD turned off lets them go (events_block) */
static int key_latch(const track_t *t) { return t->p[P_AHOLD] && !t->p[P_AMODE]; }
static __attribute__((noinline)) int key_latch_k(const track_t *t, uint32_t k)   /* (chord-harp) HARP's plate never latches */
{
    return key_latch(t) && !(harp_on(t) && !harp_ck(t, k));
}
/* (chord-harp) the latched chord let go (the next chord, LATCH off): its recording hold ends as a key let go would */
static __attribute__((noinline)) void harp_unlatch(track_t *t)
{
    uint32_t ti = trk_index(t) % NTRK, r = hh_root[ti];
    if (!hh_id[ti])
        return;
    hh_id[ti] = 0;
    if (t->rh_n) {
        t->rh_note[0] = (uint8_t)r;
        t->rh_n = 1;
        rec_release(t, r);
    }
}
static void latch_release(track_t *t)
{
    uint32_t i;
    harp_unlatch(t);
    for (i = 0; i < t->nheld; i++) {
        rec_release(t, t->held[i]);                 /* (its recording hold ends here: key_off kept it) */
        trk_note_off(t, t->held[i]);
        mo_key(t, 0x08u | (0x80u | trk_midi_ch(trk_index(t))) << 8 | (uint32_t)t->held[i] << 16);
    }
    t->nheld = 0;
}

/* (0.6.19) the UI (main loop) lets a track's held / latched notes go through here, in events_block: SEQ TOOLS CLEAR,
 * REC CLEAR ALL (they zeroed the list from the main loop: the latched voices kept sounding, nothing left to end them) */
static volatile uint8_t latch_drop_req;
static uint8_t latch_was;                           /* bit i: track i was in LATCH last block */
static void latch_drop(track_t *t)
{
    latch_release(t);                               /* (their voices and MIDI OUT's notes end) */
    pfx_hush(t);                                    /* (0.6.28) the punch-in repeats end and forget the notes struck */
    t->pfx_lnn = 0;                                 /* (STUTTER / ARP replayed the cleared steps' last notes) */
    t->arp_phys = 0;
    if (t->arp_note)
        arp_silence(t);
}

/* key k plays kb_note[k] on track t. A note another key holds already sounds: it is not started again (nor sent
 * to MIDI OUT); the key keeps its note */
/* (0.6.81) the next note of key k's chord in (a note another key holds already sounds: not started again) */
static void key_next(uint32_t k, track_t *t)
{
    uint32_t x = kb_snd[k][kb_on[k]], mc = trk_midi_ch(trk_index(t));
    if (!midi_local_held(t, x)) {
        in_norec = kb_sn[k] > 1u || (harp_on(t) && harp_idk(t, k));   /* (0.6.84: the chord's notes recorded at once,
                                                     * key_on; chord-harp: a HARP chord's, its root too, key_rec) */
        dxo_on(t, x, kb_oct[k], 100, DXO_KEY + k);  /* (0.8.5: a kit's octave; else input_on) */
        in_norec = 0;
        mo_key(t, 0x09u | (0x90u | mc) << 8 | drum_oct_note(x, kb_oct[k]) << 16 | 100u << 24);
    }
    kb_on[k]++;
}
/* (chord-harp) a HARP chord key recorded: the step its root alone with the chord as its CHORD (step_notes plays it
 * whole; a plate note recorded into the step after it goes over it), what the step held before replaced. Step entry
 * too (ui_input.c seq_entry) */
static __attribute__((noinline)) void harp_step(const track_t *t, step_t *s, uint32_t k)
{
    s->note[0] = (uint8_t)harp_root(t, k);
    s->n = 1;
    s->time = ST_NOTE;
    step_chord_set(s, harp_id(t, k));
}
/* (0.6.84) recording: the whole chord into one step, where the key went down (the strum's later notes would fall into
 * the next step); the sequencer strums it back as it plays (seq_step). (chord-harp) A HARP chord: its CHORD, and
 * while it is held the notes played over it record over the chord (hh_*, harp_over) */
static __attribute__((noinline)) void key_rec(track_t *t, uint32_t k, uint32_t n, int hp)
{
    uint32_t i, ti = trk_index(t) % NTRK;
    if (hp && harp_idk(t, k)) {                     /* (a J-6 chord: its notes, as a CHORD mode's) */
        uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
        step_t *s, *b;
        hh_id[ti] = 0;                              /* (a chord before it held: not this one's) */
        s = rec_note(t, harp_root(t, k), 100);
        harp_step(t, s, k);
        s->flags &= (uint8_t)~SF_SLIDE;             /* (struck: no slide on from an earlier take) */
        b = &t->step[seq_src(t, (t->rh_start + len - 1u) % len)];
        if (b->time == ST_TIE || b->comp == CP_CHORD || b->comp2 == CP_CHORD)
            b->flags &= (uint8_t)~SF_SLIDE;         /* (nor into it from a held chord before) */
        if (voice_poly((int)trk_vmode(t))) {               /* held, the notes over it record over the chord (harp_over) */
            hh_id[ti] = (uint8_t)(s->comp == CP_CHORD ? s->carg + 1u : s->carg2 + 1u);
            hh_root[ti] = s->note[0];
            hh_org[ti] = hh_last[ti] = t->rh_start;
            hh_span[ti] = hh_pnn[ti] = 0;
        }
    } else
        for (i = 0; i < n; i++)
            rec_note(t, kb_snd[k][i], 100);
}
#if FELUCCA_AMBIENT
static void amb_key(const track_t *t, const uint8_t *nt, uint32_t n, int on);   /* (amb.c) */
#else
#define amb_key(t, nt, n, on) ((void)0)
#endif
static __attribute__((noinline)) void key_on(uint32_t k, track_t *t)   /* (chord-harp: not inlined, out of mix_block's
                                                                         * RAM code: a key going down, not every block) */
{
    int hp = harp_on(t);
    uint32_t n = hp && harp_ck(t, k) ? harp_build(t, k, kb_snd[k]) : hp ? (kb_snd[k][0] = kb_note[k], 1u) :   /* (a plate
                                                     * key: its note; CHORD's too) */
                 chord_build(t, kb_note[k], kb_snd[k]);
    uint32_t gap = n > 1u && t->p[P_AMODE] < AM_STRDN ? chord_gap(t) : 0u, i;   /* (2026-11-11: the strokes sweep it themselves; a key that
                                                     * delivered its notes over STRUM let the arp fire on the first, one string) */
    key_rpc[trk_index(t) % NTRK] = (uint8_t)((hp && harp_ck(t, k) ? harp_root(t, k) : hp ? kb_note[k] : (uint32_t)chord_r) %
                                             12u);          /* (chord-harp fork) what it plays, for SCL's circle: the
                                                     * chord's root (HARP's too), a single note (a plate key's) itself */
    if (key_latch_k(t, k)) {                             /* LATCH: a new chord lets the last one go */
        uint32_t j;
        if (!t->arp_phys)
            latch_release(t);
        t->arp_phys++;
        for (j = 0; j < n; j++) {
            for (i = 0; i < t->nheld && t->held[i] != kb_snd[k][j]; i++)
                ;
            if (i == t->nheld && t->nheld < 16u)
                t->held[t->nheld++] = kb_snd[k][j];
        }
    }
    kb_on[k] = 0;
    kb_sn[k] = (uint8_t)n;
    amb_key(t, kb_snd[k], n, 1);                    /* (ARP > AMBIENT's notes down) */
    if ((n > 1u || (hp && harp_idk(t, k))) && rec_on(t) && !t->p[P_AMODE])
        key_rec(t, k, n, hp);
    for (i = 0; i < n && (!gap || !i); i++)        /* (STRUM: the first now, the others by keys_strum) */
        key_next(k, t);
    kb_sdue[k] = (uint16_t)(gap < 65535u ? gap : 65535u);
    last_note = kb_note[k];                         /* (step entry, the SAMPLE zone: the key's note) */
}
/* (chord-harp) bit i: track i's HOLD was turned on by HARP's LATCH key; harp_was: track i was in HARP last block. A
 * track leaving HARP lets its LATCH go (harp_leave): HOLD off, as it was before LATCH, so the keys do not stay latched
 * outside HARP. A HOLD set on the ARP 2 page is the user's own and stays */
static uint8_t harp_hold_set, harp_was;
static __attribute__((noinline)) void harp_leave(track_t *t, uint32_t i)
{
    int on = harp_on(t);
    if (!on && ((harp_was >> i) & 1u) && ((harp_hold_set >> i) & 1u))
        t->p[P_AHOLD] = 0;                              /* (events_block then drops the latched chord: latch_was) */
    if (!on || !t->p[P_AHOLD])
        harp_hold_set &= (uint8_t)~(1u << i);
    harp_was = (uint8_t)(on ? harp_was | 1u << i : harp_was & ~(1u << i));
}
/* (chord-harp) HARP's LATCH key: ARP 2's HOLD on / off. On, the chord keys down now keep sounding when let go (as if
 * they went down latched); off, events_block lets the latched chord go */
static void harp_latch(track_t *t)
{
    uint32_t k, i, j, ti = trk_index(t);
    t->p[P_AHOLD] = (int16_t)!t->p[P_AHOLD];
    harp_hold_set = (uint8_t)(t->p[P_AHOLD] ? harp_hold_set | 1u << ti : harp_hold_set & ~(1u << ti));
    if (!key_latch(t))
        return;
    for (k = 0; k < 27u; k++) {
        if (!kb_sn[k] || kb_trk[k] != ti || !harp_ck(t, k))
            continue;
        t->arp_phys++;
        for (j = 0; j < kb_sn[k]; j++) {
            for (i = 0; i < t->nheld && t->held[i] != kb_snd[k][j]; i++)
                ;
            if (i == t->nheld && t->nheld < 16u)
                t->held[t->nheld++] = kb_snd[k][j];
        }
    }
}
/* (chord-harp fork) the pitch classes track t's keys sound now (bit n = n semitones above C; a chord's every note):
 * SCL's and CHORD's keyboards light them (ui_lzpage.c hk_kb12) */
static uint32_t play_pcs(const track_t *t)
{
    uint32_t k, j, m = 0, ti = trk_index(t);
    for (k = 0; k < 27u; k++)
        if (kb_sn[k] && kb_trk[k] == ti)
            for (j = 0; j < kb_sn[k] && j < 8u; j++)
                m |= 1u << kb_snd[k][j] % 12u;
    return m;
}
/* (chord-harp) the chord keys down on track t, bit k = key k (the screen) */
static uint32_t harp_held(const track_t *t)
{
    uint32_t k, m = 0, ti = trk_index(t);
    for (k = 0; k < 27u; k++)
        m |= (uint32_t)(kb_sn[k] && kb_trk[k] == ti && harp_ck(t, k)) << k;
    return m;
}
/* (chord-harp fork) what the sequencer plays on track t in a harp mode, as keys: *chords, the chord key whose CHORD
 * step plays (the id and the root's pitch class the step holds; none in CHORD's own modes: no id), and the return,
 * the plate keys whose note sounds (any note a step plays: the chord's tones too). The screen and the key LEDs show it
 * as a key held (ui_lzpage.c hk_keys, ui_input.c harp_leds) */
static uint32_t harp_playing(const track_t *t, uint32_t *chords)
{
    uint32_t ti = trk_index(t) % NTRK, k, i, plate = 0, c = hp_now[ti] & 0xFFu;
    *chords = 0;
    if (!harp_on(t) || !t->seq_n)
        return 0;
    for (k = 0; k < 27u; k++) {
        if (harp_ck(t, k)) {
            if (c && !harp_cm(t) && !harp_j6(t) && harp_id(t, k) + 1u == c &&
                harp_root(t, k) % 12u == (hp_now[ti] >> 8) % 12u)
                *chords |= 1u << k;
        } else if (!key_black(k)) {
            uint32_t n = harp_map(t, k);
            for (i = 0; n <= 127u && i < t->seq_n && i < sizeof t->seq_notes; i++)
                if (t->seq_notes[i] == n)
                    plate |= 1u << k;
        }
    }
    return plate;
}
/* (chord-harp) key k going down on track t in HARP: a chord key's root (the plate follows its chord from now), the
 * LATCH key toggles and is silent; else kb_map */
static __attribute__((noinline)) uint32_t harp_key(track_t *t, uint32_t k)
{
    if (!harp_on(t))
        return kb_map(t, k);
    if (harp_ck(t, k)) {
        harp_cur[trk_index(t) % NTRK] = (uint8_t)k;
        return harp_root(t, k);
    }
    if (k == harp_lk(t))
        harp_latch(t);
    return harp_map(t, k);
}

/* (0.6.81) STRUM: the keys' chords still coming in, a block on */
static void keys_strum(uint32_t n)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        if (kb_on[k] >= kb_sn[k] || !kb_on[k])
            continue;
        if (kb_sdue[k] > n) {
            kb_sdue[k] = (uint16_t)(kb_sdue[k] - n);
            continue;
        }
        key_next(k, &trk[kb_trk[k] % NTRK]);
        kb_sdue[k] = (uint16_t)clamp((int32_t)chord_gap(&trk[kb_trk[k] % NTRK]), 1, 65535);
    }
}

/* key k is up: the note it started ends, unless another key still holds it */
static void key_off(uint32_t k, track_t *t)
{
    uint32_t n = kb_on[k], i, mc = trk_midi_ch(trk_index(t));
    if (!n)
        return;
    if (kb_oct[k] > 0)
        dxo_off(t, DXO_KEY + k);                    /* (0.8.5) its ratchet ends (LATCH too) */
    kb_on[k] = 0;                                   /* (the strum's notes not in yet: never) */
    amb_key(t, kb_snd[k], kb_sn[k], 0);             /* (ARP > AMBIENT's notes down) */
    kb_sn[k] = 0;
    if (key_latch_k(t, k)) {                        /* LATCH: it keeps sounding, so it goes on recording as held (the
                                                     * user's, 2026-11-04: it recorded as let go; its hold ends with
                                                     * the latch: latch_release) */
        if (t->arp_phys)
            t->arp_phys--;
        return;
    }
    for (i = 0; i < n; i++) {
        uint32_t x = kb_snd[k][i];
        if (midi_local_held(t, x))
            continue;
        input_off(t, x);
        mo_key(t, 0x08u | (0x80u | mc) << 8 | drum_oct_note(x, kb_oct[k]) << 16);
    }
}

#ifdef FM1_INPUT_LAT
/* the press latency stats (hal/fm1_input.h fm1_in_stat, console `inp`): from the first scan that saw
 * key k closed to its note-on here, and to the note's first sample leaving the DMA (kb_out_tick:
 * audio.c, the TIMER4 tick at which the block being rendered starts to play) */
static uint32_t kb_out_tick;
static __attribute__((noinline)) void kb_lat(uint32_t k)
{
    uint32_t t0 = fm1_in.note_t0[k], d = fm1_ticks() - t0, o = kb_out_tick - t0;
    fm1_in_stat.kb_n++;
    fm1_in_stat.kb_sum += d;
    fm1_in_stat.dac_sum += o;
    if (d > fm1_in_stat.kb_max)
        fm1_in_stat.kb_max = d;
    if (o > fm1_in_stat.dac_max)
        fm1_in_stat.dac_max = o;
}
#endif

static void keyboard_block(void)
{
    uint32_t cur = fm1_in.notes, ch, k;
    uint32_t lay, fx;
    keys_strum(CTL);                              /* (0.6.81) the chords' strums */
    ch = cur ^ kb_prev;                           /* keys also sound while entering steps */
    if (!ch)
        return;
    if (kb_boot_hold) {                           /* #137: under the splash; their release is silent too */
        for (k = 0; k < 27u; k++)
            if ((ch & cur) >> k & 1u)
                kb_note[k] = KB_SILENT;
        kb_prev = cur;
        return;
    }
    lay = (fm1_in.buttons & kb_mask) != 0u || fx_lock_on;
    fx = (fm1_in.buttons & perf_mask) != 0u || fx_lock_on;   /* (0.6.44: the FX layer locked open) */
    for (k = 0; k < 27u; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {                    /* the selected track; the key-up goes to the same one */
            kb_trk[k] = bl_is_b(&trk[song.sel]) ? bl_a - 1u : song.sel;   /* (chord-harp fork) a blend's B: A's keys
                                                     * (its harp, chords, bend), so both tracks sound alike */
            if (kb_asleep) {                      /* (SCREEN OFF: the wake press is swallowed) */
                kb_note[k] = KB_SILENT;
                continue;
            }
            if (lay) {                            /* a layer's button held (or locked): the key is the layer's */
                kb_note[k] = KB_SILENT;           /* (ui_layer.c), with FX an effect (perform.c) */
                kb_layer |= 1u << k;
                if (fx)
                    perf_press(perf_key(k), 1);
                continue;
            }
            if (song.grid == 2u)                  /* NAME (ui_name.c): every key types, none sounds */
                kb_note[k] = KB_SILENT;
            else if (song.grid)                   /* the DRUM grid: a lane key plays its lane, the rest are the UI's */
                kb_note[k] = key_black(k) && key_place(k) < NLANE ? DRUM_LANE_NOTE[key_place(k)] : KB_SILENT;
            else if (harp_on(&trk[kb_trk[k]]) && harp_bend_k(k)) {
                kb_note[k] = KB_SILENT;           /* (chord-harp fork) a bend key: the track bent while it is held */
                kb_bend |= 1u << k;
                key_bend_goal[kb_trk[k] % NTRK] += harp_bend_k(k);
                continue;
            } else
                kb_note[k] = (uint8_t)harp_key(&trk[kb_trk[k]], k);   /* (chord-harp; else kb_map) */
            kb_oct[k] = (int8_t)(!song.grid && drum_track(&trk[kb_trk[k]]) ? song.octave : 0);   /* (0.8.5) */
            if (kb_note[k] == KB_SILENT)
                continue;
            key_on(k, &trk[kb_trk[k]]);
#ifdef FM1_INPUT_LAT
            kb_lat(k);
#endif
        } else {
            if ((kb_layer >> k) & 1u) {
                kb_layer &= ~(1u << k);
                perf_press(perf_key(k), 0);
                continue;
            }
            if ((kb_bend >> k) & 1u) {            /* (chord-harp fork) a bend key let go: back */
                kb_bend &= ~(1u << k);
                key_bend_goal[kb_trk[k] % NTRK] -= harp_bend_k(k);
                continue;
            }
            if (kb_note[k] == KB_SILENT)
                continue;
            key_off(k, &trk[kb_trk[k] % NTRK]);
        }
    }
    kb_prev = cur;
}

/* (0.6.62) a JUMP's or REV's TIMES: how often it has turned the track this pass (a step each; a pass ends where the
 * pattern wraps by itself, not by a jump) and the passes since PLAY (1:2) */
static uint8_t fl_cnt[NTRK][NSTEP], fl_pass[NTRK];
static void fl_reset(uint32_t ti) { memset(fl_cnt[ti % NTRK], 0, NSTEP); }
static int fl_ok(const track_t *t, uint32_t c, uint32_t a)   /* it turns the track now (and counts it) */
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK, k = t->seq_idx % NSTEP, m = CP_TIMES(c, a);
    if (m == 3u) {                                  /* 1:2: once, every other pass */
        if (fl_pass[ti] & 1u)
            return 0;
        m = 1u;
    }
    if (m && fl_cnt[ti][k] >= m)
        return 0;
    if (m)
        fl_cnt[ti][k]++;
    return 1;
}
/* -------------------------------------------------------- sequencer --- */
static void seq_start(void)
{
    uint32_t i;
    ms_clock = 0;                                  /* (Yehnah) the modulation sequences from their step 0 */
    if (mo_clock_on()) {                           /* (0.6.72) MIDI OUT CLOCK / ALL: START, the clock from here */
        mo_send(0xFAu, 0, 0);
        mo_pulse_acc = 0;
    }
    motion_begin();
    chain_start();
    for (i = 0; i < NTRK; i++) {                   /* every track from its step 0, together */
        track_t *t = &trk[i];
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFF;                   /* step 0 fires on the first block */
        t->ext = 0;
        t->rskip_n = 0;
        t->rh_n = 0;
        t->rat_left = 0;
        t->arp_from = 0; t->loops = 0;             /* (0.6.39) the components: the first loop, no arpeggio */
        t->tonal = 0; t->pulse_from = t->jump_to = t->rev = 0;   /* (0.6.42) no TONAL, PULSE, JUMP, REV */
        t->cpos = t->cv_on = 0;                    /* (0.6.47) C.LEN's lane from its first step */
        fl_reset(i), fl_pass[i] = 0;               /* (0.6.62) TIMES from the first pass */
    }
    song.tick = 0;
    beat_pos = 0; beat_n = 0;                      /* the ARP LED's beat from the top too */
    bar_n = 0;
    clk_pos = CLK_START;                           /* the metronome's first beat with step 0 */
    song.playing = 1;
    slicer_start();                                /* slicer.c: its step 0 with the sequencer's */
    perf_start();                                  /* perform.c: its 1/16 grid too */
}

/* (0.6.37) a step's chance as the track plays it: its own, scaled by SHIFT > CHANCE (P_TCHAN 0: as it is) */
static uint32_t trk_chance(const track_t *t, const step_t *s)
{
    return step_chance(s) * (127u - (uint32_t)clamp(t->p[P_TCHAN], 0, 127)) / 127u;
}
/* (0.6.92) the step's EVERY: its components play this loop of the pattern (ALWAYS; a of b loops; the first; not the
 * first; never) */
static int every_on(const track_t *t, const step_t *s)
{
    const uint8_t *e = EV_AB[ST_EVERY(s) % EV_N];
    if (!e[0])
        return e[1] == 0u || (e[1] == 1u ? t->loops == 0u : e[1] == 2u ? t->loops != 0u : 0);
    return t->loops % e[1] == e[0] - 1u;
}
/* (0.6.82) the step's component of a type lo..hi that plays this loop: the first one's, else the second's; 0 none
 * (*a: its arguments) */
static uint32_t cp_get(const track_t *t, const step_t *s, uint32_t lo, uint32_t hi, uint8_t *a)
{
    if (!every_on(t, s))
        return 0;
    if (s->comp >= lo && s->comp <= hi) {
        if (a) *a = s->carg;
        return s->comp;
    }
    if (s->comp2 >= lo && s->comp2 <= hi) {
        if (a) *a = s->carg2;
        return s->comp2;
    }
    return 0;
}
#define CP_HAS(t, s, c, a) cp_get(t, s, c, c, a)
/* (chord-harp) step s's CHORD this loop: its chord + 1 (set * 12 + its place), 0 none. Under C.LEN the playing step's
 * own too (a recorded chord is what the step plays, not one of the lane's changes) */
static uint32_t step_chord(const track_t *t, const step_t *s)
{
    uint8_t a = 0;
    if (!CP_HAS(t, s, CP_CHORD, &a) && !(s == &t->cvs && CP_HAS(t, &seq_steps(t)[seq_src(t, t->seq_idx)], CP_CHORD, &a)))
        return 0;
    return a % CHORD_NID + 1u;
}
/* (chord-harp) the notes step s plays into out (4 + NLANE), own[i]: which of the step's notes out[i] is (its skip bit).
 * A CHORD on a POLY part (not a kit): the chord whole from the step's first note, its root (each of them note 0), then
 * its other notes over it, any the chord sounds already left out; else the step's notes */
static uint32_t step_expand(const step_t *s, uint32_t c, uint8_t *out, uint8_t *own);
static __attribute__((noinline)) uint32_t step_notes(const track_t *t, const step_t *s, uint32_t kit, uint8_t *out,
                                                     uint8_t *own)
{
    uint32_t c = kit || !s->n || !voice_poly((int)trk_vmode(t)) ? 0u : step_chord(t, s), ti = trk_index(t) % NTRK;
    hp_was[ti] = hp_now[ti];
    hp_now[ti] = (uint16_t)(c ? c | (uint32_t)s->note[0] << 8 : 0u);
    return step_expand(s, c, out, own);
}
/* (chord-harp) step s's notes into out with chord c (+ 1; 0: none) whole from its first note (step_notes; the roll) */
static uint32_t step_chord_id(const step_t *s)      /* its CHORD + 1, as stored (0 none) */
{
    return s->comp == CP_CHORD ? s->carg % CHORD_NID + 1u :
           s->comp2 == CP_CHORD ? s->carg2 % CHORD_NID + 1u : 0u;
}
static uint32_t step_expand(const step_t *s, uint32_t c, uint8_t *out, uint8_t *own)
{
    uint32_t n = s->n < 4u ? s->n : 4u, i, j, m = 0;
    if (c && n) {
        uint8_t nt[HARP_NMAX];
        uint32_t cn = hc_chord(c - 1u, 0, nt);
        int32_t d = (int32_t)s->note[0] - hc_lo(c - 1u, 0);
        for (i = 0; i < cn; i++) {
            out[m] = (uint8_t)clamp((int32_t)nt[i] + d, 0, 127);
            own[m++] = 0;
        }
        for (i = 1; i < n; i++) {
            for (j = 0; j < m && out[j] != s->note[i]; j++)
                ;
            if (j == m) {
                out[m] = s->note[i];
                own[m++] = (uint8_t)i;
            }
        }
        return m;
    }
    for (i = 0; i < n; i++) {
        out[i] = s->note[i];
        own[i] = (uint8_t)i;
    }
    return n;
}
/* (chord-harp fork) note is one of a HARP chord's on t: a chord key's down now (kb_snd, its strum still to come
 * too), or the CHORD step playing (hp_now); for OMNI's AUTO (eng_omni.c), as its voice starts. 2: an OMNI chord's
 * bass (its first note), 1 another of its notes, 0 none */
static __attribute__((noinline)) int harp_chord_note(const track_t *t, uint32_t note)
{
    uint32_t ti = trk_index(t) % NTRK, k, j, c = hp_now[ti] & 0xFFu;
    for (k = 0; k < 27u; k++)
        if (kb_trk[k] == ti && harp_ck(t, k))
            for (j = 0; j < kb_sn[k]; j++)
                if (kb_snd[k][j] == note)
                    return !j && kb_sn[k] > 1u && harp_omni(t) ? 2 : 1;
    if (c) {
        uint8_t nt[HARP_NMAX];
        uint32_t cn = hc_chord(c - 1u, 0, nt);
        int32_t d = (int32_t)(hp_now[ti] >> 8) - hc_lo(c - 1u, 0);
        for (j = 0; j < cn; j++)
            if ((int32_t)nt[j] + d == (int32_t)note)
                return !j && c - 1u >= HARP_NID ? 2 : 1;
    }
    return 0;
}
/* (chord-harp) the name of chord id (0..239) as step s plays it, from its first note (the screen; buf: sz bytes) */
static void step_chord_name(const step_t *s, uint32_t id, char *buf, uint32_t sz)
{
    char b[16];
    id %= CHORD_NID;
    hc_name(id, s->n ? ((int32_t)s->note[0] - hc_lo(id, 0) + 120) % 12 : 0, b);
    str_cpy(buf, b, sz);
}
/* (0.6.39) a TRIG condition: this loop of the pattern is one it plays on */
static int cp_trig_cond(const track_t *t, uint32_t a)
{
    const uint8_t *c = CP_TRIGS[a % CP_NTRIG];
    if (!c[0])
        return c[1] == 1u ? t->loops == 0u : t->loops != 0u;   /* FIRST, NOT FIRST */
    return t->loops % c[1] == c[0] - 1u;
}
/* (0.6.98) WALK / CLIMB: the move this loop, in degrees (WALK) or octaves (CLIMB): the loops counted in RATEs, the
 * count round the RANGE: UP 0 1 .. R 0 ..; DOWN 0 -1 .. -R; UP-DN 0 1 .. R .. 1 0 ..; RND a place of 0..R a move, by
 * a hash of the move and the step (the same every time round, the pattern's own) */
static int32_t cp_walk_off(const track_t *t, const step_t *s, uint32_t a, int climb)
{
    uint32_t k = t->loops / CP_WRATES[CP_WRATE(a) % CP_NWRATE], m = CP_WMODE(a), g = CP_WRNG(a);
    int32_t r = climb ? (int32_t)g + 1 : (int32_t)CP_WDEGS[g];
    if (m == 0u) return (int32_t)(k % ((uint32_t)r + 1u));
    if (m == 1u) return -(int32_t)(k % ((uint32_t)r + 1u));
    if (m == 2u) { int32_t p = (int32_t)(k % (2u * (uint32_t)r)); return p <= r ? p : 2 * r - p; }
    { uint32_t h = (k * 2654435761u) ^ ((uint32_t)(s - seq_steps(t)) * 40503u); h ^= h >> 13; h *= 0x5BD1E995u; h ^= h >> 15; return (int32_t)(h % ((uint32_t)r + 1u)); }
}
/* every TRIG of the step that plays this loop lets it play (any other step: yes) */
static int cp_trig_ok(const track_t *t, const step_t *s)
{
    if (!every_on(t, s))
        return 1;
    if (s->comp == CP_TRIG && !cp_trig_cond(t, s->carg))
        return 0;
    return !(s->comp2 == CP_TRIG && !cp_trig_cond(t, s->carg2));
}
/* (0.6.42) FILL: the FX button held (ui_input.c sets it each pass) */
static volatile uint8_t seq_fill;
/* the step's one roll: its TRIGs (FILL: FX held), then its chance (the track's scaling it). (0.6.82) These come
 * first: a component that keeps the note silent this loop wins over the step's other one */
static int step_rolls(const track_t *t, const step_t *s)
{
    uint32_t c = trk_chance(t, s);
    if (!seq_fill && CP_HAS(t, s, CP_FILL, 0))
        return 0;
    return cp_trig_ok(t, s) && (c >= 100u || rng() % 100u < c);
}
/* (0.6.39) the step whose ARP component plays at s: s itself (a NOTE step with one), or the one a TIE continues
 * (arp_from); 0: none (a drum kit, the slices: never) */
static int trk_kit(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e->oneshot || (e->keys && e->keys(t, 0) >= 0);
}
/* (0.6.47) C.LEN (P_CLEN 1..64): the step s with the component of the lane's next step (steps 1..C.LEN in turn, one a
 * step the track plays, apart from where the steps are: the components drift over a pattern of another length);
 * OFF: s as it is. The copy is the track's (cvs: a RATCH repeats it); an ARP or a PULSE keeps theirs (CV_IDX) */
#define CV_IDX 255u
/* (0.6.88) TIMING early: how much before its place the step after the playing one starts (its ST_MICRO < 0), so the
 * playing one ends that much sooner (seq_tick); 0 when the next step is not known yet (a JUMP or REV coming, RANDOM
 * held, a song's section about to change) */
static uint32_t seq_early(const track_t *t, uint32_t period, uint32_t len)
{
    uint32_t ni = (t->seq_idx + 1u) % (len ? len : 1u);
    const step_t *s;
    int32_t m;
    if (t->jump_to || t->rev || (t->pfx & PFX_RND) || (!ni && (chain.running || chain.armed)))
        return 0;
    s = &seq_steps(t)[seq_src(t, ni)];
    if (s->time != ST_NOTE || (!s->n && !s->hit))       /* (nothing to play there: never early) */
        return 0;
    m = ST_MICRO(s);
    return m < 0 ? (uint32_t)-m * step_samples(t, period, ni) / 64u : 0u;   /* (0.9.1: 64ths) */
}
static const step_t *comp_lane(track_t *t, const step_t *s)
{
    uint32_t cl = (uint32_t)clamp(t->p[P_CLEN], 0, NSTEP);
    const step_t *c;
    t->cv_on = 0;
    if (!cl)
        return s;
    c = &seq_steps(t)[t->cpos % cl];
    t->cpos = (uint8_t)((t->cpos + 1u) % cl);
    t->cvs = *s;
    t->cvs.comp = c->comp;
    t->cvs.carg = c->carg;
    t->cvs.comp2 = c->comp2;                        /* (0.6.82) the second one, their WHENs, DIV and LEN */
    t->cvs.carg2 = c->carg2;
    t->cvs.cwhen = (uint8_t)((c->cwhen & 15u) | (s->cwhen & 0xF0u));   /* (0.6.88: the step's own TIMING kept) */
    t->cvs.ctime = c->ctime;
    t->cv_on = 1;
    return &t->cvs;
}
static const step_t *arp_owner(const track_t *t, const step_t *s)
{
    const step_t *o = s;
    if (s->time == ST_TIE && t->arp_from)
        o = t->arp_from == CV_IDX ? &t->cva : &seq_steps(t)[(t->arp_from - 1u) % NSTEP];
    return o->time == ST_NOTE && (o->n || (o->hit && trk_kit(t))) && cp_get(t, o, CP_ARPN, CP_ARPS, 0) ? o : 0;   /* (0.6.82:
                                                         * a kit's hits too) */
}
/* the parts a step plays (*div: in how many equal parts of its length they fall, one each from the start): an ARP's
 * RATE, else its ECHO (0.6.42: x2..x4; 0.6.82: .. x8; an ARP wins over it), else its RATCH. (0.6.82) DIV: ECHO's and
 * RATCH's parts a step/2 .. /8 apart, as many as fit (0: the step shared out) */
/* (2026-11-10) a note with TIEs (not a kit, not an ARP) and a RATCH / ECHO: its parts are over its whole length (the HOLD's way,
 * seq_hold_parts), not squeezed into its first step; the steps it rings (0: not such a note) */
static uint32_t tie_ratch_len(const track_t *t, const step_t *s)
{
    uint32_t len = (uint32_t)t->p[P_SLEN], idx = (uint32_t)(s - t->step), l = 1, h;
    uint8_t a;
    if (idx >= NSTEP || s->time != ST_NOTE || !s->n || !len || idx >= len || trk_kit(t))
        return 0;
    h = CP_HAS(t, s, CP_ECHO, &a) ? 2u : step_ratchet(s);
    if (h < 2u)
        return 0;
    while (l < len && t->step[(idx + l) % len].time == ST_TIE)
        l++;
    return l > 1u ? l : 0u;
}
static uint32_t step_parts(const track_t *t, const step_t *s, uint32_t *div)
{
    const step_t *o = arp_owner(t, s);
    uint32_t h, d;
    uint8_t a;
    if (o) {
        cp_get(t, o, CP_ARPN, CP_ARPS, &a);
        return *div = CP_RATES[CP_RATE(a) % CP_NRATE];
    }
    if (s->time == ST_NOTE && !trk_kit(t) && CP_HAS(t, s, CP_HOLD, 0))   /* (2026-11-10) a HOLD: one note; its RATCH / ECHO are the
                                                         * parts over its whole length (seq_hold_parts) */
        return *div = 1;
    if (tie_ratch_len(t, s))
        return *div = 1;
    h = s->time == ST_NOTE && CP_HAS(t, s, CP_ECHO, &a) ? 2u + (a & ~CP_ECHO_EVEN) % 7u : step_ratchet(s);
    d = CT_DIVS[CT_DIV(s) % CT_NDIV];
    if (h < 2u || !d)
        return *div = h;
    *div = d;
    return h < d ? h : d;
}
/* (0.6.42) OCT's octave for this loop (semitones) */
static int32_t cp_oct(const track_t *t, uint32_t a)
{
    switch (a % 6u) {
    case 0: return 12;
    case 1: return -12;
    case 2: return t->loops & 1u ? 12 : 0;
    case 3: return t->loops & 1u ? -12 : 0;
    case 4: return t->loops & 1u ? -12 : 12;
    default: return ((int32_t)(rng() % 3u) - 1) * 12;
    }
}
/* an ARP's k-th of n in its ORDER: UP, DOWN, UP-DN, RND */
static uint32_t cp_order(uint32_t o, uint32_t k, uint32_t n)
{
    if (!n) return 0;
    if (o == 1u) return n - 1u - k % n;                 /* DOWN */
    if (o == 2u && n > 1u) { k %= 2u * n - 2u; return k >= n ? 2u * n - 2u - k : k; }   /* UP-DN */
    if (o == 3u) return rng() % n;                      /* RND */
    return k % n;                                       /* UP */
}
/* (0.6.82) an ARP on a drum kit, its k-th part: NOTES the step's sounds one after another, SEQ the pattern's in the
 * order it plays them, SCALE all of the step's at once, each part a degree further up the scale (*deg); into nn */
static uint32_t cp_kit_arp(const track_t *t, const step_t *s, uint32_t c, uint32_t a, uint32_t k, uint8_t *nn,
                           int32_t *deg)
{
    uint8_t l[16];
    uint32_t n = 0, i, m, cnt = 1;
    const step_t *src = s;
    *deg = 0;
    if (c == CP_ARPQ) {
        src = seq_steps(t);
        cnt = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    }
    for (m = 0; m < cnt && m < NSTEP; m++) {
        if (src[m].time != ST_NOTE) continue;
        for (i = 0; i < src[m].n && i < 4u; i++)
            if (n < sizeof l && (!n || l[n - 1u] != src[m].note[i])) l[n++] = src[m].note[i];
        for (i = 0; i < NLANE; i++)
            if (((src[m].hit >> i) & 1u) && n < sizeof l && (!n || l[n - 1u] != DRUM_LANE_NOTE[i]))
                l[n++] = DRUM_LANE_NOTE[i];
    }
    if (!n) return 0;
    if (c == CP_ARPS) {
        *deg = (int32_t)cp_order(CP_ORD(a), k, CP_DEGS[CP_RNG(a)]);
        for (i = 0; i < n && i < 4u + NLANE; i++) nn[i] = l[i];
        return n < 4u + NLANE ? n : 4u + NLANE;
    }
    nn[0] = l[cp_order(CP_ORD(a), k, n)];
    return 1;
}
/* (0.6.39) an ARP component's next note: its notes (NOTES: the step's, low to high; SEQ: the pattern's, 0.6.82: in the
 * order the pattern plays them, its melody fast; both over RANGE octaves; SCALE: the scale's degrees up from the step's lowest note), the k-th in its ORDER */
static uint32_t cp_arp_note(const track_t *t, const step_t *s, uint32_t c, uint32_t a, uint32_t k)
{
    uint8_t base[16];
    uint32_t nb = 0, n, i, j, m, o2, o = CP_ORD(a), len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    if (c == CP_ARPS) {
        n = CP_DEGS[CP_RNG(a)];
    } else {
        const step_t *src = c == CP_ARPQ ? seq_steps(t) : s;
        uint32_t cnt = c == CP_ARPQ ? len : 1u;
        for (m = 0; m < cnt; m++) {
            uint8_t sn[4 + NLANE], so[4 + NLANE];       /* (chord-harp) the step's notes, a CHORD's whole */
            uint32_t ns = src[m].time == ST_NOTE ?
                          step_expand(&src[m], voice_poly((int)trk_vmode(t)) ? step_chord_id(&src[m]) : 0u, sn, so) : 0u;
            for (i = 0; i < ns; i++) {
                uint8_t x = sn[i];
                if (c == CP_ARPQ) {                     /* (0.6.82) SEQ: the pattern's melody as it plays (a note */
                    if (nb < sizeof base && (!nb || base[nb - 1u] != x))   /* again at once: one), not sorted */
                        base[nb++] = x;
                    continue;
                }
                for (j = 0; j < nb && base[j] < x; j++)
                    ;
                if (j < nb && base[j] == x) continue;
                if (nb == sizeof base) break;
                for (o2 = nb; o2 > j; o2--) base[o2] = base[o2 - 1u];   /* (no memmove on the target) */
                base[j] = x; nb++;
            }
        }
        if (!nb) return s->n ? s->note[0] : 60u;
        n = nb * (CP_RNG(a) + 1u);
    }
    k = cp_order(o, k, n);
    if (c == CP_ARPS) {
        uint32_t lo = 127u;
        for (i = 0; i < s->n && i < 4u; i++) if (s->note[i] < lo) lo = s->note[i];
        return (uint32_t)clamp(scale_steps(t, (int32_t)lo, (int32_t)k), 0, 127);
    }
    return (uint32_t)clamp(base[k % nb] + 12 * (int32_t)(k / nb), 0, 127);
}
static void seq_release(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++)
        seq_note_off(t, t->seq_notes[i]);
    t->seq_n = 0;
    t->seq_hold = 0;
    t->slide_glide = 0;                             /* live MONO / LEG keys must not glide after it */
    t->ss_n = t->ss_i = 0;                          /* (0.6.84) a strum still coming in: never */
}

/* (2026-11-10) CP_HOLD: the notes a HOLD step sounds are taken out of seq_notes (the gate and the next step's release leave
 * them), kept in hold_notes until the pattern comes round to step 1, the next HOLD step or STOP */
static void seq_unhold(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->hold_n; i++)
        seq_note_off(t, t->hold_notes[i]);
    t->hold_n = t->hold_left = t->hold_hits = 0;
}
/* steps: the steps it rings (0: to the pass's end); the step's RATCH / ECHO (hits parts) retrigger it over that whole length, every
 * part span / hits long (ECHO fading unless EVEN), the last until the HOLD ends (seq_tick: seq_hold_parts) */
static void seq_to_hold(track_t *t, const step_t *s, uint32_t steps, uint32_t period, uint32_t vel)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], hits, span = 0;
    uint8_t ea = 0;
    seq_unhold(t);
    for (i = 0; i < t->seq_n && i < sizeof t->hold_notes; i++)
        t->hold_notes[t->hold_n++] = t->seq_notes[i];
    t->seq_n = 0;
    t->seq_hold = 0;
    t->hold_left = (uint8_t)steps;                   /* (the steps it rings: seq_tick counts them down; 0: to the pass's end) */
    hits = CP_HAS(t, s, CP_ECHO, &ea) ? 2u + (ea & ~CP_ECHO_EVEN) % 7u : step_ratchet(s);
    if (hits > 1u && len) {
        uint32_t n = steps ? steps : len - t->seq_idx;
        for (i = 0; i < n; i++)
            span += step_samples(t, period, (t->seq_idx + i) % len);
        t->hold_hits = (uint8_t)hits;
        t->hold_k = 1;
        t->hold_vel = (uint8_t)clamp((int32_t)vel, 1, 127);
        t->hold_fade = (uint8_t)(CP_HAS(t, s, CP_ECHO, 0) && !(ea & CP_ECHO_EVEN));
        t->hold_part = t->hold_due = span / hits;
    }
}
static void seq_hold_parts(track_t *t, uint32_t n)       /* the held notes struck again at each part */
{
    uint32_t i, vel;
    if (!t->hold_hits || !t->hold_n)
        return;
    if (t->hold_due > n) {
        t->hold_due -= n;
        return;
    }
    t->hold_due = t->hold_due + t->hold_part - n;
    vel = t->hold_fade ? (uint32_t)t->hold_vel * (t->hold_hits - t->hold_k) / t->hold_hits : t->hold_vel;
    if (vel < 8u) vel = 8u;
    for (i = 0; i < t->hold_n; i++) {
        seq_note_off(t, t->hold_notes[i]);
        seq_note_on(t, t->hold_notes[i], vel);
    }
    if (++t->hold_k >= t->hold_hits)
        t->hold_hits = 0;
}

static void seq_stop(void)
{
    uint32_t i;
    if (song.playing && mo_clock_on())             /* (0.6.72) MIDI OUT CLOCK / ALL: STOP */
        mo_send(0xFCu, 0, 0);
    song.playing = 0;
    cin_left = 0;                                  /* (a count-in: cancelled, nothing recorded) */
    if (rp_state) {                                /* (chord-harp fork) ONE PASS stopped before its end: disarmed */
        if (rp_state >= RP_PRE)
            song.rec &= (uint8_t)~(1u << ((rp_trk - 1u) % NTRK));
        rp_state = RP_OFF;
    }
    cin_flush = 0;
    memset(cin_n, 0, sizeof cin_n);
    if (song.g[G_CLOCK])                           /* external clock: the arp stops with the transport, */
        for (i = 0; i < NPART; i++)                /* its sounding note too (HOLD keeps the latched chord) */
            if (trk[i].arp_note)
                arp_off(&trk[i]);
    for (i = 0; i < NTRK; i++) {
        seq_release(&trk[i]);
        seq_unhold(&trk[i]);
        trk[i].rh_n = 0;                           /* a recorded note held over the stop: as far as it got */
        trk[i].rat_left = 0;
        trk[i].arp_from = 0;
        trk[i].pulse_from = trk[i].jump_to = trk[i].rev = 0;
        trk[i].cpos = trk[i].cv_on = 0;
    }
    mo_flush();                                    /* (0.6.72) MIDI OUT ALL: nothing left hanging */
    chain_stop();
    motion_end();
}

/* play one step: TIE extends, REST releases, NOTE (re)triggers its notes, then its lane hits (their GM
 * notes, DRUM_LANE_NOTE; an accented hit at 127); a SLIDE on the previous step makes this one legato with a
 * glide (acid style). skip: bit k = note k, bit 8 + l = the hit of lane l already sounds from live
 * recording (not triggered, not released here); SEQ_REP: a RATCH repeat (seq_ratchet).
 * RATCH x2..x4 (core.h step_ratchet): a NOTE step plays all of it again in equal parts of its swung length, each
 * part retriggered with GATE of the part; the chance is rolled once for the whole step (a failed roll: no part),
 * a slide into it glides into the first part only, and it never slides or ties out (its last part ends at its
 * gate). x1, what every older pattern holds, is the step exactly as before. */
#define SEQ_REP (1u << 16)
#define SEQ_ROLLED (1u << 17)   /* seq_tick rolled the chance already (before the step's parameter locks): it passed */
#define SEQ_MISS (1u << 18)     /* .. it failed: nothing plays */
/* (0.6.81) a chord's slide on a POLY track (SLD on a chord step): its notes glide to the next chord's, low to
 * high, none struck again; the next chord with more notes: each extra one rises from the nearest of the old (its
 * octave nearest), fading in; with fewer: each old one left glides to the nearest new one, fading out as it goes (its
 * release). The glide over three quarters of the step */
static int32_t cg_near(int32_t x, const uint8_t *set, uint32_t n, int other)   /* the note of set, in the octave
                                                         * nearest x (other: never x itself, 0.6.82) */
{
    uint32_t i, o;
    int32_t best = x, bd = 1000;
    for (i = 0; i < n; i++)
        for (o = 0; o < 2u; o++) {                      /* (its octave nearest, and the next nearest) */
            int32_t y = set[i] + 12 * ((x - set[i] + 6 + 1200) / 12 - 100), d;
            if (o) y += y > x ? -12 : 12;
            d = y > x ? y - x : x - y;
            if (other && !d) continue;
            if (d < bd) { bd = d; best = y; }
        }
    return clamp(best, 0, 127);
}
static int32_t glide_q8(int32_t d, uint32_t ticks)   /* (0.6.82) a glide's step over ticks: gstep, Q8 */
{
    int32_t g = ((d < 0 ? -d : d) * 256 + (int32_t)ticks - 1) / (int32_t)(ticks ? ticks : 1u);   /* (rounded up: there in time) */
    return g ? g : 1;
}
/* (0.6.82) the samples from this step to the next that is not a TIE: a chord's slide lasts as long as the chord */
static uint32_t tie_span(const track_t *t, uint32_t period, uint32_t len)
{
    uint32_t n = step_samples(t, period, t->seq_idx), k;
    for (k = 1; k < len; k++) {
        uint32_t i = (t->seq_idx + k) % len;
        if (seq_steps(t)[seq_src(t, i)].time != ST_TIE)
            break;
        n += step_samples(t, period, i);
    }
    return n;
}
/* (chord-harp) a step sliding into a CHORD step of the same chord (a chord held as it was recorded, harp_hold): the
 * chord's voices go on, the notes over it that change start and end; 1 done, 0 not such a step (chord_glide) */
static __attribute__((noinline)) int harp_tie(track_t *t, const uint8_t *nn, const uint8_t *vv, uint32_t m, uint32_t sk)
{
    uint32_t ti = trk_index(t) % NTRK, i, j;
    if (!hp_now[ti] || hp_now[ti] != hp_was[ti])
        return 0;
    for (i = 0; i < t->seq_n; i++) {                /* the old notes not in this step end */
        for (j = 0; j < m && (nn[j] != t->seq_notes[i] || ((sk >> j) & 1u)); j++)
            ;
        if (j == m)
            seq_note_off(t, t->seq_notes[i]);
    }
    for (j = 0; j < m; j++) {                       /* its notes not sounding yet start */
        if ((sk >> j) & 1u)
            continue;
        for (i = 0; i < t->seq_n && t->seq_notes[i] != nn[j]; i++)
            ;
        if (i == t->seq_n)
            seq_note_on(t, nn[j], vv[j]);
    }
    return 1;
}
static void chord_glide(track_t *t, const uint8_t *nn, const uint8_t *vv, uint32_t m, uint32_t sk, uint32_t ssamp)
{
    uint8_t o[4 + NLANE], n[4 + NLANE], nv[4 + NLANE];   /* (chord-harp: a CHORD's notes and more; was 8) */
    uint32_t a = 0, b = 0, i, j, k, ti = trk_index(t) % NTRK;
    uint32_t ticks = (uint32_t)((t->p[P_GLIDE] > 0 ? (uint64_t)ssamp * (uint32_t)t->p[P_GLIDE] / 127u :
                                 (uint64_t)ssamp * 3u / 4u) / CTL);   /* (GLIDE: that much of the chord, its TIEs
                                                         * too (0.6.82); SLD alone: three quarters) */
    int32_t tr = trk_trans(t);
    if (!ticks) ticks = 1;
    for (i = 0; i < t->seq_n && a < sizeof o; i++)            /* (0.6.81) the old chord: its notes still sounding (a voice
                                                         * taken by another note is not one) */
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active && t->v[k].gate && t->v[k].note == t->seq_notes[i]) { o[a++] = t->seq_notes[i]; break; }
    for (i = 0; i < m && b < sizeof n; i++)
        if (!((sk >> i) & 1u)) { n[b] = nn[i]; nv[b++] = vv[i]; }
    for (i = 1; i < a; i++) for (j = i; j > 0 && o[j] < o[j - 1]; j--) { uint8_t x = o[j]; o[j] = o[j - 1]; o[j - 1] = x; }
    for (i = 1; i < b; i++) for (j = i; j > 0 && n[j] < n[j - 1]; j--) {
        uint8_t x = n[j]; n[j] = n[j - 1]; n[j - 1] = x; x = nv[j]; nv[j] = nv[j - 1]; nv[j - 1] = x; }
    for (i = 0; i < a || i < b; i++) {
        voice_t *v = 0;
        if (i < a)
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].active && t->v[k].gate && t->v[k].note == o[i]) { v = &t->v[k]; break; }
        if (i < a && i < b && v) {                      /* a pair: the old note glides to the new */
            int32_t d;
            if (mo_out[ti][o[i]]) { mo_send(0x80u | trk_midi_ch(ti), mo_out[ti][o[i]] - 1u, 0); mo_out[ti][o[i]] = 0; }
            v->note = n[i];
            glide_from_trk(t, v);                       /* (0.9.8) VOICE's S-curve from where it sounds */
            v->pitch16 = ((int32_t)n[i] + tr) * 16 + t->pfx_rpit;
            d = v->pitch16 - v->pitch_cur;
            v->gstep = glide_q8(d, ticks);
            if (mo_lvl == MO_ALL && mo_plays(t, n[i])) {
                mo_out[ti][n[i]] = (uint8_t)(n[i] + 1u);
                mo_send(0x90u | trk_midi_ch(ti), n[i], nv[i] ? nv[i] : 1u);
            }
        } else if (i < b) {                             /* a new one: from the nearest old note, fading in */
            uint32_t a0 = vage;
            seq_note_on(t, n[i], nv[i]);
            if (vage != a0 && a)
                for (k = 0; k < NVOICE; k++)
                    if (t->v[k].age == vage && t->v[k].active) {
                        voice_t *w = &t->v[k];
                        int32_t d;
                        w->pitch_cur = (cg_near(n[i], o, a, 1) + tr) * 16 + t->pfx_rpit;
                        w->gfrom = (int16_t)w->pitch_cur;   /* (0.9.8: VOICE's glide from there) */
                        d = w->pitch16 - w->pitch_cur;
                        w->gstep = glide_q8(d, ticks);
                        w->gacc = 0;
                        w->fin_d = (uint16_t)(d < 0 ? -d : d);   /* (in as it glides; 0.6.82) */
                    }
        } else if (v) {                                 /* an old one left: to the nearest new note, fading out */
            int32_t d;
            glide_from_trk(t, v);
            v->pitch16 = (cg_near(o[i], n, b, 0) + tr) * 16 + t->pfx_rpit;
            d = v->pitch16 - v->pitch_cur;
            v->gstep = glide_q8(d, ticks);
            seq_note_off(t, o[i]);
        }
    }
    t->slide_glide = 0;
}

/* (0.6.81) GLIDE on a POLY track: every chord glides into the next by itself (no SLD needed): its notes held to the next
 * step when that one plays notes */
static int chord_auto(const track_t *t, uint32_t len)
{
    const step_t *nx;
    if (!voice_poly(t->p[P_VOICE]) || t->p[P_GLIDE] <= 0)
        return 0;
    nx = &seq_steps(t)[seq_src(t, (t->seq_idx + 1u) % len)];
    return nx->time == ST_NOTE && nx->n;
}

/* (0.6.87) HUMAN: SEQ > TRACK's HUMAN amount (0..127; stored 127 - it in P_HVEL: 127 none; 0.6.92: one knob, it moves
 * each note's velocity, start, gate, decay and release); its own random numbers (never libc's rng: the UI's dice keep
 * theirs) */
static uint32_t hum_rs = 0x2545F491u;
static uint32_t hum_rnd(void) { hum_rs ^= hum_rs << 13; hum_rs ^= hum_rs >> 17; hum_rs ^= hum_rs << 5; return hum_rs; }
static uint32_t hum_amt(const track_t *t, uint32_t id)
{
    if (YEHNAH_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL) return 0;
    return 127u - (uint32_t)clamp(t->p[id], 0, 127);
}
static int32_t hum_pm(uint32_t r) { return (int32_t)(hum_rnd() % (2u * r + 1u)) - (int32_t)r; }   /* -r .. r */
#define HUM_TMAX (FS * 40u / 1000u)                 /* at 100 %: up to 40 ms late (0.6.92: was 30) */
static __attribute__((noinline)) void seq_step(track_t *t, const step_t *s, uint32_t period, uint32_t skip)
{
    uint32_t i, j, gate = period * (uint32_t)t->p[P_SGATE] / 128u;
    uint32_t vel = (s->flags & SF_ACCENT) ? 127u : (s->vel ? s->vel : 96u);
    uint8_t ha = 0;                                 /* (HOLD's steps) */
    uint32_t slide_in = t->seq_hold && t->seq_n && !(skip & SEQ_REP);
    uint32_t len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    uint32_t next_tie = seq_steps(t)[seq_src(t, (t->seq_idx + 1u) % len)].time == ST_TIE;
    uint32_t div, hits = step_parts(t, s, &div);
    const step_t *arp = arp_owner(t, s);            /* (0.6.39) an ARP component: one of its notes a part */
    uint8_t ca = 0, aa = 0, ra = 0, oa = 0, ba = 0;
    uint32_t ac = 0, rnd = 0, oct = 0, bend = 0;
    int32_t walk = 0, climb = 0;                       /* (0.6.98) WALK / CLIMB: this loop's move */
    int cont = 0;                                   /* .. going on over a TIE */
    uint8_t nn[4 + NLANE], vv[4 + NLANE];           /* the notes it plays: its notes, then its hits */
    uint8_t sn[4 + NLANE], so[4 + NLANE];           /* (chord-harp) its notes, a CHORD's whole (step_notes) */
    uint32_t m = 0, sk = 0, sk0, hdel = 0, ns;
    /* QNT SEQ: the step's notes (not the lane hits) snap to the scale as they play; never on a drum kit, the slices
     * or another engine that maps the keys itself. seq_notes keeps the notes that sound: their note-offs match */
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t kit = e->oneshot || (e->keys && e->keys(t, 0) >= 0);
    uint32_t qseq = t->p[P_QUANT] == QN_SEQ && !kit;
    int32_t up = (kit ? 0 : t->p[P_POFS]) +         /* SHIFT PIT: the notes up (a kit keeps its lanes) */
                 (t->p[P_AMODE] == AM_TRNS ? t->trn : 0);   /* ARP TRNS: the keys' interval */
    if (!(skip & SEQ_REP)) {
        t->rat_left = 0;
        t->lr_on = 0;
        if ((skip & SEQ_MISS) ||
            (!(skip & SEQ_ROLLED) && !step_rolls(t, s))) {
            seq_release(t);
            return;
        }
    }
    if (s->time == ST_TIE && arp) {
        uint8_t sa = 0;
        uint32_t sl;
        cp_get(t, arp, CP_ARPN, CP_ARPS, &sa);
        sl = CP_SLOW[CP_RATE(sa) % CP_NRATE];
        if (sl && !(skip & SEQ_REP) && ++t->arp_c % sl) {   /* (2026-11-10) slower than a step: this step holds the note */
            if (t->seq_n) {
                t->seq_off = gate + period / 2u;
                t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie || chord_auto(t, len);
            }
            return;
        }
        s = arp;                                    /* (0.6.39) the arpeggio goes on over the TIE */
        cont = 1;
        vel = (s->flags & SF_ACCENT) ? 127u : (s->vel ? s->vel : 96u);
    } else if (s->time == ST_TIE) {
        if (t->seq_n) {
            t->seq_off = gate + period / 2u;
            t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie || chord_auto(t, len);   /* chains hold at any GATE /
                                                         * swing; (0.6.81) GLIDE on POLY: to the next chord */
        }
        return;
    }
    if (!(skip & SEQ_REP) && !cont && CP_HAS(t, s, CP_KILL, 0))
        trk_fade_out(t);                            /* (0.6.42) KILL: what sounds goes (~3 ms), its notes after */
    if (!(skip & SEQ_REP) && !cont && (s->time == ST_REST || (!s->n && !s->hit)) && t->pulse_from) {
        const step_t *o = t->pulse_from == CV_IDX ? &t->cvp :   /* (0.6.42) PULSE: again on an empty step */
                          &seq_steps(t)[(t->pulse_from - 1u) % NSTEP];
        if (CP_HAS(t, o, CP_PULSE, &ca) && o->time == ST_NOTE && (o->n || o->hit) && ++t->pulse_k % (ca % 4u + 1u) == 0u) {
            s = o;
            cont = 2;
            vel = (s->flags & SF_ACCENT) ? 127u : (s->vel ? s->vel : 96u);
        }
    }
    if (!(skip & SEQ_REP) && !cont && s->time == ST_NOTE && (s->n || s->hit)) {
        uint32_t pi = (uint32_t)(s - seq_steps(t));    /* (0.6.42) PULSE from here; another note ends one */
        uint32_t pul = CP_HAS(t, s, CP_PULSE, 0);
        t->pulse_from = (uint8_t)(pul && pi < NSTEP ? pi + 1u : 0u);
        if (s == &t->cvs && pul) {                      /* (0.6.47) C.LEN: the lane's step, kept */
            t->cvp = t->cvs;
            t->pulse_from = CV_IDX;
        }
        t->pulse_k = 0;
        if (CP_HAS(t, s, CP_TONAL, &ca))
            t->tonal = (int8_t)((int32_t)(ca % 15u) - 7);
    }
    if (!(skip & SEQ_REP) && !cont) {
        t->arp_from = 0;                            /* (0.6.39) a step of its own: no arpeggio, or a new one */
        if (arp) {
            uint32_t ai = (uint32_t)(arp - seq_steps(t));
            t->arp_from = (uint8_t)(ai < NSTEP ? ai + 1u : 0u);
            if (arp == &t->cvs) {                       /* (0.6.47) C.LEN: the lane's step, kept over the TIEs */
                t->cva = t->cvs;
                t->arp_from = CV_IDX;
            }
            t->arp_k = 0;
            t->arp_c = 0;
        }
    }
    if (s->time == ST_REST || (!s->n && !s->hit)) {
        seq_release(t);
        return;
    }
    if (arp)                                        /* (0.6.82) the components on these notes, one a type: the */
        ac = cp_get(t, arp, CP_ARPN, CP_ARPS, &aa); /* step's first, else its second */
    rnd = CP_HAS(t, s, CP_RAND, &ra);
    oct = CP_HAS(t, s, CP_OCT, &oa);
    bend = CP_HAS(t, s, CP_BEND, &ba);
    {   /* (0.6.98) WALK and CLIMB: this loop's move, from the step's two components (both may walk / climb) */
        uint8_t wa = 0, c2 = 0;
        walk = CP_HAS(t, s, CP_WALK, &wa) ? cp_walk_off(t, s, wa, 0) : 0;
        climb = CP_HAS(t, s, CP_CLIMB, &c2) ? cp_walk_off(t, s, c2, 1) * 12 : 0;
    }
    if (CT_LEN(s))                                  /* (0.6.82) LEN: the step's own gate, n/16 */
        gate = period * CT_LEN(s) / 16u;
    if (hits > 1u) {                                /* RATCH: the gate of a part, no slide or tie out */
        gate = step_samples(t, period, t->seq_idx) / div * (CT_LEN(s) ? CT_LEN(s) * 8u : (uint32_t)t->p[P_SGATE]) / 128u;
        if (!(skip & SEQ_REP))
            t->rat_left = (uint8_t)(hits - 1u);
        uint8_t ea = 0;
        if (!arp && CP_HAS(t, s, CP_ECHO, &ea) && !(ea & CP_ECHO_EVEN)) {   /* (0.6.42) ECHO: each repeat softer
                                                         * (0.8.5: EVEN, a ratchet: all at one level) */
            uint32_t part = (skip & SEQ_REP) ? hits - 1u - t->rat_left : 0u;
            vel = vel * (hits - part) / hits;
            if (vel < 8u) vel = 8u;
        }
    }
    ns = step_notes(t, s, kit, sn, so);             /* (an ARP's: one note a part, its own) */
    if (arp) {
        ns = 1;
        so[0] = 0;
    }
    for (i = 0; i < ns && !(kit && arp); i++) {
        uint32_t x = arp ? cp_arp_note(t, arp, ac, aa, t->arp_k++) : sn[i];
        x = (uint32_t)clamp((int32_t)x + up, 0, 127);
        if (!kit) {                                 /* (0.6.42) TONAL, RAND, OCT; (0.6.98) WALK, CLIMB */
            if (walk)
                x = (uint32_t)clamp(scale_steps(t, (int32_t)x, walk), 0, 127);
            if (climb)
                x = (uint32_t)clamp((int32_t)x + climb, 0, 127);
            if (t->tonal)
                x = (uint32_t)clamp(scale_steps(t, (int32_t)x, t->tonal), 0, 127);
            if (rnd) {
                int32_t r = ra % 8u ? (int32_t)(ra % 8u) : 1;
                x = (uint32_t)clamp(scale_steps(t, (int32_t)x, (int32_t)(rng() % (uint32_t)(2 * r + 1)) - r), 0, 127);
            }
            if (oct)                                /* (0.6.82: with a RAND too) */
                x = (uint32_t)clamp((int32_t)x + cp_oct(t, oa), 0, 127);
        }
        if (song.g[G_STRN] && !kit)                 /* (0.4) STRN: up / down the scale, every sequence */
            x = (uint32_t)clamp(scale_steps(t, (int32_t)x, song.g[G_STRN]), 0, 127);
        if (qseq) {                                 /* QNT SEQ: onto the scale now; two notes snapping */
            x = (uint32_t)clamp(scale_snap(t, (int32_t)x), 0, 127);   /* together play once */
            for (j = 0; j < m && nn[j] != x; j++)
                ;
            if (j < m)
                continue;
        }
        nn[m] = (uint8_t)x;
        vv[m] = (uint8_t)vel;
        sk |= ((skip >> so[i]) & 1u) << m;
        m++;
    }
    for (i = 0; i < NLANE && !arp && m < sizeof nn; i++)
        if ((s->hit >> i) & 1u) {
            nn[m] = DRUM_LANE_NOTE[i];
            vv[m] = (uint8_t)((s->acc >> i) & 1u ? 127u : vel);
            sk |= ((skip >> (8u + i)) & 1u) << m;
            m++;
        }
    if (kit) {                                      /* (0.6.82) on a drum kit: an ARP plays its sounds (cp_kit_arp); */
        int32_t dp = 0, deg = 0;                    /* OCT RAND TONAL and ARP SCALE tune the hits, BEND bends them */
        if (arp) {
            uint8_t kn[4 + NLANE];
            uint32_t c2 = cp_kit_arp(t, arp, ac, aa, t->arp_k++, kn, &deg);
            for (i = 0; i < c2 && m < 4u + NLANE; i++) {
                nn[m] = kn[i];
                vv[m++] = (uint8_t)vel;
            }
        }
        if (t->tonal) dp += scale_steps(t, 60, t->tonal) - 60;
        if (deg) dp += scale_steps(t, 60, deg) - 60;
        if (rnd) {
            int32_t r = ra % 8u ? (int32_t)(ra % 8u) : 1;
            dp += scale_steps(t, 60, (int32_t)(rng() % (uint32_t)(2 * r + 1)) - r) - 60;
        }
        if (oct) dp += cp_oct(t, oa);
        t->dpit = (int8_t)clamp(dp, -48, 48);
        {                                           /* (0.8.5) TOUCH: SHORT GHOST FLAM on the hits (eng_drum.c) */
            uint8_t da = 0;
            t->dtch = (uint8_t)(CP_HAS(t, s, CP_TOUCH, &da) ? 1u + da % 3u : 0u);
        }
        t->dbend = (int16_t)(bend ? ((int32_t)(ba % 25u) - 12) * 16 : 0);
        t->dbt = (uint16_t)clamp((int32_t)(step_samples(t, period, t->seq_idx) / CTL), 1, 65535);
    }
    if (slide_in && !kit && !arp && hits == 1u && m && voice_poly(t->p[P_VOICE])) {   /* (0.6.81) a POLY chord sliding into
                                                         * this one: its notes glide to these */
        if (!harp_tie(t, nn, vv, m, sk))            /* (chord-harp) the same chord: held, not glided */
            chord_glide(t, nn, vv, m, sk, tie_span(t, period, len));
        t->seq_n = 0;
        for (i = 0; i < m; i++)
            if (!((sk >> i) & 1u))
                t->seq_notes[t->seq_n++] = nn[i];
        t->seq_off = gate;
        t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie || chord_auto(t, len);
        if (!kit && !arp && hits == 1u && CP_HAS(t, s, CP_HOLD, &ha))
            seq_to_hold(t, s, ha, period, vel);
        else if (!kit && !arp && (ha = (uint8_t)tie_ratch_len(t, s)) != 0u)
            seq_to_hold(t, s, ha, period, vel);
        return;
    }
    t->slide_glide = (uint8_t)slide_in;
    if (!slide_in)
        seq_release(t);
    t->ss_n = t->ss_i = 0;
    {   /* (0.6.87) HUMAN (0.6.92: one amount): each note's velocity up to +-50 at 100 %, the gate +-40 %, the step up to
         * 40 ms late, its DEC and REL up to +-20 steps of their knobs (voice_start takes hum_env) */
        uint32_t hv = hum_amt(t, P_HVEL), hg = hv, ht = hv;
        t->hum_env = (int8_t)(hv ? hum_pm(hv * 20u / 127u) : 0);
        if (hv)
            for (i = 0; i < m; i++)
                vv[i] = (uint8_t)clamp((int32_t)vv[i] + hum_pm(hv * 50u / 127u), 1, 127);
        if (hg && hits == 1u)
            gate = (uint32_t)clamp((int32_t)gate + (int32_t)gate / 100 * hum_pm(hg * 40u / 127u), 16, 0x7FFFFFFF);
        if (ht && hits == 1u && !bend && !slide_in) {   /* (late, never early: the step can't play before it starts) */
            uint32_t d = hum_rnd() % (HUM_TMAX * ht / 127u + 1u), cap = period / 3u;
            hdel = d < cap ? d : cap;
        }
        if (t->mic > 0 && hits == 1u && !bend && !slide_in && !(skip & SEQ_REP))   /* (0.6.88) TIMING late: on top */
            hdel += (uint32_t)t->mic * step_samples(t, period, t->seq_idx) / 64u;   /* (0.9.1: 64ths) */
    }
    sk0 = sk;                                       /* (the notes the step sounds, strummed or not: seq_notes) */
    if (!kit && !arp && m > 1u && t->p[P_CSTR] > 0 && voice_poly(t->p[P_VOICE])) {   /* (0.6.84) STRUM: the chord in
                                                         * one note after another, low to high, as the keys strum it
                                                         * (the first now, the others by seq_tick); within half the gate */
        uint32_t g = chord_gap(t), cap = gate / (2u * (m - 1u)), q, w = 0;
        uint8_t ord[4 + NLANE];
        for (q = 0; q < m; q++)
            if (!((sk >> q) & 1u)) {
                for (j = w; j > 0 && nn[ord[j - 1u]] > nn[q]; j--) ord[j] = ord[j - 1u];
                ord[j] = (uint8_t)q; w++;
            }
        for (q = 1; q < w && t->ss_n < sizeof t->ss_nn; q++) {   /* (the rest, queued; marked done here; chord-harp:
                                                         * a CHORD's 6 and more, was 4) */
            t->ss_nn[t->ss_n] = nn[ord[q]];
            t->ss_vv[t->ss_n++] = vv[ord[q]];
            sk |= 1u << ord[q];
        }
        t->ss_due = t->ss_gap = (uint32_t)clamp((int32_t)(g < cap ? g : cap), 1, 65535);
    }
    if (hdel) {                                     /* (0.6.87) HUMAN TIME: every note of the step queued, hdel late
                                                     * (a strum's after it, its gaps kept; else all at once) */
        uint32_t q, gap = t->ss_n ? t->ss_gap : 0u;
        for (q = 0; q < m; q++)                     /* (the strum's first, the ones not queued yet, to the front) */
            if (!((sk >> q) & 1u) && t->ss_n < sizeof t->ss_nn) {
                for (j = t->ss_n; j > 0u; j--) { t->ss_nn[j] = t->ss_nn[j - 1u]; t->ss_vv[j] = t->ss_vv[j - 1u]; }
                t->ss_nn[0] = nn[q]; t->ss_vv[0] = vv[q];
                t->ss_n++;
                sk |= 1u << q;
            }
        t->ss_gap = gap;
        t->ss_due = hdel ? hdel : 1u;
        t->ss_tch = t->dtch;                        /* (0.8.5: the queued hits keep their TOUCH) */
        gate += hdel;
    }
    {   /* (0.8.5) a kit: each lane's own mode on its hits (TOUCH now, the ratchets after: seq_lane_rat) */
        uint32_t l;
        for (l = 0; kit && !(skip & SEQ_REP) && l < NLANE; l++)
            if (lm_hits(step_lmode(s, l)) > 1u)
                t->lr_on = 1;
    }
    for (i = 0; i < m; i++)
        if (!((sk >> i) & 1u)) {
            uint32_t a0 = vage, dt0 = t->dtch, lt = kit ? lm_touch(step_lmode(s, drum_lane(nn[i]))) : 0u;
            if (lt)
                t->dtch = (uint8_t)lt;
            seq_note_on(t, nn[i], vv[i]);
            t->dtch = (uint8_t)dt0;
            if (bend && !kit && vage != a0) {   /* (0.6.42) BEND: its voice glides over the step */
                uint32_t k2, ticks = step_samples(t, period, t->seq_idx) / CTL;
                int32_t d = ((int32_t)(ba % 25u) - 12) * 16;
                for (k2 = 0; k2 < NVOICE; k2++) {
                    voice_t *v = &t->v[k2];
                    if (v->age == vage && v->active) {
                        v->pitch_cur = v->pitch16;
                        v->pitch16 = clamp(v->pitch16 + d, 0, 2047);
                        v->gstep = glide_q8(d, ticks);
                    }
                }
            }
        }
    t->dpit = 0;                                    /* (0.6.82: the keys' hits as they are) */
    t->dbend = 0;
    t->dtch = 0;
    if (slide_in)                                   /* release what is not held over */
        for (i = 0; i < t->seq_n; i++) {
            for (j = 0; j < m && nn[j] != t->seq_notes[i]; j++)
                ;
            if (j == m)
                seq_note_off(t, t->seq_notes[i]);
        }
    t->seq_n = 0;
    for (i = 0; i < m; i++)
        if (!((sk0 >> i) & 1u))
            t->seq_notes[t->seq_n++] = nn[i];
    t->seq_off = gate;
    t->seq_hold = hits == 1u && ((s->flags & SF_SLIDE) != 0 || next_tie ||   /* next step a TIE: keep the notes to it */
                                 (!kit && !arp && chord_auto(t, len)));   /* (0.6.81) GLIDE on POLY: to the next chord */
    if (!kit && !arp && hits == 1u && CP_HAS(t, s, CP_HOLD, &ha))   /* (2026-11-10) HOLD: the notes ring on */
        seq_to_hold(t, s, ha, period, vel);
    else if (!kit && !arp && !(skip & SEQ_REP) && (ha = (uint8_t)tie_ratch_len(t, s)) != 0u)   /* (a tied note's RATCH / ECHO) */
        seq_to_hold(t, s, ha, period, vel);
}

/* RATCH: the next part of the playing step once the sequencer is that far into it (seq_step played the first).
 * The step is read again: an edit to fewer parts (or another source in a song) ends the repeats */
static __attribute__((noinline)) void seq_ratchet(track_t *t, uint32_t period)
{
    const step_t *s = t->cv_on ? &t->cvs : &seq_steps(t)[seq_src(t, t->seq_idx)];   /* (0.6.47: C.LEN's) */
    uint32_t div, hits = step_parts(t, s, &div);
    if (t->rat_left >= hits) {
        t->rat_left = 0;
        return;
    }
    if (t->seq_pos < (hits - t->rat_left) * (step_samples(t, period, t->seq_idx) / div))
        return;
    t->rat_left--;
    seq_step(t, s, period, SEQ_REP);
}

/* (0.8.5) the lanes ratcheting on the playing step (LM_X2 / X4 / X8, seq_step struck their first part): each again at
 * every step / n (the grid RATCH's parts and the held keys' ratchets keep, dxo_tick), that lane's hit and notes alone,
 * the step read again (an edit ends it); n: the samples the step moved on in this block */
static __attribute__((noinline)) void seq_lane_rat(track_t *t, uint32_t period, uint32_t n)
{
    const step_t *s = t->cv_on ? &t->cvs : &seq_steps(t)[seq_src(t, t->seq_idx)];
    uint32_t sl = step_samples(t, period, t->seq_idx), pos = t->seq_pos, prev, l, k, any = 0;
    uint32_t vel = (s->flags & SF_ACCENT) ? 127u : (s->vel ? s->vel : 96u);
    if (pos >= 0x7FFFFFFFu || s->time != ST_NOTE) {
        t->lr_on = 0;
        return;
    }
    prev = pos > n ? pos - n : 0u;
    for (l = 0; l < NLANE; l++) {
        uint32_t r = lm_hits(step_lmode(s, l)), q, pn;
        if (r < 2u)
            continue;
        any = 1;
        q = sl / r;
        pn = pos / (q ? q : 1u);
        if (pn <= prev / (q ? q : 1u) || pn >= r)
            continue;
        if ((s->hit >> l) & 1u)
            seq_note_on(t, DRUM_LANE_NOTE[l], (s->acc >> l) & 1u ? 127u : vel);
        for (k = 0; k < s->n && k < 4u; k++)
            if (drum_lane(s->note[k]) == l)
                seq_note_on(t, s->note[k], vel);
    }
    t->lr_on = (uint8_t)any;
}

/* 1/2 TEMPO let go (pfx.c): the track back where it would be had it run on, from where it was pressed */
static __attribute__((noinline)) void pfx_resync(track_t *t, uint32_t period, uint32_t len)
{
    uint32_t idx = t->pfx_idx0 % (len ? len : 1u), pos = t->pfx_pos0 >= 0x7FFFFFFFu ? 0u : t->pfx_pos0;
    pos += t->pfx_el;
    for (;;) {
        uint32_t cur = step_samples(t, period, idx);
        if (!cur || pos < cur)
            break;
        pos -= cur;
        idx = (idx + 1u) % (len ? len : 1u);
    }
    t->seq_idx = (uint16_t)idx;
    t->seq_pos = pos;
}

#if FELUCCA_AMBIENT
#include "amb.c"                                   /* (chord-harp fork) ARP > AMBIENT */
#endif

static void seq_tick(track_t *t, uint32_t n)
{
    uint32_t period, len;
    if (t->pfx_rs) {                                  /* 1/2 TEMPO let go */
        t->pfx_rs = 0;
        if (song.playing)
            pfx_resync(t, div_samples((uint32_t)t->p[P_SDIV]), (uint32_t)t->p[P_SLEN]);
    }
    if ((t->pfx & PFX_HALF) && song.playing) {        /* 1/2 TEMPO held: half the samples (the odd one carried) */
        uint32_t h = n + t->pfx_half;
        t->pfx_el += n;
        t->pfx_half = (uint8_t)(h & 1u);
        n = h >> 1;
    }
    if (t->ss_i < t->ss_n) {                         /* (0.6.84) STRUM: the step's chord coming in */
        if (t->ss_due > n) {
            t->ss_due -= n;
        } else {
            do {                                    /* (0.6.87: HUMAN TIME without a strum: the chord together) */
                t->dtch = t->ss_tch;
                if (trk_kit(t)) {                       /* (0.8.5) its lane's TOUCH, from the step playing */
                    const step_t *ps = t->cv_on ? &t->cvs : &seq_steps(t)[seq_src(t, t->seq_idx)];
                    uint32_t lt = lm_touch(step_lmode(ps, drum_lane(t->ss_nn[t->ss_i])));
                    if (lt)
                        t->dtch = (uint8_t)lt;
                }
                seq_note_on(t, t->ss_nn[t->ss_i], t->ss_vv[t->ss_i]);
                t->dtch = 0;
                t->ss_i++;
            } while (!t->ss_gap && t->ss_i < t->ss_n);
            t->ss_due = t->ss_gap;
        }
    }
    if (song.playing)
        seq_hold_parts(t, n);                         /* (2026-11-10) a HOLD's RATCH / ECHO over its length */
    if (t->seq_n && !t->seq_hold) {
        if (t->seq_off <= n)
            seq_release(t);
        else
            t->seq_off -= n;
    }
    if (!song.playing)
        return;
    period = div_samples((uint32_t)t->p[P_SDIV]);
    len = (uint32_t)t->p[P_SLEN];
    t->seq_pos += n;
    for (;;) {
        uint32_t cur_len = step_samples(t, period, t->seq_idx) + t->ext, start, e = seq_early(t, period, len);
        if (e >= cur_len) e = cur_len - 1u;             /* (0.6.88) TIMING: the next step early, this one shorter */
        if (t->seq_pos < cur_len - e && t->seq_pos != 0x7FFFFFFFu + n)
            break;
        start = t->seq_pos >= 0x7FFFFFFFu;
        if (!start && rp_state == RP_REC && rec_pass_end(t, len))
            return;                                 /* (chord-harp fork) ONE PASS done: stopped before step 1 again */
        if (!start && rp_state == RP_CIN && rec_cin_end(t, len))
            return;                                 /* (chord-harp fork) armed while playing: stopped, counting in */
        t->seq_pos = start ? chain.carry : t->seq_pos - (cur_len - e);   /* (a start: 0; a section in: its carry) */
        t->ext = (uint16_t)(start ? 0u : e);            /* (the next one longer by as much: the grid kept) */
        if (t->jump_to && !start) {                     /* (0.6.42) JUMP: the step it names */
            t->seq_idx = (uint16_t)((t->jump_to - 1u) % (len ? len : 1u));
            t->rev = 0;
        } else if (t->rev && !start && t->seq_idx > 1u) {   /* REV: back towards step 1 */
            t->seq_idx--;
        } else if (t->rev && !start) {
            t->seq_idx = 0;
            t->rev = 0;
        } else {
            t->seq_idx = (uint16_t)((t->seq_idx + 1u) % (len ? len : 1u));
            if (!t->seq_idx && !start) {                /* (0.6.62) the pattern wrapped by itself: a new pass */
                seq_unhold(t);                          /* (2026-11-10) HOLD ends with the pass */
                fl_reset((uint32_t)(t - trk));
                fl_pass[(t - trk) % NTRK]++;
            }
        }
        if (t->hold_left && !start && !--t->hold_left)
            seq_unhold(t);                              /* (2026-11-10) HOLD: its steps are over */
        t->jump_to = 0;
        if (start) t->rev = 0;
        if (!t->seq_idx) t->tonal = 0;                  /* (0.6.42) TONAL lasts to the loop's end */
        if (!t->seq_idx && !start && t->loops < 0xFFFFu)
            t->loops++;                                 /* (0.6.39) one more loop of the pattern (TRIG) */
        rec_hold(t, t->seq_idx, len ? len : 1u);
        if (trk_index(t) < NPART && dxo_n[trk_index(t)])
            dxo_rec_hold(t, t->seq_idx);                /* (0.8.5) a held ratchet records into it */
        {
            const step_t *s = &seq_steps(t)[(t->pfx & PFX_RND) && !(rng() & 1u)   /* (Yehnah) RANDOM held: half the */
                                            ? rng() % (len ? len : 1u) : seq_src(t, t->seq_idx)];   /* steps, any */
            uint32_t skip = SEQ_ROLLED, i, k;
            uint8_t ja = 0;
            t->mic = (int8_t)ST_MICRO(s);               /* (0.6.88) its own TIMING (late: seq_step) */
            s = comp_lane(t, s);                        /* (0.6.47) C.LEN: the lane's component on it */
            /* the chance first (the one roll, as seq_step made it): a step that does not play applies no lock; the
             * automation and the locks before the notes, so a note-on reads them (eng_drum's KIT, ..) */
            if (!step_rolls(t, s))
                skip = SEQ_MISS;
            motion_step(t, t->seq_idx, &motion, skip == SEQ_ROLLED);   /* (a song: its section's motion is the motion) */
            if (t->rskip_n && t->rskip_idx == t->seq_idx) {
                for (k = 0; k < t->rskip_n; k++) {
                    for (i = 0; i < s->n; i++)
                        if (s->note[i] == t->rskip[k])
                            skip |= 1u << i;
                    for (i = 0; i < NLANE; i++)
                        if (((s->hit >> i) & 1u) && DRUM_LANE_NOTE[i] == t->rskip[k])
                            skip |= 1u << (8u + i);
                }
                t->rskip_n = 0;
            }
            seq_step(t, s, period, skip);
            if (skip != SEQ_MISS && CP_HAS(t, s, CP_JUMP, &ja) && fl_ok(t, CP_JUMP, ja))
                t->jump_to = (uint8_t)(CP_JTO(ja) % NSTEP + 1u);   /* (0.6.42) then that step */
            else if (skip != SEQ_MISS && CP_HAS(t, s, CP_REV, &ja) && t->seq_idx > 0u && fl_ok(t, CP_REV, ja))
                t->rev = 1;                             /* (0.6.42) then backwards to step 1 */
        }
    }
    if (t->rat_left)
        seq_ratchet(t, period);
    if (t->lr_on)
        seq_lane_rat(t, period, n);
}

/* MIDI in: the track a channel plays (0..15): G_ROUTE CH1-4 (0) channels 1..4 their parts (5..16 never get
 * here: midi_event drops them), SEL (1) every channel the selected track */
static uint8_t midi_route;                 /* the G_ROUTE events_block last saw (a change to CH1-4: midi_route_ch14) */
static track_t *midi_track(uint32_t ch) { return ch < NPART && !song.g[G_ROUTE] ? &trk[ch] : TSEL; }

#include "midi_control.c"
#include "midi_clock.c"
/* (chord-harp fork) REC > ONE PASS: track t leaves its last step for step 1: the pass is whole, recording and the
 * transport stop (1: stopped); on another clock's transport, recording only */
static __attribute__((noinline)) int rec_pass_end(track_t *t, uint32_t len)
{
    if (rp_state != RP_REC || rp_trk != trk_index(t) + 1u || t->seq_idx + 1u < len || t->jump_to || t->rev)
        return 0;
    song.rec &= (uint8_t)~(1u << trk_index(t));
    rp_state = RP_OFF;
    rp_done = 1;
    if (midi_clock.mode)
        return 0;
    seq_stop();
    return 1;
}

static void cin_begin(void);
/* (chord-harp fork) RP_CIN: track t leaves its last step for step 1: the transport stops, the track is armed (ONE PASS:
 * RP_REC, its pass then as ever) and the count-in starts; it starts the sequencer from step 1 (1: stopped) */
static __attribute__((noinline)) int rec_cin_end(track_t *t, uint32_t len)
{
    uint32_t ti = trk_index(t), once = rp_once;
    if (rp_state != RP_CIN || rp_trk != ti + 1u || t->seq_idx + 1u < len || t->jump_to || t->rev)
        return 0;
    rp_state = RP_OFF;
    seq_stop();
    song.rec |= (uint8_t)(1u << ti);
    rp_trk = (uint8_t)(ti + 1u);
    rp_state = once ? RP_REC : RP_OFF;
    cin_begin();
    return 1;
}

/* the count-in: its first beat now (accented) */
static __attribute__((noinline)) void cin_begin(void)
{
    cin_total = cin_left = (uint8_t)(4u * cin_bars);
    cin_pos = 0;
    memset(cin_n, 0, sizeof cin_n);
    click_req = 2;
}
/* n samples on: a beat ends; the last one starts the sequencer (in this block, before seq_tick: step 0 with it) */
static __attribute__((noinline)) void cin_block(uint32_t n)
{
    uint32_t b = click_beat_len();
    cin_pos += n;
    if (cin_pos < b)
        return;
    cin_pos -= b;
    cin_pos = cin_pos < b ? cin_pos : 0u;
    if (--cin_left) {
        click_req = (uint8_t)((cin_total - cin_left) % 4u ? 1u : 2u);
        return;
    }
    seq_start();
    cin_flush = 1;
}
/* the count-in's kept notes onto step 1, which has just started (events_block, after seq_tick): as if played now */
static __attribute__((noinline)) void cin_record(void)
{
    uint32_t i, k;
    cin_flush = 0;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        for (k = 0; k < cin_n[i]; k++)
            if (rec_on(t)) {
                dx_in_rat = (uint8_t)(cin_oct[i][k] & 15u);
                dx_in_tch = (uint8_t)(cin_oct[i][k] >> 4);
                rec_note(t, cin_note[i][k], cin_vel[i][k]);
                dx_in_rat = dx_in_tch = 0;
                if ((cin_up[i] >> k) & 1u)
                    rec_release(t, cin_note[i][k]);
            }
        cin_n[i] = 0;
    }
}
/* the metronome's beat: adv samples on while the transport runs (click.c sounds it in this block) */
static void click_tick(uint32_t adv)
{
    uint32_t p = div_samples(2), beat = 0;
    if (clk_pos == CLK_START) {
        clk_pos = 0;
        clk_step = 0;
        clk_n = 0;
        beat = 2;
    } else {
        clk_pos += adv;
        while (clk_pos >= p) {                      /* (as seq_tick: the remainder kept) */
            clk_pos -= p;
            clk_step = (clk_step + 1u) & 15u;
            clk_n++;
            if (!(clk_step & 3u))
                beat = clk_step ? 1u : 2u;
        }
    }
    if (beat && click_wanted())
        click_req = (uint8_t)beat;
}
/* the beat of the bar the click is on (0..3: 0 the accented first), -1 stopped: the count-in's, else the playing
 * transport's (clk_step, internal or external clock). It moves in the block a beat clicks in (the header's metronome,
 * ui_draw.c, swings on it; CLICK OFF it moves all the same) */
static int32_t click_beat_now(void)
{
    if (cin_left)
        return (int32_t)((uint32_t)(cin_total - cin_left) % 4u);
    if (!song.playing)
        return -1;
    return clk_pos == CLK_START ? 0 : (int32_t)(clk_step >> 2);
}

/* everything that happens between two rendered blocks */
static void events_block(uint32_t n)
{
    uint32_t i, pr, fo, seq_n = n;
    uint32_t clock_mode = (uint32_t)song.g[G_CLOCK];
    if (midi_clock.mode != clock_mode) {
        seq_stop();
        memset(&midi_clock, 0, sizeof midi_clock);
        midi_clock.mode = (uint8_t)clock_mode;
        midi_beat_samples = 0;
    }
    mo_block(n);                                    /* (0.6.72) MIDI OUT: its setting, the clock */
    if (cin_left && !clock_mode)
        cin_block(n);
    if (transport_req == 1u) {
        if (clock_mode)
            midi_clock_transport(0xFAu, fm1_ms);
        else if (cin_left)
            ;                                       /* counting in already (the UI's PLAY stops it: seq_counting) */
        else if (cin_bars && song.rec && !song.playing && !chain.armed)
            cin_begin();                            /* recording from stop: the count-in first */
        else
            seq_start();
        transport_req = 0;
    } else if (transport_req == 2u) {
        seq_stop();
        transport_req = 0;
    } else if (transport_req == 3u) {                 /* REC + E4 RESTART: from the top without stopping (INT clock) */
        if (!clock_mode) {
            seq_stop();
            seq_start();
        }
        transport_req = 0;
    }
    if (midi_route != (uint8_t)song.g[G_ROUTE]) {     /* ROUT changed: CH1-4 lets go of channels 5..16 */
        midi_route = (uint8_t)song.g[G_ROUTE];
        if (!midi_route)
            midi_route_ch14();
    }
    pr = panic_req;
    panic_req = 0;
    if (pr)
        mo_flush_of(pr);                               /* (0.6.72) their MIDI OUT notes too */
    fo = pr;                                           /* (0.6.13) sound loads: what sounds fades on the old sound */
    if (midi_in_overflow) {                            /* a lost note-off must never leave a held note */
        mi_r = mi_w;
        memset(midi_sel_on, 0, sizeof midi_sel_on);
        memset(midi_ch, 0, sizeof midi_ch);
        memset(midi_owners, 0, sizeof midi_owners);
        midi_hint = 0;
        for (i = 0; i < NTRK; i++) {
            trk[i].rh_n = trk[i].rskip_n = 0;
            trk[i].seq_n = trk[i].seq_hold = trk[i].slide_glide = trk[i].rat_left = 0;
        }
        pr |= (1u << NTRK) - 1u;
        RING_PUBLISH();
        midi_in_overflow = 0;
    }
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        harp_leave(t, i);                               /* (chord-harp) out of HARP: its LATCH off */
        if (((latch_drop_req >> i) & 1u) || (((latch_was >> i) & 1u) && !key_latch(t))) {
            latch_drop_req &= (uint8_t)~(1u << i);  /* (0.6.19) a clear, or LATCH left (HOLD off, ARP on, whatever
                                                     * the keys: a key down kept it all sounding, and stacking) */
            latch_drop(t);
        }
        latch_was = (uint8_t)(key_latch(t) ? latch_was | 1u << i : latch_was & ~(1u << i));
        if ((pr >> i) & 1u) {
            if (i < NPART)
                dxo_n[i] = 0;                       /* (0.8.5) the held ratchets stop */
            if ((latch_was >> i) & 1u)
                latch_release(t);                   /* (0.6.19: MIDI OUT's latched notes end too) */
            midi_forget_track(i);
            {   /* (0.6.41) another engine: what is held sounds on in it (kept past the release, which forgets) */
                uint8_t hn[4], hv[4];
                uint32_t hc = t->eng_req != t->engine ? xf_held(t, hn, hv) : 0u, h;
                trk_all_off(t);
                for (h = 0; h < hc; h++) { t->xp_note[h] = hn[h]; t->xp_vel[h] = hv[h]; }
                t->xp_n = (uint8_t)hc;
            }
            if ((fo >> i) & 1u)
                trk_fade_out(t);                        /* (0.6.13: a sound load: on the old values, ~3 ms; voice.c) */
            t->nheld = 0;
            t->arp_phys = 0;
            arp_forget(t);
        }
        engine_block(t);                              /* engine switch: fade, then switch (voice.c) */
        /* ARP turned off, or HOLD released with no key down: drop the latched chord */
        if ((t->armp && !t->p[P_AMODE]) || (t->aholdp && !t->p[P_AHOLD] && !t->arp_phys)) {
            if (!t->armp && t->aholdp && !t->p[P_AHOLD])
                latch_release(t);                       /* (0.6.14) LATCH off: its notes go */
            t->nheld = 0;
            if (!t->p[P_AMODE])
                t->arp_phys = 0;
            if (t->arp_note)
                arp_silence(t);
        }
        t->armp = t->p[P_AMODE];
        t->aholdp = t->p[P_AHOLD];
    }
    keyboard_block();
    while (mi_r != mi_w) {                            /* USB-MIDI (and TRS) in */
        uint32_t at = mi_r % MQ, pkt = midi_in_q[at], status = (pkt >> 8) & 0xFFu;
        uint32_t st = status & 0xF0u, ch = status & 0x0Fu;
        uint32_t d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        uint32_t source = midi_in_source[at] ? midi_in_source[at] : 1u, ms = midi_in_ms[at];
        mi_r++;
        if (status >= 0xF8u) {
            if (clock_mode == source) {
                if (status == 0xF8u)
                    midi_clock_pulse(ms);
                else
                    midi_clock_transport(status, ms);
            }
        } else
            midi_event(st, ch, d1, d2);
    }
    if (clock_mode) {
        seq_n = 0;
        if (song.playing) {
            uint32_t last = midi_clock.have_pulse ? midi_clock.last_ms : midi_clock.start_ms;
            if (fm1_ms - last > 500u) {
                seq_stop();                         /* cable loss must not leave a running held note */
                midi_clock.tempo_valid = 0;
            } else
                seq_n = midi_clock_advance(fm1_ms);
        }
    }
    chain_tick(seq_n);
    for (i = 0; i < NTRK; i++)
        seq_tick(&trk[i], seq_n);
    for (i = 0; i < NPART; i++)                       /* (0.8.5) the kit's octaves held: their ratchets */
        if (dxo_n[i])
            dxo_tick(&trk[i], clock_mode ? seq_n : n);
    if (cin_flush)
        cin_record();
    if (song.playing)
        click_tick(seq_n);
    for (i = 0; i < NPART; i++)
        arp_step(&trk[i], clock_mode ? seq_n : n, clock_mode && song.playing ? seq_n : n);
#if FELUCCA_AMBIENT
    for (i = 0; i < NTRK; i++)                        /* (chord-harp fork) ARP > AMBIENT */
        if (trk[i].p[P_AMB] || amb_l[i].note[0] || amb_l[i].note[1])
            amb_tick(&trk[i], n);
#endif
    beat_pos += clock_mode ? seq_n : n;               /* the beat the ARP LED flashes on (ui_leds) */
    if (beat_pos >= beat_samples()) {
        beat_pos -= beat_samples();
        beat_pos = beat_pos < beat_samples() ? beat_pos : 0u;
        beat_n = (beat_n + 1u) & 3u;
        if (!beat_n && song.playing)
            bar_n++;
    }
    if (song.playing)
        song.tick++;
}

#include "song_play.c"                           /* the song: the ISR's part (song_chain.c) */
