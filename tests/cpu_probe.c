/* SPDX-License-Identifier: GPL-3.0-only
 * Repeatable host CPU/RAM probe of the actual 0.9.9 DSP. Host timings are
 * relative comparisons, never FM1 utilization or deadline measurements.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
static volatile uint32_t sink;
int main(int argc, char **argv)
{
    uint32_t e = argc > 1 ? strtoul(argv[1], 0, 0) : ENGI_FM6;
    uint32_t preset = argc > 2 ? strtoul(argv[2], 0, 0) : 0;
    uint32_t voices = argc > 3 ? strtoul(argv[3], 0, 0) : 1;
    uint32_t blocks = 44100u * 10u / CTL, b, k;
    int32_t out[CTL];
    uint64_t start, elapsed;
    if (e >= NENGINES || !voices || voices > NVOICE) return 2;
    host_tracks_init(); host_preset(&trk[0], e, preset);
    trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SUS] = 127;
    for (k = 0; k < voices; k++) trk_note_on(&trk[0], 48 + k * 3, 100);
    for (b = 0; b < 1024; b++) track_render(&trk[0], out, CTL);
    start = now_ns();
    for (b = 0; b < blocks; b++) {
        /* Retrigger so release/word presets cannot finish and benchmark silence. */
        if (!(b & 255u)) for (k = 0; k < voices; k++) trk_note_on(&trk[0], 48 + k * 3, 100);
        track_render(&trk[0], out, CTL); sink ^= (uint32_t)out[b % CTL];
    }
    elapsed = now_ns() - start;
    for (k = b = 0; k < NVOICE; k++) b += trk[0].v[k].active;
    printf("engine=%u preset=%u requested=%u active=%u ns_per_sample=%.3f checksum=%u\n",
           e, preset, voices, b, (double)elapsed / (blocks * CTL), sink);
    printf("RAM host sizeof: tracks=%zu fm6_notes=%zu fm6_effective=%zu fm6_bus=%zu comb=%zu chorus=%zu reverb_comb=%zu\n",
           sizeof trk, sizeof fm6_note, sizeof fm6_eff, sizeof fm6_bus_c, sizeof comb_buf, sizeof cho_buf, sizeof rev_comb);
    return 0;
}
