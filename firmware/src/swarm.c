/* SPDX-License-Identifier: GPL-3.0-only */
/* SWARM (2026-11-15, the user's idea; mock-up docs/plans/swarm-mockup.html, design notes HANDOFF.md item 15).
 * HOME's page of four dots. Each dot has a box of the screen (1 top left, 2 top right, 3 bottom left, 4 bottom right)
 * and drives TWO of the selected track's engine parameters: its X position one, its Y position the other (SW_TAB:
 * the engine's E slots, 0xFF = none). K1..K4 are always dots 1..4; SELECT scrolls the row they set (POSITION SHAPE
 * SIZE SPEED GAIN).
 *   POSITION  the dot's centre in its box, 0..100 %: 50 = the box's middle; below 50 the centre slides down the box's
 *             vertical line (0 the middle, 25 the bottom, then it pops to the top and returns to the middle at 50); above
 *             50 it slides left to right along the horizontal line (75 the right, pops to the left, 100 the middle)
 *   SHAPE     how the dot moves round its centre: ORBIT (a Lissajous loop), DRIFT (a wandering point on a spring),
 *             CHAOS (a Lorenz path, bounded and never repeating), THROW (a puck that bounces and slows; a fast knob turn
 *             throws it, and it is thrown again when it comes to rest)
 *   SIZE      how far it moves; at 100 % any shape roams the whole screen (the path is folded at its edges)
 *   SPEED     how fast
 *   GAIN      how much its distance from its box's middle adds to each parameter: 50 % = the box's edge is +-50 % of the
 *             parameter's range (100 %: +-100 %); the default is 25 %. Towards either end of the range the movement slows
 *             (sw_soft), so a parameter does not sit pinned at its minimum or maximum
 *   (the actions, on any page: the HOME layer, HOME held: its white keys from F3 FREEZE, REVERSE (the last 30 s backwards, then
 *   forwards, then live; again: stop where it is), THROW (every THROW dot), CAPTURE (the sound as it is now becomes the track's
 *   own, SWARM off), SWARM (on; off puts the sound back as it was))
 * A parameter is its BASE value + the dot's offset: the sound as loaded plays until a dot moves; a parameter changed by
 * hand, or a sound loaded, is the new base (what the page last wrote is remembered; anything else is the user's).
 * It runs in the main loop (swarm_tick), on any page, until it is turned off: the selected track at the time it was
 * started. Not saved in a project (the parameters are, as they sound: CAPTURE or OFF first to keep a sound).
 * All of its memory is in the pool: main RAM has 400 bytes spare (app.ld). */
#define SW_N 4u
#define SW_HN 300u                                      /* history: 10 samples a second, 30 s */
enum { SWS_ORBIT, SWS_DRIFT, SWS_CHAOS, SWS_THROW, SWS_N };
enum { SWR_POS, SWR_SHAPE, SWR_SIZE, SWR_SPEED, SWR_GAIN, SWR_N, SWR_TRAVEL = SWR_N, SWR_ROT };   /* (TRACE's rows: ROTATE is SPEED's slot) */
#include "trace_data.h"                                 /* TRACE: the shapes (tools/gen_trace_shapes.py) */
#define TR_ROWS 6u
static const uint8_t TR_ROW[TR_ROWS] = {SWR_TRAVEL, SWR_SHAPE, SWR_SIZE, SWR_ROT, SWR_POS, SWR_GAIN};
enum { SWA_FREEZE, SWA_REVERSE, SWA_THROW, SWA_CAPTURE, SWA_ONOFF, SWA_N };   /* the HOME layer's keys (F3 ..): ui_swarm.c swarm_act */
static const char *const SW_ROW[SWR_N] = {"POSITION", "SHAPE", "SIZE", "SPEED", "GAIN"};
static const char *const SW_SHAPE[SWS_N] = {"ORBIT", "DRIFT", "CHAOS", "THROW"};

/* per engine (ENGI_ number): dot 0's X and Y parameter, dot 1's, dot 2's, dot 3's. A parameter is an engine slot (E(k), the
 * engine's .edit[k]) or a common one (P_DETUNE, P_FCUT ..: TP[]); SWN = none. Only smooth parameters that keep what the sound
 * is (no waveform, material, model, ratio, algorithm or chorus type: a stepped setting jumps, and SWARM cannot fade it), the
 * engine's own first, a few common ones where it has an odd one out. A common parameter that starts at its end of the range
 * (FCUT open, PHAMT / DIST / REV off) only moves one way. docs/plans/swarm-params.md */
#define SWN 0xFFFFu
#define E(k) (P_E0 + (k))
static const uint16_t SW_TAB[NENGINES][8] = {
    /*  0 ANALOG  */ {E(1), E(3), E(4), E(5), E(6), E(2), P_VINT, P_PHAMT},         /* DTN NOIS | CUT RES | DRV MIX */
    /*  1 DIGITAL */ {E(4), E(6), E(5), P_FCUT, P_DETUNE, P_VINT, P_DIST, P_PHAMT}, /* IDX FB | MDEC */
    /*  2 PHASE   */ {E(2), E(7), E(3), E(4), E(6), P_FCUT, P_DETUNE, P_DIST},      /* DCW DCW2 | ENV DTN | SUB */
    /*  3 LOFI    */ {E(3), E(7), E(2), E(4), E(5), P_DETUNE, P_DIST, P_REV},       /* CRSH TONE | DUTY SWP | VIB */
    /*  4 SAMPLE  */ {P_FCUT, P_FRES, P_DIST, P_DETUNE, P_VINT, P_DRIFT, P_PHAMT, P_REV},
    /*  5 VOICE   */ {E(0), E(1), E(2), E(4), E(5), E(6), E(7), P_DRIFT},           /* VOWEL VOWEL2 | TALK BUZZ | BREATH Q | RAND */
    /*  6 TRIO    */ {P_FCUT, P_FRES, P_DIST, P_DETUNE, P_VINT, P_DRIFT, P_PHAMT, P_REV},
    /*  7 WHEEL   */ {E(1), E(2), E(3), E(5), E(6), P_VINT, P_PHAMT, P_FCUT},       /* SUB BODY | TOP CLICK | DRV */
    /*  8 GRAIN   */ {P_FCUT, P_FRES, P_DIST, P_DETUNE, P_VINT, P_DRIFT, P_PHAMT, P_REV},
    /*  9 PHYS    */ {P_FCUT, P_FRES, P_DIST, P_DETUNE, P_VINT, P_DRIFT, P_PHAMT, P_REV},
    /* 10 DRUM    */ {E(0), E(2), E(3), E(4), E(5), E(6), E(7), P_REV},            /* MORPH TONE | DECAY NOISE | FM ENV FOLD | DRIVE */
    /* 11 NOISE   */ {E(1), E(2), E(3), E(5), E(6), E(7), E(4), P_REV},            /* COLR FREQ | RES DENS | DRFT CRSH | TRK */
    /* 12 FM6     */ {E(1), E(2), E(4), E(5), E(6), P_FCUT, P_VINT, P_DIST},       /* FB MOD | MEG VMOD | DETUNE */
    /* 13 SLICE   */ {P_FCUT, P_FRES, P_DIST, P_DETUNE, P_VINT, P_DRIFT, P_PHAMT, P_REV},
    /* 14 ANIMAL  */ {E(1), E(2), E(3), E(4), E(5), P_DRIFT, P_REV, P_FCUT},       /* PRESS CHAOS | SIZE SWEEP | RATE */
    /* 15 JUNO    */ {E(1), E(2), E(3), P_DETUNE, E(5), E(6), P_VINT, P_PHAMT},    /* PW SUB | NOIS | CUT RES */
    /* 16 OMNI    */ {E(1), E(2), E(4), E(6), E(3), E(7), E(5), P_FCUT},          /* HARP1 HARP2 | SHIM BRIT | SUS DAMP | RATE */
    /* 17 KALIMBA */ {E(1), E(3), E(5), E(4), E(6), E(7), E(2), P_REV},            /* HARD TONE | WORN GLID | DAMP CLIK | DECY */
    /* 18 MINI    */ {E(0), E(1), E(2), E(6), E(3), E(4), E(5), E(7)},             /* CUT EMPH | CNTR MOD | F.ATK F.DEC | F.SUS KBD */
    /* 19 GUITAR  */ {E(1), E(2), E(4), E(5), E(6), E(7), E(3), P_FCUT},           /* PICK PLUCK | TONE BODY | MUTE DAMP | DECY */
    /* 20 ACID    */ {E(1), E(2), E(3), E(4), E(5), E(6), E(7), P_VINT},           /* CUT RES | ENV DEC | ACC DIST | SWP */
    /* 21 DRIFT   */ {E(1), E(2), E(3), E(4), E(5), E(6), E(7), P_REV},            /* TILT DRIFT | SPEED FLICK | DETUNE BLOOM | BREATH */
    /* 22 PLAITS  */ {E(1), E(2), E(3), E(4), E(5), E(6), P_FCUT, P_DETUNE},       /* HARM TIMB | MORPH AUX | TONE LPG */
    /* 23 STRINGS */ {E(0), E(1), E(2), E(3), E(4), E(7), E(5), E(6)},             /* 16' 8' | 4' ENS | CUT RES | RATE SHP */
    /* 24 VPS     */ {E(0), E(1), E(2), E(3), E(4), E(5), E(6), E(7)},             /* D V | VENV OSC2 | CUT RES | DTN SUB */
    /* 25 MODAL   */ {E(1), E(3), E(4), E(5), E(6), E(7), E(2), P_FCUT},           /* HARD TONE | BARK TREM | DAMP CLIK | DECY */
    /* 26 WSEQ    */ {E(1), E(2), E(4), E(6), E(7), P_DETUNE, P_DIST, P_VINT},     /* RATE XFADE | START CUT | RES */
    /* 27 TERRAIN */ {E(2), E(3), E(1), E(4), E(5), E(6), E(7), P_DETUNE},         /* X Y | RAD ORB | DRIFT CUT | RES */
    /* 28 VECTOR  */ {E(4), E(5), E(7), P_DETUNE, P_VINT, P_DIST, P_PHAMT, P_REV}, /* X Y | CUT */
    /* 29 HYBRID  */ {E(0), E(1), E(2), P_DETUNE, P_DIST, P_VINT, P_PHAMT, P_REV}, /* CUT RES | FENV */
};

typedef struct {
    uint8_t pos, shape, size, speed, gain;              /* 0..100 (SHAPE 0..3) */
    float ox, oy, vx, vy;                               /* the shape's offset, -1..1, and its velocity */
    float t, fa, fb, fp, pa;                            /* ORBIT's time, its two rates, its two phases */
    float lx, ly, lz;                                   /* CHAOS: the Lorenz state */
    float quiet;                                        /* THROW: seconds at rest */
    float x, y;                                         /* where it is on the screen, 0..1 */
    int16_t base[2], put[2];                            /* each parameter's base value, and what was last written */
} sw_dot_t;
static struct {
    uint8_t inited, on, frozen, rev, row, trk, eng, manual_off;
    uint8_t mode, trow, sel;                            /* 0 SWARM, 1 TRACE; TRACE's row (TR_ROW); the dot last turned */
    uint8_t tshape[SW_N], trot[SW_N];                   /* TRACE: each dot's shape, its rotation 0..100 (a turn) */
    float tph[SW_N], tcur[SW_N];                        /* TRACE: where on the path the knob put it, and where the dot is (laps) */
    float tcum[SW_N][TR_MAXP + 1u];                     /* TRACE: the path's length up to each point */
    int8_t dir;                                         /* REVERSE: +1 back in time, -1 forward again */
    uint16_t head, hn;                                  /* the history's next sample, how many it holds */
    uint32_t last_ms, rec_ms, sig;                      /* sig: the track's sound (a preset loaded: a new base) */
    float age;                                          /* REVERSE: samples behind now */
    uint8_t h[SW_HN][SW_N * 2u];                        /* X and Y of each dot, 0..255 */
    sw_dot_t d[SW_N];
} sw __attribute__((section(".pool")));

/* the row SELECT picked (SWARM's own, or TRACE's: TRAVEL first) and its name */
static uint32_t sw_rowid(void) { return sw.mode ? TR_ROW[sw.trow % TR_ROWS] : sw.row; }
static const char *sw_rowname(void)
{
    uint32_t r = sw_rowid();
    return r == SWR_TRAVEL ? "TRAVEL" : r == SWR_ROT ? "ROTATE" : SW_ROW[r];
}
/* the page (ui_swarm.c), reached from the lens, the cards and the knobs */
static void swarm_graph(void);
static void swarm_cards(void);
static void swarm_knob(uint32_t slot, int32_t steps);
static void swarm_select(int32_t s);
static int swarm_page_on(void);
static void swarm_draw(void);
static int summary_page_on(void);                       /* (ui_summary.c: HOME > TRACKS) */
static void summary_draw(void);
static void summary_graph(void);
static void summary_knob(uint32_t slot, int32_t steps);

static float sw_rnd(void) { return (float)((int32_t)(rng() & 0xFFFFu) - 32768) * (1.0f / 65536.0f); }   /* -0.5 .. 0.5 */
static float sw_fold(float u)                           /* reflected at 0 and 1: any distance stays on the screen */
{
    int32_t n = (int32_t)(u * 0.5f - (u < 0.0f ? 1.0f : 0.0f));
    u -= 2.0f * (float)n;
    return u > 1.0f ? 2.0f - u : u < 0.0f ? -u : u;
}
static void sw_seed(sw_dot_t *d)
{
    d->t = 0;
    d->fa = 1.0f + 1.5f * (sw_rnd() + 0.5f);
    d->fb = 1.0f + 1.5f * (sw_rnd() + 0.5f);
    d->fp = 6.0f * (sw_rnd() + 0.5f);
    d->pa = 0;
    d->ox = d->oy = d->vx = d->vy = d->quiet = 0;
    d->lx = 1.0f + sw_rnd();
    d->ly = 1.0f;
    d->lz = 20.0f;
}
/* TRACE: a dot walks a closed shape (TR_PTS, y down) at a constant speed: the knob moves it along the path, 0..1 a lap (the
 * MOBIUS strip is two laps: the band's side flips each lap, cos(pi s) of the way). tr_prep works the path's lengths out */
static void tr_prep(uint32_t i)
{
    uint32_t sh = sw.tshape[i], n = TR_OFF[sh + 1u] - TR_OFF[sh], k, m;
    const int8_t *p = &TR_PTS[TR_OFF[sh] * 2u];
    float c = 0.0f;
    for (k = 0; k < n; k++) {
        float dx, dy;
        m = k + 1u < n ? k + 1u : 0u;
        dx = (float)(p[m * 2u] - p[k * 2u]);
        dy = (float)(p[m * 2u + 1u] - p[k * 2u + 1u]);
        sw.tcum[i][k] = c;
        c += dx * dx + dy * dy > 0.0f ? (dx * dx + dy * dy) * fm_rsqrtf(dx * dx + dy * dy) : 0.0f;
    }
    sw.tcum[i][n] = c > 0.0f ? c : 1.0f;
}
static uint32_t tr_laps(uint32_t i) { return (TR_FLAG[sw.tshape[i]] & TRF_MOBIUS) ? 2u : 1u; }
/* the point s laps along dot i's shape, -1..1 each way, turned by its ROTATE */
static void tr_eval(uint32_t i, float s, float *x, float *y)
{
    uint32_t sh = sw.tshape[i], n = TR_OFF[sh + 1u] - TR_OFF[sh], k = 0, m, laps = tr_laps(i);
    const int8_t *p = &TR_PTS[TR_OFF[sh] * 2u];
    float u, f, px, py, ang, c, sn, tot = sw.tcum[i][n];
    while (s < 0.0f)
        s += (float)laps;
    while (s >= (float)laps)
        s -= (float)laps;
    u = (laps > 1u && s >= 1.0f ? s - 1.0f : s) * tot;
    while (k + 1u < n && sw.tcum[i][k + 1u] <= u)
        k++;
    m = k + 1u < n ? k + 1u : 0u;
    f = sw.tcum[i][k + 1u] > sw.tcum[i][k] ? (u - sw.tcum[i][k]) / (sw.tcum[i][k + 1u] - sw.tcum[i][k]) : 0.0f;
    f = f > 1.0f ? 1.0f : f;
    px = (float)p[k * 2u] + (float)(p[m * 2u] - p[k * 2u]) * f;
    py = (float)p[k * 2u + 1u] + (float)(p[m * 2u + 1u] - p[k * 2u + 1u]) * f;
    if (laps > 1u) {                                    /* the strip: offset along the path's normal (smooth: the tangents at the two ends of
                                                         * the segment, blended), to the other side on the next lap */
        uint32_t kp = k ? k - 1u : n - 1u, mn = m + 1u < n ? m + 1u : 0u;
        float ax = (float)(p[m * 2u] - p[kp * 2u]), ay = (float)(p[m * 2u + 1u] - p[kp * 2u + 1u]);
        float bx = (float)(p[mn * 2u] - p[k * 2u]), by = (float)(p[mn * 2u + 1u] - p[k * 2u + 1u]);
        float qa = ax * ax + ay * ay, qb = bx * bx + by * by, tx, ty, q;
        float w = 0.25f * 127.0f * fm_cosf(3.1415927f * s);
        qa = qa > 0.0f ? fm_rsqrtf(qa) : 0.0f;
        qb = qb > 0.0f ? fm_rsqrtf(qb) : 0.0f;
        tx = ax * qa + (bx * qb - ax * qa) * f;
        ty = ay * qa + (by * qb - ay * qa) * f;
        q = tx * tx + ty * ty;
        px *= 0.75f;                                    /* (the strip's middle line a little smaller: the band stays on the screen) */
        py *= 0.75f;
        if (q > 0.0f) {
            q = fm_rsqrtf(q);
            px += -ty * q * w;
            py += tx * q * w;
        }
    }
    ang = (float)sw.trot[i] * 0.0628319f;
    c = fm_cosf(ang);
    sn = fm_sinf(ang);
    *x = (px * c - py * sn) * (1.0f / 127.0f);
    *y = (px * sn + py * c) * (1.0f / 127.0f);
}
static void tr_shape_set(uint32_t i, uint32_t sh)
{
    sw.tshape[i] = (uint8_t)(sh % TR_N);
    sw.tph[i] = sw.tcur[i] = 0.0f;
    tr_prep(i);
}
/* REROLL / RANDOM: another shape, any of them, turned any way */
static void tr_random(uint32_t i)
{
    uint32_t sh = (rng() >> 4) % TR_N;
    if (sh == sw.tshape[i])
        sh = (sh + 1u + ((rng() >> 6) % (TR_N - 1u))) % TR_N;
    tr_shape_set(i, sh);
    sw.trot[i] = (uint8_t)((rng() >> 5) % 100u);
}
static void tr_travel(uint32_t i, int32_t steps)
{
    sw.tph[i] += (float)steps * 0.015f;
    while (sw.tph[i] >= 2.0f) { sw.tph[i] -= 2.0f; sw.tcur[i] -= 2.0f; }   /* (2 is a whole number of laps for every shape) */
    while (sw.tph[i] < 0.0f) { sw.tph[i] += 2.0f; sw.tcur[i] += 2.0f; }
}
static void swarm_defaults(void)
{
    uint32_t i;
    sw.inited = 1;
    sw.dir = 1;
    for (i = 0; i < SW_N; i++) {
        sw_dot_t *d = &sw.d[i];
        d->pos = 50;
        d->shape = (uint8_t)i;
        d->size = 60;
        d->speed = 15;                                  /* (slow: the user, 2026-11-15) */
        d->gain = 25;                                   /* (box edge +-25 % of a range; the user, 2026-11-15) */
        sw_seed(d);
        d->x = (float)(i & 1u) * 0.5f + 0.25f;
        d->y = (float)(i >> 1) * 0.5f + 0.25f;
        sw.tshape[i] = TR_DEF[i];
        sw.trot[i] = 0;
        sw.tph[i] = sw.tcur[i] = 0.0f;
        tr_prep(i);
    }
}
/* the parameter id of dot i's axis a (0 X, 1 Y) on the track's engine, or 0xFFFF */
static uint32_t sw_pid(const track_t *t, uint32_t i, uint32_t a) { return SW_TAB[trk_eng(t)][i * 2u + a]; }
/* the parameter's descriptor, for its range and name */
static const param_desc_t *sw_desc(const track_t *t, uint32_t id) { return track_desc(t, id); }

static void swarm_rebase(track_t *t)                    /* the base values: what the track's parameters are now */
{
    uint32_t i, a;
    sw.eng = (uint8_t)trk_eng(t);
    sw.sig = (uint32_t)t->preset | (uint32_t)t->user << 8 | (uint32_t)t->eng_req << 16;
    for (i = 0; i < SW_N; i++)
        for (a = 0; a < 2u; a++) {
            uint32_t id = sw_pid(t, i, a);
            sw.d[i].base[a] = sw.d[i].put[a] = id == 0xFFFFu ? 0 : t->p[id];
        }
}
static void swarm_restore(track_t *t)                   /* the sound as it was before SWARM moved it */
{
    uint32_t i, a;
    for (i = 0; i < SW_N; i++)
        for (a = 0; a < 2u; a++) {
            uint32_t id = sw_pid(t, i, a);
            if (id != 0xFFFFu && sw.d[i].put[a] == t->p[id])   /* (a value the user has since set is theirs) */
                t->p[id] = sw.d[i].base[a];
        }
}
static void swarm_start(void)
{
    if (!sw.inited)
        swarm_defaults();
    sw.trk = song.sel;
    swarm_rebase(&trk[sw.trk]);
    sw.on = 1;
    sw.manual_off = 0;
    sw.frozen = sw.rev = 0;
    sw.hn = sw.head = 0;
    sw.last_ms = sw.rec_ms = fm1_ms;
}
static void swarm_off(int restore)
{
    if (sw.on && restore)
        swarm_restore(&trk[sw.trk]);
    sw.on = sw.frozen = sw.rev = 0;
    sw.manual_off = 1;
}
/* CAPTURE: what sounds now is the track's sound; SWARM stops (the dots stay where they are) */
static void swarm_capture(void) { swarm_off(0); }
/* the page came up on another track than the one SWARM drives: it follows */
static void swarm_follow(void)
{
    if (sw.on && sw.trk != song.sel) {
        swarm_restore(&trk[sw.trk]);
        sw.trk = song.sel;
        swarm_rebase(&trk[sw.trk]);
        sw.hn = sw.head = 0;
    }
}
/* the page is shown: it starts on its first visit (not again once the user has turned it off), and follows the selected track */
static void swarm_set_mode(uint32_t m);
static void swarm_enter(void)
{
    if (!sw.inited)
        swarm_defaults();
    swarm_set_mode(PAGES[ui.page].graph == GR_TRACE ? 1u : 0u);   /* (the page shown says which of the two moves the sound) */
    if (sw.on)
        swarm_follow();
    else if (!sw.manual_off)
        swarm_start();
}
/* an offset o >= 0 towards a limit with h of the range left before it (both fractions of the range): as it is for the first
 * half of h, then slower and slower, never reaching h: a parameter is not pinned at its minimum or maximum (CUT at 0 is a muted
 * track). A dot that starts a parameter at its limit does not move it that way at all */
static float sw_soft(float o, float h)
{
    float k = h * 0.5f, r = h - k, x;
    if (o <= k)
        return o;
    if (r <= 0.0f)
        return 0.0f;
    x = (o - k) / r;
    return k + r * x / (1.0f + x);
}
/* the dot's centre in its box (0..1 each): POSITION's mapping, as in the header */
static void sw_centre(const sw_dot_t *d, uint32_t i, float *cx, float *cy)
{
    float v = (float)d->pos, lx = 0.5f, ly = 0.5f, p;
    if (v < 50.0f) {
        p = v * 0.02f + 0.5f;
        ly = p >= 1.0f ? p - 1.0f : p;
    } else {
        p = (v - 50.0f) * 0.02f + 0.5f;
        lx = p >= 1.0f ? p - 1.0f : p;
    }
    *cx = ((float)(i & 1u) + lx) * 0.5f;
    *cy = ((float)(i >> 1) + ly) * 0.5f;
}
static void sw_step(sw_dot_t *d, float dt)              /* one dot's shape, dt seconds on */
{
    float sp = (float)d->speed * 0.02f, r;
    uint32_t n, m;
    switch (d->shape) {
    case SWS_ORBIT:
        d->t += dt * sp * 0.8f;
        d->ox = fm_sinf(d->fa * d->t + d->pa);
        d->oy = fm_sinf(d->fb * d->t + d->fp);
        break;
    case SWS_DRIFT:                                     /* a random walk on a spring, in its own time T (SPEED scales it like the others; 15 % = 1 s per s)
                                                         * the kick goes with the root of T (a walk, not a tremble), so it reaches about +-0.5 of the size */
        {
            float T = dt * sp * 3.3f;
            if (T > 0.0f) {
                float kick = 7.5f * T * fm_rsqrtf(T);
                d->vx += sw_rnd() * kick - d->ox * 2.5f * T;
                d->vy += sw_rnd() * kick - d->oy * 2.5f * T;
                r = 1.0f - 3.5f * T;
                r = r < 0.0f ? 0.0f : r;
                d->vx *= r;
                d->vy *= r;
                d->ox += d->vx * T;
                d->oy += d->vy * T;
            }
        }
        r = d->ox * d->ox + d->oy * d->oy;
        if (r > 1.0f) {
            r = fm_rsqrtf(r);
            d->ox *= r;
            d->oy *= r;
        }
        break;
    case SWS_CHAOS:
        m = (uint32_t)(dt * sp * 1.1f / 0.006f) + 1u;   /* Lorenz: a step of 0.006 */
        m = m > 8u ? 8u : m;
        for (n = 0; n < m; n++) {
            float h = dt * sp * 1.1f / (float)m, ddx = 10.0f * (d->ly - d->lx), ddy = d->lx * (28.0f - d->lz) - d->ly,
                  ddz = d->lx * d->ly - 2.6666667f * d->lz;
            d->lx += ddx * h;
            d->ly += ddy * h;
            d->lz += ddz * h;
        }
        d->ox = fm_clampf(d->lx * 0.05f, -1.0f, 1.0f);
        d->oy = fm_clampf((d->lz - 25.0f) * 0.045f, -1.0f, 1.0f);
        break;
    default:                                            /* THROW */
        d->vx -= d->ox * 2.0f * dt * sp;
        d->vy -= d->oy * 2.0f * dt * sp;
        r = 1.0f - 0.2f * dt;
        d->vx *= r;
        d->vy *= r;
        d->ox += d->vx * dt * sp * 2.0f;
        d->oy += d->vy * dt * sp * 2.0f;
        if (d->ox > 1.0f || d->ox < -1.0f) {
            d->ox = d->ox > 0 ? 1.0f : -1.0f;
            d->vx = -d->vx;
        }
        if (d->oy > 1.0f || d->oy < -1.0f) {
            d->oy = d->oy > 0 ? 1.0f : -1.0f;
            d->vy = -d->vy;
        }
        r = d->ox * d->ox + d->oy * d->oy + d->vx * d->vx + d->vy * d->vy;
        d->quiet = r < 0.01f ? d->quiet + dt : 0.0f;
        if (d->quiet > 1.5f)
            d->quiet = 0, d->vx = 1.4f * sw_rnd() * 2.0f, d->vy = 1.4f * sw_rnd() * 2.0f;   /* thrown again */
        break;
    }
}
static void swarm_throw(uint32_t i)                     /* a fast knob turn: the dot is thrown */
{
    sw_dot_t *d = &sw.d[i];
    if (d->shape == SWS_THROW) {
        d->vx = 1.6f * sw_rnd() * 2.0f;
        d->vy = 1.6f * sw_rnd() * 2.0f;
        d->quiet = 0;
    }
}
/* a history sample's dot position at age (samples behind the newest) */
static void sw_hist(uint32_t i, float age, float *x, float *y)
{
    uint32_t a0 = (uint32_t)age, a1;
    float f = age - (float)a0;
    const uint8_t *p, *q;
    if (sw.hn < 2u) {
        *x = sw.d[i].x;
        *y = sw.d[i].y;
        return;
    }
    a0 = a0 > sw.hn - 1u ? sw.hn - 1u : a0;
    a1 = a0 + 1u < sw.hn ? a0 + 1u : a0;
    p = sw.h[(sw.head + SW_HN - 1u - a0) % SW_HN];
    q = sw.h[(sw.head + SW_HN - 1u - a1) % SW_HN];
    *x = ((float)p[i * 2u] + ((float)q[i * 2u] - (float)p[i * 2u]) * f) * (1.0f / 255.0f);
    *y = ((float)p[i * 2u + 1u] + ((float)q[i * 2u + 1u] - (float)p[i * 2u + 1u]) * f) * (1.0f / 255.0f);
}
/* the phase (-pi/2 .. pi/2) whose sine is s */
static float sw_phase(float s)
{
    float best = 0.0f, bd = 9.0f, a;
    uint32_t k;
    for (k = 0; k <= 64u; k++) {
        a = -1.5707963f + 3.1415927f * (float)k * (1.0f / 64.0f);
        if (fm_fabsf(fm_sinf(a) - s) < bd)
            bd = fm_fabsf(fm_sinf(a) - s), best = a;
    }
    return best;
}
/* REVERSE stopped part way: each dot goes on from where it is, as its shape would from there (not back to where it was
 * turned on) */
static void sw_resume(sw_dot_t *d, uint32_t i)
{
    float cx, cy, s = (float)d->size * 0.01f;
    sw_centre(d, i, &cx, &cy);
    if (s < 0.02f)
        return;
    d->ox = fm_clampf((d->x - cx) / s, -1.0f, 1.0f);
    d->oy = fm_clampf((d->y - cy) / s, -1.0f, 1.0f);
    d->vx = d->vy = 0.0f;
    d->pa = sw_phase(d->ox) - d->fa * d->t;             /* (ORBIT: its phases to match) */
    d->fp = sw_phase(d->oy) - d->fb * d->t;
    d->lx = d->ox * 20.0f;                              /* (CHAOS: a Lorenz state to match) */
    d->ly = d->lx;
    d->lz = d->oy / 0.045f + 25.0f;
}
static void swarm_rev_stop(void)
{
    uint32_t drop = (uint32_t)sw.age, i;
    if (!sw.rev)
        return;
    sw.rev = 0;
    sw.dir = 1;
    drop = drop > sw.hn ? sw.hn : drop;                 /* (what was recorded after this point is not the path any more) */
    sw.head = (uint16_t)((sw.head + SW_HN - drop) % SW_HN);
    sw.hn = (uint16_t)(sw.hn - drop);
    sw.age = 0.0f;
    for (i = 0; i < SW_N; i++)
        sw_resume(&sw.d[i], i);
}
/* SWARM <-> TRACE: the dots go on from where they are (the base values and the sound stay) */
static void swarm_set_mode(uint32_t m)
{
    uint32_t i;
    if (m == sw.mode)
        return;
    if (sw.rev)
        swarm_rev_stop();
    sw.mode = (uint8_t)m;
    sw.hn = sw.head = 0;
    sw.rec_ms = fm1_ms;
    if (m == 0u && sw.on)
        for (i = 0; i < SW_N; i++)
            sw_resume(&sw.d[i], i);
    for (i = 0; i < SW_N; i++)
        sw.tcur[i] = sw.tph[i];
}
/* the main loop, every frame: the dots move, the parameters follow */
static void swarm_tick(void)
{
    uint32_t now = fm1_ms, ms = now - sw.last_ms, i, a;
    float dt;
    track_t *t;
    sw.last_ms = now;
    if (!sw.on)
        return;
    ms = ms > 100u ? 100u : ms;
    dt = (float)ms * 0.001f;
    t = &trk[sw.trk];
    if (sw.sig != ((uint32_t)t->preset | (uint32_t)t->user << 8 | (uint32_t)t->eng_req << 16))   /* another sound or engine loaded: its values */
        swarm_rebase(t);
    if (sw.rev) {                                       /* REVERSE: the history, back, then forward to live */
        sw.age += (float)sw.dir * dt * 10.0f;
        if (sw.dir > 0 && sw.age >= (float)(sw.hn ? sw.hn - 1u : 0u)) {
            sw.age = (float)(sw.hn ? sw.hn - 1u : 0u);
            sw.dir = -1;
        }
        if (sw.dir < 0 && sw.age <= 0.0f) {
            sw.age = 0.0f;
            sw.rev = 0;
            sw.dir = 1;
        }
    }
    for (i = 0; i < SW_N; i++) {
        sw_dot_t *d = &sw.d[i];
        float cx, cy;
        if (sw.rev)
            sw_hist(i, sw.age, &d->x, &d->y);
        else {
            if (sw.mode) {                              /* TRACE: the dot follows the knob's place on the path, a little behind */
                float k = dt * 20.0f;
                k = k > 1.0f ? 1.0f : k;
                if (!sw.frozen) {
                    sw.tcur[i] += (sw.tph[i] - sw.tcur[i]) * k;
                    tr_eval(i, sw.tcur[i], &d->ox, &d->oy);
                }
            } else if (!sw.frozen)
                sw_step(d, dt);
            sw_centre(d, i, &cx, &cy);
            if (sw.mode) {                              /* TRACE: a shape stays in its own box (SIZE 100 % fills it) */
                float bx = (float)(i & 1u) * 0.5f, by = (float)(i >> 1) * 0.5f;
                d->x = fm_clampf(cx + d->ox * (float)d->size * 0.0025f, bx, bx + 0.5f);
                d->y = fm_clampf(cy + d->oy * (float)d->size * 0.0025f, by, by + 0.5f);
            } else {
                d->x = sw_fold(cx + d->ox * (float)d->size * 0.01f);
                d->y = sw_fold(cy + d->oy * (float)d->size * 0.01f);
            }
        }
    }
    if (!sw.mode && !sw.rev && !sw.frozen && now - sw.rec_ms >= 100u) {   /* the history: ten a second */
        sw.rec_ms += 100u;
        if (now - sw.rec_ms >= 100u)
            sw.rec_ms = now;
        for (i = 0; i < SW_N; i++) {
            sw.h[sw.head][i * 2u] = (uint8_t)(sw.d[i].x * 255.0f + 0.5f);
            sw.h[sw.head][i * 2u + 1u] = (uint8_t)(sw.d[i].y * 255.0f + 0.5f);
        }
        sw.head = (uint16_t)((sw.head + 1u) % SW_HN);
        sw.hn = sw.hn < SW_HN ? (uint16_t)(sw.hn + 1u) : SW_HN;
    }
    for (i = 0; i < SW_N; i++) {                        /* the parameters: base + the dot's offset from its box's middle */
        sw_dot_t *d = &sw.d[i];
        float mid[2], pos[2];
        mid[0] = ((float)(i & 1u) + 0.5f) * 0.5f;
        mid[1] = ((float)(i >> 1) + 0.5f) * 0.5f;
        pos[0] = d->x;
        pos[1] = d->y;
        for (a = 0; a < 2u; a++) {
            uint32_t id = sw_pid(t, i, a);
            const param_desc_t *pd;
            float v;
            int32_t lo, hi;
            if (id == 0xFFFFu)
                continue;
            pd = sw_desc(t, id);
            lo = pd->min;
            hi = pd->max;
            if (t->p[id] != d->put[a]) {                /* not what was written: the user's edit, kept as a shift of the base */
                d->base[a] = (int16_t)(d->base[a] + t->p[id] - d->put[a]);
                d->base[a] = (int16_t)(d->base[a] < lo ? lo : d->base[a] > hi ? hi : d->base[a]);
                d->put[a] = t->p[id];
            }
            if (sw.frozen)                              /* frozen: the sound is left as it is (and as the user sets it) */
                continue;
            {
                float rg = (float)(hi - lo), u, off, o, fl = 0.0f;
                off = (pos[a] - mid[a]) * 2.0f * ((float)d->gain * 0.02f);   /* (a share of the range) */
                u = rg > 0.0f ? ((float)d->base[a] - (float)lo) / rg : 0.0f;
                u = fm_clampf(u, 0.0f, 1.0f);
                if (pd->fmt == F_CUTOFF && u > 0.15f)       /* a cutoff keeps above the bottom of its range (the track muted) */
                    fl = 0.15f;
                o = off >= 0.0f ? sw_soft(off, 1.0f - u) : -sw_soft(-off, u - fl);
                v = (float)lo + (u + o) * rg;
            }
            v = fm_clampf(v, (float)lo, (float)hi);
            d->put[a] = (int16_t)(v + (v < 0.0f ? -0.5f : 0.5f));
            t->p[id] = d->put[a];
        }
    }
}
