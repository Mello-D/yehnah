/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Andre Rezemini (SLOOP)
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Adapted from SLOOP 2.7.1 / Felucca 1.6.1: stopping core 1 can raise a delayed bus-invalid flag on core 0.
 * Core 0 only: mask those traps during the hold, clear the delayed flags, then restore protection and its lock. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"
#ifndef FM1_STOP_READ
#define FM1_STOP_READ(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define FM1_STOP_WRITE(a, v) (*(volatile uint32_t *)(uintptr_t)(a) = (v))
#define FM1_STOP_SYNC() __asm__ volatile("csync" ::: "memory")
#define FM1_STOP_MASK(v) __asm__("" : "+r"(v))
#endif
FM1_INLINE void fm1_core1_stop(void)
{
    uint32_t locked = !(FM1_STOP_READ(0x1EEE240u) & 1u), en, bus = 0x003F0030u;
    volatile uint32_t n;
    FM1_STOP_MASK(bus);                         /* retain the vendor compiler's register mask sequence */
    if (locked) FM1_STOP_WRITE(0x1EEE240u, 0xE7u);
    en = FM1_STOP_READ(0x1EEE340u);
    FM1_STOP_WRITE(0x1EEE340u, en & ~bus);
    FM1_STOP_SYNC();
    FM1_STOP_WRITE(0x1EEE004u, FM1_STOP_READ(0x1EEE004u) | 2u);
    FM1_STOP_WRITE(0x1EEE004u, FM1_STOP_READ(0x1EEE004u) & ~8u);
    FM1_STOP_SYNC();
    for (n = 0; n < 256u; n++) ;
    FM1_STOP_WRITE(0x1EEE248u, bus);
    FM1_STOP_SYNC();
    FM1_STOP_WRITE(0x1EEE340u, en);
    if (locked && (FM1_STOP_READ(0x1EEE240u) & 1u))
        FM1_STOP_WRITE(0x1EEE240u, 0xE7u);
    FM1_STOP_SYNC();
}
