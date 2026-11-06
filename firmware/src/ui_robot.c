/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork, 2026-10-28, the user's design: tools/robots/engine_robots.html, its source of truth) Each engine a
 * robot in pixel art, 20 x 30 art pixels on a 30 x 34 grid (its arms out), drawn wherever the engine's picture was (EDIT's
 * panel, HOME's ENGINE and ROUTES, PRESETS, SAVE, the USER slots' faces ..) at the largest whole scale that fits, and on
 * HOME's road, dancing. (2026-11-03) Restyled for the cyberpunk street: the user's palette (ui_robots.h), silver-white
 * steel, a dark outline and a rim light (cyan left, pink right: rb_rim, drawn by rb_blit), six body shapes (rb_build), and
 * DIGITAL's robot (a calculator).
 * A robot is put together on its grid (rb_px, palette indices) as the page does it: the antenna, the arms, the legs and
 * feet, the body, its chest, the head, its eyes and mouth, then outlined (dark all round). The still parts (the heads, the body, the arms in every state, the legs, the foot) are the page's drawings as
 * tables (ui_robots.h: tools/robots/export_parts.mjs -> PNGs -> robots_convert.py); what moves with the sound is code,
 * ported from the page line by line: the chest (the record spins, the tape turns, the drawbars rise, the static fizzes,
 * a key goes down, the harp string shakes ..), the antenna (lit on a note; the Leslie turns, the halo bobs), the eyes
 * (a blink, the visor's scan, the single eye looking round with the note), the mouth (VOICE's opens with the level).
 * The dance (the user's, 2026-11-03: to the track's notes, the song playing or not, a sequence or the keys): a new move
 * each note (RB_POSE, picked at random, never the same twice running), a knee dip after it, a hop on it; arms down
 * again 1.2 s after the last (the right one raised by a note): no notes, no dance. MENU > ANIM OFF: still.
 * Included by ui_engline.c (el_in, vxs) after ui_pixart.c. */
#include "ui_robots.h"
#define RB_W 30
#define RB_H 34
static uint8_t rb_px[RB_H][RB_W] __attribute__((section(".lowbuf")));   /* the grid: 0 nothing, else RB_PAL's (1..15 the robot's own) */
static uint32_t rb_occ[RB_H] __attribute__((section(".lowbuf")));       /* the robot before its outline, a bit a pixel (the rim) */
#define RB_OC(x, y) ((uint32_t)(x) < RB_W && (uint32_t)(y) < RB_H && (rb_occ[y] >> (x) & 1u))
static int32_t rb_ox, rb_oy;                            /* where the robot's 0 0 is on it (the page's Layer ox oy: 5 2) */
static struct {
    float ang[NTRK];                                    /* each track's record (ANALOG's chest): it spins with the sound */
    uint32_t ms[NTRK];                                  /* .. its last frame (fm1_ms) */
    uint32_t beat, bms;                                 /* the dance's beats so far, when the last came (fm1_ms) */
    uint8_t q;                                          /* the beat in the bar seen (clk_step / 4; 0xFF stopped) */
} rbs;
typedef struct { float t, hit, level, ang; int32_t note; uint8_t blink, pose, dip, walk; } rb_in_t;   /* the page's s (walk:
                                                         * rb_in 1 not dancing, HOME's road 1 + its stride 0..3) */

static int32_t rb_fl(float v) { int32_t i = (int32_t)v; return i - (v < (float)i); }   /* Math.floor */
#define RB_RND(v) rb_fl((v) + 0.5f)                     /* Math.round */
static VX_CODE float rb_sin(float a) { return fm_sinf(a); }   /* (one copy each: fastmath's are inline) */
static VX_CODE float rb_cos(float a) { return fm_cosf(a); }
static VX_CODE float rb_sqrt(float a) { return fm_sqrtf(a); }
static uint32_t rb_hsh(uint32_t n)                      /* the page's hsh */
{
    n = (n ^ (n >> 16)) * 0x45d9f3bu;
    n = (n ^ (n >> 16)) * 0x45d9f3bu;
    return n ^ (n >> 16);
}
static void rb_set(int32_t x, int32_t y, uint32_t c)
{
    x += rb_ox, y += rb_oy;
    if ((uint32_t)x < RB_W && (uint32_t)y < RB_H)
        rb_px[y][x] = (uint8_t)c;
}
static void rb_setf(float x, float y, uint32_t c) { rb_set(rb_fl(x), rb_fl(y), c); }
static void rb_rect(int32_t x0, int32_t y0, int32_t w, int32_t h, uint32_t c)
{
    int32_t x, y;
    for (y = y0; y < y0 + h; y++)
        for (x = x0; x < x0 + w; x++)
            rb_set(x, y, c);
}
/* a ramp: five colours dark to light (RB_PAL's), r[0] .. r[4] */
#define RP(b) ((const uint8_t[5]){(uint8_t)(b), (uint8_t)((b) + 1u), (uint8_t)((b) + 2u), (uint8_t)((b) + 3u), (uint8_t)((b) + 4u)})
#define RH(i) (1u + (i))                                /* the robot's own: its head's ramp, its body's, its accent */
#define RBR(i) (6u + (i))
#define RAC(i) (11u + (i))
#define RS(i) (RB_STEEL + (i))
static void rb_box(int32_t x0, int32_t y0, int32_t w, int32_t h, const uint8_t *r, int round)   /* the page's box */
{
    int32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint32_t c;
            if (round && w > 2 && h > 2 && (i == 0 || i == w - 1) && (j == 0 || j == h - 1))
                continue;
            if (j == 0) c = (float)i > (float)w * 0.4f ? r[4] : r[3];
            else if (j == h - 1) c = r[0];
            else if (i == w - 1) c = r[3];
            else if (i == 0 || j == h - 2) c = r[1];
            else c = (float)j < (float)h * 0.4f ? r[3] : r[2];
            rb_set(x0 + i, y0 + j, c);
        }
}
static void rb_ell(float cx, float cy, float rx, float ry, const uint8_t *r)   /* the page's ell: a ball lit from up left */
{
    int32_t x, y, y1 = -rb_fl(-(cy + ry)), x1 = -rb_fl(-(cx + rx));
    for (y = rb_fl(cy - ry); y <= y1; y++)
        for (x = rb_fl(cx - rx); x <= x1; x++) {
            float dx = ((float)x + 0.5f - cx) / rx, dy = ((float)y + 0.5f - cy) / ry, d2 = dx * dx + dy * dy, li;
            int32_t k;
            uint32_t c;
            if (d2 > 1.0f)
                continue;
            li = dx * 0.55f + dy * -0.5f + rb_sqrt(1.0f - d2) * 0.67f;
            k = RB_RND((li + 1.0f) * 0.5f * 4.0f - 0.3f);
            c = r[k < 1 ? 1 : k > 4 ? 4 : k];
            if (li > 0.93f) c = r[4];
            if (d2 > 0.7f && li < 0.15f) c = r[0];
            rb_set(x, y, c);
        }
}
/* a still part (ui_robots.h) with its top left at x y (the robot's grid), its marker ramp the role's (RB_ROLE_*) */
static void rb_part(uint32_t p, int32_t x, int32_t y, uint32_t role)
{
    const rb_part_t *q = &RB_PART[p];
    const uint8_t *d = RB_DATA + q->off;
    uint32_t n = (uint32_t)q->w * q->h, i = 0, k, v;
    while (i < n) {
        k = 1u, v = *d++;
        if (v >= 0xF0u)
            k = v - 0xF0u + 2u, v = *d++;
        for (; k; k--, i++)
            if (v)
                rb_set(x + (int32_t)(i % q->w), y + (int32_t)(i / q->w), v <= 5u ? v + role : v);
    }
}
static void rb_part_at(uint32_t p, uint32_t role) { rb_part(p, RB_PART[p].x, RB_PART[p].y, role); }

static void rb_antenna(const rb_robot_t *sp, const rb_in_t *s, int32_t ty)
{
    int lit = s->hit > 0.3f;
    int32_t x, k;
    switch (sp->ant) {
    case RB_ANT_BALL: rb_rect(10, ty - 2, 1, 2, RS(2)); rb_ell(10.5f, (float)ty - 2.5f, 1.5f, 1.5f, lit ? RP(RB_YELLOW) : RP(RAC(0))); break;
    case RB_ANT_TWIN:
        for (x = 6; x <= 13; x += 7) { rb_rect(x, ty - 3, 1, 3, RS(2)); rb_set(x, ty - 4, lit ? RAC(4) : RAC(2)); }
        break;
    case RB_ANT_DISH: rb_rect(8, ty - 3, 5, 1, RS(3)); rb_rect(9, ty - 2, 3, 1, RS(2)); rb_set(10, ty - 1, RS(1)); if (lit) rb_set(10, ty - 4, RAC(4)); break;
    case RB_ANT_EARS:
        rb_rect(5, ty - 1, 3, 1, RH(3)); rb_rect(5, ty - 2, 2, 1, RH(3)); rb_set(5, ty - 3, RH(3)); rb_set(6, ty - 1, RB_PINK + 3u);
        rb_rect(12, ty - 1, 3, 1, RH(3)); rb_rect(13, ty - 2, 2, 1, RH(3)); rb_set(14, ty - 3, RH(3)); rb_set(13, ty - 1, RB_PINK + 3u);
        if (lit) { rb_set(5, ty - 3, RH(4)); rb_set(14, ty - 3, RH(4)); }
        break;
    case RB_ANT_LESLIE:
        rb_set(10, ty - 1, RS(1));
        rb_box(7, ty - 5, 7, 4, RP(RB_WOOD), 1);
        rb_rect(8 + rb_fl(s->t / (140.0f - s->level * 90.0f)) % 5, ty - 4, 1, 2, RB_INK);
        break;
    case RB_ANT_HALO: {
        int32_t b = RB_RND(rb_sin(s->t * 0.004f));
        rb_rect(7, ty - 4 + b, 6, 1, lit ? RB_C_fdf0b0 : RB_C_fce693); rb_set(6, ty - 3 + b, RB_C_fce693); rb_set(13, ty - 3 + b, RB_C_fce693);
        rb_rect(7, ty - 2 + b, 6, 1, RB_C_ff6c11);
        break;
    }
    case RB_ANT_BOLT: {
        static const int8_t B[6][2] = {{10, 1}, {11, 2}, {10, 3}, {11, 3}, {12, 4}, {11, 5}};
        for (k = 0; k < 6; k++)
            rb_set(B[k][0], ty - 6 + B[k][1], lit ? RB_C_ffffff : B[k][1] < 3 ? RB_C_fdf0b0 : RB_C_f9c80e);
        break;
    }
    case RB_ANT_RABBIT:
        for (k = 0; k < 4; k++) { rb_set(8 - k, ty - 1 - k, RS(3)); rb_set(11 + k, ty - 1 - k, RS(3)); }
        rb_set(4, ty - 5, lit ? RAC(4) : RAC(2)); rb_set(15, ty - 5, lit ? RAC(4) : RAC(2));
        break;
    default: break;
    }
}
static void rb_eyes(const rb_robot_t *sp, const rb_in_t *s)
{
    int32_t x;
    switch (sp->eyes) {
    case RB_EYES_DOTS: case RB_EYES_SLEEPY:
        for (x = 6; x <= 12; x += 6) {
            if (s->blink) { rb_rect(x, 8, 2, 1, RB_INK); continue; }
            rb_set(x, 7, RB_C_ffffff); rb_set(x + 1, 7, RB_C_6cebee); rb_set(x, 8, RB_C_6cebee); rb_set(x + 1, 8, RB_C_239fab);
            if (sp->eyes == RB_EYES_SLEEPY) rb_rect(x, 7, 2, 1, RH(1));
            else { rb_set(x - 1, 8, RB_TEAL + 2u); rb_set(x + 2, 7, RB_TEAL + 2u); }
        }
        break;
    case RB_EYES_PIXEL:
        for (x = 6; x <= 12; x += 6) {
            if (s->blink) { rb_rect(x, 8, 2, 1, RB_INK); continue; }
            rb_rect(x, 7, 2, 2, RB_C_2de2e6); rb_set(x, 7, RB_C_c0f6f7);
        }
        break;
    case RB_EYES_SHADES:
        rb_rect(5, 7, 4, 2, RB_INK); rb_rect(11, 7, 4, 2, RB_INK); rb_rect(9, 7, 2, 1, RB_INK);
        rb_set(6, 7, RB_C_6a6894); rb_set(12, 7, RB_C_6a6894);
        break;
    case RB_EYES_VISOR: {
        int32_t g = rb_fl(s->t / 110.0f) % 14, p = g < 8 ? 6 + g : 6 + (13 - g);
        p = p < 6 ? 6 : p > 13 ? 13 : p;
        rb_rect(6, 7, 8, 2, RB_INK);
        rb_set(p, 7, RB_C_ff4365); rb_set(p, 8, RB_RED + 2u); rb_set(p - 1, 7, RB_RED + 1u); rb_set(p + 1, 7, RB_RED + 1u);
        break;
    }
    default: {                                          /* single: one big eye, looking round with the note */
        int32_t dx = RB_RND(rb_cos((float)s->note) * 0.9f), dy = RB_RND(rb_sin((float)s->note) * 0.6f);
        rb_ell(10.0f, 8.5f, 2.6f, 2.6f, RP(RB_CREAM));
        rb_rect(9 + dx, 8 + dy, 2, 2, s->blink ? RB_CREAM + 1u : RB_INK);
        if (!s->blink) rb_set(10 + dx, 8 + dy, RB_C_6cebee);
        break;
    }
    }
}
static void rb_mouth(const rb_robot_t *sp, const rb_in_t *s)
{
    int32_t open = sp->talk ? RB_RND(s->level * 2.2f) : s->hit > 0.5f, y0 = open > 1 ? 10 : 11, x, k;
    for (x = 8; x < 12; x++)
        for (k = 0; k <= open; k++)
            rb_set(x, y0 + k, RB_INK);
    if (open > 1) rb_rect(9, y0 + open, 2, 1, RB_RED + 2u);
}
/* the chest, x 5..14, y 16..22: each engine's own, moving with the sound */
static void rb_screen(void) { static const uint8_t S[5] = {RB_BLACK, RB_BLACK, RB_BLACK + 1u, RB_BLACK + 2u, RB_BLACK + 3u}; rb_box(5, 16, 10, 7, S, 1); }
static void rb_emblem(const rb_robot_t *sp, const rb_in_t *s)
{
    const float t = s->t, lv = s->level, hit = s->hit;
    const uint32_t B = RB_BLACK, C = RB_CREAM, S = RB_STEEL;
    int32_t x, y, i, k;
#define PICK(n) (((s->note % (n)) + (n)) % (n))
    switch (sp->em) {
    case RB_EM_RECORD:
        for (y = 15; y < 24; y++)
            for (x = 5; x < 15; x++) {
                float dx = (float)x + 0.5f - 10.0f, dy = (float)y + 0.5f - 19.5f, d = rb_sqrt(dx * dx + dy * dy);
                uint32_t c;
                if (d > 3.9f) continue;
                c = d > 2.1f && d < 2.9f ? B + 2u : B + 1u;
                if (d > 1.6f && dx > 0.0f && -dy > dx * 0.5463f && -dy < dx * 3.6021f)   /* (atan2 between -1.3 and -0.5) */
                    c = d > 2.9f ? B + 4u : B + 3u;
                if (d < 1.4f) c = dx > 0.0f && dy < 0.0f ? RAC(4) : RAC(2);
                rb_set(x, y, c);
            }
        rb_setf(10.0f + rb_cos(s->ang) * 2.8f, 19.5f + rb_sin(s->ang) * 2.8f, B + 3u);
        rb_setf(10.0f + rb_cos(s->ang), 19.5f + rb_sin(s->ang), RB_C_ffffff);
        break;
    case RB_EM_GRILLE:
        for (y = 16; y < 23; y++)
            for (x = 5; x < 12; x++)
                rb_set(x, y, (y & 1) ? S + 2u : S);
        rb_rect(5 + PICK(7), 16, 1, 7, RAC(4));
        rb_ell(13.5f, 19.5f, 1.7f, 1.7f, RP(RAC(0)));
        break;
    case RB_EM_MOON: {
        float sx = 6.0f * rb_sin(t * 0.0006f);
        for (y = 16; y < 23; y++)
            for (x = 6; x < 14; x++) {
                float dx = (float)x + 0.5f - 10.0f, dy = (float)y + 0.5f - 19.5f, d = rb_sqrt(dx * dx + dy * dy), ex = dx - sx;
                if (d > 3.4f) continue;
                rb_set(x, y, rb_sqrt(ex * ex + dy * dy) < 3.4f ? RBR(0) : d > 2.6f ? RB_MOON + 2u : (x + y) % 5 == 0 ? RB_MOON + 1u : RB_MOON + 3u);
            }
        if (hit > 0.4f) rb_set(11, 18, RB_C_ffffff);
        break;
    }
    case RB_EM_TAPE: {
        static const uint8_t T[5] = {RB_C_0d0221, RB_C_241734, RB_C_3a3466, RB_C_8a88b0, RB_C_ffc49a};
        int32_t a = rb_fl(t / (180.0f - lv * 120.0f)) & 1;
        rb_box(5, 17, 10, 6, T, 1);
        rb_rect(7, 18, 6, 3, RB_INK);
        for (x = 8; x <= 11; x += 3) { rb_rect(x, 19, 2, 1, T[3]); rb_set(x + a, 19, RB_C_ffffff); }
        rb_rect(7, 22, 6, 1, T[1]);
        break;
    }
    case RB_EM_SPEAKER: {
        float r = 3.0f + lv * 0.7f;
        rb_ell(10.0f, 19.5f, r, r, RP(C));
        rb_ell(10.0f, 19.5f, 1.4f, 1.4f, RP(B));
        for (i = 0; i < 12; i++) {
            float an = (float)i / 12.0f * 6.283f;
            if (rb_cos(an - 2.3f) > 0.2f) rb_setf(10.0f + rb_cos(an) * (r - 1.0f), 19.5f + rb_sin(an) * (r - 1.0f), C + 1u);
        }
        break;
    }
    case RB_EM_DRAWBARS: {
        static const uint8_t COL[5] = {RB_WOOD + 2u, RB_WOOD + 2u, RB_CREAM + 3u, RB_CREAM + 3u, RB_BLACK + 2u};
        for (i = 0; i < 5; i++) {
            int32_t h = 1 + RB_RND((rb_sin((float)i * 1.7f + t * 0.003f) * 0.5f + 0.5f) * (1.0f + lv * 4.0f));
            rb_rect(6 + i * 2, 22 - h, 1, h, COL[i]); rb_set(6 + i * 2, 22 - h, RB_C_ffffff);
            rb_set(6 + i * 2, 22, S + 1u);
        }
        break;
    }
    case RB_EM_DRUM: {
        static const uint8_t LIT[5] = {RB_CREAM + 3u, RB_CREAM + 3u, RB_CREAM + 4u, RB_CREAM + 4u, RB_C_ffffff};
        rb_box(5, 18, 10, 5, RP(RB_RED), 0);
        for (x = 6; x < 14; x++) rb_set(x, 19 + (x & 1) * 2, C + 2u);
        rb_ell(10.0f, 17.5f, 5.0f, 1.6f, hit > 0.4f ? LIT : RP(C));
        break;
    }
    case RB_EM_STATIC: {
        uint32_t f = (uint32_t)rb_fl(t / 70.0f), v;
        rb_screen();
        for (y = 17; y < 22; y++)
            for (x = 6; x < 14; x++)
                if ((float)(v = rb_hsh(f * 1000u + (uint32_t)(y * 20 + x)) % 100u) < 25.0f + lv * 50.0f)
                    rb_set(x, y, v < 8u ? RB_C_ffffff : v < 30u ? RB_C_8a88b0 : RB_C_3a3466);
        break;
    }
    case RB_EM_PAW: {
        float w = hit > 0.4f ? 1.0f : 0.0f;
        rb_ell(10.0f, 20.5f, 2.4f, 1.8f, RP(RB_PINK));
        rb_ell(7.0f, 18.0f, 1.1f, 1.1f, RP(RB_PINK)); rb_ell(10.0f, 17.0f - w, 1.1f, 1.1f, RP(RB_PINK)); rb_ell(13.0f, 18.0f, 1.1f, 1.1f, RP(RB_PINK));
        break;
    }
    case RB_EM_KEYS:
        k = PICK(5);
        for (i = 0; i < 5; i++)
            for (y = 17; y < 22; y++) {
                rb_set(5 + 2 * i, y, i == k && hit > 0.3f ? C + 1u : y == 17 ? C + 4u : C + 3u);
                rb_set(6 + 2 * i, y, C + 2u);
            }
        rb_rect(6, 17, 1, 3, B + 1u); rb_rect(8, 17, 1, 3, B + 1u); rb_rect(12, 17, 1, 3, B + 1u);
        break;
    case RB_EM_HARP:
        rb_rect(6, 16, 1, 7, RB_WOOD + 3u);
        for (x = 6; x < 14; x++) rb_set(x, 16 + RB_RND((float)(x - 6) * 0.3f), RB_C_f9c80e);
        k = PICK(4);
        for (i = 0; i < 4; i++) {
            int32_t hx = 8 + i * 2, top = 17 + RB_RND((float)(hx - 6) * 0.3f);
            for (y = top; y < 22; y++)
                rb_set(hx + (i == k && hit > 0.2f && y > top && ((rb_fl(t / 40.0f) + y) & 1) ? 1 : 0), y, C + 3u);
        }
        rb_rect(6, 22, 9, 1, RB_WOOD + 2u);
        break;
    case RB_EM_TINES: {
        static const uint8_t H[5] = {3, 4, 5, 4, 3};
        k = PICK(5);
        rb_rect(5, 17, 10, 1, S + 1u);
        for (i = 0; i < 5; i++) {
            int ring = i == k && hit > 0.2f;
            rb_rect(6 + i * 2, 18, 1, H[i], ring ? RB_C_ffffff : S + 3u);
            if (ring && (rb_fl(t / 50.0f) & 1)) rb_set(7 + i * 2, 17 + H[i], S + 4u);
        }
        break;
    }
    case RB_EM_GUITAR:
        rb_ell(10.0f, 19.5f, 2.6f, 2.6f, RP(B));
        for (i = 0; i < 12; i++) {
            float an = (float)i / 12.0f * 6.283f;
            rb_setf(10.0f + rb_cos(an) * 3.2f, 19.5f + rb_sin(an) * 3.2f, C + 2u);
        }
        for (i = 0; i < 4; i++) rb_rect(8 + i, 16, 1, 7, S + 4u);
        if (hit > 0.3f)
            for (i = 0; i < 4; i++) rb_set(8 + i, 18 + (rb_fl(t / 40.0f) + i) % 3, RB_C_ffffff);
        break;
    case RB_EM_SCOPE:
        rb_screen();
        for (x = 6; x < 14; x += 2) rb_set(x, 19, RB_C_073a4a);
        for (x = 6; x < 14; x++) {
            float yy = 19.5f + rb_sin((float)x * 0.9f + t * 0.012f) * (0.4f + lv * 2.4f);
            rb_setf((float)(x - 1), yy, RB_C_1a5c70); rb_setf((float)x, yy, RB_C_2de2e6);
        }
        break;
    case RB_EM_BANDS: {
        static const uint8_t COL[3] = {RB_GREEN + 3u, RB_MINT + 3u, RB_VIOLET + 3u};
        rb_screen();
        for (k = 0; k < 3; k++)
            for (x = 6; x < 14; x++)
                rb_setf((float)x, 17.5f + (float)k * 1.6f + rb_sin((float)x * 0.6f + t * 0.004f + (float)k * 1.3f) * (0.6f + lv * 0.6f), COL[k]);
        break;
    }
    case RB_EM_BRAID: {
        static const uint8_t COL[3] = {RB_PINK + 3u, RB_YELLOW + 3u, RB_TEAL + 3u};
        int front;
        for (front = 0; front < 2; front++)              /* the back halves first, then the front */
            for (k = 0; k < 3; k++)
                for (x = 5; x < 15; x++) {
                    float p = (float)x * 0.9f - t * 0.006f + (float)k * 2.09f;
                    if ((rb_cos(p) > 0.0f) == front)
                        rb_setf((float)x, 19.5f + rb_sin(p) * 2.4f, front ? COL[k] : RB_VIOLET + 1u);
                }
        break;
    }
    case RB_EM_BOW: {
        int32_t bx = 4 + RB_RND((rb_sin(t * 0.004f) * 0.5f + 0.5f) * 5.0f * (0.3f + lv));
        rb_rect(8, 16, 4, 7, S + 3u);
        rb_rect(bx, 19, 7, 1, C + 3u); rb_rect(bx, 18, 7, 1, RB_WOOD + 1u); rb_set(bx, 18, RB_WOOD + 3u);
        break;
    }
    case RB_EM_VPS: {
        float d = 0.25f + 0.5f * (rb_sin(t * 0.002f) * 0.5f + 0.5f);
        rb_screen();
        for (x = 6; x < 14; x++) {
            float p = (float)(x - 6) / 7.0f, yy = p < d ? p / d * 0.6f : 0.6f + (p - d) / (1.0f - d) * 0.4f;
            rb_setf((float)x, 21.0f - yy * 4.0f, RB_C_fb8dd3);
        }
        break;
    }
    case RB_EM_BARS: {
        int32_t my;
        k = PICK(4);
        for (i = 0; i < 4; i++)
            rb_rect(5 + i, 16 + i * 2, 10 - i * 2, 1, i == k && hit > 0.3f ? RB_C_ffffff : RB_YELLOW + 3u - (uint32_t)(i & 1));
        my = 15 + k * 2 - (hit > 0.3f ? 0 : 1);
        rb_set(13 - k, my, RB_RED + 3u); rb_set(14 - k, my - 1, RB_WOOD + 3u);
        break;
    }
    case RB_EM_WAVE: {
        int32_t c = 5 + rb_fl(t / 125.0f) % 10;
        rb_rect(5, 16, 10, 3, RB_C_25aab5); rb_rect(5, 19, 10, 3, RB_C_023788); rb_rect(5, 22, 10, 1, RB_C_ffc49a);
        rb_set(c, 19, RB_C_e8eaf6); rb_set(c + 1, 18, RB_C_e8eaf6); rb_set(c + 2, 18, RB_C_e8eaf6);
        rb_set(c + 3, 19, RB_C_c4c6dc); rb_set(c - 1, 20, RB_C_c4c6dc);
        rb_set(13, 17, RB_YELLOW + 4u);
        break;
    }
    case RB_EM_SEGS: {                                  /* (2026-11-03) DIGITAL: its note in seven segments, two digits */
        static const uint8_t SEG7[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
        static const uint8_t SC[5] = {RB_BLACK, RB_BLACK, RB_BLACK + 1u, RB_BLACK + 1u, RB_BLACK + 2u};
        uint32_t n = (uint32_t)PICK(100), on = sp->ac + (hit > 0.3f ? 4u : 3u), m;
        rb_box(5, 16, 10, 7, SC, 1);
        for (k = 0; k < 2; k++) {
            x = 6 + k * 4, m = SEG7[k ? n % 10u : n / 10u];
            if (m & 1u) rb_rect(x + 1, 17, 2, 1, on);
            if (m & 2u) rb_set(x + 3, 18, on);
            if (m & 4u) rb_set(x + 3, 20, on);
            if (m & 8u) rb_rect(x + 1, 21, 2, 1, on);
            if (m & 16u) rb_set(x, 20, on);
            if (m & 32u) rb_set(x, 18, on);
            if (m & 64u) rb_rect(x + 1, 19, 2, 1, on);
        }
        break;
    }
    default: break;
    }
#undef PICK
}
/* robot ri in its pose on rb_px, outlined (the page's robot). Its shape (RB_SHAPE_*, 2026-11-03): STD as before; TALL
 * (raised 2, a narrower body, stilts), SQUAT (lowered 2, a wide body, stumps, wide feet), ROUND (an oval body), TRACKS (a
 * tread on four wheels, its lugs moving), HOVER (no legs: a thruster, its flame with the level, bobbing). up: the upper
 * body's shift, spread: the shoulders' */
static void rb_build(uint32_t ri, const rb_in_t *s)
{
    const rb_robot_t *sp = &RB_ROBOTS[ri % RB_NROBOTS];
    const rb_pose_t *P = &RB_POSE[s->pose % RB_NPOSES];
    const uint32_t sh = sp->shape, legs = sh != RB_SHAPE_TRACKS && sh != RB_SHAPE_HOVER, D = RB_STEELD;
    int32_t lean = P->lean, squat = P->squat > s->dip ? P->squat : s->dip, nod = P->nod, ty = RB_HEAD_TOP[sp->head], lu, ru, x, y, k;
    int32_t up = sh == RB_SHAPE_TALL ? -2 : sh == RB_SHAPE_SQUAT ? 2 : sh == RB_SHAPE_HOVER ? RB_RND(rb_sin(s->t * 0.005f)) - 1 : 0;
    int32_t spread = sh == RB_SHAPE_SQUAT ? 2 : sh == RB_SHAPE_TALL ? -1 : sh == RB_SHAPE_ROUND ? 1 : 0;
    for (y = 0; y < RB_H; y++)
        for (x = 0; x < RB_W; x++)
            rb_px[y][x] = 0;
    if (!legs && squat > 1)
        squat = 1;
    rb_ox = 5 + lean, rb_oy = 2 + up + squat + nod;
    rb_antenna(sp, s, ty);
    rb_ox -= lean + spread, rb_oy -= nod;
    rb_part_at(RBP_ARM_L_DOWN0 + P->la, 0);
    rb_ox += 2 * spread;
    rb_part_at(RBP_ARM_R_DOWN0 + (P->la == RB_ARM_DOWN && P->ra == RB_ARM_DOWN ? (uint32_t)RB_RND(s->hit * 3.0f) : P->ra), 0);
    rb_ox -= spread, rb_oy -= squat + up;
    lu = (s->walk ? s->walk == 2 : legs && P->lift == 1) ? 2 : 0, ru = (s->walk ? s->walk == 4 : legs && P->lift == 2) ? 2 : 0;
    if (sh == RB_SHAPE_TALL)                            /* stilts, small feet */
        for (k = 0; k < 2; k++) {
            int32_t lx = k ? 11 : 7, u = k ? ru : lu;
            rb_rect(lx, 23 + squat - u, 1, 6 - squat, RS(3)); rb_rect(lx + 1, 23 + squat - u, 1, 6 - squat, RS(1));
            rb_box(k ? 11 : 5, 29 - u, 4, 2, RP(D), 0);
        }
    else if (sh == RB_SHAPE_SQUAT) {                    /* stumps, wide flat feet */
        rb_box(4, 27 + (squat > 1 ? 1 : squat) - lu, 4, 1, RP(RB_STEEL), 0); rb_box(12, 27 + (squat > 1 ? 1 : squat) - ru, 4, 1, RP(RB_STEEL), 0);
        rb_box(2, 28 - lu, 7, 2, RP(D), 0); rb_box(11, 28 - ru, 7, 2, RP(D), 0);
    } else if (sh == RB_SHAPE_TRACKS) {                 /* a tread round four wheels, its lugs moving */
        int32_t ph = rb_fl(s->t / 90.0f) % 3;
        rb_box(7, 24, 6, 2, RP(RB_STEEL), 0);
        rb_box(1, 26, 18, 5, RP(D), 1);
        for (x = 2; x < 18; x++)
            if ((x + ph) % 3 == 0) rb_set(x, 26, D + 4u), rb_set(19 - x, 30, D);
        for (x = 4; x <= 16; x += 4) rb_ell((float)x, 28.5f, 1.5f, 1.5f, RP(RB_STEEL));
    } else if (sh == RB_SHAPE_HOVER) {                  /* a thruster, its flame */
        int32_t f = 3 + RB_RND(s->level * 2.0f + rb_sin(s->t * 0.03f)), w;
        rb_box(6, 24 + up, 8, 2, RP(D), 0); rb_rect(7, 26 + up, 6, 1, RS(1));
        for (k = 0; k < f; k++)
            w = 3 - (k >> 1) > 1 ? 3 - (k >> 1) : 1,
            rb_rect(10 - w, 27 + up + k, w * 2, 1, k == 0 ? RB_C_ffffff : k < 2 ? RB_TEAL + 4u : k < 4 ? RB_TEAL + 3u : RB_PINK + 3u);
    } else {
        if (squat < 2) {
            rb_part(squat ? RBP_LEG_BENT : RBP_LEG, 6, 25 + squat - lu, 0);
            rb_part(squat ? RBP_LEG_BENT : RBP_LEG, 11, 25 + squat - ru, 0);
        } else {
            rb_part(RBP_LEG_WIDE, 3, 25, 0);
            rb_part(RBP_LEG_WIDE, 13, 25, 0);
        }
        rb_part(RBP_FOOT, 4, 27 - lu, 0);
        rb_part(RBP_FOOT, 11, 27 - ru, 0);
    }
    rb_oy += squat + up, rb_ox += lean;
    if (sh == RB_SHAPE_ROUND) rb_ell(10.0f, 19.5f, 8.0f, 6.3f, RP(sp->br)), rb_rect(8, 12, 4, 1, RS(1));
    else if (sh == RB_SHAPE_SQUAT) {
        rb_box(0, 14, 20, 10, RP(sp->br), 1); rb_rect(8, 13, 4, 1, RS(1));
        rb_set(1, 15, sp->br + 4u); rb_set(18, 15, sp->br + 4u); rb_set(1, 22, sp->br + 4u); rb_set(18, 22, sp->br + 4u);
    } else if (sh == RB_SHAPE_TALL) rb_box(4, 14, 12, 11, RP(sp->br), 1), rb_rect(9, 12, 2, 2, RS(2));
    else if (sh == RB_SHAPE_HOVER) rb_box(3, 14, 14, 10, RP(sp->br), 1), rb_rect(8, 13, 4, 1, RS(1));
    else rb_part_at(RBP_BODY, RB_ROLE_B);
    rb_emblem(sp, s);
    rb_oy += nod;
    rb_part_at(RBP_HEAD_BOX + sp->head, RB_ROLE_H);
    rb_eyes(sp, s);
    rb_mouth(sp, s);
    if (sp->head == RB_HEAD_BOX || sp->head == RB_HEAD_WIDE || sp->head == RB_HEAD_TALL)
        rb_set(sp->head == RB_HEAD_WIDE ? 16 : sp->head == RB_HEAD_TALL ? 13 : 14, ty + 1, RB_C_ffffff);
    rb_ox = 5, rb_oy = 2;
    for (y = 0; y < RB_H; y++)                          /* the outline: dark all round (rb_occ: the robot before it, for the rim) */
        for (rb_occ[y] = 0, x = 0; x < RB_W; x++)
            rb_occ[y] |= (uint32_t)(rb_px[y][x] != 0) << x;
    for (y = 0; y < RB_H; y++)
        for (x = 0; x < RB_W; x++)
            if (!RB_OC(x, y) && (RB_OC(x + 1, y) || RB_OC(x - 1, y) || RB_OC(x, y - 1) || RB_OC(x, y + 1)))
                rb_px[y][x] = (uint8_t)RB_INK;
}
/* the rim light (the page's lit): an edge of the robot (not its own ink) lit, cyan on the left, pink on the right, a little
 * white on top: 1 2 3, 0 none */
static uint32_t rb_rim(int32_t x, int32_t y)
{
    if (!RB_OC(x, y) || rb_px[y][x] == RB_INK) return 0u;
    return !RB_OC(x - 1, y) ? 1u : !RB_OC(x + 1, y) ? 2u : !RB_OC(x, y - 1) ? 3u : 0u;
}
static uint16_t rb_lit(uint16_t c, uint32_t rim)        /* RGB565 c mixed half with cyan / pink, a quarter with white */
{
    static const uint16_t K[3] = {0x2F1C, 0xF039, 0xFFFF};   /* #2de2e6 #f706cf #ffffff */
    uint32_t k, a, r, g, b;
    if (!rim) return c;
    k = K[rim - 1u], a = rim == 3u ? 1u : 2u;           /* (quarters of the light) */
    r = ((c >> 11) * (4u - a) + (k >> 11) * a + 2u) / 4u;
    g = (((c >> 5) & 63u) * (4u - a) + ((k >> 5) & 63u) * a + 2u) / 4u;
    b = ((c & 31u) * (4u - a) + (k & 31u) * a + 2u) / 4u;
    return (uint16_t)(r << 11 | g << 5 | b);
}
static uint16_t rb_col(const rb_robot_t *sp, uint32_t i)
{
    return RB_PAL[i <= 5u ? sp->hr + i - 1u : i <= 10u ? sp->br + i - 6u : i <= 15u ? sp->ac + i - 11u : i];
}
/* rb_px's columns c0.. and rows r0.. (w x h of them, each an s x s square, or with dv > 1 one pixel a dv x dv block of
 * them) at screen x y, clipped to x0 y0 .. x1 y1 (canvas px, past it) */
static float rb_shear;                                  /* (HOME's pivot: each screen row moved by its height over rb_piv times this) */
static int32_t rb_piv;
static void rb_blit(uint32_t ri, int32_t c0, int32_t r0, int32_t w, int32_t h, int32_t x, int32_t y, int32_t s, int32_t dv,
                    int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    const rb_robot_t *sp = &RB_ROBOTS[ri % RB_NROBOTS];
    int32_t i, jj, n, nq = rb_shear != 0.0f ? s : 1;   /* (sheared: a row at a time) */
    for (jj = 0; jj < h * nq; jj++) {
        int32_t j = jj / nq, sy = y + j * s + jj % nq, sh = s / nq, ox = nq > 1 ? RB_RND((float)(rb_piv - sy) * rb_shear) : 0;
        if (sy < y0) sh -= y0 - sy, sy = y0;
        if (sy + sh > y1) sh = y1 - sy;
        if (sh <= 0)
            continue;
        for (i = 0; i < w; i += n) {
            uint32_t v = 0, k, rim = 0;
            int32_t sx, sw;
            if (dv > 1) {                               /* (a block: its centre's pixel, else any drawn) */
                static const uint8_t ORD[4] = {3, 0, 1, 2};
                for (k = 0; k < 4u && !v; k++) {
                    int32_t cx = c0 + i * dv + (ORD[k] & 1u), cy = r0 + j * dv + (ORD[k] >> 1);
                    if ((uint32_t)cx < RB_W && (uint32_t)cy < RB_H) v = rb_px[cy][cx], rim = rb_rim(cx, cy);
                }
                n = 1;
            } else {
                v = (uint32_t)(c0 + i) < RB_W && (uint32_t)(r0 + j) < RB_H ? rb_px[r0 + j][c0 + i] : 0u;
                rim = v ? rb_rim(c0 + i, r0 + j) : 0u;
                for (n = 1; i + n < w && rb_px[r0 + j][c0 + i + n] == v && rb_rim(c0 + i + n, r0 + j) == rim; n++)
                    ;
            }
            if (!v)
                continue;
            sx = x + ox + i * s, sw = n * s;
            if (sx < x0) sw -= x0 - sx, sx = x0;
            if (sx + sw > x1) sw = x1 - sx;
            if (sw <= 0)
                continue;
            cv_rect(sx, sy, sw, sh, rb_lit(rb_col(sp, v), rim));
        }
    }
}

/* the engine's robot (RB_NROBOTS: none) */
static uint32_t rb_of(uint32_t ei)
{
    static const uint8_t M[NENGINES] = {
        [ENGI_ANALOG] = RB_ANALOG + 1, [ENGI_FM6] = RB_FM6 + 1, [ENGI_PHASE] = RB_PHASE + 1, [ENGI_LOFI] = RB_LOFI + 1,
        [ENGI_VOICE] = RB_VOICE + 1, [ENGI_WHEEL] = RB_WHEEL + 1, [ENGI_DRUM] = RB_DRUM + 1, [ENGI_NOISE] = RB_NOISE + 1,
        [ENGI_ANIMAL] = RB_ANIMAL + 1, [ENGI_JUNO] = RB_JUNO + 1, [ENGI_OMNI] = RB_OMNI + 1, [ENGI_KALIMBA] = RB_KALIMBA + 1,
        [ENGI_GUITAR] = RB_GUITAR + 1, [ENGI_ACID] = RB_ACID + 1, [ENGI_DRIFT] = RB_DRIFT + 1, [ENGI_PLAITS] = RB_PLAITS + 1,
        [ENGI_STRINGS] = RB_STRINGS + 1, [ENGI_VPS] = RB_VPS + 1, [ENGI_MODAL] = RB_MODAL + 1, [ENGI_WSEQ] = RB_WSEQ + 1,
        [ENGI_TERRAIN] = RB_TERRAIN + 1, [ENGI_VECTOR] = RB_VECTOR + 1, [ENGI_DIGITAL] = RB_DIGITAL + 1,
        [ENGI_HYBRID] = RB_HYBRID + 1};
    return ei < NENGINES && M[ei] && eng_ok(ei) ? M[ei] - 1u : RB_NROBOTS;
}
/* the dance's clock: a beat each four 1/16 steps of the song playing (mod.c clk_step) */
static void rb_clock(void)
{
    uint32_t q = song.playing ? clk_step >> 2 : 0xFFu;
    if (q != rbs.q) {
        rbs.q = (uint8_t)q;
        if (q != 0xFFu)
            rbs.beat++, rbs.bms = fm1_ms;
    }
}
/* track t's robot ri as it moves now (the page's s), en_home: HOME's level for it */
static rb_in_t rb_in(const track_t *t, uint32_t ri, int32_t en_home)
{
    uint32_t k = VX_K(t), dm = fm1_ms - rbs.ms[k], i;
    el_in_t e = el_in(t, 0, en_home);
    rb_in_t s;
    rb_clock();
    rbs.ms[k] = fm1_ms;
    if (dm > 100u) dm = 100u;
    s.level = e.en, s.hit = e.e, s.note = e.note, s.t = (float)((uint32_t)e.ms & 0xFFFFFu);
    if (e.en > 0.0f) {
        rbs.ang[k] += (float)dm * (0.004f + e.en * 0.01f);
        if (rbs.ang[k] > 6.2832f) rbs.ang[k] -= 6.2832f;
    }
    s.ang = rbs.ang[k];
    s.blink = e.en > 0.0f && ((uint32_t)s.t / 125u + k * 5u) % 29u == 0u;   /* (its clock: still in silence) */
    s.pose = 0, s.dip = 0, s.walk = 1;
    if (e.age < 1200u) {                                /* (the page's poseAt: never the same move twice running;
                                                         * a move a note played, the rest pose 1.2 s after) */
        uint32_t seed = k * 13u + ri, n = vxs.nn[k];
        i = rb_hsh(n * 31u + seed * 977u) % RB_NPOSES;
        if (i == rb_hsh((n - 1u) * 31u + seed * 977u) % RB_NPOSES)
            i = (i + 5u) % RB_NPOSES;
        s.pose = (uint8_t)i;
        s.dip = !RB_POSE[i].squat && e.age > 120u && e.age < 260u;
        s.walk = 0;
    }
    return s;
}
/* the robot of engine ei, moving as track t sounds, as large as it fits x y w h (whole pixels, centred; smaller than
 * 30 x 34: its face, half size); 0: the engine has none */
static int rb_draw(const track_t *t, uint32_t ei, int32_t x, int32_t y, int32_t w, int32_t h, int32_t en_home)
{
    uint32_t ri = rb_of(ei);
    rb_in_t s;
    int32_t sc;
    if (ri >= RB_NROBOTS || !RES_OK())                  /* (chord-harp fork) the parts in the resource region (res.c) */
        return 0;
    s = rb_in(t, ri, en_home);
    s.walk = 0;                                         /* (walking: HOME's road only) */
    rb_build(ri, &s);
    sc = w / RB_W < h / RB_H ? w / RB_W : h / RB_H;
    if (sc < 1) {                                       /* (a thumbnail: the face, rows 2..15, a pixel a 2 x 2) */
        rb_blit(ri, 5, 2, 10, 7, x + (w - 10) / 2, y + (h - 7) / 2, 1, 2, x, y, x + w, y + h);
        return 1;
    }
    rb_blit(ri, 0, 0, RB_W, RB_H, x + (w - RB_W * sc) / 2 + RB_POSE[s.pose].shift * sc,
            y + (h - RB_H * sc) / 2 - RB_RND(s.hit * 1.5f * (float)sc), sc, 1, x, y, x + w, y + h);
    return 1;
}

#include "ui_road.c"                                    /* (2026-10-29) HOME: the road */
