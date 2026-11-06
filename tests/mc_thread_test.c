/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Yehnah's real mixer with deferred or genuinely concurrent core-1 jobs. Inspired by SLOOP's cpu1_host.c modes. */
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#define MC_HOST_STORAGE _Thread_local
#define MC_HOST_ASYNC 1
static int mc_run(void (*fn)(uint32_t), uint32_t arg);
static int mc_wait(void);
#define main hostsim_main
#include "hostsim.c"
#undef main
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static int threaded, chaos, pending, go, done, stop, started;
static uint32_t jobs, fail_at, failures;
static void (*job_fn)(uint32_t);
static uint32_t job_arg;
static void *work(void *unused)
{
    uint32_t seed = 12345;
    (void)unused;
    mc_host_cpu = 1;
    pthread_mutex_lock(&mutex);
    for (;;) {
        void (*fn)(uint32_t);
        uint32_t arg;
        while (!go && !stop) pthread_cond_wait(&condition, &mutex);
        if (stop) break;
        fn = job_fn; arg = job_arg; go = 0;
        pthread_mutex_unlock(&mutex);
        if (chaos) { seed = seed * 1664525u + 1013904223u; usleep(seed % 71u); }
        fn(arg);
        if (chaos) { seed = seed * 1664525u + 1013904223u; usleep(seed % 71u); }
        pthread_mutex_lock(&mutex);
        done = 1;
        pthread_cond_signal(&condition);
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}
static int mc_run(void (*fn)(uint32_t), uint32_t arg)
{
    if (!mc_host) return 0;
    if (pending) abort();
    pending = 1; jobs++;
    pthread_mutex_lock(&mutex);
    job_fn = fn; job_arg = arg; done = 0;
    if (jobs != fail_at && threaded) {
        if (!started) { if (pthread_create(&worker, 0, work, 0)) abort(); started = 1; }
        go = 1; pthread_cond_signal(&condition);
    }
    pthread_mutex_unlock(&mutex);
    return 1;
}
static int mc_wait(void)
{
    if (!pending) return 0;
    pending = 0;
    if (jobs == fail_at) { failures++; mc_host = 0; return -1; }
    if (threaded) {
        pthread_mutex_lock(&mutex);
        while (!done) pthread_cond_wait(&condition, &mutex);
        pthread_mutex_unlock(&mutex);
    } else {
        mc_host_cpu = 1; job_fn(job_arg); mc_host_cpu = 0;
    }
    return 0;
}
int main(int argc, char **argv)
{
    static const uint8_t EN[6] = {ENGI_DRUM, ENGI_VOICE, ENGI_ANIMAL, ENGI_LOFI, ENGI_ANALOG, ENGI_FM6};
    int32_t out[2 * CTL];
    uint32_t k, b, both = 0;
    uint64_t hash = 1469598103934665603ull;
    const char *mode = argc > 1 ? argv[1] : "threaded", *scene = argc > 2 ? argv[2] : "mixed";
    threaded = strcmp(mode, "deferred") != 0;
    chaos = !strcmp(mode, "chaos");
    fail_at = !strcmp(mode, "timeout") ? 17u : 0u;
    host_tracks_init(); mc_host = strcmp(mode, "serial") != 0;
    /* Force admission in the serial reference too: compare exactly the same
     * twelve voices, regardless of the device's two-core admission policy. */
    if (!strcmp(scene, "polyphony")) voice_headroom = 1;
    for (k = 0; k < NTRK; k++) {
        uint32_t e = !strcmp(scene, "voice") ? ENGI_VOICE : !strcmp(scene, "parallel") ? (k & 1u ? ENGI_ANALOG : ENGI_FM6) : EN[k];
        if (!strcmp(scene, "twovoice") && k == 4u) e = ENGI_VOICE;
        if (!strcmp(scene, "polyphony")) e = k & 1u ? ENGI_VOICE : ENGI_FM6;
        host_preset(&trk[k], e, 0);
        trk[k].p[P_VOICE] = !strcmp(scene, "polyphony") ? V_POLY : V_MONO;
        trk[k].p[P_SUS] = 127;
        if (!strcmp(scene, "shared")) {
            trk[k].p[P_APDEP] = 70; trk[k].p[P_APWAVE] = 4; trk[k].p[P_APRATE] = 127;
            trk[k].p[P_LRATE] = k == 1u ? -1 : 127; trk[k].lfo_ph = 0xFFFFFFF0u;
            trk[k].p[P_LWAVE] = 4;
        }
        trk_note_on(&trk[k], e == ENGI_DRUM ? DRUM_LANE_NOTE[0] : 48 + k * 2, 100);
        if (!strcmp(scene, "polyphony")) trk_note_on(&trk[k], 55 + k * 2, 100);
    }
    if (!strcmp(scene, "shared")) {
        trk[0].p[P_M1SRC] = MS_LFO; trk[0].p[P_M1DST] = MD_RATE; trk[0].p[P_M1AMT] = 30;
        song.g[G_DSPRY] = 80; song.g[G_DPIT] = 3;
    }
    for (b = 0; b < 1024u; b++) {
        if (!strcmp(scene, "fade") && b == 192u) host_preset_req(&trk[0], ENGI_VOICE, 2);
        if (!strcmp(scene, "fade") && b == 512u) host_preset_req(&trk[1], ENGI_ANALOG, 0);
        if (!strcmp(scene, "transition") && b == 128u) trk_live = 4;
        if (!strcmp(scene, "transition") && b == 256u) trk_live = 6;
        mix_block(out, CTL);
        if (!strcmp(scene, "polyphony") && b == 127u && (voices_busy() != 12u || voice_kills)) return 1;
        if (mc_mask_last && mc_mask_last != (1u << NPART) - 1u) both++;
        for (k = 0; k < 2u * CTL; k++) hash = (hash ^ (uint32_t)out[k]) * 1099511628211ull;
    }
    hash = (hash ^ rng_state) * 1099511628211ull;
    hash = (hash ^ (uint32_t)formant_nz) * 1099511628211ull;
    hash = (hash ^ ap_rng) * 1099511628211ull;
    if (started) {
        pthread_mutex_lock(&mutex); stop = 1; pthread_cond_signal(&condition); pthread_mutex_unlock(&mutex);
        pthread_join(worker, 0);
    }
    if (fail_at && (failures != 1u || mc_host || jobs != fail_at)) return 1;
    if (!fail_at && mc_host && !strcmp(scene, "parallel") && !both) return 1;
    printf("mc-thread: %s %s hash=%016llx jobs=%u failures=%u overlap=%u\n", mode, scene, (unsigned long long)hash, jobs, failures, both);
    return 0;
}
