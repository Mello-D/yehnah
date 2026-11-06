/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; (chord-harp fork) the line drawings, Mello-D */
/* (chord-harp fork, replacing 0.9.4's creatures) Every engine's drawing, in the HARP rings' pen line (ui_lzpage.c
 * harp_ring): wobbly antialiased strokes (ui_organic.c og_line_a) with a fainter second pass beside them, in the
 * palette's colours (THEME, ACCENT, the knobs', TEXT), no text. On EDIT's SOUND (ui_lzedit.c graph_sound_lz), HOME's
 * ENGINE page (ui_lzhome.c), FM6's OPERATOR / OP ENV pages and DRUM's KIT / SOUND pages (ui_graph.c). Each drawing is
 * laid out in the mock-up's units (mockups/engine_drawings.html: a 240 x 140 panel) and scaled to fill the rectangle it
 * is given with an even margin (el_fit, its bounds as measured there). Each moves only with its sound (vx_level,
 * vx_age: the track's level and its newest note; its time runs with the sound; MENU > ANIM OFF: at rest).
 *   ANALOG   one cycle of the wave (WAVE), a fainter second for DETUNE, the filter's curve under it (CUT, RES)
 *   FM6      (2026-10-26: pixel art, ui_pixart.c px_fm6ops / px_fm6env) the operators wired as the algorithm
 *   PHASE    the phase going round a circle, its hand bent by DCW, and the bent wave it draws
 *   LOFI     the wave as a staircase (CHIP's bits), its sample points as dots, scrolling as it plays
 *   WHEEL    the nine drawbars as REG pulls them, swaying with the rotor (its phase), PERC striking, CLICK's bubbles
 *   VOICE    two faces in profile singing at each other in turns, the jaw and lips as the vowel sounds, breath
 *   DRUM-X   the kit as shapes (kick, snare, clap, hats, tom, rim, cowbell): a hit squashes its shape
 *   JUNO     six faders (PW SUB NOIS HPF CUT RES) under the chorus's two waves
 *   OMNI     the strings, the plucked one shaking
 *   KALIMBA  the tines fanned on the bridge, the plucked one bending
 *   GUITAR   the guitar (NYLON / STEEL: its body's two bouts and the sound hole; ELEC: a solid body, two pickups), its
 *            six strings over the neck, the plucked one (by its register) shaking; MUTE a palm on them by the bridge
 *   ACID     the 303's knobs (CUT RES ENV DEC ACC, the cutoff kicked by each note), its wave ringing at the resonance
 *            after each edge, sixteen steps running as it plays
 *   NOISE    a noise line (smoother as COLOR darkens), the band it is filtered to (FREQ, PEAK), DENSE's dust
 *   DRIFT    the six partials (PARTS) as lines at their pitches, each wandering (DRIFT, SPEED) and fading (FLICK, TILT)
 * ANIMAL has none: its being (ui_being.c). */
#define VX_POOL __attribute__((section(".pool")))
#define VX_CODE __attribute__((noinline))
#define VX_NE NTRK                                       /* (chord-harp fork) a track each (VX_K): was a drawing each */
static struct {
    uint32_t tm[VX_NE], fr[VX_NE];                       /* each track's (VX_K) time (it runs with its sound), the frame it ran */
    int32_t en[VX_NE];                                   /* its level now (0..256) */
    uint32_t mt[VX_NE], ms[VX_NE];                       /* the mock-up's time (Q8 frames, 30 a second: with the sound), ms */
    uint32_t nage[VX_NE], nms[VX_NE];                    /* .. the newest note seen (voice_t.age), when it came (fm1_ms) */
    uint32_t nn[VX_NE];                                  /* .. how many notes it has seen (VOICE's turns) */
    int16_t w[3];                                        /* (ANIMAL's, kept for the host tests) */
    uint8_t age[8], last;                                /* DRUM-X: each lane's age seen; the lane struck last */
    uint32_t hitm[8], kms, swt;                          /* .. its last hit (fm1_ms); the ms drawn */
    uint32_t rph[2], bub[9];                             /* WHEEL: the rotors' phases seen; each bar's bubble (fm1_ms) */
    uint8_t kc, init;
} vxs VX_POOL;
/* (chord-harp fork) the drawings' state is each track's (was each drawing's, k: two tracks of one engine, as HOME's
 * tiles show them, shared it: one's notes moved the other, or neither) */
#define VX_K(t) ((uint32_t)((t) - trk) % NTRK)
/* the level of track t now (0..256: its part's peak, slewed: a hit at once, a quick fall), k: which drawing (unused:
 * the state is the track's); its time runs with it (once a frame) */
static int32_t vx_level(const track_t *t, uint32_t k0, int32_t en_home)
{
    uint32_t k = VX_K(t), dt = ui.frame - vxs.fr[k], dm = fm1_ms - vxs.ms[k];
    (void)k0;
    int32_t v;
    if (!dt)
        return vxs.en[k];
    if (dt > 8u) dt = 8u;
    vxs.fr[k] = ui.frame;
    vxs.ms[k] = fm1_ms;
    if (dm > 100u) dm = 100u;                            /* (a pause: no jump) */
    if (vxs.en[k])                                       /* (0.9.6) the mock-up's time: 30 frames a second while it sounds */
        vxs.mt[k] += dm * 7680u / 1000u;
    if (en_home >= 0)
        v = en_home;                                     /* (HOME: its own, eco_step's) */
    else {
        v = ((track_t *)t)->peak * 256 / 7000;
        ((track_t *)t)->peak = 0;
        v = v > 256 ? 256 : v;
        v = v > vxs.en[k] ? v : vxs.en[k] + (v - vxs.en[k] - 2) / 3;
    }
    v = v > 4 ? v : 0;                                   /* (HOME's level rests a little above 0: silence) */
    vxs.en[k] = v;
    if (v)
        vxs.tm[k] += dt * (uint32_t)(120 + v * 4);
    return v;
}

/* ------------------------------------------------------------------------------------------------- VOICE --- */
static int32_t vx_fall(uint32_t ms, uint32_t half)
{
    uint32_t n = ms / half;
    int32_t v;
    if (n > 8u) return 0;
    v = 256 >> n;
    return v - (int32_t)((uint32_t)v * (ms % half) / (2u * half));
}
/* ms since track t's newest note (4000 at most; k unused: the track's); *note: that note */
static uint32_t vx_age(const track_t *t, uint32_t k0, int32_t *note)
{
    uint32_t i, a = 0, n = 60, k = VX_K(t);
    (void)k0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].age > a) { a = t->v[i].age; n = t->v[i].note; }
    if (a > vxs.nage[k]) {
        vxs.nms[k] = fm1_ms;
        vxs.nn[k]++;
    }
    vxs.nage[k] = a;
    if (note) *note = (int32_t)n;
    return fm1_ms - vxs.nms[k] < 4000u ? fm1_ms - vxs.nms[k] : 4000u;
}

/* ---------------------------------------------------------------------------------------------- FM6 --- */
#ifndef VX_FM6_HOOK
#define VX_FM6_HOOK(k, x, y, r) ((void)0)               /* ui_render.c (host tests): each bicho as drawn (its lint) */
#endif
static const uint8_t FM6_CELL[32][6];                    /* (ui_graph.c) */
static void fm6_routes(uint32_t a, uint8_t *m);
static uint32_t fm6_fb_op(uint32_t a);
static uint32_t fm6_car_ops(uint32_t a);
static uint32_t fm6_alg_of(const track_t *t);
static void fop_freq(const uint8_t *op, char *s);
typedef struct {
    const uint8_t *p;                                    /* the patch as it plays (the macros on it) */
    int32_t env[6], amp[6], rat[6], rad[6], x[6], y[6];  /* per operator 1..6: its envelope now, level x envelope (0..256),
                                                          * ratio (Q8), radius, place (Q4 units) */
    uint32_t alg, car, fbo, st[6];                       /* the algorithm, its carriers, the feedback operator; each stage */
    uint8_t m[6];
} vxfm_t;
/* the operators of track t now: the sounding notes' envelopes (the loudest), each one's level, ratio, size, place */
static void vx_fm_ops(const track_t *t, vxfm_t *o)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, k, v, hi = 0, top = 0;
    o->p = fm6_eff[tr].ok ? fm6_eff[tr].p : fm6_patch[tr];
    o->alg = fm6_alg_of(t);
    o->car = fm6_car_ops(o->alg);
    o->fbo = fm6_fb_op(o->alg);
    fm6_routes(o->alg, o->m);
    for (k = 0; k < 6u; k++) {
        const uint8_t *op = o->p + (5u - k) * FP_OP;
        int32_t e = 0, ol = op[FP_OL] % 100u, a = ol * 256 / 99, c = op[FP_FC] & 31;
        o->st[k] = 4;
        for (v = 0; v < FM6_POLY; v++) {                 /* (msfa's level: scaleout units 0..127, the stage's L 0..99) */
            const fm6_env_t *n = &fm6_note[tr][v].env[5u - k];
            int32_t s = (((n->level >> 16) - n->outlevel + 4256) / 32 - 28) * 256 / 99;
            if (!fm6_note[tr][v].live || !t->v[v].active) continue;
            if (s > e || o->st[k] == 4u) { o->st[k] = n->ix | (n->down ? 0u : 8u); }
            e = s > e ? s : e;
        }
        o->env[k] = clamp(e, 0, 256);
        o->amp[k] = ol ? (a * ((a * 154 >> 8) + 102) / 256) * o->env[k] / 256 : 0;   /* ((ol / 99) ^ 1.6) x env */
        o->rat[k] = op[FP_MODE] ? 256 : (c ? c * 256 : 128) * (100 + op[FP_FF] % 100u) / 100;
        o->rad[k] = 120 * (141 + 141 * ol / 99) / 256 * (200 + (o->env[k] * 82 >> 8)) / 256;
        hi = (FM6_CELL[o->alg][k] & 7u) > hi ? FM6_CELL[o->alg][k] & 7u : hi;
        top = (uint32_t)(FM6_CELL[o->alg][k] >> 4) > top ? (uint32_t)FM6_CELL[o->alg][k] >> 4 : top;
    }
    for (k = 0; k < 6u; k++) {                           /* (fmPos: columns 26 apart, the carriers on the floor 38) */
        o->x[k] = ((int32_t)(FM6_CELL[o->alg][k] & 7u) * 2 - (int32_t)hi) * 13 * 16;
        o->y[k] = 28 * 16 - (int32_t)(FM6_CELL[o->alg][k] >> 4) * (top >= 3u ? 324 : 352);
    }
}

/* (0.9.7) HOME's beings (ui_lzhome.c): as the beings (bg_col) */
static uint16_t vk_heat(int32_t h) { return bg_col(clamp(h, 0, 255)); }

/* ------------------------------------------------------------------------------------------- the pen --- */
/* the frame drawn in: the mock-up's units to px (x = ox + u * s), fitted to a rectangle A (x y w h px) with an even
 * margin from the drawing's bounds bb (x0 y0 x1 y1, units) */
typedef struct { float s, ox, oy; } el_t;
static el_t E;
#define EL_TAU 6.2831853f
static float el_sin(float a) { return (float)og_sin((uint32_t)(int32_t)(a * 10430.378f) & 0xFFFFu) * (1.0f / 32768.0f); }
static float el_cos(float a) { return el_sin(a + 1.5707963f); }
/* a stroke: the points given (units) as a wobbly line, a fainter pass beside it (a pen's, as harp_ring's); al 0..256 */
#define EL_NP 48u
static float el_p[2u * EL_NP];
static uint32_t el_n;
static void el_pt(float x, float y)
{
    if (el_n < EL_NP) { el_p[2u * el_n] = x; el_p[2u * el_n + 1u] = y; el_n++; }
}
static void el_stroke(uint16_t c, uint32_t seed, int32_t al)
{
    uint32_t i, pass;
    for (pass = 0; pass < 2u; pass++) {
        int32_t px = 0, py = 0;
        for (i = 0; i < el_n; i++) {
            float w = 0.45f * el_sin((float)i * 0.9f + (float)seed * 3.1f + (float)pass), v = 0.45f * el_cos((float)i * 1.3f + (float)seed);
            int32_t x = (int32_t)((E.ox + el_p[2u * i] * E.s + w + (pass ? 0.5f : 0.0f)) * 16.0f);
            int32_t y = (int32_t)((E.oy + el_p[2u * i + 1u] * E.s + v + (pass ? 0.4f : 0.0f)) * 16.0f);
            if (i)
                og_line_a(px, py, x, y, c, pass ? al * 2 / 5 : al);
            px = x;
            py = y;
        }
    }
    el_n = 0;
}
static void el_line(float x0, float y0, float x1, float y1, uint16_t c, uint32_t seed, int32_t al)
{
    float dx = x1 - x0, dy = y1 - y0, l = dx * dx + dy * dy;
    uint32_t n = l > 1600.0f ? 10u : l > 400.0f ? 5u : 2u, i;
    for (i = 0; i <= n; i++)
        el_pt(x0 + dx * (float)i / (float)n, y0 + dy * (float)i / (float)n);
    el_stroke(c, seed, al);
}

/* track t's drawing k: its level (0..1), how fresh its newest note is (1 at the hit, falling), its time (ms, running
 * with the sound) */
typedef struct { float en, e, ms; int32_t note; uint32_t age; } el_in_t;
static el_in_t el_in(const track_t *t, uint32_t k, int32_t en_home)
{
    el_in_t r;
    r.en = (float)vx_level(t, k, en_home) * (1.0f / 256.0f);
    r.age = vx_age(t, k, &r.note);
    r.e = r.en > 0.0f ? (float)vx_fall(r.age, 150u) * (1.0f / 256.0f) : 0.0f;
    r.ms = (float)vxs.mt[VX_K(t)] * (1000.0f / (256.0f * 30.0f));
    return r;
}
/* (chord-harp fork) vx_pe: the EDIT values drawn instead of the track's (the DICE's BEFORE), 0: the track's */
static const int16_t *vx_pe;
#define EL_P(t, i) ((float)(vx_pe ? vx_pe[i] : (t)->p[P_E0 + (i)]) * (1.0f / 127.0f))   /* EDIT value i, 0..1 */

/* ------------------------------------------------------------------------------------------- the engines --- */


#include "ui_pixart.c"                                  /* (chord-harp fork) pixel art: FM6's operators */
#include "ui_robot.c"                                   /* (2026-10-28) the engines' pictures: a robot each */

/* ------------------------------------------------------------------------------------------------ the glue --- */
/* the engine's drawing in the rectangle x y w h (px: EDIT's left panel, HOME's middle, or a whole panel), en_home: HOME's
 * level for the track (-1: its own); 0 when the engine has none (its being then) */
/* (chord-harp fork) .. as engine ei's (another than the track's: a USER slot's thumbnail, the DICE's BEFORE; its moving
 * parts from track t) */
/* (2026-10-28) the engine's robot (ui_robot.c) */
static int vx_draw_e(const track_t *t, uint32_t ei, int32_t x, int32_t y, int32_t w, int32_t h, int32_t en_home)
{
    return rb_draw(t, ei, x, y, w, h, en_home);
}
static int vx_draw(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, int32_t en_home)
{
    return vx_draw_e(t, trk_eng(t), x, y, w, h, en_home);
}
/* DRUM-X: each lane's newest hit seen (vxs: the sound last struck, KIT's name on the right; the host tests call it to
 * see the lanes) */
static void vx_kit(const track_t *t)
{
    drum_lane_t *K = drum_kit_of(t);
    uint32_t i, now = fm1_ms;
    vxs.kms = now;
    for (i = 0; i < 8u; i++) {
        uint8_t a = K ? K[i].age : 255u;
        if (K && a < vxs.age[i] && !dx_lane_muted(i) && drum_hit_env(&K[i]) > 0) {   /* a new hit */
            vxs.hitm[i] = now;
            vxs.last = (uint8_t)i;
        }
        vxs.age[i] = a;
    }
}
/* EDIT > KIT (lane -1) and SOUND 1..4 (a DRUM track): DRUM-X's robot on the panel (2026-10-29: was the kit from above),
 * KIT over it on the left; on the right the sound last struck (KIT) or the sound edited (SOUND: lane) */
static void vx_kit_page(const track_t *t, int32_t lane)
{
    char b[16];
    uint32_t l;
    vx_kit(t);
    (void)vx_draw(t, 4, 14, 232, 112, -1);
    l = lane < 0 ? vxs.last & 7u : (uint32_t)lane & 7u;
    str_cpy(b, drum_lane_abbr(t, l), sizeof b);
    str_cpy(b + str_len(b), " \xB7 ", sizeof b - str_len(b));
    str_cpy(b + str_len(b), drum_lane_name(t, l), sizeof b - str_len(b));
    cv_text(8, 6 - AF_X_CAP_Y, &AF_X, "KIT", T_THEME);
    cv_text_r(232, 6 - AF_X_CAP_Y, &AF_X, b, lane < 0 ? T_TEXT : T_THEME, T_SURF);
}
/* OPERATOR / OPERATOR 2: the operators on the whole panel, the one picked lit; "ALG 05" on the left, its frequency on
 * the right ("OP3 x1.50") */
static VX_CODE void vx_fm6_page(const track_t *t, uint32_t sel, int op2)
{
    static const int32_t A[4] = {6, 20, 228, 116};
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    uint32_t alg = fm6_alg_of(t);
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    char b[16] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
    (void)op2;
    cv_oy = 0;
    px_fm6ops(t, A, sel);                                /* (chord-harp fork: pixel art, was el_fm6's lines) */
    if (!t->p[P_E0])
        str_cpy(b + 6, " PAT", 5);
    cv_text(10, 4, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
    cv_text_r(228, 4, &AF_S, fq, T_TEXT, T_SURF);
}
/* OP ENV: the picked operator's envelope as the patch has it (from L4 to L1 at R1, L2 at R2, L3 at R3, held while the key
 * is down, back to L4 at R4; a stage as wide as its rate is slow), the stage KNOB 3 / 4 edit in the accent, a dot where
 * the operator is now (its level); "OP3" top left, the stage's R and L right */
static VX_CODE void vx_fm6env(const track_t *t)
{
    static const char *const STG[4] = {"ATK", "DEC", "SUS", "REL"};
    static const int32_t A[4] = {6, 20, 228, 114};
    const uint8_t *op = fop_bytes((uint32_t)(t - trk));
    uint32_t ko = fop_op % 6u, sg = fop_stg & 3u;
    char b[24] = {'O', 'P', (char)('1' + ko), 0};
    cv_oy = 0;
    px_fm6env(t, A, op, ko, sg);                         /* (chord-harp fork: pixel art, was line drawings) */
    cv_text(10, 4, &AF_S, b, T_MID);
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(228, 4, &AF_S, b, T_TEXT, T_SURF);
}
