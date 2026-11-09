#define main hostsim_main
#include "hostsim.c"
#undef main
#include <unistd.h>
/* meas osc E PRESET NOTE  -> raw int32 mono, 1 s after 0.5 s settle (44.1k)
   meas flt MODEL CUT RES -> raw int32, noise through fmod, 65536 samples
   meas np E -> prints number of presets */
static int32_t ob[CTL];
#include <string.h>
int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    if (!strcmp(argv[1], "np")) { uint32_t e = atoi(argv[2]); printf("%u\n", (unsigned)ENGINES[e]->npresets); return 0; }
    if (!strcmp(argv[1], "trimdb")) {   /* trimdb E P : the mixer gain of this preset (LEVEL + TRIM against LEVEL), dB, and P_LEVEL */
        uint32_t e = atoi(argv[2]), p = atoi(argv[3]); int lv, tr;
        host_tracks_init(); host_preset(&trk[0], e, p);
        lv = trk[0].p[P_LEVEL]; tr = trk[0].p[P_TRIM];
        printf("%d %d %.3f\n", lv, tr, lv ? 20.0 * log10((double)LEVEL_Q12[clamp(lv + tr, 1, 127)] / LEVEL_Q12[lv]) : 0.0);
        return 0;
    }
    if (!strcmp(argv[1], "mix")) {   /* mix E P NOTE : through mix_block, dry; prints peak dBFS, loudest-250ms rms dBFS, limiter min gain */
        static int32_t mo[2 * CTL]; uint32_t e = atoi(argv[2]), p = atoi(argv[3]), n = atoi(argv[4]), b, k, w = 11025 / CTL;
        double pk = 0, bestr = 0, acc[64]; double gmin = 1; int nacc = 0; static double blk[400];
        track_t *t = &trk[0];
        host_tracks_init(); for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
        host_preset(t, e, p); t->engine = t->eng_req; t->p[P_VOICE] = V_POLY; t->p[P_SUS] = 127;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        lim_reset(&lim_dac); trk_note_on(t, n, 100);
        for (b = 0; b < 44100 / CTL; b++) {
            double s2 = 0; mix_block(mo, CTL);
            for (k = 0; k < 2 * CTL; k++) { double v = mo[k]; if (v < 0) v = -v; if (v > pk) pk = v; s2 += v * v; }
            blk[b] = s2 / (2 * CTL); { double g = lim_dac.g / 32768.0; if (b > 2 && g < gmin) gmin = g; }
        }
        for (b = 0; b + w < 44100 / CTL; b++) { double a = 0; for (k = 0; k < w; k++) a += blk[b + k]; a /= w; if (a > bestr) bestr = a; }
        printf("%.2f %.2f %.3f\n", 20 * log10(pk / 32768 + 1e-9), 10 * log10(bestr / (32768.0 * 32768.0) + 1e-18), gmin);
        return 0;
    }
    if (!strcmp(argv[1], "osc")) {
        uint32_t e = atoi(argv[2]), p = atoi(argv[3]), n = atoi(argv[4]), b, tot = 44100 / CTL * 1 + 1, set = 44100 * 1 / (2 * CTL);
        host_tracks_init(); host_preset(&trk[0], e, p);
        trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SUS] = 127; trk[0].p[P_GLIDE] = 0; trk[0].p[P_ED_PIT] = 0; trk[0].p[P_ED_FLT] = 0; trk[0].p[P_ED_SHP] = 0; trk[0].p[P_LD_PIT] = 0; trk[0].p[P_LD_FLT] = 0; trk[0].p[P_LD_SHP] = 0; trk[0].p[P_LD_AMP] = 0; trk[0].p[P_DETUNE] = 0; trk[0].p[P_TRANS] = 0;
        trk_note_on(&trk[0], n, 100);
        for (b = 0; b < set; b++) track_render(&trk[0], ob, CTL);
        for (b = 0; b < 44100 / CTL; b++) { track_render(&trk[0], ob, CTL); fwrite(ob, 4, CTL, stdout); }
        return 0;
    }
    if (!strcmp(argv[1], "raw")) {   /* raw E NOTE k=v ... : preset 0, then params (P_E0..P_E7 as e0..e7, or numeric index) */
        uint32_t e = atoi(argv[2]), n = atoi(argv[3]), b, set = 44100 / (2 * CTL); int a;
        host_tracks_init(); host_preset(&trk[0], e, 0);
        trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SUS] = 127; trk[0].p[P_GLIDE] = 0; trk[0].p[P_ED_PIT] = 0; trk[0].p[P_ED_FLT] = 0; trk[0].p[P_ED_SHP] = 0; trk[0].p[P_LD_PIT] = 0; trk[0].p[P_LD_FLT] = 0; trk[0].p[P_LD_SHP] = 0; trk[0].p[P_LD_AMP] = 0; trk[0].p[P_DETUNE] = 0; trk[0].p[P_TRANS] = 0;
        for (a = 4; a < argc; a++) { int idx, val; if (sscanf(argv[a], "e%d=%d", &idx, &val) == 2) trk[0].p[P_E0 + idx] = val; else if (sscanf(argv[a], "%d=%d", &idx, &val) == 2) trk[0].p[idx] = val; }
        trk_note_on(&trk[0], n, 100);
        for (b = 0; b < set; b++) track_render(&trk[0], ob, CTL);
        for (b = 0; b < 44100 / CTL; b++) { track_render(&trk[0], ob, CTL); fwrite(ob, 4, CTL, stdout); }
        return 0;
    }
    if (!strcmp(argv[1], "flt")) {
        int model = atoi(argv[2]), cut = atoi(argv[3]), res = atoi(argv[4]), i;
        fmod_t fm; int32_t sv[2] = {0, 0}, ls[4] = {0, 0, 0, 0}; uint32_t r = 12345;
        fmod_coef(&fm, model, cut << 8, res);
        for (i = 0; i < 65536 + 4096; i++) {
            int32_t x, y;
            r = r * 1664525u + 1013904223u; x = ((int32_t)(r >> 16) - 32768) / 8;   /* +-4096 of +-131072: linear */
            y = fmod_tick(&fm, x, sv, ls);
            if (i >= 4096) fwrite(&y, 4, 1, stdout);
        }
        return 0;
    }
    return 2;
}
