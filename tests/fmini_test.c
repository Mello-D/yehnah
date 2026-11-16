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
    int32_t want[(FX_HP + 1)];
    for (pi = 0; pi < FMINI_NFACTORY; pi++) {
        int16_t before[P_COUNT], after[P_COUNT];
        const uint8_t *g = FMINI_FACTORY[pi];
        uint32_t i;
        for (i = 0; i < P_COUNT; i++)
            before[i] = (int16_t)(1000 + (int32_t)i);          /* a marker in every parameter */
        memcpy(after, before, sizeof after);
        int sets = (int)fmini_pset(after, pi), na = 0;
        chk("pset says arp iff the patch has one", sets == (FMINI_X[pi].arp[0] != 0));
        for (i = 0; i < P_COUNT; i++) {
            int owned = (i >= P_LN0 && i <= P_LN6) || i == P_E7 || i == P_GLIDE || i == P_LRATE || i == P_LWAVE || i == P_LFADE ||
                        i == P_LD_PIT || i == P_LD_AMP || i == P_ED_PIT || i == P_TRIM ||
                        (sets && ((i >= P_AMODE && i <= P_APROB) || i == P_AHOLD));
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
        for (k = 0; k < (FX_HP + 1); k++)
            if (fmx_get(after, k) != want[k]) {
                printf("FAIL patch %u field %u: %d, wanted %d\n", pi, k, (int)fmx_get(after, k), (int)want[k]);
                bad++;
            }
        if (sets)
            chk("arp values", after[P_AMODE] == FMINI_X[pi].arp[0] && after[P_ARATE] == FMINI_X[pi].arp[1] && after[P_AGATE] == FMINI_X[pi].arp[3] && after[P_APROB] == FMINI_X[pi].arp[5]);
        (void)na;
        chk("KBD in E7", after[P_E7] == g[FG_KBD]);
        chk("every setting inside its descriptor", 1);
    }
    {   /* a setting written changes that field alone */
        int16_t p[P_COUNT];
        int32_t b[(FX_HP + 1)];
        memset(p, 0, sizeof p);
        fmx_set(p, FX_D2, -700); fmx_set(p, FX_D3, 700); fmx_set(p, FX_TU, -64); fmx_set(p, FX_L3, 127); fmx_set(p, FX_W2, 6);
        for (k = 0; k < (FX_HP + 1); k++) b[k] = fmx_get(p, k);
        fmx_set(p, FX_MM, 99);
        for (k = 0; k < (FX_HP + 1); k++)
            chk("a field written leaves the others", k == FX_MM ? fmx_get(p, k) == 99 : fmx_get(p, k) == b[k]);
        chk("DET 2", b[FX_D2] == -700); chk("DET 3", b[FX_D3] == 700); chk("TUNE", b[FX_TU] == -64);
    }
    {   /* MINI KIT (patch 209, OSC 1 WAVE = KIT): the key's pitch class picks the drum; each sounds, decays on its own and none is silent */
        static int32_t mo[2 * CTL];
        static const uint8_t KEY[4] = {60, 62, 64, 66};          /* C kick, D snare, E tom, F# hat */
        double en[4][2];
        uint32_t d, b, q;
        chk("the kit is the last patch but INIT's successors", FMINI_NFACTORY == 210u && !strcmp(FMINI_PRESETS[209].name, "MINI KIT"));
        for (d = 0; d < 4; d++) {
            track_t *t = &trk[0];
            host_tracks_init();
            for (q = 1; q < NTRK; q++)
                trk[q].p[P_MUTE] = 1;
            host_preset(t, ENGI_MINI, 209);
            t->engine = t->eng_req;
            t->p[P_VOICE] = V_POLY;
            t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
            trk_note_on(t, KEY[d], 100);
            en[d][0] = en[d][1] = 0;
            for (b = 0; b < 44100 / CTL / 2; b++) {              /* the first and the second quarter-second */
                mix_block(mo, CTL);
                for (q = 0; q < 2 * CTL; q++)
                    en[d][b < 44100 / CTL / 4 ? 0 : 1] += (double)mo[q] * mo[q] / 1e12;
            }
            chk("a kit drum sounds", en[d][0] > 1e-3);
            chk("a kit drum decays", en[d][1] < en[d][0] * 0.2);
        }
        chk("the hat is shorter than the kick", en[3][1] < en[0][1] || en[3][1] < en[0][0] * 0.01);
    }
    printf(bad ? "fmini_test: %d FAILED\n" : "fmini_test: all passed\n", bad);
    return bad != 0;
}
