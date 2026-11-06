/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Copyright (C) 2026 Mello-D
 * (chord-harp fork, 2026-11-04) after Melodee's res.c (github.com/keremimo/melodee); no old preset banks to move here */
/* The resource region: read-only data the app does not need to start (res_decl.h YEHNAH_RES: the wavetables, the
 * presets, kits, the J-6 sets, the genres / patterns, the big and medium faces, the keycaps), linked at
 * its flash address (app.ld .fres) and read in place through the plain XIP window (fm1_flash.h fl_plain_window_init).
 * 0xCA000..0xDBFFF: where songs 7 and 8 were (storage.c). The update loader writes it from the package's res.bin
 * (firmware/loader/ldr_core.c ldr_res), editor command 82 (editor.c ed_res_handle: the web installer) when the loader
 * could not (a first install from another firmware); build.py patches res_want with the length and CRC-32 this
 * build's payload has, so a code-only update keeps the region. Missing, torn or another build's: res_ok 0, the
 * readers fall back and the start asks for another install (main.c).
 *   0xCA000  header: magic, length, CRC-32 of the payload, header CRC (written last: the commit record)
 *   0xCA020  payload: .fres as linked; the sector tails hold data too (build.py: none parses as an update record) */
#define RES_BASE 0xCA000u
#define RES_SECTORS 18u                               /* to 0xDBFFF, below the user preset banks */
#define RES_HEAD 32u
#define RES_MAGIC 0x3153524Du                       /* MRS1 (Melodee's) */
#define RES_WANT 0x544E5752u                        /* RWNT */
typedef struct { uint32_t magic, len, crc, rsv[4], hcrc; } res_hdr_t;
_Static_assert(sizeof(res_hdr_t) == RES_HEAD, "resource commit record");
_Static_assert(RES_BASE + RES_SECTORS * ST_SECTOR == 0xDC000u, "the region ends at the user preset banks");
#ifndef RES_VIEW                                     /* (tests/res_test.c: its simulated flash, its own res_want) */
extern const uint8_t _fres_start[], _fres_end[];
#define RES_VIEW _fres_start                         /* the payload as the app reads it (XIP) */
#define RES_LINKED ((uint32_t)(_fres_end - _fres_start))
#define RES_WANT_CONST const
#endif
/* build.py finds this by name and writes the payload's length and CRC-32 into the image */
__attribute__((used)) static RES_WANT_CONST volatile uint32_t res_want[3] = {RES_WANT, 0u, 0u};

static uint8_t res_open;                             /* begin erased the region: writes may follow */
static int res_valid(void)
{
    res_hdr_t h;
    uint32_t len = res_want[1];
    if (!flash_ok || res_want[0] != RES_WANT || !len || len != RES_LINKED) return 0;
    if (st_read(RES_BASE, &h, sizeof h) || h.magic != RES_MAGIC || h.hcrc != st_crc32(&h, sizeof h - 4u) ||
        h.len != len || h.crc != res_want[2])
        return 0;
    return st_crc32(RES_VIEW, len) == h.crc;      /* the payload as the app reads it */
}
/* boot, after persist_boot: the plain window first, then the check */
static void res_boot(void)
{
    if (flash_ok)
        fl_plain_window_init();
    res_ok = (uint8_t)res_valid();
}

/* begin: the transport stopped, the readers off, the region erased */
static int res_begin(void)
{
    if (!flash_ok) return 2;
    if (transport_busy()) return 4;
    res_ok = 0;
    for (uint32_t s = 0; s < RES_SECTORS; s++)
        if (st_erase(RES_BASE + s * ST_SECTOR)) return 3;
    res_open = 1;
    return 0;
}
/* write payload bytes at off (RAM, any length), split at the NOR pages */
static int res_write(uint32_t off, const uint8_t *src, uint32_t n)
{
    if (!flash_ok || !res_open || off > res_want[1] || n > res_want[1] - off) return 1;
    uint32_t at = RES_BASE + RES_HEAD + off;
    while (n) {
        uint32_t k = 256u - (at & 255u);
        if (k > n) k = n;
        if (st_prog(at, src, k)) return 3;
        at += k; src += k; n -= k;
    }
    return 0;
}
/* commit: the payload checked as the app will read it, then the header last */
static int res_commit(void)
{
    if (!flash_ok || !res_open) return 1;
    res_open = 0;
    if (st_crc32(RES_VIEW, res_want[1]) != res_want[2]) return 7;
    res_hdr_t h;
    memset(&h, 0, sizeof h);
    h.magic = RES_MAGIC; h.len = res_want[1]; h.crc = res_want[2];
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if (st_prog(RES_BASE, &h, sizeof h)) return 3;
    res_ok = (uint8_t)res_valid();
    return res_ok ? 0 : 7;
}
