/* SPDX-License-Identifier: GPL-3.0-only */
/* (0.6.83) CPU cost on any host with valgrind: the instruction counts of the audio path, counted by callgrind
 * (the same on every run: exact, unlike the time), section by section, for tests/cpu_vg.py to compare with
 * tests/cpu_baseline_vg.txt. The proc_pid_rusage counts of regress / reverb_test / mod_test exist on the Mac only;
 * this is the alarm everywhere else (Linux, CI).
 *   valgrind --tool=callgrind --instr-atstart=no build/host/cpu_vg    (tests/cpu_vg.py runs it)
 * Each section: the state set, a warm-up, then CALLGRIND_ZERO_STATS, NB blocks of mix_block (the whole audio block:
 * the sequencer, the parts, the buses, the master) and CALLGRIND_DUMP_STATS_AT(its name). Sections:
 *   eng/<ENGINE>   one part, 8 notes held (VOICE: 4), the engine's heaviest preset (hostsim.c's list)
 *   eng/ANIMAL.<MODE>  (0.8.3) ANIMAL's five sources at their heaviest: 8 notes, no gaps (GROUP HOLD), every option on
 *   rev/<TYPE>     a pad soaked in the reverb, each REVERB TYPE (ROOM SPRING SHIMR RESO CLOUD)
 *   dly/<RATE>     (0.8.4) a pad into the delay, its line at the full rate, half, a quarter (fx.c dly_lo)
 *   mix/full       six parts: four ANALOG / FM6 chords, drums, every FX on
 *   seq/plain      the sequencer: six tracks of chords / hits on every step at 200 BPM
 *   seq/heavy      .. with two step components on each (ARP SEQ / ECHO x8, RAND / OCT), DIV, LEN, chord glides
 *   song/cputest   SAVE > TOOLS 6 CPU TEST's song (song_dice.c cpu_test_song), playing: what the device is measured on
 *   song/cputest.djf  (0.9.1) .. with the master DJ FILTER on (GLO > MIXER KNOB 4 at -24, a low-pass): its cost on top */
#define UI_TEST_NO_MAIN
#include "ui_test.c"
#include <valgrind/callgrind.h>

#define NB 256u                                     /* blocks a section (8192 samples) */
static int32_t vo[2 * CTL];

static uint32_t vg_preset(uint32_t e, const char *n)
{
    uint32_t k;
    for (k = 0; k < ENGINES[e]->npresets; k++)
        if (str_eq(ENGINES[e]->presets[k].name, n))
            return k;
    return 0;
}
static void vg_section(const char *name)
{
    uint32_t b;
    for (b = 0; b < 64u; b++)
        mix_block(vo, CTL);                         /* (warm: the attacks, the glides, the FX lines filling) */
    CALLGRIND_ZERO_STATS;
    CALLGRIND_START_INSTRUMENTATION;
    for (b = 0; b < NB; b++)
        mix_block(vo, CTL);
    CALLGRIND_DUMP_STATS_AT(name);
    CALLGRIND_STOP_INSTRUMENTATION;
}
static void vg_quiet(void)                          /* every part muted but what the section plays */
{
    uint32_t k;
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        trk[k].p[P_MUTE] = 1;
}
static void vg_part(uint32_t k, uint32_t e, uint32_t pr, uint32_t notes)
{
    static const uint8_t N[8] = {48, 52, 55, 59, 62, 65, 69, 72};
    track_t *t = &trk[k];
    uint32_t i;
    set_engine_of(t, e);
    t->engine = t->eng_req;
    apply_preset_to(t, pr);
    t->p[P_MUTE] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_SUS] = 127;
    for (i = 0; i < 512u && (i < 64u || t->xf_on || t->eng_req != t->engine); i++)
        mix_block(vo, CTL);                         /* (the sound load and the engine switch over first: they let held notes go) */
    for (i = 0; i < notes && i < 8u; i++)
        trk_note_on(t, e == ENGI_DRUM ? DRUM_LANE_NOTE[i] : N[i], 100);
}
static void vg_seq(int heavy)
{
    uint32_t k, i;
    ui_power_on();
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        if (k == 5u) set_engine_of(t, ENGI_DRUM);
        else { set_engine_of(t, k == 1u ? ENGI_FM6 : ENGI_ANALOG); apply_preset_to(t, 0); }
        t->engine = t->eng_req;
        t->p[P_VOICE] = V_POLY; t->p[P_SLEN] = 64; t->p[P_GLIDE] = heavy ? 100 : 0; t->p[P_SGATE] = 120;
        t->p[P_MUTE] = 0;
        for (i = 0; i < 64u; i++) {
            step_t *s = &t->step[i];
            memset(s, 0, sizeof *s);
            s->time = ST_NOTE; s->vel = 100;
            if (k == 5u) s->hit = 0xFF;
            else {
                s->n = 4; s->note[0] = (uint8_t)(48 + i % 12); s->note[1] = (uint8_t)(52 + i % 12);
                s->note[2] = (uint8_t)(55 + i % 12); s->note[3] = (uint8_t)(59 + i % 12);
            }
            if (heavy) {
                s->comp = i % 3u == 0u ? CP_ARPQ : CP_ECHO;
                s->carg = s->comp == CP_ARPQ ? CP_ARG(3u, 4u, 3u) : 6u;
                s->comp2 = i % 2u ? CP_RAND : CP_OCT; s->carg2 = i % 2u ? 7u : 5u;
                s->ctime = 5u | (6u << 3);
            }
        }
    }
    song.g[G_BPM] = 200;
    transport_req = 1; ui_input();
}
int main(void)
{
    static const struct { const char *eng, *pre; uint32_t notes; } E[] = {
        {"ANALOG", "FAT BASS", 8}, {"PHASE", "STRING", 8}, {"LOFI", "BYTE BENT", 8}, {"VOICE", "WOW BASS", 4},
        {"WHEEL", "ROCK DRIVE", 8}, {"DRUM", "", 8}, {"NOISE", "METAL", 8}, {"FM6", "BRASS", 8}, {"ANIMAL", "ROAR", 8}};
    static const int16_t AM[AN_NMODE][8] = {           /* (0.8.3) ANIMAL's modes, every option on, no gaps */
        {AN_CMOS, 127, 127, 64, 63, 127, 0, 40}, {AN_PULSE, 127, 127, 26, 63, 127, 0, 22},
        {AN_WHSTL, 127, 127, 40, 63, 127, 0, 22}, {AN_WING, 127, 127, 64, 63, 127, 0, 22},
        {AN_GLOTT, 127, 127, 64, 63, 127, 0, 40}};
    static const char *const RT[5] = {"ROOM", "SPRING", "SHIMR", "RESO", "CLOUD"};
    char name[40];
    uint32_t i, e;
    for (i = 0; i < sizeof E / sizeof E[0]; i++) {
        for (e = 0; e < NENGINES && !str_eq(ENGINES[e]->name, E[i].eng); e++)
            ;
        if (e == NENGINES) continue;
        vg_quiet();
        vg_part(0, e, vg_preset(e, E[i].pre), E[i].notes);
        snprintf(name, sizeof name, "eng/%s", E[i].eng);
        vg_section(name);
        if (!i) {                                   /* (a self-check: the part does sound) */
            uint32_t k, n = 0;
            for (k = 0; k < NVOICE; k++) n += trk[0].v[k].active;
            if (n < 4u) { printf("cpu_vg: the part does not sound (%u voices)\n", n); return 1; }
        }
    }
    for (i = 0; i < AN_NMODE; i++) {                    /* (0.8.3) ANIMAL's five sources */
        uint32_t k;
        vg_quiet();
        set_engine_of(&trk[0], ENGI_ANIMAL);
        trk[0].engine = trk[0].eng_req;
        for (k = 0; k < 8u; k++)
            trk[0].p[P_E0 + k] = AM[i][k];
        trk[0].p[P_MUTE] = 0; trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SUS] = 127;
        for (k = 0; k < 64u; k++)
            mix_block(vo, CTL);
        for (k = 0; k < 8u; k++)
            trk_note_on(&trk[0], 48 + 4 * k, 100);
        snprintf(name, sizeof name, "eng/ANIMAL.%s", N_ANIMAL_MODE[i]);
        vg_section(name);
    }
    for (i = 0; i < 5u; i++) {
        vg_quiet();
        song.g[G_RTYPE] = (int16_t)i;
        vg_part(0, ENGI_ANALOG, 0, 4);
        trk[0].p[P_REV] = 127;
        snprintf(name, sizeof name, "rev/%s", RT[i]);
        vg_section(name);
    }
    for (i = 0; i < 3u; i++) {                          /* (0.8.4) the delay's line at each rate (fx.c dly_lo): 634 ms
                                                         * full, 1.03 s half, 2.13 s a quarter (DLY_LEN 32768) */
        static const char *const DN[3] = {"dly/full", "dly/half", "dly/quarter"};
        static const uint8_t DT[3] = {DT_FREE + 40, DT_FREE + 44, DT_FREE + 50};
        vg_quiet();
        song.g[G_DTIME] = DT[i]; song.g[G_DFDBK] = 70; song.g[G_DMOD] = 20;
        vg_part(0, ENGI_ANALOG, 0, 4);
        trk[0].p[P_DLY] = 127;
        vg_section(DN[i]);
    }
    vg_quiet();
    for (i = 0; i < 4u; i++) {
        vg_part(i, i == 1u ? ENGI_FM6 : ENGI_ANALOG, 0, 4);
        trk[i].p[P_DLY] = 60; trk[i].p[P_REV] = 60; trk[i].p[P_CHOR] = 60; trk[i].p[P_DIST] = 40;
    }
    vg_part(5, ENGI_DRUM, 0, 4);
    song.g[G_RTYPE] = 4;
    vg_section("mix/full");
    vg_seq(0);
    vg_section("seq/plain");
    vg_seq(1);
    vg_section("seq/heavy");
    ui_power_on();
    cpu_test_song();
    transport_req = 1; ui_input();
    vg_section("song/cputest");
    song.g[G_DJF] = -24;                                /* (0.9.1) the same song through the DJ FILTER (a low-pass, ~1.4 kHz) */
    vg_section("song/cputest.djf");
    song.g[G_DJF] = 0;
    return 0;
}
