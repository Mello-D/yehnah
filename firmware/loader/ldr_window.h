/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* Where the update loader may erase / program (fm1_flash.h FL_RANGE_OK; tests/ldr_test.c refuses the same): the app area
 * and update records, never the head nor Felucca's store [0x97000, 0xE0000) (user data, never a record: ldr_core.c),
 * but (chord-harp fork, 2026-11-04) the resource region inside it (ldr_core.c ldr_res, src/res.c): left out before,
 * every erase there was refused and the installer's command 82 wrote the region after each install */
#pragma once
#ifndef FL_IN
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))
#endif
#define FL_RANGE_OK(off, n) (FL_IN(off, n, 0x4000u, 0x97000u) || FL_IN(off, n, 0xCA000u, 0xDC000u) || \
                             FL_IN(off, n, 0xE0000u, 0xFC000u))
