/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* (chord-harp fork, 2026-11-04) the resource region (firmware/src/res.c) on a simulated NOR: valid only whole and this
 * build's (a torn payload, a bad header, another build's length or CRC: res_ok 0); begin / write / commit, the header
 * last; and the sounds that read it (VECTOR, TERRAIN, WSEQ, PLAITS, OMNI) silent without it, as before with it. */
#define YEHNAH_RESREGION 1
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint8_t nor[0x100000], flash_ok = 1;
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { erases++; memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"
static int transport_busy(void) { return song.playing; }
static void fl_plain_window_init(void) {}
#define RES_VIEW (nor + RES_BASE + RES_HEAD)        /* the app reads the region in place */
#define RES_LINKED 5000u
#define RES_WANT_CONST
#include "../firmware/src/res.c"

static int check(const char *what, int ok) { printf("res: %-74s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }

static uint8_t payload[RES_LINKED];
static int install(void)                           /* as editor command 82 sends it: begin, chunks, commit */
{
    int rc = res_begin();
    for (uint32_t off = 0; !rc && off < RES_LINKED; off += 256u)
        rc = res_write(off, payload + off, RES_LINKED - off < 256u ? RES_LINKED - off : 256u);
    return rc ? rc : res_commit();
}
static double level(uint32_t eng)                  /* a note's RMS on track 0 with the engine's first preset */
{
    track_t *t = &trk[0];
    double sum = 0;
    host_tracks_init();
    for (uint32_t f = 0; f < 4u * FS; f += CTL) {  /* (the last note's tail out first) */
        int32_t o[2 * CTL];
        mix_block(o, CTL);
    }
    host_preset(t, eng, 0);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_LEVEL] = 100;
    trk_note_on(t, 60, 110);
    for (uint32_t f = 0; f < FS / 4u; f += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (uint32_t i = 0; i < CTL; i++) sum += (double)o[2 * i] * o[2 * i];
    }
    trk_note_off(t, 60);
    return sum;
}

int main(void)
{
    int bad = 0;
    for (uint32_t i = 0; i < RES_LINKED; i++) payload[i] = (uint8_t)(i * 31u + (i >> 5));
    memset(nor, 0xFF, sizeof nor);
    res_want[1] = RES_LINKED; res_want[2] = st_crc32(payload, RES_LINKED);
    res_boot();
    bad += check("blank flash: not installed", !res_ok);
    bad += check("installed through begin / write / commit", install() == 0 && res_ok && erases == RES_SECTORS &&
                 !memcmp(nor + RES_BASE + RES_HEAD, payload, RES_LINKED));
    res_boot();
    bad += check("the next boot finds it", res_ok == 1);
    nor[RES_BASE + RES_HEAD + 4000u] &= 0xFEu;     /* (a bit cleared: what a cut or a worn cell leaves) */
    res_boot();
    bad += check("a torn payload: not installed", !res_ok);
    bad += check(".. and installed again", install() == 0 && res_ok);
    nor[RES_BASE + 4u] ^= 0x01u;                   /* (the length) */
    res_boot();
    bad += check("a damaged header: not installed", !res_ok);
    (void)install();
    res_want[2] ^= 1u;
    res_boot();
    bad += check("another build's (its CRC): not installed, its commit refused", !res_ok && install() == 7 && !res_ok);
    res_want[2] ^= 1u;
    res_want[1] = RES_LINKED - 4u;
    res_boot();
    bad += check("another build's (its length): not installed", !res_ok);
    res_want[1] = RES_LINKED;
    {
        uint32_t e = erases;
        int rc = res_begin(), w;
        w = res_write(RES_LINKED - 10u, payload, 11u);
        bad += check("begin: the readers off at once; a write past the payload refused",
                     rc == 0 && !res_ok && erases == e + RES_SECTORS && w == 1);
        bad += check("a commit before the payload is whole: refused, no header (7)",
                     res_commit() == 7 && !res_ok && nor[RES_BASE] == 0xFFu);
        bad += check("a write or commit without begin: refused", res_write(0, payload, 4u) == 1 && res_commit() == 1);
    }
    song.playing = 1;
    bad += check("begin refused while the transport runs (4)", res_begin() == 4);
    song.playing = 0;
    bad += check("installed again", install() == 0 && res_ok);
    {
        static const uint32_t ENG[5] = {ENGI_VECTOR, ENGI_TERRAIN, ENGI_WSEQ, ENGI_PLAITS, ENGI_OMNI};
        static const char *const NAME[5] = {"VECTOR", "TERRAIN", "WSEQ", "PLAITS", "OMNI"};
        char what[96];
        for (uint32_t k = 0; k < 5u; k++) {
            double on, off;
            res_ok = 0; off = level(ENG[k]);
            res_ok = 1; on = level(ENG[k]);
            snprintf(what, sizeof what, "%s sounds with the region, silent without it", NAME[k]);
            bad += check(what, on > 1e6 && off == 0.0);
        }
        res_ok = 1;
    }
    printf("%s\n", bad ? "RESOURCE TEST FAILED" : "resource test passed");
    return bad != 0;
}
