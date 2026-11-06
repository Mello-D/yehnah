/* SPDX-License-Identifier: GPL-3.0-only */
/* (chord-harp fork, 2026-11-05) ARP > AMBIENT: a playable generative mode. What the keys hold is the material: the
 * notes down on any melodic track (a HARP chord key's chord, keys held together) are the note pool, shared by every
 * track. Each track with AMBIENT on plays its own loop over the pool, a tape loop of its own length (AMB_LEN eighths:
 * 23, 29, 31, 37, so the tracks drift against each other, after Eno's loops of unequal length), with DENSITY events in
 * it; each pass MOTION re-rolls some of them (0: the same loop for ever); SPACE draws the notes out (a short touch .. two
 * events' length, overlapping) and spreads them over the octaves.
 *   AMBIENT: OFF; HOLD (it plays while a key is down, from the notes down); LATCH (it plays on from the last chord: two
 *   or more notes down together replace the pool, a single note alone plays on top and leaves it, the first note ever
 *   starts it). The keys always sound as they do (on top). It runs with the transport stopped too, at its tempo.
 * Cost: a few operations a block a track on; the notes go out as the sequencer's (seq_note_on: MIDI OUT ALL sends them) */
#define AMB_EV 8u
#define AMB_NP 12u
enum { AMB_OFF, AMB_HOLD, AMB_LATCH };
static const uint8_t AMB_LEN[NTRK] = {23, 29, 31, 37};      /* each track's loop, eighths (primes: they never line up) */
typedef struct {
    uint32_t pos, off[2];                                    /* samples into its eighth; to each note's end */
    uint8_t idx, nev;                                        /* the eighth it is on; its events */
    uint8_t at[AMB_EV], pick[AMB_EV];                        /* each event's eighth; its note: pool place | octave << 4 */
    uint8_t note[2];                                         /* sounding (0: none), the newer in [0] */
} amb_loop_t;
static amb_loop_t amb_l[NTRK];
static uint8_t amb_dn[AMB_NP], amb_ndn;                      /* the notes the keys hold now */
static uint8_t amb_pool[AMB_NP], amb_np;                     /* LATCH's: the last chord */

static uint32_t amb_pick(uint32_t space)                     /* a note: a pool place, an octave now and then (SPACE) */
{
    uint32_t o = rng() % 100u < space * 40u / 127u ? (rng() & 1u ? 1u : 3u) : 0u;   /* 1 up, 3 down */
    return (rng() & 15u) | o << 4;
}
static void amb_roll(amb_loop_t *a, uint32_t i, uint32_t len, uint32_t space)
{
    a->at[i] = (uint8_t)(rng() % len);
    a->pick[i] = (uint8_t)amb_pick(space);
}
/* a key's notes went down (on = 1) or up (key_on / key_off): the notes down, and LATCH's pool */
static __attribute__((noinline)) void amb_key(const track_t *t, const uint8_t *nt, uint32_t n, int on)
{
    uint32_t i, j;
    if (trk_kit(t))
        return;
    for (i = 0; i < n; i++) {
        if (on) {
            if (amb_ndn < AMB_NP)
                amb_dn[amb_ndn++] = nt[i];
            continue;
        }
        for (j = 0; j < amb_ndn && amb_dn[j] != nt[i]; j++)
            ;
        if (j < amb_ndn)
            amb_dn[j] = amb_dn[--amb_ndn];
    }
    if (on && (amb_ndn >= 2u || !amb_np)) {
        memcpy(amb_pool, amb_dn, amb_ndn);
        amb_np = amb_ndn;
    }
}
static void amb_off(track_t *t, amb_loop_t *a, uint32_t k)
{
    if (a->note[k])
        seq_note_off(t, a->note[k]);
    a->note[k] = 0;
}
/* once a block a track (events, after the arp), with AMBIENT on or a note of it still sounding */
static __attribute__((noinline)) void amb_tick(track_t *t, uint32_t n)
{
    uint32_t ti = trk_index(t) % NTRK, k, i, len = AMB_LEN[ti], step = div_samples(1), mode = (uint32_t)t->p[P_AMB];
    uint32_t want, np, space = (uint32_t)clamp(t->p[P_ASPC], 0, 127), mot = (uint32_t)clamp(t->p[P_AMOT], 0, 127);
    const uint8_t *pool;
    amb_loop_t *a = &amb_l[ti];
    for (k = 0; k < 2u; k++)
        if (a->note[k]) {
            if (a->off[k] <= n)
                amb_off(t, a, k);
            else
                a->off[k] -= n;
        }
    pool = mode == AMB_HOLD ? amb_dn : amb_pool;
    np = mode == AMB_HOLD ? amb_ndn : amb_np;
    if (!mode || trk_kit(t) || !np) {
        if (!mode || trk_kit(t)) {
            amb_off(t, a, 0);
            amb_off(t, a, 1);
        }
        return;
    }
    want = 1u + (uint32_t)clamp(t->p[P_ADEN], 0, 127) * (AMB_EV - 1u) / 127u;
    while (a->nev < want)                                    /* (DENSITY up: events added, the others kept) */
        amb_roll(a, a->nev++, len, space);
    a->nev = (uint8_t)(a->nev > want ? want : a->nev);
    if (step < CTL)
        step = CTL;
    a->pos += n;
    if (a->pos < step)
        return;
    a->pos -= step;
    a->pos = a->pos < step ? a->pos : 0u;
    if (++a->idx >= len) {                                   /* a pass: MOTION re-rolls some */
        a->idx = 0;
        for (i = 0; i < a->nev; i++)
            if (rng() % 254u < mot) {
                a->pick[i] = (uint8_t)amb_pick(space);
                if (mot > 64u && rng() % 254u < mot)
                    a->at[i] = (uint8_t)(rng() % len);
            }
    }
    for (i = 0; i < a->nev; i++) {
        int32_t x;
        uint32_t g, o;
        if (a->at[i] != a->idx)
            continue;
        o = a->pick[i] >> 4;
        x = pool[(a->pick[i] & 15u) % np] + (o == 1u ? 12 : o == 3u ? -12 : 0) + t->p[P_POFS];
        if (song.g[G_STRN])
            x = scale_steps(t, x, song.g[G_STRN]);
        amb_off(t, a, 1);                                    /* (two at most: the older one ends) */
        a->note[1] = a->note[0];
        a->off[1] = a->off[0];
        a->note[0] = (uint8_t)clamp(x, 1, 127);
        g = step + (uint32_t)((uint64_t)step * (len * 2u / a->nev) * space / 127u);   /* SPACE: an eighth .. two gaps */
        a->off[0] = g;
        seq_note_on(t, a->note[0], 50u + rng() % 46u);
        if (a->note[1] == a->note[0])
            a->note[1] = 0;                                  /* (the same note struck again: one voice) */
    }
}
