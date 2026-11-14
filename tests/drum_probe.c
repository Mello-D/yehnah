/* SPDX-License-Identifier: GPL-3.0-only
 * Host CPU / RAM probe of DRUM's MACHINEs (DRUM-X, LCD-4's, HORT's): the same eight lanes struck
 * every 0.25 s, ns per sample. Relative comparisons only (the host is not the FM-1). Usage: drum_probe MACHINE (DXK_*) */
#define main hostsim_main
#include "hostsim.c"
#undef main
static volatile uint32_t sink;
int main(int argc, char **argv)
{
    static const uint8_t NOTES[8] = {36, 38, 39, 42, 46, 45, 37, 49};
    uint32_t m = argc > 1 ? strtoul(argv[1], 0, 0) : 0, blocks = 44100u * 10u / CTL, b, k, peak = 0;
    int32_t out[CTL];
    uint64_t start, elapsed;
    host_tracks_init(); host_preset(&trk[0], ENGI_DRUM, 0);
    trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SUS] = 127;
    dx_mach = 0; dx_mach_req = (uint8_t)m; drum_mach_block(CTL);
    for (b = 0; b < 256; b++) track_render(&trk[0], out, CTL);
    start = now_ns();
    for (b = 0; b < blocks; b++) {
        if (!(b % 344u)) for (k = 0; k < 8; k++) trk_note_on(&trk[0], NOTES[k], 100);
        track_render(&trk[0], out, CTL);
        for (k = 0; k < CTL; k++) { sink ^= (uint32_t)out[k]; if ((uint32_t)(out[k] < 0 ? -out[k] : out[k]) > peak) peak = (uint32_t)(out[k] < 0 ? -out[k] : out[k]); }
    }
    elapsed = now_ns() - start;
    printf("machine=%u ns_per_sample=%.1f peak=%u checksum=%u\n", m, (double)elapsed / (blocks * CTL), peak, sink);
#if YEHNAH_HORT
    printf("sizeof HORT: dvoice_t=%zu ht_inst=%zu x%u hmt_t=%zu\n", sizeof(dvoice_t), sizeof(ht_inst_t), HT_NINST, sizeof(hmt_t));
#endif
    printf("sizeof: ld_voice_t=%zu drum_lane_t=%zu drum_kit=%zu\n", sizeof(ld_voice_t), sizeof(drum_lane_t), sizeof drum_kit);
    return 0;
}
