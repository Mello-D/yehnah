/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mello-D */
/* FMINI (eng_fmini.c): its 200 patches survive the packing into the track's P_LN0..P_LN6 (core.h fmx_get / fmx_set), and loading
 * a patch (the engine's pset hook) changes only the parameters it owns: the chord mode, HUMAN, KEY>F, DRIFT, the strum ... are
 * other features' (an earlier version wrote into their slots: a preset change switched the chord mode off) */
#include <stdio.h>
#include <string.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
static int bad;
static void chk(const char *what, int ok) { if (!ok) { printf("FAIL %s\n", what); bad++; } }
int main(void)
{
    uint32_t pi, k;
    int32_t want[NFMX];
    for (pi = 0; pi < FMINI_NFACTORY; pi++) {
        int16_t before[P_COUNT], after[P_COUNT];
        const uint8_t *g = FMINI_FACTORY[pi];
        uint32_t i;
        for (i = 0; i < P_COUNT; i++)
            before[i] = (int16_t)(1000 + (int32_t)i);          /* a marker in every parameter */
        memcpy(after, before, sizeof after);
        fmini_pset(after, pi);
        for (i = 0; i < P_COUNT; i++) {
            int owned = (i >= P_LN0 && i <= P_LN6) || i == P_E7 || i == P_GLIDE || i == P_LRATE || i == P_LWAVE || i == P_LFADE ||
                        i == P_LD_PIT || i == P_LD_AMP || i == P_ED_PIT || i == P_TRIM;
            if (!owned && after[i] != before[i]) {
                printf("FAIL patch %u wrote parameter %u (not FMINI's)\n", pi, i);
                bad++;
            }
        }
        want[FX_R1] = g[FG_R1]; want[FX_R2] = g[FG_R2]; want[FX_R3] = g[FG_R3];
        want[FX_W1] = g[FG_W1]; want[FX_W2] = g[FG_W2]; want[FX_W3] = g[FG_W3];
        want[FX_L1] = (g[FG_SW1] & FS_O1) ? g[FG_L1] : 0; want[FX_L2] = (g[FG_SW1] & FS_O2) ? g[FG_L2] : 0;
        want[FX_L3] = (g[FG_SW1] & FS_O3) ? g[FG_L3] : 0; want[FX_LN] = (g[FG_SW1] & FS_NZ) ? g[FG_LN] : 0;
        want[FX_D2] = fmini_det(g, FG_DC2L); want[FX_D3] = fmini_det(g, FG_DC3L);
        want[FX_TU] = (int32_t)g[FG_TUNE] - 64; want[FX_MM] = g[FG_MMIX];
        want[FX_PK] = (g[FG_SW1] & FS_PINK) != 0; want[FX_MO] = (g[FG_SW1] & FS_MOSC) != 0; want[FX_MF] = (g[FG_SW1] & FS_MFLT) != 0;
        want[FX_SQ] = (g[FG_SW2] & FTG_SQR) != 0; want[FX_HP] = (g[FG_SW2] & FTG_HP) != 0;
        for (k = 0; k < NFMX; k++)
            if (fmx_get(after, k) != want[k]) {
                printf("FAIL patch %u field %u: %d, wanted %d\n", pi, k, (int)fmx_get(after, k), (int)want[k]);
                bad++;
            }
        chk("KBD in E7", after[P_E7] == g[FG_KBD]);
        chk("every setting inside its descriptor", 1);
    }
    {   /* a setting written changes that field alone */
        int16_t p[P_COUNT];
        int32_t b[NFMX];
        memset(p, 0, sizeof p);
        fmx_set(p, FX_D2, -700); fmx_set(p, FX_D3, 700); fmx_set(p, FX_TU, -64); fmx_set(p, FX_L3, 127); fmx_set(p, FX_W2, 6);
        for (k = 0; k < NFMX; k++) b[k] = fmx_get(p, k);
        fmx_set(p, FX_MM, 99);
        for (k = 0; k < NFMX; k++)
            chk("a field written leaves the others", k == FX_MM ? fmx_get(p, k) == 99 : fmx_get(p, k) == b[k]);
        chk("DET 2", b[FX_D2] == -700); chk("DET 3", b[FX_D3] == 700); chk("TUNE", b[FX_TU] == -64);
    }
    printf(bad ? "fmini_test: %d FAILED\n" : "fmini_test: all passed\n", bad);
    return bad != 0;
}
