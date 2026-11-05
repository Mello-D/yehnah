/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader, the portable part (step 2 of the M-UPGRADE update
 * protocol).
 *
 * The host serves the package ("logical image") with cmd 0x30 reads (ota.c).
 * The loader:
 *   1. reads the UFW header and finds flash.bin;
 *   2. checks that the package's app area decrypts with THIS chip's key
 *      (taken from isd_config.ini in the device's own flash head), so a
 *      package for another key is refused before anything is erased;
 *   2b. reads the whole flash.bin once and checks it against the CRC16 of its
 *      UFW entry (the vendor tools' check), keeping each app sector's CRC16:
 *      a damaged or truncated package is refused before anything is erased;
 *   3. writes ONLY the app area [0x4000, 0x93000), sector by sector, skipping
 *      sectors that are already equal, and verifies each one; a sector that
 *      comes in different from the checked pass is read again, never written;
 *      the flash head (SPL, isd_config) is never written;
 *   3b. (chord-harp fork) the resource region from the package's res.bin
 *      (ldr_res), unless it already holds this build's;
 *   4. asks 0xF0000000 ("success"), then invalidates the update record (RAM
 *      and flash: 4K boundary - 256 between 0x93000 and 0xFC000, outside
 *      Felucca's store, where only user data lives) so the SPL boots the new
 *      app, and resets.
 * Power loss during 3: the record is still there, the SPL runs the loader
 * again on the next power-on and the host can resume.
 *
 * Hooks from the platform (loader.c, ldr_test.c):
 *   ldr_fread(off, p, n)  ldr_erase(off)  ldr_prog(off, p, n)   flash, 0 = ok
 *   ldr_record_clear()    forget the RAM update record
 *   ldr_progress(done, total)
 * plus the ota.c hooks (frames, time, idle). */
#define LDR_APP_LO 0x4000u
#define LDR_APP_HI 0x93000u
#define LDR_REC_LO 0x93000u                     /* update records live above the app ... */
#define LDR_REC_HI 0xFC000u                     /* ... and below Felucca's globals (Felucca's records: 0xE4F00, */
#define LDR_DATA_LO 0x97000u                    /* the stock firmware's: 0xE8F00), never in Felucca's store, whose */
#define LDR_DATA_HI 0xE0000u                    /* bytes (the store) the host writes */
#define LDR_NSEC ((LDR_APP_HI - LDR_APP_LO) >> 12)
#define LDR_RES_TYPE 101u                       /* (chord-harp fork) res.bin, the resource region (src/res.c): */
#define LDR_RES_LO 0xCA000u                     /* inside the store's range, so the record sweep below skips it */
#define LDR_RES_HI 0xDC000u
#define LDR_RES_MAGIC 0x3153524Du               /* MRS1 */
#define LDR_FL_MAX 0x100000u                    /* flash.bin: at most the 1 MiB part */

static int ldr_fread(uint32_t off, void *p, uint32_t n);
static int ldr_erase(uint32_t off);
static int ldr_prog(uint32_t off, const void *p, uint32_t n);
static void ldr_record_clear(void);
static void ldr_progress(uint32_t done, uint32_t total);

/* SFC cipher: per 32-byte block, key ^ (block offset >> 2) */
static void ldr_sfc(uint8_t *p, uint32_t n, uint32_t base_off, uint32_t key)
{
    uint32_t i, j;
    for (i = 0; i < n; i += 32u) {
        uint32_t k = key ^ ((base_off + i) >> 2);
        for (j = 0; j < 32u && i + j < n; j++) {
            p[i + j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
    }
}

/* chip key from the device's flash head: JLFS top entries (ENC 0xFFFF) -> isd_config.ini blob */
static int ldr_chip_key(uint32_t *key)
{
    uint8_t e[32], blob[34];
    uint32_t off, i, sum, k;
    for (off = 32; off < 0x400u; off += 32u) {
        if (ldr_fread(off, e, 32))
            return -1;
        ota_jl_enc(e, 32);
        if (ota_crc16(e + 2, 30, 0) != ota_rd16(e))
            return -2;
        if (!ota_memeq(e + 16, (const uint8_t *)"isd_config.ini", 15)) {
            if (ota_rd16(e + 14))
                return -3;                               /* last entry, not found */
            continue;
        }
        if (ldr_fread(ota_rd32(e + 4), blob, 34) || ota_crc16(blob, 32, 0) != ota_rd16(blob + 32))
            return -4;
        for (i = 0, sum = 0; i < 16u; i++)
            sum += blob[i];
        sum &= 0xFFu;
        sum = sum >= 0xE0u ? 0xAAu : sum <= 0x10u ? 0x55u : sum;
        for (i = 0, k = 0; i < 16u; i++)
            if ((uint32_t)(blob[16 + i] ^ blob[15 - i]) < sum)
                k |= 1u << i;
        *key = k;
        return 0;
    }
    return -3;
}

/* 2b. flash.bin [0, fl_size) from the host, before any erase: its CRC16 must be the UFW entry's (want); sc gets
 * the CRC16 of each app sector as it came in */
static int ldr_check(uint32_t fl_off, uint32_t fl_size, uint32_t want, uint16_t *sc, uint8_t *b)
{
    uint32_t i, n, crc = 0;
    for (i = 0; i < fl_size; i += n) {
        n = fl_size - i > 512u ? 512u : fl_size - i;
        if (ota_read(fl_off + i, b, n))
            return -7;
        crc = ota_crc16(b, n, crc);
        if (i >= LDR_APP_LO && i < LDR_APP_HI) {     /* (512-byte reads from 0: never across a sector) */
            uint32_t k = (i - LDR_APP_LO) >> 12;
            sc[k] = (uint16_t)ota_crc16(b, n, (i & 0xFFFu) ? sc[k] : 0u);
        }
    }
    return crc == want ? 0 : -12;
}

/* one sector as sec holds it: left when equal, else erased, written and read back (twice at most) */
static int ldr_put(uint32_t s, const uint8_t *sec, uint8_t *cur)
{
    uint32_t tries, k;
    if (ldr_fread(s, cur, 0x1000u))
        return -8;
    for (tries = 0; !ota_memeq(sec, cur, 0x1000u); tries++) {
        if (tries == 2u)
            return -11;                                  /* verify failed twice */
        if (ldr_erase(s))
            return -9;
        for (k = 0; k < 0x1000u; k += 256u)
            if (ldr_prog(s + k, sec + k, 256))
                return -10;
        if (ldr_fread(s, cur, 0x1000u))
            return -8;
    }
    return 0;
}

/* (chord-harp fork, 2026-11-04, after Melodee's ldr_res) 3b. the resource region (firmware/src/res.c) from the
 * package's res.bin (type 101: its commit record, the payload, padded to sectors). Left as it is when it already holds
 * this build's region whole (a code-only update reads nothing more). Otherwise the commit record's sector goes blank
 * first and its first page is written last: a valid record only ever stands over a whole region. A failure here
 * costs no boot: the app falls back (res_ok 0) and asks for another install */
static uint32_t ldr_crc32(uint32_t c, const uint8_t *p, uint32_t n)   /* running, from 0xFFFFFFFF (zlib's) */
{
    uint32_t j;
    while (n--)
        for (c ^= *p++, j = 0; j < 8u; j++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    return c;
}
static int ldr_res_head(const uint8_t *h)
{
    return ota_rd32(h) == LDR_RES_MAGIC && ota_rd32(h + 28) == ~ldr_crc32(0xFFFFFFFFu, h, 28);
}
static int ldr_res_sector(uint32_t off, uint32_t s, uint8_t *sec)     /* the package's copy of region sector s */
{
    uint32_t k;
    for (k = 0; k < 0x1000u; k += 512u)
        if (ota_read(off + (s - LDR_RES_LO) + k, sec + k, 512))
            return -7;
    return 0;
}
static void ldr_res(const uint8_t *hdr, uint8_t *sec, uint8_t *cur)
{
    uint32_t i, s, k, c, len, off = 0, size = 0;
    for (i = 0; i < ota_rd16(hdr + 8); i++)
        if (ota_rd16(hdr + 0x40 + i * 0x50u) == LDR_RES_TYPE) {
            off = ota_rd32(hdr + 0x40 + i * 0x50u + 8u);
            size = ota_rd32(hdr + 0x40 + i * 0x50u + 12u);
        }
    if (!off || !size || (size & 0xFFFu) || size > LDR_RES_HI - LDR_RES_LO || ota_read(off, sec, 512) ||
        !ldr_res_head(sec) || (len = ota_rd32(sec + 4)) > size - 32u || ldr_fread(LDR_RES_LO, cur, 32))
        return;
    if (ota_memeq(sec, cur, 32)) {
        for (s = 0, c = 0xFFFFFFFFu; s < len; s += k) {
            k = len - s > 0x1000u ? 0x1000u : len - s;
            if (ldr_fread(LDR_RES_LO + 32u + s, cur, k))
                return;
            c = ldr_crc32(c, cur, k);
        }
        if (~c == ota_rd32(sec + 8))
            return;                                      /* this build's, whole */
    }
    if (ldr_erase(LDR_RES_LO))
        return;
    for (s = LDR_RES_LO + 0x1000u; s < LDR_RES_LO + size; s += 0x1000u) {
        if (ldr_res_sector(off, s, sec) || ldr_put(s, sec, cur))
            return;
        ldr_progress(LDR_APP_HI - LDR_APP_LO, LDR_APP_HI - LDR_APP_LO);
    }
    if (ldr_res_sector(off, LDR_RES_LO, sec))
        return;
    for (k = 0x100u; k < 0x1000u; k += 0x100u)
        if (ldr_prog(LDR_RES_LO + k, sec + k, 256))
            return;
    if (ldr_fread(LDR_RES_LO + 0x100u, cur, 0xF00u) || !ota_memeq(sec + 0x100u, cur, 0xF00u))
        return;
    (void)ldr_prog(LDR_RES_LO, sec, 256);                /* the commit record */
}

static int ldr_session(void)
{
    static uint8_t hdr[0x400], sec[0x1000], cur[0x1000];
    static uint16_t sc[LDR_NSEC];
    uint32_t i, k, fl_off, fl_size, ota_off, ota_len, key, s, want = ~0u, done = 0;   /* (no CRC16: no flash.bin entry) */
    int rc;
    /* 1. UFW header + entry list (ota.c; it leaves the entries decoded in hdr) */
    if ((rc = ota_ufw(hdr, &fl_off, &fl_size, &ota_off, &ota_len)) != 0)
        return rc;
    if (!fl_off || fl_size < LDR_APP_HI || fl_size > LDR_FL_MAX)
        return -3;
    for (i = 0; i < ota_rd16(hdr + 8); i++)
        if (ota_rd16(hdr + 0x40 + i * 0x50u) == 0)
            want = ota_rd16(hdr + 0x40 + i * 0x50u + 4u);   /* flash.bin's data CRC16 */
    /* 2. the package's app area must decrypt with this chip's key */
    if ((rc = ldr_chip_key(&key)) != 0)
        return -40 + rc;
    if (ota_read(fl_off + LDR_APP_LO, sec, 32))
        return -5;
    ldr_sfc(sec, 32, 0, key);
    if (ota_crc16(sec + 2, 30, 0) != ota_rd16(sec))
        return -6;                                       /* package for another chip key */
    /* 2b. the whole package first: nothing is erased for a damaged one */
    if ((rc = ldr_check(fl_off, fl_size, want, sc, sec)) != 0)
        return rc;
    /* 3. app area, sector by sector */
    for (s = LDR_APP_LO; s < LDR_APP_HI; s += 0x1000u) {
        uint32_t again;
        for (again = 0;; again++) {                      /* as checked in 2b, else read it again */
            for (k = 0; k < 0x1000u; k += 512u)
                if (ota_read(fl_off + s + k, sec + k, 512))
                    return -7;
            if (ota_crc16(sec, 0x1000u, 0) == sc[(s - LDR_APP_LO) >> 12])
                break;
            if (again == 2u)
                return -13;
        }
        if ((rc = ldr_put(s, sec, cur)) != 0)
            return rc;
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO);
    }
    ldr_res(hdr, sec, cur);
    /* 4. finish: the host confirms, the record goes, the SPL boots the app */
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xF0000000u, sec, 8))
            break;
    ldr_record_clear();
    for (s = LDR_REC_HI; s > LDR_REC_LO; s -= 0x1000u) {   /* flash records: 4K boundary - 256 */
        uint8_t r[80];
        if (s - 0x1000u >= LDR_DATA_LO && s - 0x1000u < LDR_DATA_HI)
            continue;                                    /* Felucca's store: user data that may look like one */
        if (ldr_fread(s - 0x100u, r, 80))
            break;
        if (ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0))
            ldr_erase(s - 0x1000u);
    }
    return 0;
}
