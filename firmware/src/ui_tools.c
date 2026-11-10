/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The SEQ held layer's tools (ui_layer.c sq_key; the tools were a page, SEQ > TOOLS, 2026-11-03 .. 2026-11-10, and a held
 * layer before that; 2026-11-10: the random ones (RANDOM, COOK, RND LANE), CLEAR (REC held + F3) and CLR AUTO (SEQ >
 * AUTOMATION > CLEAR) are gone, CLEAR ALL is REC held + G5). Their keys (TL_*):
 *   F3 REVERSE  G3 SHIFT <  A3 SHIFT >     the whole sequence
 *   B3 BEAT (DRUM)                         a basic beat
 *   G4 CLR LANE  A4 REV LANE  B4 FILL LANE the selected lane (DRUM: ui.lane, black keys 1..8 pick it)
 * Each works on the selected track's steps 1..LEN (the steps past LEN stay); the track's automation stays. No undo
 * (REC held + D4 UNDO for the last big change; the song layer's RECALL brings the section back as it was stored).
 * A step that held something and is empty after loses its locks (as EDIT's step clear); REVERSE and SHIFT move the
 * steps' automation and locks with them. A sequence of ties reversed keeps its note first: N T T -> N T T at the
 * mirrored place */
enum { TL_REV, TL_LEFT, TL_RIGHT, TL_BEAT, TL_LCLR, TL_LREV, TL_LFILL, TL_N };

/* the steps as they were just before an action (tl_take): what REVERSE and SHIFT read; in the pool (RAM's .bss is
 * tight) */
static step_t tl_src[NSTEP] __attribute__((section(".pool")));

static uint32_t tl_len(const track_t *t) { return (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }
static uint32_t tl_lane(const step_t *s, uint32_t l) { return (step_lanes(s) >> l) & 1u; }

/* the steps of t as they are now, before an action */
static void tl_take(const track_t *t) { memcpy(tl_src, t->step, sizeof tl_src); }

/* steps 1..len of t that are not empty (a REST); after the action the ones emptied lose their locks */
static uint64_t tl_used(const track_t *t, uint32_t len)
{
    uint64_t m = 0;
    uint32_t i;
    for (i = 0; i < len; i++)
        if (t->step[i].time != ST_REST)
            m |= (uint64_t)1 << i;
    return m;
}
static void tl_unlock(track_t *t, uint64_t was, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++)
        if (((was >> i) & 1u) && t->step[i].time == ST_REST)
            (void)motion_clear_locks(t, i);
}

/* step j of t becomes the step src[j] as it was (tl_src), its automation and locks with it */
static void tl_order(track_t *t, uint32_t len, const uint8_t *src)
{
    uint8_t to[NSTEP];
    uint32_t j, k = trk_index(t), f;
    for (j = 0; j < len; j++) {
        t->step[j] = tl_src[src[j]];
        to[src[j]] = (uint8_t)j;
    }
    f = motion_guard();
    for (j = 0; j < motion.count; j++) {
        motion_event_t *e = &motion.event[j];
        if (MOTION_TRK(e) == k && MOTION_STEP(e) < len)
            e->place = (uint8_t)((e->place & 0xC0u) | to[MOTION_STEP(e)]);   /* (the track's bits kept, MOTION_TRK) */
    }
    motion_unguard(f);
}

/* REVERSE: the mirror order; a note's ties (after it) would come before it, so the note goes first again */
static void tl_reverse(track_t *t, uint32_t len)
{
    uint8_t src[NSTEP], h;
    uint32_t j, e, x;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)(len - 1u - j);
    for (j = 0; j < len; j++) {
        for (e = j; e < len && tl_src[src[e]].time == ST_TIE; e++)
            ;
        if (e == j)
            continue;
        if (e < len && tl_src[src[e]].time == ST_NOTE) {   /* ties j .. e - 1, their note at e: the note to j */
            for (h = src[e], x = e; x > j; x--)
                src[x] = src[x - 1u];
            src[j] = h;
        }
        j = e;                                          /* (past the run and its note) */
    }
    tl_order(t, len, src);
}

/* SHIFT: rotated by one step inside LEN, d = 1 later (>), len - 1 earlier (<) */
static void tl_shift(track_t *t, uint32_t len, uint32_t d)
{
    uint8_t src[NSTEP];
    uint32_t j;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)((j + len - d) % len);
    tl_order(t, len, src);
}

/* ------------------------------------------------------------- drums --- */
static void tl_lane_clear(track_t *t, uint32_t len, uint32_t l)
{
    uint32_t i;
    for (i = 0; i < len; i++)
        grid_hit(t, i, l, 0);
}
/* FILL LANE: each press the next of every step, every 2nd, every 4th, none (anything else: every step).
 * Returns the division now (0: cleared) */
static uint32_t tl_fill(track_t *t, uint32_t len, uint32_t l)
{
    static const uint8_t E[3] = {1, 2, 4};
    uint32_t e, i, now = 0;
    for (e = 0; e < 3u && !now; e++) {
        for (i = 0; i < len && tl_lane(&t->step[i], l) == (i % E[e] == 0u); i++)
            ;
        if (i == len)
            now = e + 1u;
    }
    e = now == 3u ? 0u : E[now % 3u];
    for (i = 0; i < len; i++)
        grid_hit(t, i, l, e && i % e == 0u);
    return e;
}

/* ------------------------------------------------------------ actions --- */
/* the action of place p on track t (lane l): its message; 0 = nothing done (said why) */
static const char *tl_do(track_t *t, uint32_t p, uint32_t l)
{
    static char b[24];
    uint32_t len = tl_len(t), i, n = 0;
    uint64_t was = tl_used(t, len);
    const char *msg = 0;
    l %= NLANE;
    tl_take(t);
    switch (p) {
    case TL_REV: tl_reverse(t, len); msg = "REVERSED"; break;
    case TL_LEFT: tl_shift(t, len, len - 1u); msg = "SHIFTED LEFT"; break;
    case TL_RIGHT: tl_shift(t, len, 1u); msg = "SHIFTED RIGHT"; break;
    case TL_BEAT:                                       /* four on the floor, the backbeat, 8th hats */
        for (i = 0; i < len; i++) {
            step_clear(&t->step[i]);
            if (i % 4u == 0u) grid_hit(t, i, DV_KICK, 1);
            if (i % 8u == 4u) grid_hit(t, i, DV_SNARE, 1);
            if (i % 2u == 0u) grid_hit(t, i, DV_HATC, 1);
        }
        msg = "BASIC BEAT";
        break;
    default:                                            /* the lane's */
        str_cpy(b, drum_lane_name(t, l), sizeof b);
        if (p == TL_LCLR) {
            tl_lane_clear(t, len, l);
            msg = " CLEARED";
        } else if (p == TL_LREV) {
            for (i = 0; i < len; i++) {
                const step_t *s = &tl_src[len - 1u - i];
                grid_hit(t, i, l, tl_lane(s, l));
                if (tl_lane(s, l))
                    grid_acc(t, i, l, (step_accents(s) >> l) & 1u);
            }
            msg = " REVERSED";
        } else {
            n = tl_fill(t, len, l);
            msg = n == 1u ? " EVERY STEP" : n == 2u ? " EVERY 2ND" : n ? " EVERY 4TH" : " CLEARED";
        }
        str_cpy(b + str_len(b), msg, sizeof b - str_len(b));
        msg = b;
        break;
    }
    tl_unlock(t, was, len);
    return msg;
}

/* ------------------------------------------------- (0.6.85) SEQ > STRETCH --- */
/* the selected track's steps 1..LEN as a whole, its automation and locks with them (tl_src: the steps before):
 *   STRETCH xk: step i to step i*k (LEN x k, up to 64: what falls past 64 is cut); a note held into the next step (a TIE
 *     after it) keeps holding over the new steps, a short note stays short (its gate), a hit stays a hit
 *   STRETCH /k: step j takes the first step that plays among i = j*k .. j*k+k-1 (a TIE if the note before holds on);
 *     LEN / k (rounded up)
 *   REPEAT xk: steps 1..LEN again after them, k times in all (LEN x k, up to 64; TO 64: as many whole times as fit),
 *     the automation and locks copied while there is room
 * Returns the new LEN (0: nothing to do) */
static void tl_motion_map(track_t *t, uint32_t olen, uint32_t num, uint32_t den, uint32_t nlen)   /* event step s < olen
                                                                                                * -> s*num/den */
{
    uint32_t i, n = 0, k = trk_index(t), f = motion_guard();
    for (i = 0; i < motion.count; i++) {
        motion_event_t e = motion.event[i];
        if (MOTION_TRK(&e) == k && MOTION_STEP(&e) >= olen && MOTION_STEP(&e) < nlen) continue;   /* (its step cleared) */
        if (MOTION_TRK(&e) == k && MOTION_STEP(&e) < olen) {
            uint32_t s = MOTION_STEP(&e) * num / den;
            if (s >= nlen) continue;                    /* (cut with its step) */
            e.place = (uint8_t)((e.place & 0xC0u) | s);
        }
        motion.event[n++] = e;
    }
    memset(motion.event + n, 0, (MOTION_MAX - n) * sizeof motion.event[0]);
    motion.count = (uint8_t)n;
    RING_PUBLISH();
    motion_unguard(f);
}
static uint32_t tl_stretch(track_t *t, int32_t k)
{
    uint32_t len = tl_len(t), nlen, i, j;
    if (k >= 2) {
        uint32_t m = (uint32_t)k;
        nlen = len * m > NSTEP ? NSTEP : len * m;
        tl_take(t);
        for (j = 0; j < nlen; j++) step_clear(&t->step[j]);   /* (the steps past the new LEN stay, as every tool's) */
        for (i = 0; i < len && i * m < NSTEP; i++) {
            const step_t *s = &tl_src[i];
            int hold = s->time == ST_TIE || (s->time == ST_NOTE && s->n && i + 1u < len && tl_src[i + 1u].time == ST_TIE);
            t->step[i * m] = *s;
            for (j = 1; j < m && i * m + j < NSTEP; j++)
                if (hold) { t->step[i * m + j].time = ST_TIE; t->step[i * m + j].vel = s->vel; }
        }
        tl_motion_map(t, len, m, 1, nlen);
    } else if (k <= -2) {
        uint32_t d = (uint32_t)-k;
        nlen = (len + d - 1u) / d;
        tl_take(t);
        for (j = 0; j < len; j++) step_clear(&t->step[j]);
        for (j = 0; j < nlen; j++) {
            uint32_t pick = NSTEP, tie = 0;
            for (i = j * d; i < j * d + d && i < len; i++) {
                if (tl_src[i].time == ST_NOTE && (tl_src[i].n || tl_src[i].hit)) { pick = i; break; }
                tie |= tl_src[i].time == ST_TIE;
            }
            if (pick < NSTEP) t->step[j] = tl_src[pick];
            else if (tie && j) { t->step[j].time = ST_TIE; t->step[j].vel = tl_src[j * d].vel; }
        }
        tl_motion_map(t, len, 1, d, nlen);
    } else
        return 0;
    t->p[P_SLEN] = (int16_t)(nlen ? nlen : 1u);
    return nlen;
}
static uint32_t tl_repeat(track_t *t, uint32_t k)       /* k: 2..8 times, 0: to 64 */
{
    uint32_t len = tl_len(t), nlen, j, i, n, tk = trk_index(t), f;
    nlen = k ? len * k : (NSTEP / len) * len;
    if (nlen > NSTEP) nlen = NSTEP;
    if (nlen <= len) return 0;
    for (j = len; j < nlen; j++)
        t->step[j] = t->step[j % len];
    tl_motion_map(t, len, 1, 1, nlen);                  /* (what the copies cover loses its own automation) */
    f = motion_guard();                                 /* the automation and locks of steps 1..LEN on each copy */
    n = motion.count;
    for (i = 0; i < n; i++) {
        motion_event_t e = motion.event[i];
        uint32_t s = MOTION_STEP(&e);
        if (MOTION_TRK(&e) != tk || s >= len) continue;
        for (j = s + len; j < nlen && motion.count < MOTION_MAX; j += len) {
            motion_event_t c = e;
            c.place = (uint8_t)((c.place & 0xC0u) | j);
            motion.event[motion.count++] = c;
        }
    }
    RING_PUBLISH();
    motion_unguard(f);
    t->p[P_SLEN] = (int16_t)nlen;
    return nlen;
}
