/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Copyright (C) 2026 Mello-D
 * (chord-harp fork, 2026-11-04) after Melodee's editor_res.c, the same command and replies */
/* The resource region (res.c). 82: [0] status; [1] begin (the transport stopped, the region erased); [2, offset in
 * 3 x 7 bits, packed bytes (at most 256)] write; [3] commit (checked, then the header).
 * Reply: rc (0 ok, 1 bad request, 2 no flash, 3 flash error, 4 busy, 7 check failed), installed, then this build's
 * payload length and CRC-32 (5 x 7 bits each). */
#define ED_RES 82u
static int ed_res_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd != ED_RES) return 0;
    uint32_t rc = 1;
    if (n == 1u && a[0] == 0u) rc = 0;
    else if (n == 1u && a[0] == 1u) rc = ed_flash_stop() ? 4u : (uint32_t)res_begin();
    else if (n > 4u && a[0] == 2u) {
        uint32_t off = a[1] | (uint32_t)a[2] << 7 | (uint32_t)a[3] << 14;
        uint32_t k = ed_unpack7(a + 4, n - 4u, ed_bk_buf, sizeof ed_bk_buf);
        rc = k ? (uint32_t)res_write(off, ed_bk_buf, k) : 1u;
    } else if (n == 1u && a[0] == 3u) rc = (uint32_t)res_commit();
    ed_b(rc); ed_b(res_ok);
    ed_bk_u32(res_want[1]);
    ed_bk_u32(res_want[2]);
    return 1;
}
