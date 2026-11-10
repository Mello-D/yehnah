/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Voice allocation, envelopes, LFO and per-track rendering.
 * Runs in the audio ISR.
 *
 * Each synth part has its own NVOICE voices (so MONO / LEGATO / UNISON keep using
 * v[0..]), but all parts share a measured-load budget (8 by default, up to 12 in FULL): a part that starts
 * a voice while the budget is full takes one from any part (voice_victim): the oldest
 * released voice, else the oldest extra UNISON voice, else the oldest held voice of a
 * POLY part that is not its lowest note. A voice taken from another part fades out
 * over one block (stage 4: the envelope goes to 0, the block's amplitude ramp
 * declicks it); one of the part's own is restarted in place, as before. Extra UNISON
 * voices only start when there is room. */
static uint32_t vage;                                   /* voice ages: one clock for every part */
static int32_t lfo_wave(track_t *t, uint32_t ph)
{
    switch (t->p[P_LWAVE]) {
    case 1:
        return osc_tri(ph);
    case 2:
        return (int32_t)(ph >> 16) - 32768;
    case 3:
        return ph < 0x80000000u ? 32767 : -32767;
    case 4:
        return (int32_t)(t->lfo_rnd >> 16) - 32768;
    default:
        return osc_sine(ph);
    }
}

/* the S&H value of a new LFO cycle. Parts 1..3 draw it from the shared generator (rng); part 4 has its
 * own xorshift, so the shared sequence, which also seeds the engines' noise (PHYS, VOICE, ..), does not
 * depend on part 4's LFO (the golden renders rely on it) */
#define LFO_SHARED_RNG 3u
static uint32_t lfo_rand(track_t *t)
{
    uint32_t s = t->lfo_rnd ? t->lfo_rnd : 0x9E3779B9u;
    if ((uint32_t)(t - trk) < LFO_SHARED_RNG)
        return rng();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

/* (0.5) ANALOG's FILTER TYPE COMB: the part's sum through a feedback comb, tuned to its newest note moved by
 * CUT (+-32 semitones; 64: the note: a resonance at the pitch), RES its feedback (up to ~0.99: long metallic rings);
 * the level kept about the dry's. One line a part (1024 samples: down to ~43 Hz) */
#define COMB_LEN 1024u
static int16_t comb_buf[NTRK][COMB_LEN] __attribute__((section(".pool")));   /* (0.6: six tracks, in the pool) */
static uint16_t comb_w[NTRK];
static int16_t comb_p16[NTRK];
static __attribute__((noinline)) void track_comb(track_t *t, int32_t *out, uint32_t n, int32_t cut, int32_t res)
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK, i, per, age = 0, w = comb_w[ti];
    int32_t p16 = comb_p16[ti], fb = 16000 + res * 130, g = 32768 - fb;
    int16_t *b = comb_buf[ti];
    for (i = 0; i < NVOICE; i++)                        /* the newest note sounding */
        if (t->v[i].active && t->v[i].age >= age) {
            age = t->v[i].age;
            p16 = t->v[i].pitch_cur;
        }
    comb_p16[ti] = (int16_t)p16;
    p16 = clamp(p16 + (cut - 64) * 8, 0, 2047);
    per = 0xFFFFFFFFu / ((pitch_inc((uint32_t)p16) >> 8) | 1u);   /* the period, Q8 */
    per = per < (2u << 8) ? 2u << 8 : per > ((COMB_LEN - 2u) << 8) ? (COMB_LEN - 2u) << 8 : per;
    for (i = 0; i < n; i++) {
        uint32_t d = per >> 8;
        int32_t a = b[(w - d) & (COMB_LEN - 1u)], c = b[(w - d - 1u) & (COMB_LEN - 1u)];
        int32_t x = (a + (((c - a) * (int32_t)(per & 255u)) >> 8)) << 1, y = out[i] + mulq15(x, fb);
        b[w & (COMB_LEN - 1u)] = (int16_t)clamp(y >> 1, -32768, 32767);
        w++;
        y = clamp(y, -262000, 262000);                  /* (0.6.81: y * g overflowed with a loud part, ~96000 in: the
                                                         * sign flipped, a full-scale crackle; a quarter first) */
        out[i] = soft_knee((((y >> 2) * g) >> 15) * 8 + (out[i] >> 2), 24000);
    }
    comb_w[ti] = (uint16_t)w;
}

/* (0.5) the FILTER of every engine but ANALOG (its own, per voice): the part's sum through a drive (DRV: x1 ..
 * x4 into the soft clip, the level kept) and a state-variable filter, LP HP BP at CUT (moved by the LFO and the loudest
 * voice's envelope through LFO DEST / ENV DEST FLT) and RES; COMB the comb above (CUT about the newest note, RES its
 * feedback). LP open (127), no drive: out of the chain.
 * (0.6.8, the crackles turning CUT / RES) going in and out of the chain, and from one TYPE to another, crossfade
 * over the block, the state taken from the input on the way in (it was stale: a step each time CUT or RES crossed the
 * open LP) */
static int32_t tf_ic1[NTRK], tf_ic2[NTRK];
static int32_t tf_side[NTRK][4];                /* (0.6.81) a DRUM part's IMAGE sides' filter states (L ic1 ic2, R) */
/* (0.6.81) a DRUM part's IMAGE: what each side gives up (eng_drum.c dx_sl dx_sr) filtered as the part is, or the side
 * taken out of the filtered sum was unfiltered: its highs back in raw, a grit like a bitcrusher. The same filter, linear,
 * in and out with the part's fades */
static __attribute__((noinline)) void tf_sides(uint32_t ti, const tsvf_t *c, int32_t k, uint32_t ft, uint32_t was,
                                               uint32_t on, uint32_t n)
{
    uint32_t sd, i;
    for (sd = 0; sd < 2u; sd++) {
        int32_t *b = sd ? dx_sr : dx_sl, ic1 = tf_side[ti][sd * 2u], ic2 = tf_side[ti][sd * 2u + 1u];
        if (!was) {
            ic1 = 0;
            ic2 = b[0];
        }
        for (i = 0; i < n; i++) {
            int32_t x = b[i], y, w = (int32_t)((i + 1u) << (12 - CTL_LOG2));
            int32_t v3 = x - ic2, v1 = (c->a1 * ic1 + c->a2 * v3) >> 13, v2 = ic2 + ((c->a2 * ic1 + c->a3 * v3) >> 13);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            y = ft == FT_HP ? x - v2 - ((k * v1) >> 12) : ft == FT_BP ? (k * v1) >> 12 : v2;
            if (!was)
                y = x + (((y - x) * w) >> 12);
            else if (!on)
                y = y + (((x - y) * w) >> 12);
            b[i] = y;
        }
        tf_side[ti][sd * 2u] = ic1;
        tf_side[ti][sd * 2u + 1u] = ic2;
    }
}
static uint8_t tf_on[NTRK], tf_ft[NTRK];        /* the SVF in the chain the last block; the TYPE playing (0.6.71: a
                                                 * byte a track, not a bit: two cores write two tracks at once) */
static uint8_t tf_cmb[NTRK];                    /* (0.6.81) the COMB ran the last block (its line live) */
static uint8_t tf_xb[NTRK], tf_fo[NTRK];        /* (0.6.81) a TYPE's crossfade: its blocks left, the TYPE from (TF_DRY:
                                                 * no filter) */
#define TF_XB 8u                                /* (~5 ms: one block's fade between two filters was a tick) */
#define TF_DRY 0xFFu
/* the filter of TYPE ft on a part's block; fo (not TF_DRY, an SVF TYPE): from fo's output to ft's, w0 + (i + 1) dw
 * (Q12) of the way (the SVF's three outputs from one state); own: the engine filters its notes itself (ANALOG, LOFI):
 * only the COMB here, at cut / res */
static __attribute__((noinline)) void tf_run(track_t *t, int32_t *out, uint32_t n, uint32_t ft, uint32_t fo, int32_t w0,
                                             int32_t dw, uint32_t own, int32_t ccut, int32_t cres)
{
    const int16_t *p = t->p;
    uint32_t ti = (uint32_t)(t - trk) % NTRK, i;
    int32_t cut = p[P_FCUT] << 8, envq = 0, ic1 = tf_ic1[ti], ic2 = tf_ic2[ti], k;
    tsvf_t c;
    if (ft == FT_COMB) {
        if (!tf_cmb[ti])                                /* (0.6.81) in: its line cleared (the last time's, stale) */
            memset(comb_buf[ti], 0, sizeof comb_buf[ti]);
        track_comb(t, out, n, ccut, cres);
        tf_cmb[ti] = 1;
        return;
    }
    if (own)
        return;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].stage && t->v[i].stage < 4u && (t->v[i].env >> 9) > envq)
            envq = t->v[i].env >> 9;
    {   /* (0.6.81) the sweep kept where it is heard: CUT moved down so its top stays open at most (an open filter swept
         * up did nothing: half an LFO's cycle, a whole ENV, lost) */
        int32_t span = (p[P_LD_FLT] < 0 ? -p[P_LD_FLT] : p[P_LD_FLT]) * 256 + (p[P_ED_FLT] > 0 ? p[P_ED_FLT] * 256 : 0);
        if (cut + span > (127 << 8))
            cut = (127 << 8) - span > 0 ? (127 << 8) - span : 0;
    }
    cut += ((mulq15(t->lfo_val, t->lfo_fade) * p[P_LD_FLT]) >> 7) + ((envq * p[P_ED_FLT]) >> 7);
    {
        uint32_t on = !(ft == FT_LP && fo == TF_DRY && cut >= (127 << 8) && !p[P_FRES]), was = tf_on[ti];
        if (!on && !was)
            return;                                     /* (open: nothing to filter) */
        if (!was) {                                     /* in: the state as the input (no step) */
            ic1 = 0;
            ic2 = out[0];
        }
        tsvf_coef(&c, cut, p[P_FRES]);
        k = 8192 - p[P_FRES] * 7600 / 127;
        for (i = 0; i < n; i++) {
            int32_t x = out[i], y, w = (int32_t)((i + 1u) << (12 - CTL_LOG2));   /* 0 .. 1 over the block, Q12 */
            int32_t v3 = x - ic2, v1 = (c.a1 * ic1 + c.a2 * v3) >> 13, v2 = ic2 + ((c.a2 * ic1 + c.a3 * v3) >> 13);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            y = ft == FT_HP ? x - v2 - ((k * v1) >> 12) : ft == FT_BP ? (k * v1) >> 12 : v2;
            if (fo != TF_DRY) {                         /* TYPE changed: from the old one's output */
                int32_t y0 = fo == FT_HP ? x - v2 - ((k * v1) >> 12) : fo == FT_BP ? (k * v1) >> 12 : v2;
                y = y0 + (((y - y0) * (w0 + (int32_t)(i + 1u) * dw)) >> 12);
            }
            y = soft_knee(y, 30000);                    /* (the dry part has no knee: the fades after it) */
            if (!was)
                y = x + (((y - x) * w) >> 12);          /* in */
            else if (!on)
                y = y + (((x - y) * w) >> 12);          /* out */
            out[i] = y;
        }
        if ((dx_xon & 1u) && ENGINES[t->engine] == &ENG_DRUM)
            tf_sides(ti, &c, k, ft, was, on, n > CTL ? CTL : n);
        tf_on[ti] = (uint8_t)on;
    }
    tf_ic1[ti] = ic1;
    tf_ic2[ti] = ic2;
}

/* the FILTER after the voices. (0.6.81) A TYPE turned crossfades over TF_XB blocks from the one playing (the TYPE held
 * until it is done): between LP HP BP inside the SVF; into or out of the COMB (or into it from no filter) both run, the
 * old on a copy (was: one block, and the COMB left out: a step each way) */
static int32_t tf_tmp[CTL];
static __attribute__((noinline)) void track_filter(track_t *t, int32_t *out, uint32_t n, uint32_t own, int32_t ccut,
                                                   int32_t cres)
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK, r = (uint32_t)t->p[P_FTYPE] & 3u, ft, fo, i;
    uint32_t act = (tf_on[ti] && !own) || tf_cmb[ti];
    int32_t w0, dw;
    if (own && r != FT_COMB)
        r = FT_LP;                                      /* (an engine's own filter: COMB or nothing here) */
    if (!tf_xb[ti]) {
        if (r != tf_ft[ti] && (act || r == FT_COMB)) {
            tf_fo[ti] = (uint8_t)(act ? tf_ft[ti] : TF_DRY);
            tf_xb[ti] = TF_XB;
        }
        tf_ft[ti] = (uint8_t)r;
    }
    ft = tf_ft[ti];
    if (!tf_xb[ti]) {
        tf_run(t, out, n, ft, TF_DRY, 0, 0, own, ccut, cres);
        tf_cmb[ti] = ft == FT_COMB;
        return;
    }
    if (n > CTL)
        n = CTL;
    fo = tf_fo[ti];
    w0 = (int32_t)(((TF_XB - tf_xb[ti]) << 12) / TF_XB);
    dw = (4096 / (int32_t)TF_XB) >> CTL_LOG2;
    if (fo != TF_DRY && fo != FT_COMB && ft != FT_COMB && !own) {
        tf_run(t, out, n, ft, fo, w0, dw, own, ccut, cres);   /* (one SVF, two of its outputs) */
    } else {
        for (i = 0; i < n; i++)
            tf_tmp[i] = out[i];
        if (fo != TF_DRY)
            tf_run(t, tf_tmp, n, fo, TF_DRY, 0, 0, own, ccut, cres);   /* the old one */
        tf_run(t, out, n, ft, TF_DRY, 0, 0, own, ccut, cres);
        for (i = 0; i < n; i++)
            out[i] = tf_tmp[i] + (((out[i] - tf_tmp[i]) * (w0 + (int32_t)(i + 1u) * dw)) >> 12);
    }
    if (!--tf_xb[ti])
        tf_cmb[ti] = ft == FT_COMB;
}

/* the FILTER after the voices: every engine's but ANALOG's (its own, per voice: Clipper's MODL), the whole filter
 * (track_filter). Its own function: the audio ISR's loop stays as it was */
static __attribute__((noinline)) void track_post(track_t *t, const engine_t *e, int32_t *out, uint32_t n)
{
    if (e != &ENG_ANALOG)
        track_filter(t, out, n, 0, t->p[P_FCUT], t->p[P_FRES]);
}

/* (chord-harp fork) FX > PHASER: four first-order allpass stages on the part's sum, swept by a triangle LFO of its own
 * (RATE as the LFO's), the break from 150 Hz to 3.2 kHz (the two notches at ~0.41 and ~2.4 times it, -39 / -31 dB);
 * FDBK the last stage's output back into the first (up to 0.71): resonance, peaks up to +6.5 dB between notches that
 * widen (~-13 dB). AMOUNT the wet half of the mix (127: (dry + wet) / 2, the notches deepest). Coefficients once a block. noinline, in the flash: the audio ISR's loop only tests AMOUNT */
static const int16_t PHS_A[33] = {-4009, -4001, -3991, -3981, -3970, -3957, -3943, -3928, -3912, -3894, -3874, -3853,
                                 -3829, -3803, -3775, -3744, -3710, -3674, -3634, -3590, -3542, -3491, -3435, -3374,
                                 -3308, -3236, -3159, -3076, -2986, -2889, -2785, -2673, -2553};   /* (a + 1/z) / (1 + a/z),
                                                         * Q12, at 150 * (3200 / 150)^(s / 32) Hz (44.1 kHz) */
static int32_t ph_w[NTRK][4], ph_y[NTRK];
static uint32_t ph_ph[NTRK];                            /* the sweep's phase (ui_lzfx.c draws it) */
static __attribute__((noinline)) void track_phaser(track_t *t, int32_t *out, uint32_t n)
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK, i, k, tri;
    int32_t *w = ph_w[ti], y = ph_y[ti], a, fb = t->p[P_PHFB] * 23, m = t->p[P_PHAMT];   /* (fb Q12) */
    ph_ph[ti] += LFO_INC[clamp(t->p[P_PHRATE], 0, 127)] * (n >> CTL_LOG2);
    tri = (ph_ph[ti] >> 31 ? ~ph_ph[ti] : ph_ph[ti]) >> 16;   /* 0..32767 */
    a = PHS_A[tri >> 10] + (((PHS_A[(tri >> 10) + 1u] - PHS_A[tri >> 10]) * (int32_t)(tri & 1023u)) >> 10);
    for (i = 0; i < n; i++) {
        int32_t x = out[i], v = x + ((y * fb) >> 12);
        for (k = 0; k < 4u; k++) {                      /* w = v - a w', y = a w + w' */
            int32_t wk = clamp(v - ((a * w[k]) >> 12), -262143, 262143);
            v = ((a * wk) >> 12) + w[k];
            w[k] = wk;
        }
        y = clamp(v, -262143, 262143);
        out[i] = x + (((y - x) * m) >> 8);
    }
    ph_y[ti] = y;
}

static uint32_t beat_samples(void);                     /* (fx.c) */
static void track_lfo_tick(track_t *t)
{
    uint32_t old = t->lfo_ph;
    int32_t r = t->p[P_LRATE];
    if (r < 0) {                                        /* (0.6.51) in time: 4BAR .. 1/32 (params.c N_LSYNC), on
                                                         * the transport's grid while it plays (PHASE its offset), free
                                                         * while it is stopped */
        static const uint16_t U[12] = {384, 192, 96, 48, 24, 18, 16, 12, 8, 6, 4, 3};   /* 1/24 of a quarter */
        uint32_t len = beat_samples() * U[(uint32_t)(-r - 1) % 12u] / 24u;
        if (len < 64u)
            len = 64u;
        if (song.playing)
            t->lfo_ph = (uint32_t)(((uint64_t)(ms_clock % len) << 32) / len) +
                        (t->p[P_LPHASE] > 0 ? (uint32_t)t->p[P_LPHASE] << 25 : 0u);
        else
            t->lfo_ph += (uint32_t)(((uint64_t)CTL << 32) / len);
    } else
        t->lfo_ph += LFO_INC[r & 127];
    if (t->lfo_ph < old)
        t->lfo_rnd = lfo_rand(t);
    t->lfo_val = lfo_wave(t, t->lfo_ph);
    if (t->lfo_fade < 32767) {
        int32_t step = (int32_t)(ENV_LIN[t->p[P_LFADE] & 127] >> 9);
        t->lfo_fade = t->p[P_LFADE] ? clamp(t->lfo_fade + (step ? step : 1), 0, 32767) : 32767;
    }
}

/* voices the engine may use (POLY and UNISON): its cap, else all of them */
/* (0.6.29, 0.6.30) GLO > SYSTEM > LOAD (MENU > SYSTEM > LOAD): how much of the CPU the heavy parts may take,
 * swapped while they sound, the project keeping its settings (ui.c ui_load, rp_apply). FULL: all as set. LITE: their
 * light kin: FM6 two voices, the reverb SHIMMER / CLOUD as ROOM, RESO as SPRING, the GRAIN delay plain (fx.c). ECO:
 * LITE, and every reverb a SPRING (0.6.70: its chain shortened, fx.c SP_N_ECO), no chorus, no SLICER (slicer.c: its
 * tracks straight to the mix, its pages hidden), at most ECO_POLY voices a track (an engine of hits, DRUM, as ever),
 * ECO_VOICES between all (voice_cap), the load budget sooner (audio.c TIGHT_ON_ECO) */
enum { LOAD_FULL, LOAD_LITE, LOAD_ECO };
static volatile uint8_t sys_load;
#define LITE_FM6_POLY 2u
#define ECO_POLY 4u
#define ECO_VOICES 6u            /* (0.6.70) LOAD ECO: the voices all parts share (NVOICE else) */
static uint32_t trk_nvoice(const track_t *t)
{
    uint32_t c = ENGINES[t->engine]->poly;
    if (sys_load && t->engine == ENGI_FM6 && c > LITE_FM6_POLY)
        c = LITE_FM6_POLY;
    if (sys_load == LOAD_ECO && !ENGINES[t->engine]->oneshot && (!c || c > ECO_POLY))
        c = ECO_POLY;
    return c && c < NVOICE ? c : NVOICE;
}

/* the part's voice mode: P_VOICE, POLY for an engine of hits (engine_t.oneshot: DRUM) */
static uint32_t trk_vmode(const track_t *t)
{
    return ENGINES[t->engine]->oneshot ? V_POLY : (uint32_t)t->p[P_VOICE];
}

/* ------------------------------------------------- the shared voice budget --- */
/* (0.6.16) the load budget: audio.c sets it when a half took more than 65 % of its time (the render), clears
 * it after a while under 60 %. While it is set the sound may not grow: a new note takes a voice (the oldest released
 * first) instead of adding one (voice_room), CLOUD spawns half its grains (fx.c). Ahead of the overload guard (80 %:
 * a voice faded) and of a late half (a click): the load stops climbing before it gets there */
static volatile uint8_t load_tight;
/* Extra shared voices are admitted only after the audio ISR establishes
 * sustained deadline headroom on two healthy cores. No extra per-part slots. */
static uint8_t voice_headroom;
#define FULL_VOICES 12u
/* (0.6.17) what the audio did since power-on, for MENU > ABOUT (audio.c fills it): the slowest half's share of its
 * time (%), the halves that came late (each a click), the voices the overload guard faded */
static struct { volatile uint32_t peak, late, shed, late_play; } load_diag;   /* (chord-harp fork) late_play: the late
                                                         * halves while the transport played (an erase's or a load's
                                                         * pause, in silence, counts only in late) */
/* (0.6.70) the budget shared by all parts: NVOICE, LOAD ECO ECO_VOICES (4 parts with reverb and delay still
 * crackled on ECO: the peaks are the voices, a tail ringing on each part as the next hits come) */
static uint32_t voice_cap(void)
{
    return sys_load == LOAD_ECO ? ECO_VOICES :
           sys_load == LOAD_FULL && voice_headroom ? FULL_VOICES : NVOICE;
}
static uint32_t voices_busy(void)                       /* sounding voices of all parts (not the fading ones) */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}

/* (2026-11-09, the user's call after the CPU test song) the budget is also in COST: a voice weighs what it costs to
 * render, in quarter ANALOG voices (4 = one ANALOG voice; host ns a voice 9.7 ANALOG, 20 JUNO, ~20 DRIFT, 15.7 FM6,
 * ~16 PLAITS, ~15 HYBRID, 14.6 VOICE; the other engines count as ANALOG). The count caps stay (8, 12 with headroom);
 * the cost caps hold the heavy engines back: 40 (10 ANALOG voices, ~6 heavy ones) and, with headroom, 56 (12 ANALOG,
 * ~8 heavy). On the device (USB off, one section alone) six heavy parts at 8 voices peaked 108 % with ~100 late halves a
 * minute; the same engines in threes (8 voices) 78..94 % and clean. LOAD ECO: the count cap only, as before */
#define VCOST_ANALOG 4u
#define UCAP_BASE 40u
#define UCAP_FULL 56u
static uint32_t vcost(const track_t *t)
{
    switch (t->engine) {
    case ENGI_JUNO: case ENGI_DRIFT: return 8u;
    case ENGI_FM6: case ENGI_PLAITS: return 7u;
    case ENGI_HYBRID: case ENGI_VOICE: return 6u;
    default: return VCOST_ANALOG;
    }
}
static uint32_t voices_cost(void)                       /* the sounding voices' weight (not the fading ones) */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++) {
        uint32_t c = vcost(&trk[p]);
        for (i = 0; i < NVOICE; i++)
            n += c * (trk[p].v[i].active && trk[p].v[i].stage != 4u);
    }
    return n;
}
static uint32_t ucap(void)
{
    return sys_load == LOAD_ECO ? ~0u : sys_load == LOAD_FULL && voice_headroom ? UCAP_FULL : UCAP_BASE;
}
/* no room for a new voice of part t: the count budget or the cost budget is full */
static int voice_full(const track_t *t)
{
    return voices_busy() >= voice_cap() || voices_cost() + vcost(t) > ucap();
}

static uint32_t lowest_held(const track_t *t)           /* index of the lowest held note (the bass), NVOICE = none */
{
    uint32_t i, low = NVOICE;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && (low == NVOICE || t->v[i].note < t->v[low].note))
            low = i;
    return low;
}

/* the voice to give up for a new one (see the top); soft: only released voices and
 * extra voices of other non-POLY parts (also ones left after a mode change).
 * Returns its index, *pp its part; NVOICE = none */
static uint32_t voice_victim(const track_t *self, int soft, track_t **pp)
{
    uint32_t p, i, best = NVOICE, cat = 4;
    for (p = 0; p < NPART; p++) {
        track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t);
        uint32_t low = voice_poly((int)mode) ? lowest_held(t) : NVOICE;
        if (soft && t == self)
            continue;
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &t->v[i];
            uint32_t c;
            if (!v->active || v->stage == 4u)
                continue;
            if (!v->gate)
                c = 1;                                  /* released */
            else if (!voice_poly((int)mode) && i > 0)
                c = 2;                                  /* extra UNISON, or a voice left by a mode / cap change */
            else if (!soft && voice_poly((int)mode) && i != low)
                c = 3;                                  /* held, not the bass */
            else
                continue;
            /* (2026-11-09) among the released the quietest tail goes (its cut is the smallest click), the oldest of equals;
             * the held ones as before: the oldest */
            if (c < cat || (c == cat && (c == 1 && v->env_out != (*pp)->v[best].env_out ? v->env_out < (*pp)->v[best].env_out
                                                                                        : v->age < (*pp)->v[best].age))) {
                cat = c;
                best = i;
                *pp = t;
            }
        }
    }
    return best;
}

/* (0.8.9) console status: voices_given_up counts events, one per voice faded out early, whatever asked (a voice
 * already fading is not counted again); voice_trims the share of them the load budget took: a released tail already
 * under -50 dB (inaudible, one per note while LOAD is tight); the overload guard's are audio.c shed_count; the rest
 * were taken for a new note with the budget of voices full (voices_stolen) */
static uint32_t voice_kills, voice_trims;               /* voices given up (budget, overload): console, hostsim */
static void voice_kill(voice_t *v)                      /* fade out over ~6 ms (env_tick stage 4; 0.6.74, was a block) */
{
    v->gate = 0;
    if (v->stage != 4u) {
        v->kfade = 0;                                   /* (0.6.74: the fade from its start) */
        voice_kills++;
    }
    v->stage = 4;
}

/* (0.6.18) the load budget takes only a tail that is nearly gone: the oldest released voice under -50 dB (any part; 0.6.19, was -30).
 * NVOICE = none: the new voice is added and the tails ring out (0.6.16-17 took the oldest released one, loud or not:
 * with a sequence running every note cut the tails; the overload guard at 80 % stays the last line) */
static uint32_t quiet_tail(track_t **pp)
{
    uint32_t p, i, best = NVOICE;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[p].v[i];
            if (!v->active || v->gate || v->stage == 4u || v->env_out >= 104)
                continue;
            if (best == NVOICE || v->age < (*pp)->v[best].age) {
                best = i;
                *pp = &trk[p];
            }
        }
    return best;
}

/* the budget is full: free a voice for part t. 0 = nothing to take (soft) */
static int voice_room(track_t *t, int soft)
{
    track_t *vp = 0;
    uint32_t k;
    if (!voice_full(t)) {
        if (load_tight && (k = quiet_tail(&vp)) < NVOICE) {
            voice_kill(&vp->v[k]);                      /* (the load budget: a dying tail makes the room) */
            voice_trims++;
        }
        return 1;
    }
    for (;;) {                                          /* (the cost budget: a cheap victim may not free enough: more) */
        k = voice_victim(t, soft, &vp);
        if (k == NVOICE)
            return !soft;                               /* hard: over the budget (cannot happen: each of the
                                                         * NPART < NVOICE parts protects one voice at most) */
        voice_kill(&vp->v[k]);
        if (!voice_full(t))
            return 1;
    }
}

static voice_t *voice_reuse(track_t *t, voice_t *v)
{
    if (v->stage == 4u)
        voice_room(t, 0);                               /* its budget was taken before this block */
    return v;
}

/* POLY allocation (within the engine's voice cap). DRUM: reuse its lane; other engines: the same note.
 * Else a free voice: ROTATE takes
 * the next one round-robin (release tails ring out), REUSE (or any GLIDE) takes
 * the free voice whose last pitch is closest, so poly portamento moves each
 * voice the shortest way. Steal: the oldest released voice, else the oldest
 * held one, never the lowest held note (the bass). A free voice needs room in the
 * shared budget: when the voice to give up is one of this part's, it is restarted
 * in place instead. */
static voice_t *voice_alloc(track_t *t, uint32_t note)
{
    uint32_t i, np = trk_nvoice(t), best = np, low = np, nfree = 0;
    int32_t bd = 0x7FFFFFFF;
    if (t->engine == ENGI_DRUM) {
        voice_t *v = drum_reuse(t, note);
        if (v)
            return voice_reuse(t, v);
    }
    for (i = 0; i < np; i++) {
        if (t->v[i].active && t->v[i].note == note)
            return voice_reuse(t, &t->v[i]);
        nfree += !t->v[i].active;
    }
    if (nfree && (voice_full(t) || load_tight)) {       /* (0.6.16: the load budget too; 0.6.18: a dying tail) */
        track_t *vp = 0;
        uint32_t full = voice_full(t), k = full ? voice_victim(t, 0, &vp) : quiet_tail(&vp);
        if (k < np && vp == t)
            return &t->v[k];                            /* our own: restart it in place (no click) */
        if (k < NVOICE) {
            voice_kill(&vp->v[k]);
            voice_trims += !full;                       /* (the load budget's quiet tail, not a steal) */
        }
        while (full && voice_full(t) && (k = voice_victim(t, 0, &vp)) < NVOICE) {   /* (the cost budget: a cheaper victim) */
            if (k < np && vp == t)
                return &t->v[k];
            voice_kill(&vp->v[k]);
        }
    }
    if (t->p[P_ALLOC] || t->p[P_GLIDE]) {
        for (i = 0; i < np; i++) {
            int32_t d = t->v[i].pitch_cur - (int32_t)note * 16;
            if (t->v[i].active)
                continue;
            d = d < 0 ? -d : d;
            if (!t->v[i].pitch_cur)
                d = 0x7FFFFFF0;                          /* never played: last choice */
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
    } else {
        for (i = 0; i < np && best == np; i++) {
            uint32_t k = (t->rr + i) % np;
            if (!t->v[k].active)
                best = k;
        }
        t->rr = (uint8_t)((best == np ? t->rr : best) + 1u) % np;
    }
    if (best < np)
        return &t->v[best];
    for (i = 0; i < np; i++)                             /* the lowest held note is protected */
        if (t->v[i].gate && (low == np || t->v[i].note < t->v[low].note))
            low = i;
    for (i = 0; i < np; i++)                             /* oldest released (not one fading out for another part) */
        if (!t->v[i].gate && t->v[i].stage != 4u && (best == np || t->v[i].age < t->v[best].age))
            best = i;
    if (best == np)
        for (i = 0; i < np; i++)                         /* oldest held, not the bass */
            if (i != low && (best == np || t->v[i].age < t->v[best].age))
                best = i;
    return voice_reuse(t, &t->v[best == np ? 0 : best]);
}

/* (VOICE) a glide sets out from where the voice sounds: the S-curve (eng_formant.c glide_sung) folded into pitch_cur (a
 * new note mid-glide: no jump) */
static void glide_from(voice_t *v)
{
    v->pitch_cur = (glide_sung(v) + 128) >> 8;
    v->gfrom = (int16_t)v->pitch_cur;
}
static __attribute__((noinline)) void glide_from_trk(const track_t *t, voice_t *v)   /* (a legato note, a chord glide) */
{
    if (ENGINES[t->engine % NENGINES] == &ENG_FORMANT)
        glide_from(v);
}

/* glide: RATE = a fixed speed, TIME = the same time for any interval. (0.9.8) glide 2: VOICE singing words, a legato
 * note with GLIDE 0: a singer's ~60 ms */
static void glide_set(track_t *t, voice_t *v, int glide)
{
    if (!glide || !v->pitch_cur) {
        v->pitch_cur = v->pitch16;
        v->gstep = 0;
        return;
    }
    if (t->p[P_GLMODE] || glide == 2) {
        int32_t d = v->pitch16 - v->pitch_cur, ticks = glide == 2 ? 80 : 1 + t->p[P_GLIDE] * t->p[P_GLIDE] / 8;
        d = d < 0 ? -d : d;
        v->gstep = d * 256 / ticks > 0 ? d * 256 / ticks : 1;
    } else {
        v->gstep = 0;
    }
}

/* (0.6.81) SCL TRANS: the track's every note this many semitones up or down as it sounds (the keys, the
 * sequence, MIDI, the ARP alike; recorded as played: it was the keys' alone, the sequence did not follow). Kits: none */
static int32_t trk_trans(const track_t *t)
{
    const engine_t *e = ENGINES[t->engine % NENGINES];
    return e->oneshot || e->keys ? 0 : t->p[P_TRANS];
}
/* (chord-harp fork, 2026-10-29) SAVE > MORPH, the track BLEND: TRACK A and TRACK B (G_BLA, G_BLB) play one part, each
 * its own sound: a note to A goes to both, B's own steps stay silent while blended (its keys and MIDI play A: seq.c
 * input_on); BLEND shares the sound between them by their LEVELs (fx.c bl_block, mix_part). bl_a, bl_b: the pair the
 * audio plays (track + 1, 0 none); bl_g: each track's share, Q12 (4096 not in a blend); bl_now: where it is (0..127) */
static uint8_t bl_a, bl_b, bl_now;
static uint16_t bl_g[NTRK] = {4096, 4096, 4096, 4096, 4096, 4096};
static track_t *bl_twin(const track_t *t) { return bl_b && (uint32_t)(t - trk) + 1u == bl_a ? &trk[bl_b - 1u] : 0; }
static int bl_is_b(const track_t *t) { return bl_b && (uint32_t)(t - trk) + 1u == bl_b; }

static void voice_start(track_t *t, voice_t *v, uint32_t note, uint32_t vel, int glide)
{
    const engine_t *e = ENGINES[t->engine];
    int sounding = v->active && v->stage != 0;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t s0 = v->s[0], s1 = v->s[1], s4 = v->s[4], s5 = v->s[5], s6 = v->s[6], s7 = v->s[7];
    uint32_t keep = e->keep;
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->gate = 1;
    v->active = 1;
    v->stage = 1;
    v->age = ++vage;
    if (e == &ENG_FORMANT)
        glide_from(v);
    v->fin_d = 0;
    v->gacc = 0;
    v->pitch16 = ((int32_t)note + trk_trans(t)) * 16 + t->pfx_rpit;   /* (pfx.c RANDOM: 0 but while it is held) */
    v->mvel = t->m_vel;                                 /* mod.c: the note's VEL and RAND */
    v->mrnd = t->m_rnd;
    v->henv = t->hum_env;                               /* (0.6.92) HUMAN: this note's DEC / REL moved */
    t->m_vi = (uint8_t)(v - t->v);
    glide_set(t, v, glide);
    if (!sounding) {
        v->env = 0;
        v->env_out = 0;
    }                                                   /* sounding: the attack starts from the current level */
    e->note_on(t, v);
    if (sounding && !e->sampled) {                     /* retrigger / steal: keep phases and filter */
        v->ph[0] = ph0;                                 /* states (resetting them clicks) */
        v->ph[1] = ph1;
        v->ph[2] = ph2;
        if (keep & 0x01u)                               /* engine_t.keep: the slots the engines use */
            v->s[0] = s0;
        if (keep & 0x02u)
            v->s[1] = s1;
        if (keep & 0x10u)
            v->s[4] = s4;
        if (keep & 0x20u)
            v->s[5] = s5;
        if (keep & 0x40u)
            v->s[6] = s6;
        if (keep & 0x80u)
            v->s[7] = s7;
    }
}

/* (chord-harp fork: Clipper's voice.c, fbd8434) a UNISON voice's start phases moved by r (Q32: a turn of its note), as
 * if it had started that much of a period sooner: in phase the voices stack, spread they sum as noise. Each of
 * Clipper's engines moves its own oscillators; the others ph[0] (as before) */
static void unison_spread(track_t *t, voice_t *v, uint32_t r)
{
    const engine_t *e = ENGINES[t->engine];
    if (e == &ENG_ANALOG)
        analog_spread(t, v, r);
    else if (e == &ENG_DIGITAL)
        digital_spread(t, v, r);
    else if (e == &ENG_PHASE)
        phase_spread(t, v, r);
    else if (e == &ENG_LOFI)
        lofi_spread(t, v, r);
#if YEHNAH_NOISE
    else if (e == &ENG_NOISE)
        noise_spread(t, v, r);
#endif
    else if (e == &ENG_STRINGS)
        strings_spread(t, v, r);
    else if (e == &ENG_VPS)
        vps_spread(t, v, r);
    else if (e == &ENG_JUNO)
        juno_spread(t, v, r);
    else if (e == &ENG_WSEQ)
        wseq_spread(t, v, r);
    else if (e == &ENG_FM6)
        fm6_spread(t, v, r);
    else if (e == &ENG_HYBRID)
        hybrid_spread(t, v, r);
    else
        v->ph[0] += r;
}

/* (chord-harp fork: Clipper 0.3's, voice.c fbd8434) UNI2 / UNI4, poly unison: each key 2 or 4 voices, spread by
 * DETUNE; the voices it takes: free ones, then the oldest released, then the oldest held that is not the bass. The same
 * note again restarts its own voices in place */
static int puni_taken(const uint32_t *idx, uint32_t n, uint32_t i)
{
    uint32_t j;
    for (j = 0; j < n; j++)
        if (idx[j] == i)
            return 1;
    return 0;
}
static uint32_t puni_pick(track_t *t, const uint32_t *idx, uint32_t n, uint32_t np)
{
    uint32_t i, best = np, low = lowest_held(t);
    if (voices_busy() < NVOICE)
        for (i = 0; i < np; i++)
            if (!t->v[i].active && !puni_taken(idx, n, i))
                return i;
    for (i = 0; i < np; i++)
        if (t->v[i].active && !t->v[i].gate && t->v[i].stage != 4u && !puni_taken(idx, n, i) &&
            (best == np || t->v[i].age < t->v[best].age))
            best = i;
    if (best < np)
        return best;
    if (voice_room(t, 1))                                /* another part gives one up (soft) */
        for (i = 0; i < np; i++)
            if (!t->v[i].active && !puni_taken(idx, n, i))
                return i;
    for (i = 0; i < np; i++)
        if (t->v[i].gate && !puni_taken(idx, n, i) && (low >= NVOICE || t->v[i].note != t->v[low].note) &&
            (best == np || t->v[i].age < t->v[best].age))
            best = i;
    return best;
}
static void puni_on(track_t *t, uint32_t note, uint32_t vel)
{
    static uint32_t seed = 0x2468ACEu;
    uint32_t np = trk_nvoice(t), k = t->p[P_VOICE] == V_PUNI2 ? 2u : 4u, idx[4], n = 0, i, j;
    if (k > np)
        k = np;
    for (i = 0; i < np && n < k; i++)                  /* the same note: its voices again */
        if (t->v[i].active && t->v[i].note == note && t->v[i].stage != 4u)
            idx[n++] = i;
    while (n < k) {
        i = puni_pick(t, idx, n, np);
        if (i >= np)
            break;
        idx[n++] = i;
    }
    for (j = 0; j < n; j++) {
        voice_t *v = &t->v[idx[j]];
        int was = v->active && v->stage != 0;
        int32_t s = n > 1u ? (2 * (int32_t)j - (int32_t)(n - 1u)) * 7 : 0;   /* -7 .. 7 (x (n - 1)) */
        v->fine = n > 1u ? s * t->p[P_DETUNE] * 56 / (889 * (int32_t)(n - 1u)) : 0;
        voice_start(t, v, note, n >= 4u ? vel / 2u : n > 1u ? vel * 70u / 100u : vel, t->p[P_GLIDE] != 0);
        if (j && !was) {                                /* random start phases (in phase they stack) */
            seed = seed * 1664525u + 1013904223u;
            unison_spread(t, v, seed);
        }
    }
}

/* MONO / LEGATO / UNISON: one note on one voice (eight for UNISON, or the
 * engine's voice cap, spread by DETUNE over the same width) */
static void mono_play(track_t *t, uint32_t note, uint32_t vel, int retrig, int glide)
{
    uint32_t mode = (uint32_t)t->p[P_VOICE], nv = mode == V_UNISON ? trk_nvoice(t) : 1u, i;
    for (i = 0; i < nv; i++) {
        voice_t *v = &t->v[i];
        int32_t k = 2 * (int32_t)i - (int32_t)(nv - 1u);   /* -7 .. 7 */
        if (nv > 1u && nv < NVOICE)
            k = k * (NVOICE - 1) / (int32_t)(nv - 1u);      /* fewer voices: the outer ones as wide */
        v->fine = nv > 1u ? k * t->p[P_DETUNE] * 56 / 889 : 0;   /* up to ~±40 cents */
        if (retrig || !v->active || !v->gate) {
            if ((!v->active || v->stage == 4u) && !voice_room(t, i > 0))
                continue;                                   /* no room for this extra UNISON voice */
            /* level: about the same sum for 8 or 4 voices at random phases */
            voice_start(t, v, note, nv >= NVOICE ? vel * 36u / 100u : nv > 1u ? vel / 2u : vel, glide);
            if (nv > 1u && i && !ENGINES[t->engine]->sampled) {   /* random start phases: */
                static uint32_t seed = 0x1234567u;          /* in phase they stack, evenly spread they cancel */
                seed = seed * 1664525u + 1013904223u;
                unison_spread(t, v, seed);                  /* (Clipper's: every oscillator of the voice) */
            }
        } else {                                            /* legato: new pitch, same envelope */
            v->note = (uint8_t)note;
            glide_from_trk(t, v);
            v->pitch16 = ((int32_t)note + trk_trans(t)) * 16 + t->pfx_rpit;
            v->mvel = t->m_vel;
            v->mrnd = t->m_rnd;
            if (vel > v->vel && nv == 1u)
                v->vel = (uint8_t)vel;
            glide_set(t, v, glide);
        }
    }
    t->mono_note = (uint8_t)note;
}

/* the note the mono stack asks for (PRIO: LAST / LOW / HIGH), 0 = none */
static uint32_t mono_pick(const track_t *t)
{
    uint32_t i, n;
    if (!t->nmono)
        return 0;
    n = t->mono_stack[t->nmono - 1u];
    for (i = 0; i < t->nmono; i++) {
        if (t->p[P_PRIO] == 1 && t->mono_stack[i] < n)
            n = t->mono_stack[i];
        if (t->p[P_PRIO] == 2 && t->mono_stack[i] > n)
            n = t->mono_stack[i];
    }
    return n;
}

static void mono_remove(track_t *t, uint32_t note)
{
    uint32_t j, k = 0;
    for (j = 0; j < t->nmono; j++)
        if (t->mono_stack[j] != note)
            t->mono_stack[k++] = t->mono_stack[j];
    t->nmono = (uint8_t)k;
}

static int pfx_note(track_t *t, uint32_t note, uint32_t vel);   /* pfx.c (in fx.c) */
static uint8_t trk_hush;
static uint8_t trk_live = 4;
static volatile uint8_t asleep_hit;                     /* (0.6.28) a note went to a track asleep (ui_input: the hint) */                            /* (0.6.18) MENU > TRACKS: the tracks that play (4 LOW CPU, the
                                                         * default; 6 HIGH): the ones past it get no notes and stop,
                                                         * their sounds and steps kept (fx.c mix_part) */                                /* (0.6.15) bit k: track k muted by GLO's solo / the FX layer's
                                                         * black key (perform.c): no new notes, as MUTE (a MIDI mute: a
                                                         * silenced track's voices end, it costs nothing) */
static void trk_note_on1(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t any = 0, i, mode = trk_vmode(t);
    if ((uint32_t)(t - trk) >= trk_live && !t->p[P_MUTE]) {
        asleep_hit = 1;                                 /* (0.6.28) a note for a track asleep: the UI says so */
        return;
    }
    if (t->p[P_MUTE] || ((trk_hush >> ((uint32_t)(t - trk) % NTRK)) & 1u) ||
        (t->engine == ENGI_DRUM && drum_muted(note)))   /* (DRUM: its group mutes) */
        return;
    if (pfx_note(t, note, vel))                         /* the punch-in effects (pfx.c): a repeat holds the track */
        return;
    if (t->xf_on || t->eng_req != t->engine) {          /* engine switch under way: after the fade */
        for (i = 0; i < t->xp_n && t->xp_note[i] != note; i++)
            ;
        if (i == t->xp_n && t->xp_n < 4u)
            t->xp_n++;
        i = i < t->xp_n ? i : t->xp_n - 1u;             /* (full: the last one is replaced) */
        t->xp_note[i] = (uint8_t)note;
        t->xp_vel[i] = (uint8_t)vel;
        return;
    }
    mod_note(t, note, vel);                             /* the matrix's VEL / KEY / RAND */
    for (i = 0; i < NVOICE; i++)
        any |= t->v[i].gate;
    if (!any) {                                        /* fresh phrase: LFO retrigger and fade */
        if (t->p[P_LPHASE] >= 0)                        /* (0.6.51: PHASE FREE: it runs on) */
            t->lfo_ph = (uint32_t)t->p[P_LPHASE] << 25;
        t->lfo_fade = 0;
    }
    if (voice_poly((int)mode)) {
        voice_t *v;
        t->nmono = 0;                                   /* no stale mono stack after a mode change */
        t->mono_note = 0;
        if (mode >= V_PUNI2) {                          /* (Clipper's) poly unison */
            puni_on(t, note, vel);
            return;
        }
        v = voice_alloc(t, note);
        v->fine = 0;
        voice_start(t, v, note, vel, t->p[P_GLIDE] != 0 && !ENGINES[t->engine]->oneshot);
        return;
    }
    {
        uint32_t sounding = t->mono_note && t->v[0].active && t->v[0].gate, want;
        mono_remove(t, note);
        if (t->nmono >= 8u)
            mono_remove(t, t->mono_stack[0]);
        t->mono_stack[t->nmono++] = (uint8_t)note;
        want = mono_pick(t);
        if (sounding && want == t->mono_note)
            return;                                     /* LOW / HIGH priority: this key does not win */
        if (mode == V_MONO)                             /* MONO: always retrigger, glide when GLIDE is set */
            mono_play(t, want, vel, 1, t->p[P_GLIDE] != 0 || t->slide_glide);
        else                                            /* LEGATO / UNISON: glide only between held notes */
            mono_play(t, want, vel, !sounding, sounding && (t->p[P_GLIDE] != 0 || t->slide_glide));
    }
}

static void trk_note_off1(track_t *t, uint32_t note)
{
    uint32_t i, k = 0, mode = trk_vmode(t);
    for (i = 0; i < t->xp_n; i++)                       /* not sounding yet (engine switch): forget it */
        if (t->xp_note[i] != note) {
            t->xp_note[k] = t->xp_note[i];
            t->xp_vel[k++] = t->xp_vel[i];
        }
    t->xp_n = (uint8_t)k;
    mono_remove(t, note);                              /* a mode change must not retain a released key */
    if (!voice_poly((int)mode)) {
        uint32_t nv = mode == V_UNISON ? trk_nvoice(t) : 1u;
        if (t->mono_note == note) {
            uint32_t next = mono_pick(t);
            if (next) {                                 /* fall back to a held note, legato */
                mono_play(t, next, t->v[0].vel, 0, t->p[P_GLIDE] != 0);
            } else {
                for (i = 0; i < nv; i++)
                    if (t->v[i].gate) {
                        t->v[i].gate = 0;
                        t->v[i].stage = 3;
                    }
                t->mono_note = 0;
            }
        }
    }
    for (i = 0; i < NVOICE; i++)                        /* a fallback above already has its new pitch */
        if (t->v[i].note == note && t->v[i].gate) {
            t->v[i].gate = 0;
            t->v[i].stage = 3;
        }
}

static void trk_all_off1(track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        t->v[i].gate = 0;
        t->v[i].stage = t->v[i].active ? 3 : 0;
    }
    t->nmono = 0;
    t->mono_note = 0;
    t->xp_n = 0;
}

/* every note's way in (seq.c, midi_control.c): a blend's A plays B too, B alone nothing (its own steps silent) */
static void trk_note_on(track_t *t, uint32_t note, uint32_t vel)
{
    track_t *b = bl_twin(t);
    if (bl_is_b(t))
        return;
    trk_note_on1(t, note, vel);
    if (b)
        trk_note_on1(b, note, vel);
}
static void trk_note_off(track_t *t, uint32_t note)
{
    track_t *b = bl_twin(t);
    if (bl_is_b(t))
        return;
    trk_note_off1(t, note);
    if (b)
        trk_note_off1(b, note);
}
static void trk_all_off(track_t *t)
{
    track_t *b = bl_twin(t);
    trk_all_off1(t);
    if (b)
        trk_all_off1(b);
}

/* engine switch, at each block start (events_block), before any note of the block. The UI writes
 * eng_req and the new engine's P_E0..P_E7 together (IRQ off). The part's sounding voices (released by
 * the preset change) then fade out over XF_BLOCKS blocks on the old engine, rendered with its own
 * parameters (pe_old: an engine never reads another engine's values, which index its tables); only
 * then the engine switches and the notes that came during the fade start on it. A part with nothing
 * sounding switches at once. */
#define XF_BLOCKS 4u                                    /* 4 x 32 samples: 2.9 ms */
/* (0.6.41) smoother: an engine switch fades out over XF_ENG (11.6 ms) and what was held (keys, the sequencer's notes:
 * xf_held, before the load's release) starts on the new engine, fading in over XF_ENG; an engine's list value
 * (WAVE, CHIP, TYPE .. F_ENUM) changed while it sounds: a dip, XF_DIP out on the old value and in on the new one, the
 * voices going on (it clicked: the wave swapped mid-cycle); (0.6.81) a value with names too (FM6's ALG, its SLOT) */
#define XF_ENG 16u
#define XF_DIP 4u                                       /* (0.6.81: 2.7 ms each way, was 1.3: a tick) */
static int eng_kit(uint32_t e) { return ENGINES[e % NENGINES]->oneshot; }
static uint32_t xf_held(const track_t *t, uint8_t *nt, uint8_t *vl)   /* the notes held (up to 4): their count */
{
    uint32_t i, k, n = 0;
    if (eng_kit(t->engine) || eng_kit(t->eng_req))
        return 0;                                       /* (hits: nothing held) */
    for (i = 0; i < NVOICE; i++) {
        const voice_t *v = &t->v[i];
        if (!v->active || !v->gate)
            continue;
        for (k = 0; k < n && nt[k] != v->note; k++)
            ;
        if (k == n && n < 4u) { nt[n] = v->note; vl[n++] = v->vel; }
    }
    return n;
}
static void engine_block(track_t *t)
{
    uint32_t i, any = 0;
    if (!t->xf_on && t->eng_req == t->engine && !eng_kit(t->engine)) {   /* a list value changed while it sounds */
        const engine_t *e = ENGINES[t->engine % NENGINES];
        for (i = 0; i < 8u; i++) {
            const param_desc_t *d = e->desc ? e->desc(t, i) : 0;
            if (t->p[P_E0 + i] != t->pe_old[i] && ((d ? d : &e->edit[i])->fmt == F_ENUM || (d ? d : &e->edit[i])->names))
                break;
        }
        for (any = 0; i < 8u && any < NVOICE; any++)
            if (t->v[any].active && t->v[any].env_out)
                break;
        if (i < 8u && any < NVOICE) {
            t->xf_on = 1; t->xf = t->xf_len = XF_DIP; t->xf_dip = 1;
            return;                                     /* (pe_old: the old value, for the dip's way out) */
        }
        any = 0;
    }
    if (t->xf_on || t->eng_req != t->engine) {
        if (!t->xf_on) {
            for (i = 0; i < NVOICE; i++)
                any |= t->v[i].active;
            if (any) {
                t->xf_on = 1;
                t->xf = t->xf_len = XF_ENG;
                t->xf_dip = 0;
                return;
            }
        } else if (t->xf) {
            return;                                     /* still fading (track_render counts down) */
        }
        if (t->xf_dip) {                                /* the dip's end: the same voices, in again on the new value */
            t->xf_on = t->xf_dip = 0;
            t->xin = t->xin_len = XF_DIP;
            for (i = 0; i < 8u; i++)
                t->pe_old[i] = t->p[P_E0 + i];
            return;
        }
        for (i = 0; i < NVOICE; i++) {                  /* faded to 0: gone */
            voice_t *v = &t->v[i];
            v->active = v->gate = 0;
            v->stage = 0;
            v->env = v->env_out = 0;
        }
        any = t->engine != eng_idx(t->eng_req);
        t->engine = eng_idx(t->eng_req);
        t->xf_on = 0;
        t->nmono = 0;
        t->mono_note = 0;
        {
            uint32_t n = t->xp_n;
            t->xp_n = 0;
            for (i = 0; i < n; i++)
                trk_note_on1(t, t->xp_note[i], t->xp_vel[i]);
            if (n && any)                               /* (0.6.41) on the new engine: in, not at once */
                t->xin = t->xin_len = XF_ENG;
        }
    }
    for (i = 0; i < 8u; i++)                            /* the engine's own values, for a later fade */
        t->pe_old[i] = t->p[P_E0 + i];
}

/* (0.6.13) the sound a track had before a load (ui.c load_begin, the main loop, before it writes the new values): the
 * fade renders with all of it, not only its engine's values (pe_old): the filter, its envelope, the LFO .. as they were.
 * One at a time (a load is one track's; a section's six: the others fade on pe_old as before) */
static int16_t xf_snap[P_COUNT];
static volatile uint8_t xf_snap_t = 0xFFu;              /* its track, 0xFF none */
/* (0.6.13) a sound loaded (a preset, a user preset, a section: seq.c panic_req): what still sounds fades out over
 * XF_BLOCKS on the values it had (as an engine switch does), not its release on the new sound's (its filter, its
 * levels: the old notes jumped bright and loud, sharp highs at every preset change); the notes asked for meanwhile
 * after it */
static void trk_fade_out(track_t *t)
{
    uint32_t i, any = 0;
    if (t->xf_on)
        return;
    for (i = 0; i < NVOICE; i++)
        any |= t->v[i].active;
    if (any) {
        t->xf_on = 1;
        t->xf = t->xf_len = t->eng_req != t->engine ? XF_ENG : XF_BLOCKS;   /* (0.6.41: another engine, longer) */
        t->xf_dip = 0;
    } else if (xf_snap_t == (uint32_t)(t - trk)) {
        xf_snap_t = 0xFFu;                              /* (nothing to fade: no snapshot left for a later one) */
    }
}

/* (chord-harp fork) a trim past LEVEL's 127 (fx.c: JUNO's 106 bank, up to +50 dB) lifts the release's end too: its
 * cut at -72 dB clicked, so such a voice fades out there as a stolen one does (stage 4, ~4.4 ms) */
static inline int rel_lifted(const int16_t *p)
{
    return p[P_LEVEL] && p[P_LEVEL] + p[P_TRIM] > 127;
}

/* one control tick (CTL samples) of the amplitude envelope; returns Q15 */
static int32_t env_tick(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    int32_t sus = (int32_t)p[P_SUS] << 17;              /* Q24 */
    switch (v->stage) {
    case 1:
        if (ENGINES[t->engine % NENGINES] == &ENG_MINI) {   /* (2026-11-11) FMINI, after FMini-1: the Minimoog's capacitor charges
                                                         * toward a level above full: a concave rise that ends in a corner */
            int32_t d = (int32_t)(((int64_t)((5 << 22) - v->env) * (int32_t)(ENV_LIN[p[P_ATK] & 127] >> 1) * 824) >> 32);
            v->env += d < 1 ? 1 : d;
        } else
            v->env += (int32_t)ENV_LIN[p[P_ATK] & 127];
        if (v->env >= (1 << 24)) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env += mulq16(sus - v->env, ENV_EXP[clamp(p[P_DEC] + v->henv, 0, 127)]);
        if (p[P_ELOOP] > 0 && v->gate && v->env - sus < (1 << 18))   /* (0.4) LOOP: at the sustain (within
                                                         * 1/64), held: the attack again from here (an ADSR-shaped LFO) */
            v->stage = 1;
        break;
    case 3:
        v->env -= mulq16(v->env, ENV_EXP[clamp(p[P_REL] + v->henv, 0, 127)]);
        if (v->env < (1 << 12)) {
            if (rel_lifted(p)) {                        /* (chord-harp fork) faded, not cut */
                v->kfade = 0;
                v->stage = 4;
            } else {
                v->env = 0;
                v->stage = 0;
                v->active = 0;
            }
        }
        break;
    case 4:                                             /* given up (another part, the load): it fades out. (Yehnah
                                                         * 0.6.74) A half cosine over 6 blocks (~4.4 ms), each block's
                                                         * ramp between (KFADE: each block's gain over the last's):
                                                         * gentle in and out. One block (0.7 ms, full to 0, as before)
                                                         * clicked on every steal, a bass note most; longer would keep
                                                         * more voices rendering under load */
    {
        static const uint8_t KFADE[6] = {239, 206, 171, 128, 69, 0};
        v->env = (int32_t)(((int64_t)v->env * KFADE[v->kfade < 6u ? v->kfade : 5u]) >> 8);
        v->kfade++;
        if (v->kfade >= 6u || !v->env) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
        }
        break;
    }
    default:
        v->env = 0;
        break;
    }
    return v->env >> 9;
}

/* (chord-harp fork: Clipper's voice.c, fbd8434) Clipper 0.9, VINTAGE (DRIFT > VINTG): the voices of an analog polysynth
 * are never quite alike (the Prophet-5's "slop"): each voice here has its own small tuning error and filter offset (its
 * "card", fixed), a slow drift of its pitch (a random walk, a new target every second or so) and each note a little of
 * its own. At 127: the card +-6 cents, the drift +-5, the note +-2, the filter +-1.5 steps; 0: nothing, bit for bit */
static int32_t vcard(uint32_t k, uint32_t salt)         /* the voice card's value, Q15 */
{
    uint32_t h = (k + 1u) * 0x9E3779B1u ^ (salt + 7u) * 0x85EBCA77u;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12;
    return (int32_t)(h >> 16) - 32768;
}
static uint32_t vint_clk[NTRK];                         /* (chord-harp fork) the track's blocks: the drift's own clock, so a
                                                         * track drifts the same alone (a stem) as in the mix (no rng()) */
static int32_t vintage_pitch(voice_t *v, uint32_t k, int32_t vint)   /* 1/4096 semitone; once a block */
{
    int32_t h = vcard(k, vint_clk[(k / NVOICE) % NTRK] + 2u);
    if ((h & 1023) < 2)                                 /* (~1.4 s between targets) */
        v->vdtgt = (int16_t)vcard(k, vint_clk[(k / NVOICE) % NTRK] + 0x51EDu);
    v->vdrift = (int16_t)(v->vdrift + ((v->vdtgt - v->vdrift) >> 9));
    return ((((vcard(k, 0u) * 246) >> 15) + ((v->vdrift * 205) >> 15) + ((v->mrnd * 82) >> 15)) * vint) >> 7;
}

/* (0.7.8) DRIFT: each voice its own slow value noise (mod.c drift_noise), on the track's blocks */
static uint32_t drift_clk[NTRK];

/* render one block of a part into out (cleared here); returns the voices rendered */
/* Channel bend is live performance state, outside projects/presets. Q8 semitones. */
static int32_t midi_bend_q8[NTRK], midi_bend_target[NTRK];
/* (chord-harp fork) HARP / PROG's bend keys (seq.c harp_bend_k): the bend they hold (1/256 semitone), and where it is on
 * the way there (a short glide: ~1/12 of the way a block, ~30 ms) */
static int32_t key_bend_goal[NTRK], key_bend_cur[NTRK];
static uint32_t track_render(track_t *t, int32_t *out, uint32_t n)
{
    const engine_t *e = ENGINES[t->engine];
    const int16_t *p = t->p;
    uint32_t i;
    int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);
    /* TUNE in cents: whole 1/16 semitones in the pitch, the rest as a fine factor (no dead zone) */
    int32_t tune = song.g[G_TUNE] >= 0 ? song.g[G_TUNE] * 16 / 100 : -((-song.g[G_TUNE] * 16 + 99) / 100);
    int32_t tune_fine = (song.g[G_TUNE] * 16 - tune * 100) * 2367 / 16000;   /* rest, in 1/4096 (1 ct = 2.367) */
    int32_t bend, bend16, bend_fine;
    uint32_t ti = (uint32_t)(t - trk), gl_go = 1;
    if (ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM) {
        midi_bend_q8[ti] = midi_bend_target[ti] = 0;
    } else {
        int32_t d = midi_bend_target[ti] - midi_bend_q8[ti];
        /* ~6 ms smoothing, with an exact landing (no permanent small offset). */
        midi_bend_q8[ti] += d > 0 ? (d < 4 ? d : (d + 3) / 4) : (d > -4 ? d : (d - 3) / 4);
    }
    {   /* (chord-harp fork) the bend keys' glide */
        int32_t d = key_bend_goal[ti] - key_bend_cur[ti];
        key_bend_cur[ti] += d > 0 ? (d < 6 ? d : (d + 11) / 12) : (d > -6 ? d : (d - 11) / 12);
    }
    bend = midi_bend_q8[ti] + key_bend_cur[ti];
    bend16 = bend >= 0 ? bend / 16 : -((-bend + 15) / 16);
    bend_fine = (bend - bend16 * 16) * 2367 / 2560;
    uint32_t nr = 0, fade = t->xf_on && t->xf, snap = fade && xf_snap_t == ti;
    int32_t damt;
    int16_t pe_new[8], p_new[P_COUNT];
    for (i = 0; i < n; i++)
        out[i] = 0;
    if (snap) {                                         /* (0.6.13) a sound load: the whole sound as it was */
        memcpy(p_new, t->p, sizeof p_new);
        memcpy(t->p, xf_snap, sizeof p_new);
    } else if (fade)                                    /* engine switch: the old engine, its own values */
        for (i = 0; i < 8u; i++) {
            pe_new[i] = t->p[P_E0 + i];
            t->p[P_E0 + i] = t->pe_old[i];
        }
    track_lfo_tick(t);
    dx_xon = 0;                                         /* (0.6.48) a DRUM part's IMAGE and SENDs (eng_drum.c) */
    if (e == &ENG_DRUM)
        drum_side_begin(t, n);
    {   /* (2026-11-11) FMINI: FMini-1's glide moves every 12th block (its build's GLIDE_SLOW: 12x slower than Felucca's, which the
         * other engines keep); gl_slow counts the blocks */
        static uint8_t gl_slow[NTRK];
        gl_slow[ti % NTRK] = (uint8_t)(gl_slow[ti % NTRK] >= 11u ? 0u : gl_slow[ti % NTRK] + 1u);
        gl_go = e != &ENG_MINI || gl_slow[ti % NTRK] == 0u;
    }
    if (e->block)                                       /* the engine's per-part work (WHEEL: bars, rotor) */
        e->block(t);
    damt = e == &ENG_DRUM ? 0 : 127 - clamp(p[P_DRIFT], 0, 127);   /* (0.7.8) DRIFT, 127 stored = 0 % */
    if (damt)
        drift_clk[ti]++;
    /* (chord-harp fork: Clipper 0.3's) the mod wheel's destination (P_MWDST; CC1, t->mw) and velocity to the cutoff
     * (P_VELF); VIB and TREM ride a vibrato LFO of their own (~5.5 Hz sine), WAH the sound's LFO. VINTAGE (P_VINT) */
    static uint32_t vib_ph[NTRK];
    int32_t mw = p[P_MWDST] && e != &ENG_HYBRID ? t->mw : 0,   /* (HYBRID: its matrix takes the wheel, Clipper's) */
            vint = e == &ENG_DRUM ? 0 : clamp(p[P_VINT], 0, 127), vib = 0;
    if (vint)
        vint_clk[ti % NTRK]++;
    if (mw && (p[P_MWDST] == MWD_VIB || p[P_MWDST] == MWD_TREM)) {
        vib_ph[ti % NTRK] += 535642u * n;
        vib = osc_sine(vib_ph[ti % NTRK]);
    }
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &t->v[i];
        vmod_t m;
        int32_t env, pitch, dfine, dcut, fsum;
        if (!v->active)
            continue;
        {
            env = env_tick(t, v);
            if (e->ownenv && v->active) {               /* the engine's envelopes (FM6): they end the voice */
                if (e->done(t, v)) {
                    v->active = v->gate = 0;
                    v->stage = 0;
                    v->env = v->env_out = 0;
                    continue;
                }
                if (v->stage != 4u) {                   /* (0.6.74: given up, env_tick's fade) */
                    v->env = 1 << 24;                   /* (the ADSR's release never ends it) */
                    env = 32767;
                }
            }
            if (e->amp)                                 /* the engine's own amplitude curve */
                env = e->amp(t, v, env);
            m.envq15 = e->ownenv ? 0 : env;
            m.amp1 = e->ownenv ? env : mulq15(env, v->vel * 258);
            if (p[P_LD_AMP])
                m.amp1 = mulq15(m.amp1, 32767 - mulq15((lfo + 32768) >> 1, p[P_LD_AMP] * 258));
            if (mw && p[P_MWDST] == MWD_TREM)           /* (Clipper's) TREM: up to 60 % of the level away */
                m.amp1 = mulq15(m.amp1, 32767 - mulq15((vib + 32768) >> 1, mw * 155));
            if (fade)                                   /* linear to 0 over the fade */
                m.amp1 = m.amp1 * (int32_t)(t->xf - 1u) / (int32_t)(t->xf_len ? t->xf_len : XF_BLOCKS);
            else if (t->xin)                            /* (0.6.41) in, linear, after a switch or a dip */
                m.amp1 = m.amp1 * (int32_t)(t->xin_len - t->xin + 1u) / (int32_t)(t->xin_len + 1u);
            if (v->fin_d) {                             /* (0.6.81) a chord glide's new note, in as it glides */
                int32_t r = v->pitch16 - v->pitch_cur;
                r = r < 0 ? -r : r;
                if (r >= (int32_t)v->fin_d) r = v->fin_d - 1;
                m.amp1 = m.amp1 * ((int32_t)v->fin_d - r) / (int32_t)v->fin_d;
                if (!r) v->fin_d = 0;
            }
            m.amp0 = v->env_out;
            v->env_out = m.amp1;
        }
        if (v->pitch_cur != v->pitch16 && gl_go) {      /* glide (FMINI: every 12th block) */
            int32_t d = v->pitch16 - v->pitch_cur;
            int32_t st;
            if (v->gstep) {                             /* TIME: Q8, the fraction carried (0.6.82) */
                int32_t q = v->gacc + v->gstep;
                st = q >> 8;
                v->gacc = (uint8_t)(q & 255);
            } else {
                st = (int32_t)(ENV_LIN[(p[P_GLIDE] ? p[P_GLIDE] : 40) & 127] >> 14);   /* slide: ~45 ms */
                if (st < 1)
                    st = 1;
            }
            v->pitch_cur += d > 0 ? (d < st ? d : st) : (-d < st ? d : -st);
        }
        if (!env && !m.amp0 && v->stage == 2 && !e->sampled)
            continue;                                   /* held at a silent sustain (SUS 0): nothing to render */
        pitch = v->pitch_cur + t->pfx_pit + tune + bend16 + ((lfo * p[P_LD_PIT] * 3) >> 15) + ((m.envq15 * p[P_ED_PIT] * 3) >> 15);
        m.pitch16 = clamp(pitch, 0, 2047);
        m.inc = pitch_inc(m.pitch16);
        dfine = dcut = 0;
        if (damt) {                                     /* (0.7.8) DRIFT: up to ~27 ct, and the filter a little */
            dfine = (drift_noise(ti * 8u + i * 1013u, drift_clk[ti] + i * 397u, 10u) * damt) >> 16;   /* (~0.74 s) */
            dcut = (drift_noise(ti * 8u + i * 1013u + 0x5bd1u, drift_clk[ti] + i * 211u, 11u) * damt) >> 12;   /* (~1.5 s) */
        }
        fsum = v->fine + tune_fine + bend_fine + dfine;
        if (vint || (mw && p[P_MWDST] == MWD_VIB)) {    /* (Clipper's) VINTAGE, VIB (up to +-half a semitone): 1/4096
                                                         * semitone, as fine (1/16 st = 14.8) */
            int32_t q = (mw && p[P_MWDST] == MWD_VIB ? (vib * mw) >> 11 : 0) +
                        (vint ? vintage_pitch(v, ti * NVOICE + i, vint) : 0);
            fsum += (q * 3792) >> 16;
        }
        m.fine = fsum;
        if (fsum)                                       /* residual below 1/16 semitone */
            m.inc += (uint32_t)((int32_t)(m.inc >> 12) * fsum);
        m.cutoff = ((lfo * p[P_LD_FLT]) >> 7) + ((m.envq15 * p[P_ED_FLT]) >> 7) + dcut;
        if (v->vel > 110)                               /* accent opens the filter with the env */
            m.cutoff += (m.envq15 * 24) >> 7;
        m.shape = (64 << 8) + ((lfo * p[P_LD_SHP]) >> 7) + ((m.envq15 * p[P_ED_SHP]) >> 7);
        /* (Clipper's) VEL>F: around velocity 96, up to ~2 octaves up (127) and ~4.5 down (32); the wheel's CUT: up to ~4
         * octaves, WAH: the sound's LFO on the cutoff, up to ~+-2 octaves, SHP: the shape up to its top; VINTAGE: each
         * voice's filter a little off */
        if (p[P_VELF])
            m.cutoff += ((int32_t)v->vel - 96) * p[P_VELF] * 4;
        if (p[P_KTRK])                                  /* (chord-harp fork) KEY>F: 1:1 (100 %) = 14 cutoff steps an octave */
            m.cutoff += ((v->pitch16 - 60 * 16) * p[P_KTRK] * 48) >> 8;
        if (mw) {
            if (p[P_MWDST] == MWD_CUT)
                m.cutoff += mw * 120;
            else if (p[P_MWDST] == MWD_WAH)
                m.cutoff += (lfo * mw) >> 9;
            else if (p[P_MWDST] == MWD_SHP)
                m.shape = clamp(m.shape + mw * 128, 0, 127 << 8);
        }
        if (vint)
            m.cutoff += ((vcard(ti * NVOICE + i, 1u) * 400) >> 15) * vint >> 7;
        if (MOD.on)                                     /* the modulation matrix (mod.c) */
            mod_voice(t, v, &m, fsum);
        e->render(t, v, out, n, &m);
        nr++;
    }
    if (nr && e == &ENG_DRUM)                           /* (0.6.16) DRUM-X: DRV and the knee on the kit's sum */
        drum_bus(t, out, n);
    if (nr && (t->p[P_FTYPE] || t->p[P_FCUT] < 127 || t->p[P_FRES] || tf_on[(uint32_t)(t - trk) % NTRK] ||
               ((e == &ENG_WHEEL || e == &ENG_DRUM) && (t->p[P_LD_FLT] || t->p[P_ED_FLT])) ||   /* (0.6.81: their
                                                         * only filter, for the LFO / ENV -> FILTER) */
               tf_xb[(uint32_t)(t - trk) % NTRK] || tf_cmb[(uint32_t)(t - trk) % NTRK]))
        /* (0.5) the FILTER (track_post); 0.6.8: and a block more to leave the chain (track_filter's fade) */
        track_post(t, e, out, n);
    if (nr && t->p[P_PHAMT] > 0)
        track_phaser(t, out, n);                        /* (chord-harp fork) FX > PHASER */
    if (snap)
        memcpy(t->p, p_new, sizeof p_new);
    else if (fade)
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = pe_new[i];
    if (!fade && t->xin)
        t->xin--;                                       /* (0.6.41) the fade-in's next block */
    if (fade && !--t->xf && snap)
        xf_snap_t = 0xFFu;                              /* (faded out: the snapshot done with) */
    return nr;
}
