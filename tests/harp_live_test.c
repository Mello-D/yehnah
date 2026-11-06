/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp) SCL > CHORD HARP played on the real keyboard path (seq.c keyboard_block): a chord key sounds its chord
 * whole (6 notes), the plate's white keys the last chord's tones one a key, its black keys silent, ROOT and the octave
 * move both, MONO the root alone, LATCH (the top F#) holds the chords and never the plate. Checked on MIDI OUT's
 * notes, as scale_test.c does */
#include <assert.h>
#include <string.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint32_t ons[64], offs[64], non, noff;
static void mo_take(void)                   /* MIDI OUT's notes since the last look: ons[], offs[] */
{
    non = noff = 0;
    while (mo_r != mo_w) {
        uint32_t e = midi_out_q[mo_r++ % (sizeof midi_out_q / sizeof midi_out_q[0])], st = (e >> 8) & 0xF0u;
        if (st == 0x90u && (e >> 24))
            ons[non++] = (e >> 16) & 127u;
        else if (st == 0x80u || st == 0x90u)
            offs[noff++] = (e >> 16) & 127u;
    }
}
static void keys(uint32_t m)
{
    fm1_in.notes = m;
    keyboard_block();
    mo_take();
}
static int same(const uint32_t *a, uint32_t na, const uint8_t *b, uint32_t nb)   /* the same notes, any order */
{
    uint32_t i, j;
    if (na != nb)
        return 0;
    for (i = 0; i < nb; i++) {
        for (j = 0; j < na && a[j] != b[i]; j++)
            ;
        if (j == na)
            return 0;
    }
    return 1;
}

static uint32_t sounding(const track_t *t, uint32_t *o)   /* the notes t's voices sound, gated, into o */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate)
            o[n++] = t->v[i].note;
    return n;
}

static void upto(track_t *t, uint32_t idx)          /* on till the sequencer enters step idx */
{
    uint32_t i;
    for (i = 0; i < 100000u && t->seq_idx != idx; i++)
        events_block(CTL);
    assert(t->seq_idx == idx);
    mo_take();
}

static const uint8_t PLATE_KEYS[9] = {12, 14, 16, 18, 19, 21, 23, 24, 26}, SILENT_KEYS[5] = {13, 15, 17, 20, 22};

int main(void)
{
    track_t *t = &trk[0];
    uint8_t want[HARP_NMAX], pl[HARP_NPLATE];
    uint32_t n, i, k, set, r;
    host_tracks_init();
    kb_prev = 0;
    usb.config = 1;
    mo_w = mo_r = 0;
    assert(t->p[P_VOICE] == V_POLY && !t->p[P_AMODE] && !t->p[P_AHOLD]);
    assert(CHORD_DESC[0].max == CD_PROG && track_desc(t, P_CVOI) == &CHORD_DESC[1]);
    t->p[P_CHRD] = CD_HARP;
    assert(track_desc(t, P_CVOI) == &HARP_SET_DESC);   /* VOICING is the SET in HARP */
    for (k = 0; k < 27u; k++)                           /* the chord keys, the black keys over them: not single notes */
        if (k < HARP_NCH || key_black(k))
            assert(kb_map(t, k) == KB_SILENT);

    for (set = 0; set < HARP_NSET; set++)               /* every chord of every set whole, in three ROOTs */
        for (r = 0; r < 12u; r += 5u) {
            t->p[P_CVOI] = (int16_t)set;
            t->p[P_ROOT] = (int16_t)r;
            for (k = 0; k < HARP_NCH; k++) {
                n = harp_chord(set, k, (int32_t)r, want);
                keys(1u << k);
                assert(same(ons, non, want, n) && !noff);
                assert(harp_cur[0] == k);
                harp_plate(set, k, (int32_t)r, pl);     /* the plate follows it, the chord key still down or not */
                for (i = 0; i < 9u; i++)
                    assert(kb_map(t, PLATE_KEYS[i]) == pl[i]);
                keys(0);
                assert(same(offs, noff, want, n) && !non);
            }
        }
    puts("harp: every chord whole on its key (up to 6 notes), let go whole, in ROOT C F A#: ok");

    t->p[P_CVOI] = 7;                                   /* MAJ8 */
    t->p[P_ROOT] = 0;
    keys(1u << 11);                                     /* G7(b9,b13): six notes */
    assert(non == 6u);
    keys(0);
    harp_plate(7, 11, 0, pl);
    for (i = 0; i < 9u; i++) {                          /* the plate after the chord key is up: one note a key */
        keys(1u << PLATE_KEYS[i]);
        assert(non == 1u && ons[0] == pl[i]);
        keys(0);
        assert(noff == 1u && offs[0] == pl[i]);
    }
    for (i = 0, k = 0; i < 9u; i++)                     /* the whole plate swept at once */
        k |= 1u << PLATE_KEYS[i];
    keys(k);
    assert(non == 9u);
    keys(0);
    for (i = 0; i < 5u; i++) {                          /* the black keys over the chords: silent */
        keys(1u << SILENT_KEYS[i]);
        assert(!non && !noff && harp_cur[0] == 11u);
        keys(0);
        assert(!non && !noff);
    }
    song.octave = 1;                                    /* the octave moves chords and plate */
    n = harp_chord(7, 2, 0, want);
    keys(1u << 2);
    for (i = 0; i < n; i++)
        want[i] = (uint8_t)(want[i] + 12);
    assert(same(ons, non, want, n));
    keys(0);
    harp_plate(7, 2, 0, pl);
    keys(1u << 12);
    assert(non == 1u && ons[0] == pl[0] + 12u);
    keys(0);
    song.octave = 0;
    puts("harp: the plate follows the last chord, one note a key, swept whole; black keys silent; the octave: ok");

    t->p[P_VOICE] = V_MONO;                             /* MONO: the chord's root alone */
    keys(1u << 3);
    assert(non == 1u && ons[0] == (uint32_t)harp_lo(7, 3, 0));
    keys(0);
    t->p[P_VOICE] = V_POLY;
    puts("harp: MONO plays the chord's root alone: ok");

    keys(1u << HARP_KLATCH);                            /* LATCH on: silent, ARP 2's HOLD */
    assert(!non && t->p[P_AHOLD] == 1 && key_latch(t));
    events_block(CTL);                                  /* (a block: events_block sees LATCH on) */
    keys(0);
    assert(!noff && t->p[P_AHOLD] == 1);
    n = harp_chord(7, 0, 0, want);
    keys(1u);                                           /* a chord, let go: it keeps sounding */
    assert(same(ons, non, want, n));
    keys(0);
    assert(!noff && t->nheld == n && !t->arp_phys);
    keys(1u << 12);                                     /* the plate over it: never latched, the chord stays */
    assert(non == 1u);
    keys(0);
    assert(noff == 1u && t->nheld == n);
    keys(1u << 1);                                      /* the next chord lets the last one go */
    assert(same(offs, noff, want, n) && non == harp_chord(7, 1, 0, want));
    keys(0);
    assert(!noff);
    t->p[P_CVOI] = 15;                                  /* MIN8's Am11 latched (six notes), three plate keys at once: */
    n = harp_chord(15, 8, 0, want);                     /* all three sound, none a note the chord holds, and the chord */
    keys(1u << 8);                                      /* keeps its notes when they are let go (but what the voices
                                                         * could not hold) */
    keys(0);
    keys(1u << 12 | 1u << 14 | 1u << 16);
    assert(non == 3u);
    for (i = 0; i < 3u; i++)
        for (k = 0; k < n; k++)
            assert(ons[i] != want[k]);
    keys(0);
    assert(noff == 3u);
    for (k = 0, r = 0; k < n; k++) {                    /* (6 + 3 over the 8 voices: the oldest one taken, never */
        for (i = 0; i < NVOICE && !(t->v[i].active && t->v[i].gate && t->v[i].note == want[k]); i++)
            ;                                           /* the bass; the rest of the chord keeps sounding) */
        r += i < NVOICE;
        assert(k || i < NVOICE);
    }
    assert(r == n - (n + 3u > voice_cap() ? n + 3u - voice_cap() : 0u));
    keys(1u << 1);                                      /* (back to MAJ8's chord 2 latched, as below expects) */
    keys(0);
    t->p[P_CVOI] = 7;
    keys(1u << 1);
    keys(0);
    keys(1u << HARP_KLATCH);                            /* LATCH off: events_block lets it go */
    assert(t->p[P_AHOLD] == 0 && !key_latch(t));
    keys(0);
    events_block(CTL);
    mo_take();
    assert(noff == harp_chord(7, 1, 0, want) && t->nheld == 0);
    n = harp_chord(7, 5, 0, want);                      /* LATCH on while a chord is down: it stays when let go */
    keys(1u << 5);
    keys(1u << 5 | 1u << HARP_KLATCH);
    assert(t->p[P_AHOLD] == 1 && t->arp_phys == 1u && t->nheld == n);
    events_block(CTL);
    keys(0);
    assert(!noff);
    keys(1u << HARP_KLATCH);
    keys(0);
    events_block(CTL);
    mo_take();
    assert(same(offs, noff, want, n));
    puts("harp: LATCH holds the chords till the next one, never the plate, on with a chord down, off lets it go: ok");
    /* leaving HARP lets LATCH go: HOLD back off, the latched chord ends, the keys no longer latch; a HOLD set by hand
     * on the ARP 2 page stays when HARP is left */
    n = harp_chord(7, 2, 0, want);
    keys(1u << HARP_KLATCH);
    keys(0);
    keys(1u << 2);
    keys(0);
    events_block(CTL);
    mo_take();
    assert(key_latch(t) && !noff && t->nheld == n);
    t->p[P_CHRD] = CD_OFF;                              /* (the CHORD page: HARP to OFF) */
    events_block(CTL);
    events_block(CTL);
    mo_take();
    assert(t->p[P_AHOLD] == 0 && !key_latch(t) && same(offs, noff, want, n) && t->nheld == 0);
    t->p[P_CHRD] = CD_HARP;
    events_block(CTL);
    t->p[P_AHOLD] = 1;                                  /* HOLD by hand (ARP 2), not LATCH */
    events_block(CTL);
    t->p[P_CHRD] = CD_OFF;
    events_block(CTL);
    assert(t->p[P_AHOLD] == 1);
    t->p[P_AHOLD] = 0;
    t->p[P_CHRD] = CD_HARP;
    events_block(CTL);
    mo_take();
    puts("harp: leaving HARP turns its LATCH off (the chord let go); a HOLD set by hand stays: ok");

    /* recording (phase 4): a chord key into the step as its CHORD (the root and the chord's id), a plate key over it as
     * a note; the step plays the chord back whole with the plate note, STRUM all of it, MONO
     * the step's own notes */
    memset(t->step, 0, sizeof t->step);
    t->p[P_SLEN] = 4;
    t->p[P_CVOI] = 15;                                  /* MIN8: Am11, six notes */
    seq_start();
    events_block(CTL);                                  /* (in step 1) */
    song.rec = 1;
    keys(1u << 8);
    n = harp_chord(15, 8, 0, want);
    assert(same(ons, non, want, n));
    assert(t->step[0].time == ST_NOTE && t->step[0].n == 1u && t->step[0].note[0] == (uint32_t)harp_lo(15, 8, 0));
    assert(t->step[0].comp == CP_CHORD && t->step[0].carg == 15u * HARP_NCH + 8u);
    keys(0);
    harp_plate(15, 8, 0, pl);
    keys(1u << 12);                                     /* the plate's first note: over it, a note */
    keys(0);
    assert(t->step[0].n == 2u && t->step[0].note[1] == pl[0] && t->step[0].comp == CP_CHORD);
    song.rec = 0;
    seq_stop();
    mo_take();
    {
        uint32_t got[16], period = div_samples((uint32_t)t->p[P_SDIV]);
        uint8_t all[HARP_NMAX + 1];
        char name[16];
        memcpy(all, want, n);
        all[n] = pl[0];
        seq_start();                                    /* played back: the chord whole and the plate note */
        events_block(CTL);
        for (i = 0; i < t->seq_n; i++) got[i] = t->seq_notes[i];
        assert(same(got, t->seq_n, all, n + 1u));
        assert(same(got, sounding(t, got), all, n + 1u));   /* (their voices) */
        seq_stop();
        t->p[P_CSTR] = 40;                              /* STRUM: every one of the 7, one after another (was 5 at most) */
        seq_start();
        events_block(CTL);
        assert(sounding(t, got) == 1u);
        {                                               /* (the notes that come in over the step, all of them) */
            uint32_t seen[16], ns = 0, c, q, j;
            for (k = 0; k < period / CTL; k++) {
                events_block(CTL);
                for (q = 0, c = sounding(t, got); q < c; q++) {
                    for (j = 0; j < ns && seen[j] != got[q]; j++)
                        ;
                    if (j == ns)
                        seen[ns++] = got[q];
                }
            }
            assert(same(seen, ns, all, n + 1u));
        }
        seq_stop();
        t->p[P_CSTR] = 0;
        t->p[P_VOICE] = V_MONO;                         /* MONO: the step's own notes (the root, the plate note) */
        seq_start();
        events_block(CTL);
        assert(t->seq_n == 2u && t->seq_notes[0] == t->step[0].note[0] && t->seq_notes[1] == pl[0]);
        seq_stop();
        t->p[P_VOICE] = V_POLY;
        t->p[P_ROOT] = 2;                               /* the step keeps its chord (as a note keeps its pitch) */
        t->step[0].note[0] += 2;                        /* moved (the step editor, TONAL): the chord with it, Bm11 */
        t->step[0].note[1] += 2;
        seq_start();
        events_block(CTL);
        for (i = 0; i <= n; i++) all[i] = (uint8_t)(all[i] + 2u);
        for (i = 0; i < t->seq_n; i++) got[i] = t->seq_notes[i];
        assert(same(got, t->seq_n, all, n + 1u));
        seq_stop();
        step_chord_name(&t->step[0], t->step[0].carg, name, sizeof name);
        assert(!strcmp(name, "Bm11"));
        t->step[0].note[0] -= 2;
        t->step[0].note[1] -= 2;
        t->step[0].note[1] = (uint8_t)want[n - 1u];     /* a note the chord sounds: once */
        seq_start();
        events_block(CTL);
        for (i = 0; i < t->seq_n; i++) got[i] = t->seq_notes[i];
        assert(same(got, t->seq_n, want, n));
        seq_stop();
        t->step[0].note[1] = pl[0];
        t->p[P_ROOT] = 0;
        mo_take();
    }
    song.rec = 1;                                       /* a plain note into an empty step after it: no CHORD left */
    t->step[1].comp = CP_CHORD;
    t->step[1].carg = 3;
    t->seq_pos = 0x7FFFFFFFu;
    t->seq_idx = 1;
    rec_note(t, 60, 100);
    assert(t->step[1].n == 1u && t->step[1].comp == 0u);
    song.rec = 0;
    puts("harp: a chord key records its CHORD, the plate over it; played back whole, strummed, MONO, moved: ok");

    /* a chord held over the whole pattern, a plate note over it in step 2: the chord goes on under it (step 2 the chord
     * and the note, steps 1 and 2 sliding on, step 3 the chord again, 4 a TIE; round at step 1 the hold ends, as a note
     * held over the pattern); played back, the chord's voices are struck once a pass, the plate note in step 2 alone */
    memset(t->step, 0, sizeof t->step);
    seq_start();
    events_block(CTL);
    song.rec = 1;
    keys(1u << 8);                                      /* step 1: the chord down */
    upto(t, 1);
    assert(t->step[1].time == ST_TIE);
    keys(1u << 8 | 1u << 12);                           /* step 2: a plate note over it */
    keys(1u << 8);
    upto(t, 2);
    upto(t, 3);
    upto(t, 0);                                         /* round again, the chord still down, let go there */
    keys(0);
    song.rec = 0;
    seq_stop();
    mo_take();
    {
        const uint8_t root = (uint8_t)harp_lo(15, 8, 0);
        const step_t *st = t->step;
        assert(st[0].time == ST_NOTE && st[0].n == 1u && st[0].note[0] == root && st[0].comp == CP_CHORD);
        assert(st[1].time == ST_NOTE && st[1].n == 2u && st[1].note[0] == root && st[1].note[1] == pl[0] &&
               st[1].comp == CP_CHORD && st[1].carg == st[0].carg);
        assert(st[2].time == ST_NOTE && st[2].n == 1u && st[2].note[0] == root && st[2].comp == CP_CHORD);
        assert(st[3].time == ST_TIE);
        assert((st[0].flags & SF_SLIDE) && (st[1].flags & SF_SLIDE) && !(st[2].flags & SF_SLIDE) && !(st[3].flags & SF_SLIDE));
    }
    {
        uint32_t age[HARP_NMAX], got[16], c, q, pass;
        seq_start();
        events_block(CTL);
        c = sounding(t, got);
        assert(same(got, c, want, n));
        for (i = 0; i < n; i++)                         /* each chord note's voice, as struck */
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].active && t->v[k].gate && t->v[k].note == want[i])
                    age[i] = t->v[k].age;
        for (pass = 0; pass < 1u; pass++)
            for (q = 1; q < 4u; q++) {
                upto(t, q);
                events_block(CTL);
                c = sounding(t, got);
                for (i = 0; i < n; i++) {               /* the chord: the same voices, never struck again */
                    for (k = 0; k < NVOICE && !(t->v[k].active && t->v[k].gate && t->v[k].note == want[i]); k++)
                        ;
                    assert(k < NVOICE && t->v[k].age == age[i]);
                }
                for (k = 0; k < c && got[k] != pl[0]; k++)
                    ;
                assert((k < c) == (q == 1u));           /* the plate note in step 2 alone */
                assert(c == n + (q == 1u));
            }
        seq_stop();
    }
    puts("harp: a chord held over the pattern with a plate note over it: recorded and played back unbroken: ok");

    /* LATCH: a latched chord records as held (what is heard is recorded): struck and let go in step 1, a plate note in
     * step 3 over it, the next chord in step 4 ends it; LATCH off ends a latched chord's hold too */
    memset(t->step, 0, sizeof t->step);
    keys(1u << HARP_KLATCH);
    keys(0);
    assert(key_latch(t));
    seq_start();
    events_block(CTL);
    song.rec = 1;
    keys(1u << 8);                                      /* step 1: Am11, let go: it sounds on */
    keys(0);
    upto(t, 1);
    assert(t->step[1].time == ST_TIE);
    upto(t, 2);
    keys(1u << 12);                                     /* step 3: a plate note over it */
    keys(0);
    upto(t, 3);
    keys(1u << 1);                                      /* step 4: the next chord, let go */
    keys(0);
    {
        const step_t *st = t->step;
        const uint8_t root = (uint8_t)harp_lo(15, 8, 0);
        assert(st[0].time == ST_NOTE && st[0].note[0] == root && st[0].comp == CP_CHORD && !(st[0].flags & SF_SLIDE));
        assert(st[1].time == ST_TIE && (st[1].flags & SF_SLIDE));
        assert(st[2].time == ST_NOTE && st[2].n == 2u && st[2].note[0] == root && st[2].note[1] == pl[0]);
        assert(!(st[2].flags & SF_SLIDE));              /* (the next chord is another: struck, no slide into it) */
        assert(st[3].time == ST_NOTE && st[3].carg == 15u * HARP_NCH + 1u && st[3].note[0] == (uint32_t)harp_lo(15, 1, 0));
    }
    upto(t, 0);                                         /* round: chord 2 still latched, its hold over step 1 (chord 1 */
    assert(t->step[0].time == ST_TIE && hh_id[0]);      /* sounds no more: what is heard) */
    upto(t, 3);                                         /* round to its own step: the hold ends, chord 2 kept there */
    assert(!hh_id[0] && t->step[3].time == ST_NOTE && t->step[3].carg == 15u * HARP_NCH + 1u);
    keys(1u << 2);                                      /* chord 3 latched in step 4 .. */
    keys(0);
    assert(hh_id[0] && t->rh_n);
    keys(1u << HARP_KLATCH);                            /* .. LATCH off: the latched chord and its hold end */
    keys(0);
    events_block(CTL);
    assert(!hh_id[0] && !t->rh_n);
    upto(t, 0);
    assert(t->step[0].time == ST_TIE);                  /* (chord 2's, from the pass before: nothing written now) */
    song.rec = 0;
    seq_stop();
    mo_take();
    puts("harp: LATCH: a latched chord records as held, the plate over it, till the next chord or LATCH off: ok");
    {   /* (2026-11-04, the user's: a latched chord recorded only the steps its key was down) SCL > CHORD's chords (no
         * HARP id: their notes, rec_note) latched: let go in step 1 they record on as held, TIEs, till the next chord;
         * LATCH off ends the hold */
        int16_t c0 = t->p[P_CHRD];
        memset(t->step, 0, sizeof t->step);
        t->p[P_CHRD] = CD_TRI;
        keys(1u << HARP_KLATCH);
        keys(0);
        assert(key_latch(t));
        seq_start();
        events_block(CTL);
        song.rec = 1;
        keys(1u << 1);                                  /* step 1: a chord, let go at once (the pattern: 4 steps) */
        keys(0);
        upto(t, 1);
        upto(t, 2);
        assert(t->step[0].time == ST_NOTE && t->step[0].n > 1u);
        assert(t->step[1].time == ST_TIE && t->step[2].time == ST_TIE);
        upto(t, 3);
        keys(1u << 2);                                  /* step 4: the next chord ends it */
        keys(0);
        assert(t->step[3].time == ST_NOTE && t->step[3].n > 1u && t->step[2].time == ST_TIE);
        upto(t, 0);                                     /* round: it holds over step 1 */
        assert(t->step[0].time == ST_TIE);
        keys(1u << HARP_KLATCH);                        /* LATCH off: the hold ends */
        keys(0);
        events_block(CTL);
        assert(!t->rh_n);
        upto(t, 1);
        assert(t->step[0].time == ST_NOTE && t->step[1].time == ST_TIE);   /* (off early in step 1: not held into it,
                                                         * the first chord back there; step 2 its TIE, left) */
        upto(t, 2);
        assert(t->step[2].time == ST_TIE && t->step[3].time == ST_NOTE);
        song.rec = 0;
        seq_stop();
        mo_take();
        t->p[P_CHRD] = c0;
        puts("harp: LATCH: SCL > CHORD's latched chords record as held till the next chord or LATCH off: ok");
    }

    {   /* (chord-harp fork) OMNI's AUTO: a chord key's notes the CHORD voice (quiet, under the strings), a plate key's a
         * HARP string; latched, the chord holds while a string rings down */
        static int32_t ob[2 * CTL];
        static int16_t p0[P_COUNT];
        uint32_t pn, q, held, strings;
        uint8_t e0 = t->eng_req, pr0 = t->preset;
        memcpy(p0, t->p, sizeof p0);
        host_preset(t, ENGI_OMNI, 0);                   /* (OMNICHORD: MODE AUTO) */
        t->engine = t->eng_req;
        assert(t->p[P_E0] == OM_AUTO);
        t->p[P_CHRD] = CD_HARP;
        t->p[P_CVOI] = 7;
        t->p[P_AHOLD] = 0;
        n = harp_chord(7, 2, 0, want);
        keys(1u << 2);
        events_block(CTL);
        pn = (uint32_t)kb_map(t, 12);
        keys(1u << 2 | 1u << 12);
        events_block(CTL);
        for (i = 0, held = strings = 0; i < NVOICE; i++) {
            const om_voice_t *o = &om_vs[0][i];
            if (!t->v[i].active || !t->v[i].gate)
                continue;
            for (k = 0; k < n && want[k] != t->v[i].note; k++)
                ;
            if (k < n)
                held += o->mode == OM_CHORD && o->quiet;
            else if (t->v[i].note == pn)
                strings += o->mode == OM_HARP;
        }
        assert(held == n && strings == 1u);
        keys(0);
        keys(1u << HARP_KLATCH);                        /* LATCH: the chord let go, the plate key struck and let go */
        keys(0);
        keys(1u << 2);
        keys(0);
        keys(1u << 12);
        keys(0);
        for (q = 0; q < 6u * FS / CTL; q++) {           /* 6 s */
            events_block(CTL);
            mix_block(ob, CTL);
        }
        for (i = 0, held = strings = 0; i < NVOICE; i++)
            if (t->v[i].active) {
                held += om_vs[0][i].mode == OM_CHORD;
                strings += om_vs[0][i].mode == OM_HARP;
            }
        assert(held == n && !strings);
        keys(1u << HARP_KLATCH);                        /* LATCH off: the chord goes */
        keys(0);
        for (q = 0; q < FS / CTL; q++) {
            events_block(CTL);
            mix_block(ob, CTL);
        }
        for (i = 0; i < NVOICE; i++)
            assert(!t->v[i].active);
        mo_take();
        t->eng_req = t->engine = e0;                    /* (the track as it was) */
        t->preset = pr0;
        memcpy(t->p, p0, sizeof p0);
        puts("harp: OMNI AUTO: a chord key the CHORD voice, the plate a string; latched, the chord holds, the string fades: ok");
    }
    {   /* a CHORD step with an ARP NOTES component: the arpeggio runs over the chord's six notes (was the step's stored
         * notes: the root alone) */
        uint32_t seen[16], ns = 0, got[16], c, q, j, period = div_samples((uint32_t)t->p[P_SDIV]);
        memset(t->step, 0, sizeof t->step);
        t->p[P_SLEN] = 1;
        t->step[0] = (step_t){{(uint8_t)harp_lo(15, 8, 0)}, 1, ST_NOTE};
        t->step[0].comp = CP_ARPN;
        t->step[0].carg = CP_ARG(0u, 3u, 0u);           /* UP, x6, one octave */
        t->step[0].comp2 = CP_CHORD;
        t->step[0].carg2 = 15u * HARP_NCH + 8u;
        n = harp_chord(15, 8, 0, want);
        seq_start();
        for (q = 0; q < period / CTL; q++) {
            events_block(CTL);
            for (i = 0, c = sounding(t, got); i < c; i++) {
                for (j = 0; j < ns && seen[j] != got[i]; j++)
                    ;
                if (j == ns && ns < 16u)
                    seen[ns++] = got[i];
            }
        }
        seq_stop();
        mo_take();
        assert(same(seen, ns, want, n));
        memset(t->step, 0, sizeof t->step);
        puts("harp: a CHORD step's ARP NOTES arpeggiates the chord's six notes: ok");
    }

    {   /* (chord-harp) CHORD OMNI, on HARP's keys: the lowest 12 FoMni's buttons (SET FOMNI, + C7) or the circle of
         * fifths (MAJ MIN 7 M7), each a triad in F#3..F4 over its root in C2..B2; the 9 plate keys the strings, root
         * third fifth folded into octave windows from F#3; the top F# LATCH; a chord key records its chord whole */
        static const char *const FOMNI_C[OMNI_NPAD] = {"F", "C", "G", "Dm", "Am", "Em", "G7", "E7", "D7", "Bb", "A7", "C7"};
        static const char *const ROW_M7[OMNI_NPAD] = {"Cm7", "Gm7", "Dm7", "Am7", "Em7", "Bm7", "F#m7", "C#m7", "Abm7",
                                                      "Ebm7", "Bbm7", "Fm7"};   /* (chord-harp fork: the root first) */
        static const uint8_t C_MAJ[4] = {36, 55, 60, 64}, C_STR[OMNI_NSTR] = {60, 64, 55, 72, 76, 67, 84, 88, 79};
        uint8_t sp[OMNI_NSTR], out[4 + NLANE], own[4 + NLANE];
        uint32_t whites[OMNI_NSTR], nw = 0, id;
        char nm[16];
        step_t st;
        t->p[P_CHRD] = CD_OMNI;
        t->p[P_CVOI] = 0;
        t->p[P_ROOT] = 0;
        t->p[P_VOICE] = V_POLY;
        t->p[P_AHOLD] = 0;
        assert(track_desc(t, P_CVOI) == &OMNI_SET_DESC);
        for (k = HARP_NCH; k < 27u; k++)                /* the strings: the white keys F4 .. G5 */
            if (!key_black(k))
                whites[nw++] = k;
        assert(nw == OMNI_NSTR && kb_map(t, HARP_KLATCH) == KB_SILENT);
        for (k = 0; k < OMNI_NPAD; k++) {               /* FoMni's buttons in C */
            hc_name(omni_id(0, k, 0), 0, nm);
            assert(!strcmp(nm, FOMNI_C[k]));
            hc_name(omni_id(4, k, 0), 0, nm);
            assert(!strcmp(nm, ROW_M7[k]));
        }
        assert(hc_chord(omni_id(0, 1, 0), 0, want) == 4u && !memcmp(want, C_MAJ, 4));   /* C: FoMni's voicing */
        hc_plate(omni_id(0, 1, 0), 0, sp);
        assert(!memcmp(sp, C_STR, OMNI_NSTR));
        for (set = 0; set < OMNI_NSET; set++)           /* every button of every set, in three ROOTs */
            for (r = 0; r < 12u; r += 5u) {
                t->p[P_CVOI] = (int16_t)set;
                t->p[P_ROOT] = (int16_t)r;
                for (k = 0; k < HARP_NCH; k++) {
                    id = omni_id(set, k, (int32_t)r);
                    n = hc_chord(id, 0, want);
                    assert(kb_map(t, k) == KB_SILENT);
                    keys(1u << k);
                    assert(same(ons, non, want, n) && !noff && harp_cur[0] == k);
                    hc_plate(id, 0, sp);
                    for (i = 0; i < OMNI_NSTR; i++)
                        assert(kb_map(t, whites[i]) == sp[i]);
                    keys(0);
                    assert(same(offs, noff, want, n) && !non);
                }
            }
        t->p[P_CVOI] = 0;
        t->p[P_ROOT] = 0;
        keys(1u << 1);                                  /* (key 1: the second button, C) */
        keys(0);
        for (i = 0; i < OMNI_NSTR; i++) {               /* a string: one note */
            keys(1u << whites[i]);
            assert(non == 1u && ons[0] == C_STR[i]);
            keys(0);
        }
        keys(1u << HARP_KLATCH);                        /* the top F#: LATCH, silent */
        keys(0);
        assert(!non && key_latch(t));
        keys(1u << 1);
        assert(non == 4u);
        keys(0);
        assert(!noff);                                  /* latched: it sounds on */
        events_block(CTL);
        keys(1u << HARP_KLATCH);
        keys(0);
        events_block(CTL);
        assert(!key_latch(t) && !sounding(t, offs));    /* LATCH off: it goes */
        memset(&st, 0, sizeof st);                      /* recorded: the chord whole from its bass, any octave */
        song.octave = 1;
        harp_step(t, &st, 9u);                          /* (key 9: the tenth button, Bb) */
        id = omni_id(0, 9, 0);
        assert(st.comp == CP_CHORD && st.carg == id && id >= HARP_NID && st.note[0] == 36u + 10u + 12u);
        n = hc_chord(id, 0, want);
        assert(step_expand(&st, step_chord_id(&st), out, own) == n);
        for (i = 0; i < n; i++)
            assert(out[i] == want[i] + 12u);
        step_chord_name(&st, st.carg, nm, sizeof nm);
        assert(!strcmp(nm, "Bb"));
        song.octave = 0;
        keys(1u << 1);                                  /* AUTO's voices: the bass the BASS voice, the triad the CHORD's */
        assert(harp_chord_note(t, 36u) == 2 && harp_chord_note(t, 60u) == 1 && harp_chord_note(t, 48u) == 0);
        keys(0);
        t->p[P_CHRD] = CD_HARP;                         /* (HARP: its chords' notes all the CHORD voice) */
        t->p[P_CVOI] = 7;
        keys(1u << 2);
        n = harp_chord(7, 2, 0, want);
        assert(harp_chord_note(t, want[0]) == 1);
        keys(0);
        mo_take();
        puts("harp: OMNI: FoMni's buttons and the rows of fifths, voiced as FoMni's, every one in three ROOTs; the 9 "
             "strings; LATCH on the top F#; recorded whole; AUTO's bass: ok");
    }

    t->p[P_CHRD] = 0;                                   /* CHORD OFF: the keys as ever */
    for (k = 0; k < 27u; k++)
        assert(kb_map(t, k) == 53u + k);
    keys(1u << HARP_KLATCH);
    assert(non == 1u && ons[0] == 53u + HARP_KLATCH && t->p[P_AHOLD] == 0);
    keys(0);
    puts("harp: CHORD OFF keys unchanged: ok");
    puts("harp live: all ok");
    return 0;
}
