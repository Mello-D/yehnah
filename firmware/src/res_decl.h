/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork, 2026-11-04) the resource region's declarations: core.h's, and gfx.c's (included before core.h: the
 * fonts' and keycaps' tables are in the region) */
#pragma once
#include <stdint.h>
/* (chord-harp fork, 2026-11-04, after Melodee) the resource region (res.c): read-only data marked YEHNAH_RES is linked
 * at its own flash address (app.ld .fres), outside the app, and read in place. RES_OK() 0: missing or another build's;
 * its readers fall back (a sound without its data stays silent). The host (tests, the web emulator): plain data, 1 */
#ifndef YEHNAH_RESREGION
#define YEHNAH_RESREGION 0
#endif
#if YEHNAH_RESREGION
#define YEHNAH_RES __attribute__((section(".fres"), aligned(4)))
static volatile uint8_t res_ok;          /* res.c: installed and this build's */
#define RES_OK() (res_ok != 0u)
#elif defined(YEHNAH_RES_SIM)              /* (host: tests/ui_test.c's fallbacks, the region "missing" at will) */
#define YEHNAH_RES
static volatile uint8_t res_ok = 1;
#define RES_OK() (res_ok != 0u)
#else
#define YEHNAH_RES
#define RES_OK() 1
#endif
#define ENG_NP(e) (RES_OK() ? (uint32_t)(e)->npresets : 0u)   /* (chord-harp fork) engine e's presets: none while the
                                                         * resource region is missing (the EDIT defaults, INIT, only) */
