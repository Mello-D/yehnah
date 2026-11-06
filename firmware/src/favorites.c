/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#define YEHNAH_FAVORITES 1
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..127; engines 16..30 in rows 0..14's upper half (favorite_byte);
                              * row 15's bytes 19.. are settings (ui.c usb_out ..) */
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites;
/* (chord-harp fork) a factory preset's bit: engines 0..15 in their rows' bytes 0..15, engines 16..30 (OMNI ..) in rows
 * 0..14's bytes 16..31, presets 0..127 each (no engine had more: those bytes were never set; row 15's 19.. are settings) */
static uint8_t *favorite_byte(uint32_t engine, uint32_t preset)
{
    return engine < NENGINES && engine < 31u && preset < 128u ?
           &favorites.factory[engine & 15u][(engine >= 16u ? 16u : 0u) + preset / 8u] : 0;
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    const uint8_t *b;
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((favorites.user >> preset) & 1u);
    b = favorite_byte(engine, preset);
    return b && ((*b >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    uint8_t *b = engine == NENGINES ? 0 : favorite_byte(engine, preset);
    if (engine == NENGINES ? preset >= UP_SLOTS : !b)
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        if (on) favorites.user |= 1u << preset;
        else favorites.user &= ~(1u << preset);
    } else {
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
