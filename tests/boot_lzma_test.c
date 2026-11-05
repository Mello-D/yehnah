/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Charles Vestal */
/* (chord-harp fork, 2026-11-04) after Melodee's tests/boot_lzma_test.c: boot_lzma.h against this build's own packed
 * RAM load images (tools/boot_lzma.py, main.c boot_image): each record after .text unpacked and compared with the
 * section the linker made, then the bounds and malformed streams on the first one.
 *   boot_lzma_test build/yehnah.bin build/text.bin build/ramtext.bin build/c1text.bin build/hottext.bin build/data.bin */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../firmware/src/boot_lzma.h"

static uint8_t *slurp(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *p;
    if (!f) { *n = 0; return calloc(1, 1); }       /* an empty section has no file */
    assert(!fseek(f, 0, SEEK_END)); *n = ftell(f); rewind(f);
    p = malloc((size_t)*n + 1u); assert(p);
    assert(fread(p, 1, (size_t)*n, f) == (size_t)*n); fclose(f);
    return p;
}
static uint32_t word(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int main(int argc, char **argv)
{
    long n, tn;
    uint8_t *img = slurp(argv[1], &n);
    uint16_t *probs = malloc(BOOT_LZMA_PROBS * sizeof *probs);
    uint32_t at, first = 0, first_n = 0, first_size = 0;
    assert(argc == 7 && probs);
    free(slurp(argv[2], &tn));
    at = ((uint32_t)tn + 3u) & ~3u;
    while (at + 16u <= (uint32_t)n && word(img + at) != BOOT_LZMA_MAGIC)
        at += 4u;                                  /* (the 0xFF fill up to _rt_load) */
    for (int i = 3; i < 7; i++) {
        long rn;
        uint8_t *raw = slurp(argv[i], &rn), *out = malloc((size_t)rn + 16u);
        uint32_t packed;
        assert(out && at + 16u <= (uint32_t)n && word(img + at) == BOOT_LZMA_MAGIC);
        packed = word(img + at + 4u);
        assert(word(img + at + 8u) == (uint32_t)rn && word(img + at + 12u) == 0u && at + 16u + packed <= (uint32_t)n);
        memset(out, 0x5a, (size_t)rn + 16u);
        assert(boot_lzma(out, (uint32_t)rn, img + at + 16u, packed, probs));
        assert(!memcmp(out, raw, (size_t)rn));
        for (long k = rn; k < rn + 16; k++)
            assert(out[k] == 0x5a);
        printf("  %s: %ld B from %u packed, as linked\n", argv[i], rn, packed);
        if (rn && !first) { first = at + 16u; first_n = packed; first_size = (uint32_t)rn; }
        at += 16u + ((packed + 3u) & ~3u);
        free(raw); free(out);
    }
    {   /* Melodee's bounds: cut short, too little room, too much asked, a byte after the end; then fuzzed streams
         * never write outside the output */
        uint8_t *in = malloc(first_n + 1u), *out = malloc(first_size + 16u), fuzz[512], bounded[4096 + 32];
        uint32_t seed = 90125u;
        assert(in && out && first_n);
        memcpy(in, img + first, first_n);
        assert(!boot_lzma(out, first_size, in, first_n - 1u, probs));
        assert(!boot_lzma(out, first_size - 1u, in, first_n, probs));
        assert(!boot_lzma(out, first_size + 1u, in, first_n, probs));
        in[first_n] = 0; assert(!boot_lzma(out, first_size, in, first_n + 1u, probs));
        for (uint32_t trial = 0; trial < 10000u; trial++) {
            uint32_t len, cap;
            seed = seed * 1664525u + 1013904223u; len = seed % sizeof fuzz;
            seed = seed * 1664525u + 1013904223u; cap = seed % 4097u;
            for (uint32_t i = 0; i < len; i++) {
                seed = seed * 1664525u + 1013904223u;
                fuzz[i] = (trial & 1u) && i < first_n ? in[i] : (uint8_t)(seed >> 24);
            }
            if (len) { seed = seed * 1664525u + 1013904223u; fuzz[seed % len] ^= (uint8_t)(seed >> 16); }
            memset(bounded, 0xa7, sizeof bounded);
            (void)boot_lzma(bounded + 16u, cap, fuzz, len, probs);
            for (uint32_t i = 0; i < 16u; i++) assert(bounded[i] == 0xa7);
            for (uint32_t i = 16u + cap; i < sizeof bounded; i++) assert(bounded[i] == 0xa7);
        }
        free(in); free(out);
    }
    puts("boot images: all four unpack as linked; bounds and 10000 malformed streams ok");
    free(img); free(probs);
    return 0;
}
