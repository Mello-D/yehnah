/* SPDX-License-Identifier: GPL-3.0-only
 * LCD-4's drum kits, from LCD-4 (devrvega/LCD-4 9676218, firmware/src/dsp/lcd4_drums.c, GPL-3.0): its four kits of
 * eleven parts, each synthesized the way its machine did it:
 *   808     bridged-T resonators (a sine that rings and dies), the six square oscillators of its cymbal and hats
 *           through band-passes, a clap of noise bursts, the cowbell's two squares
 *   909     a swept kick with a click, triangles and noise for the snare, swept toms, hats and cymbals of metal and
 *           noise (the real ones were 6-bit samples)
 *   ARCADE  a games console's sound chip: pulses and a triangle swept in pitch, a 15-bit noise register (long, or short
 *           and metallic), volume in sixteen steps; blips, zaps and a coin
 *   BOX     a CR-78 and a TR-606: soft kicks, a snare, toms, a conga, claves, the guiro's scrape, metallic hats, the
 *           tambourine and the cowbell
 * Modifications (chord-harp fork): DRUM's MACHINE (eng_drum.c: dx_mach, the section's, with its DRUM-X kit): each of
 * DRUM's 8 lanes plays one part at a time, chosen by the note (TOM: LCD-4's low, mid and high by the GM tom; BELL: the
 * cymbal for the GM cymbals, else the kit's last part); one voice a lane (ld_voice_t, in drum_lane_t's place of the
 * DRUM-X hit); its pitch from the note's GM offset, DRUM's TUNE, the step's components and RANDOM, struck at the hit;
 * DECAY and TONE move every part from LCD-4's values (its per-part knobs: tune and decay as built, its tone and level);
 * the stereo pan is DRUM's IMAGE; the metal bank shared by the clock, no state (ld_metal); velocity, LEVEL and the rest
 * DRUM's. In float (the core's FPU; fastmath.h), as LCD-4. */
#include "fastmath.h"

enum { DXK_DRUMX, DXK_808, DXK_909, DXK_ARC, DXK_BOX,
#if YEHNAH_HORT
       DXK_HORT, DXK_HORT1, DXK_HORT2, DXK_HORT3, DXK_HORT4, DXK_HORT5, DXK_HORT6,   /* (drum_hort.c: HT_NKIT kits) */
#endif
       DXK_N };   /* DRUM's MACHINE: DRUM-X (drumx_voice.c), LCD-4's four */
#define LD_SR 44100.0f
#define LD_NPART 11u
enum { LT_KICK, LT_SNARE, LT_TOM, LT_RIM, LT_CLAP, LT_HAT, LT_CYM, LT_COWBELL, LT_RIDE, LT_CLAVES, LT_GUIRO, LT_TAMB,
       LT_CKICK, LT_CSNARE, LT_CTOM, LT_CHAT, LT_BLIP, LT_ZAP, LT_COIN, LT_CCRASH };
typedef struct {
    uint8_t type;
    float f;                           /* the pitch (Hz) */
    float dec;                         /* the decay, -60 dB (s) */
    float a, b;                        /* the type's own */
} ld_part_t;
static const ld_part_t LD_KIT[4][LD_NPART] YEHNAH_RES = {   /* (chord-harp fork: the resource region, res.c) */
    {   /* 808 */
        {LT_KICK, 50.0f, 0.95f, 1.45f, 0.007f},            /* a: the sweep's start (x), b: its time */
        {LT_SNARE, 180.0f, 0.2f, 330.0f, 0.0f},            /* a: the second resonator, b: the sweep (x) */
        {LT_TOM, 92.0f, 0.5f, 1.25f, 0.06f},
        {LT_TOM, 130.0f, 0.42f, 1.25f, 0.05f},
        {LT_TOM, 180.0f, 0.36f, 1.25f, 0.045f},
        {LT_RIM, 1667.0f, 0.03f, 455.0f, 0.0f},
        {LT_CLAP, 1100.0f, 0.32f, 3.0f, 0.0f},             /* a: the bursts */
        {LT_HAT, 7200.0f, 0.06f, 0.0f, 0.0f},              /* a: noise in the metal (0..1) */
        {LT_HAT, 7200.0f, 0.5f, 0.0f, 0.0f},
        {LT_CYM, 3440.0f, 1.4f, 0.0f, 0.0f},
        {LT_COWBELL, 540.0f, 0.4f, 800.0f, 0.0f},
    },
    {   /* 909 */
        {LT_KICK, 54.0f, 0.55f, 4.2f, 0.022f},
        {LT_SNARE, 190.0f, 0.22f, 345.0f, 1.5f},
        {LT_TOM, 98.0f, 0.38f, 1.9f, 0.09f},
        {LT_TOM, 135.0f, 0.34f, 1.9f, 0.08f},
        {LT_TOM, 190.0f, 0.3f, 1.9f, 0.07f},
        {LT_RIM, 1250.0f, 0.035f, 520.0f, 1.0f},
        {LT_CLAP, 1250.0f, 0.38f, 4.0f, 0.0f},
        {LT_HAT, 8800.0f, 0.07f, 0.55f, 0.0f},
        {LT_HAT, 8800.0f, 0.55f, 0.55f, 0.0f},
        {LT_CYM, 5200.0f, 1.8f, 0.7f, 0.0f},               /* the crash */
        {LT_RIDE, 3200.0f, 1.6f, 0.0f, 0.0f},
    },
    {   /* ARCADE */
        {LT_CKICK, 50.0f, 0.22f, 8.0f, 0.04f},             /* a: the sweep's start (x), b: its time */
        {LT_CSNARE, 210.0f, 0.2f, 0.0f, 0.0f},
        {LT_CTOM, 110.0f, 0.22f, 0.0f, 0.0f},
        {LT_CTOM, 165.0f, 0.2f, 0.0f, 0.0f},
        {LT_CTOM, 245.0f, 0.18f, 0.0f, 0.0f},
        {LT_BLIP, 784.0f, 0.16f, 0.0f, 0.0f},
        {LT_ZAP, 2600.0f, 0.3f, 0.0f, 0.0f},
        {LT_CHAT, 0.0f, 0.045f, 1.0f, 0.0f},               /* a: 1 short (metallic) */
        {LT_CHAT, 0.0f, 0.32f, 0.0f, 0.0f},
        {LT_CCRASH, 0.0f, 0.9f, 0.0f, 0.0f},
        {LT_COIN, 988.0f, 0.4f, 0.0f, 0.0f},
    },
    {   /* BOX: a CR-78 and a TR-606 */
        {LT_KICK, 58.0f, 0.35f, 1.2f, 0.004f},
        {LT_SNARE, 235.0f, 0.16f, 410.0f, 0.0f},
        {LT_TOM, 120.0f, 0.3f, 1.1f, 0.03f},
        {LT_TOM, 175.0f, 0.26f, 1.1f, 0.03f},
        {LT_TOM, 330.0f, 0.18f, 1.05f, 0.01f},             /* the conga */
        {LT_CLAVES, 2500.0f, 0.06f, 0.0f, 0.0f},
        {LT_GUIRO, 2600.0f, 0.2f, 0.0f, 0.0f},
        {LT_HAT, 6800.0f, 0.04f, 0.35f, 0.0f},
        {LT_HAT, 6800.0f, 0.3f, 0.35f, 0.0f},
        {LT_TAMB, 7500.0f, 0.3f, 0.0f, 0.0f},
        {LT_COWBELL, 720.0f, 0.22f, 1080.0f, 0.0f},
    },
};
/* each part's tone and level as LCD-4's kits are born (its tune and decay: 50, as built), 0..100 */
static const uint8_t LD_DEF[4][LD_NPART][2] = {
    {{40, 90}, {50, 72}, {30, 70}, {30, 70}, {30, 70}, {50, 60}, {50, 70}, {50, 56}, {50, 52}, {50, 44}, {50, 52}},
    {{55, 90}, {55, 72}, {30, 70}, {30, 70}, {30, 70}, {50, 60}, {50, 70}, {50, 56}, {50, 52}, {50, 44}, {50, 44}},
    {{60, 82}, {50, 66}, {50, 64}, {50, 64}, {50, 64}, {50, 52}, {50, 52}, {50, 50}, {50, 46}, {50, 42}, {50, 48}},
    {{40, 86}, {45, 66}, {30, 66}, {30, 66}, {50, 64}, {50, 58}, {50, 54}, {50, 52}, {50, 48}, {50, 46}, {50, 50}},
};
/* the kits made as loud as each other (a plain beat at the same level) */
static const float LD_GAIN[4] = {0.83f, 1.21f, 1.97f, 1.46f};
/* the metal: six square oscillators (the 808's 205.3 .. 800 Hz; the 909's and the box's tuned elsewhere), as phase
 * increments of a 32-bit turn */
static const float LD_METAL_F[4][6] = {
    {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f},
    {263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f},
    {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f},
    {317.0f, 441.0f, 513.0f, 610.0f, 763.0f, 1056.0f},
};
static const char *const LD_KIT_NAME[DXK_N] = {[DXK_DRUMX] = "DRUM-X", [DXK_808] = "808", [DXK_909] = "909", [DXK_ARC] = "ARCADE", [DXK_BOX] = "BOX",
#if YEHNAH_HORT
                                                 [DXK_HORT] = "HORT 808", [DXK_HORT1] = "HORT 909", [DXK_HORT2] = "HORT DIGI", [DXK_HORT3] = "HORT FM", [DXK_HORT4] = "HORT PUNCH", [DXK_HORT5] = "HORT BOOM", [DXK_HORT6] = "HORT BASIC",
#endif
};

typedef struct {                       /* a lane's hit (drum_lane_t: in the DRUM-X hit's place) */
    uint8_t on, trig, type, kit;       /* kit 0..3: LCD-4's */
    uint8_t part, shortm, choked, rsv;
    float g;                           /* gain */
    float amp, ad;                     /* the main envelope and its decay a sample */
    float amp2, ad2;                   /* a second (the snare's second head, the cowbell's tail) */
    float ph, inc, inc_end, pd;        /* the oscillator; its pitch sweeps from inc to inc_end, pd a sample */
    float ph2, inc2;
    float nmp, nd;                     /* the noise's envelope */
    float s1[2], c1[4], c2;            /* the filter: its state, coefficients; a one-pole's */
    float lp, hp, tone;
    float nph, ninc, nout;             /* the console's noise, clocked at ninc */
    int32_t t;                         /* samples since the hit */
    int32_t gap, bursts;               /* the clap's bursts, the coin's second note, the blip's */
    uint32_t lfsr, rng;                /* the console's noise register; the white noise */
} ld_voice_t;

/* the clock the metal runs on (samples, the block's start: fx.c dx_mot_block): every lane's metal in phase, no state */
static uint32_t ld_clock, ld_t0;

#define LD_NI static __attribute__((noinline))

static inline float ld_white(ld_voice_t *v)
{
    v->rng = v->rng * 1664525u + 1013904223u;
    return (float)(int32_t)v->rng * (1.0f / 2147483648.0f);
}
LD_NI float ld_sine(float ph) { return fm_sin_turns(ph); }   /* (out of line: ld_run calls it from ten places) */
static inline float ld_sat(float x) { return x > 1.5f ? 1.0f : x < -1.5f ? -1.0f : x - 0.148148148f * x * x * x; }
static inline float ld_sq(float ph) { return ph < 0.5f ? 1.0f : -1.0f; }
static inline float ld_step4(float a) { return (float)(int32_t)(a * 15.0f + 0.5f) * (1.0f / 15.0f); }   /* 4-bit volume */
static inline float ld_tri32(float ph)            /* the console's triangle: 32 steps */
{
    int32_t k = (int32_t)(ph * 32.0f) & 31;
    return (float)(k < 16 ? k : 31 - k) * (2.0f / 15.0f) - 1.0f;
}
static inline float ld_lfsr(ld_voice_t *v)
{
    uint32_t b = (v->lfsr ^ (v->lfsr >> (v->shortm ? 6 : 1))) & 1u;
    v->lfsr = (v->lfsr >> 1) | (b << 14);
    return (v->lfsr & 1u) ? 1.0f : -1.0f;
}
LD_NI float ld_ex(float x) { return fm_expf(x); }
static float ld_t60(float secs) { return ld_ex(-6.9f / (fm_maxf(secs, 0.0005f) * LD_SR)); }   /* -60 dB in secs */
static float ld_onepole(float f) { return 1.0f - ld_ex(-FM_TWO_PI * f * (1.0f / LD_SR)); }
static float ld_fall(float secs) { return ld_ex(-1.0f / (secs * LD_SR)); }
LD_NI void ld_svfc(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 20.0f, 19000.0f) * (1.0f / LD_SR));
    k = fm_maxf(k, 0.05f);
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}
LD_NI float ld_svf(float *s, const float *c, float x, float *bp)   /* the low-pass out, the band-pass in *bp */
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    *bp = v1;
    return v2;
}

/* the part of kit k (0..3) lane l plays for a note of DRUM_GM's st semitones from the lane's pitch (eng_drum.c), and what
 * is left of st once the part is picked (*rest) */
static uint32_t ld_part_of(uint32_t k, uint32_t lane, uint32_t note, int32_t st, int32_t *rest)
{
    static const uint8_t P[8] = {0, 1, 6, 7, 8, 3, 5, 10};   /* KICK SNARE CLAP HATCL HATOP TOM RIM BELL */
    uint32_t p = P[lane & 7u];
    *rest = st;
    if (lane == DV_TOM) {                        /* the three toms: the GM low, mid and high ones */
        p = st < -3 ? 2u : st > 3 ? 4u : 3u;
        *rest = st - (p == 2u ? -6 : p == 4u ? 6 : 0);
    } else if (lane == DV_BELL && (note == 49u || note == 52u || note == 55u || note == 57u ||
                                   ((note == 51u || note == 53u || note == 59u) && k != 1u))) {
        p = 9u;                                  /* the crashes (and the rides but the 909's, which has one): its cymbal */
        *rest = 0;
    } else if (lane == DV_BELL) {
        *rest = 0;                               /* (a ride, a cowbell, a coin: as built) */
    }
    return p;
}

/* a hit of part p of kit k (0..3): its pitch st16 1/16 semitones from the part's, DECAY dofs and TONE tofs (-64..63)
 * from LCD-4's as built */
LD_NI void ld_fire(ld_voice_t *v, uint32_t k, uint32_t p, int32_t st16, int32_t dofs, int32_t tofs)
{
    if (!RES_OK()) {                               /* (chord-harp fork) the kits in the resource region (res.c): silent */
        v->on = 0;
        return;
    }
    const ld_part_t *d = &LD_KIT[k & 3u][p % LD_NPART];
    float tu = fm_exp2f((float)clamp(st16, -24 * 16, 24 * 16) * (1.0f / 192.0f));
    float dm = fm_exp2f((float)clamp(dofs, -64, 63) * (1.0f / 28.0f));
    float tone = fm_clampf((float)LD_DEF[k & 3u][p % LD_NPART][0] * 0.01f + (float)tofs * (1.0f / 128.0f), 0.0f, 1.0f);
    float lvl = (float)LD_DEF[k & 3u][p % LD_NPART][1] * 0.01f, f = d->f * tu, dec = d->dec * dm;
    uint32_t seed = v->rng;
    memset(v, 0, sizeof *v);
    v->rng = seed ^ 0x2545F491u;
    v->on = 1;
    v->type = d->type;
    v->kit = (uint8_t)(k & 3u);
    v->part = (uint8_t)p;
    v->g = lvl * lvl * LD_GAIN[k & 3u];
    v->tone = tone;
    v->amp = 1.0f;
    v->ad = ld_t60(dec);
    switch (d->type) {
    case LT_KICK:                                  /* the sine, swept a little (808) or a lot (909); the click */
        v->inc_end = f * (1.0f / LD_SR);
        v->inc = v->inc_end * d->a;
        v->pd = ld_fall(d->b);
        v->nmp = 0.25f + tone * 1.1f;
        v->nd = ld_t60(0.006f);
        break;
    case LT_SNARE:                                 /* two heads and the snares: tone is how snappy */
        v->inc_end = f * (1.0f / LD_SR);
        v->inc = v->inc_end * (d->b > 0.0f ? d->b : 1.0f);
        v->pd = ld_fall(0.012f);
        v->inc2 = d->a * tu * (1.0f / LD_SR);
        v->ad = ld_t60(0.16f * dm);
        v->amp2 = 0.7f;
        v->ad2 = ld_t60(0.09f * dm);
        v->nmp = 0.35f + tone * 1.0f;
        v->nd = ld_t60(dec);
        v->c1[0] = ld_onepole(k == 1u ? 900.0f : 1600.0f);   /* the noise's band: a high-pass ... */
        v->c2 = ld_onepole(9000.0f);                         /* ... after a low-pass */
        break;
    case LT_TOM:
        v->inc_end = f * (1.0f / LD_SR);
        v->inc = v->inc_end * d->a;
        v->pd = ld_fall(d->b);
        v->nmp = 0.08f + tone * 0.3f;
        v->nd = ld_t60(0.03f);
        ld_svfc(v->c1, f * 4.0f, 1.0f);
        break;
    case LT_RIM:                                   /* two short rings and a click */
        v->inc = f * (1.0f / LD_SR);
        v->inc2 = d->a * tu * (1.0f / LD_SR);
        v->ad2 = ld_t60(dec * 0.7f);
        v->amp2 = 0.8f;
        v->nmp = 0.6f + tone * 0.6f;
        v->nd = ld_t60(0.004f);
        ld_svfc(v->c1, 3000.0f, 1.0f);
        break;
    case LT_CLAP:                                  /* noise through a band-pass, in bursts, then the room */
        v->bursts = (int32_t)d->a;
        v->gap = (int32_t)(0.0105f * LD_SR);
        v->nd = ld_t60(0.012f);
        ld_svfc(v->c1, f * (0.8f + 0.5f * tone), 0.9f);
        break;
    case LT_HAT:
    case LT_CYM:
    case LT_RIDE:                                  /* the metal (and noise), band-passed, high-passed */
        ld_svfc(v->c1, f * (0.75f + 0.5f * tone), d->type == LT_CYM ? 1.1f : 0.85f);
        v->c2 = ld_onepole(d->type == LT_RIDE ? 2600.0f : 5200.0f * (0.8f + 0.4f * tone));
        v->nmp = d->a;
        v->inc = 3150.0f * tu * (1.0f / LD_SR);    /* the ride's bell */
        v->amp2 = d->type == LT_RIDE ? 0.5f + tone * 0.5f : 0.0f;
        v->ad2 = ld_t60(0.6f * dm);
        break;
    case LT_COWBELL:                               /* two squares, a band-pass; a quick fall, then a tail */
        v->inc = f * (1.0f / LD_SR);
        v->inc2 = d->a * tu * (1.0f / LD_SR);
        v->amp2 = 1.0f;
        v->ad2 = ld_t60(0.035f);
        ld_svfc(v->c1, (f + d->a * tu) * 1.1f * (0.8f + 0.4f * tone), 0.7f);
        break;
    case LT_CLAVES:
        v->inc = f * (1.0f / LD_SR);
        break;
    case LT_GUIRO:                                 /* noise, band-passed, scraped by the teeth */
        ld_svfc(v->c1, f * (0.7f + 0.6f * tone), 0.6f);
        v->inc = 42.0f * (1.0f / LD_SR);
        break;
    case LT_TAMB:
        ld_svfc(v->c1, f, 0.7f);
        v->inc = 16.0f * (1.0f / LD_SR);           /* the shake */
        break;
    case LT_CKICK:
        v->inc_end = f * (1.0f / LD_SR);
        v->inc = v->inc_end * d->a;
        v->pd = ld_fall(d->b);
        break;
    case LT_CSNARE:
        v->inc = f * (1.0f / LD_SR) * 1.6f;
        v->inc_end = f * (1.0f / LD_SR) * 0.7f;
        v->pd = ld_fall(0.03f);
        v->lfsr = 0x4A5Bu;
        v->ninc = (4000.0f + 9000.0f * tone) * (1.0f / LD_SR);
        break;
    case LT_CTOM:
        v->inc = f * 1.6f * (1.0f / LD_SR);
        v->inc_end = f * 0.55f * (1.0f / LD_SR);
        v->pd = ld_fall(0.06f);
        break;
    case LT_CHAT:
    case LT_CCRASH:
        v->lfsr = 0x1u + p;
        v->shortm = d->type == LT_CHAT && d->a > 0.5f;
        v->ninc = fm_minf((d->type == LT_CCRASH ? 2500.0f + 6000.0f * tone : 9000.0f + 15000.0f * tone) * tu *
                          (1.0f / LD_SR), 1.0f);
        break;
    case LT_BLIP:
        v->inc = f * (1.0f / LD_SR);
        v->gap = (int32_t)(0.04f * LD_SR);
        break;
    case LT_ZAP:
        v->inc = f * (1.0f / LD_SR);
        v->inc_end = 70.0f * (1.0f / LD_SR);
        v->pd = ld_fall(0.03f + 0.09f * tone);
        break;
    case LT_COIN:
        v->inc = f * (1.0f / LD_SR);
        v->inc2 = f * 1.3348f * (1.0f / LD_SR);   /* a fourth up */
        v->gap = (int32_t)(0.07f * LD_SR);
        break;
    default: break;
    }
}

/* the metal of kit k at sample j of this block: the six squares from the clock (their phases a 32-bit turn) */
static inline float ld_metal(const uint32_t *inc, uint32_t j)
{
    uint32_t t = ld_t0 + j, i;
    int32_t s = 0;
    for (i = 0; i < 6u; i++)
        s += (int32_t)((inc[i] * t) >> 31);              /* (1 in the second half of the turn) */
    return (float)(3 - s) * (1.0f / 3.0f);               /* (six signs, +-1 each: (6 - 2 s) / 6) */
}
LD_NI void ld_metal_incs(uint32_t k, uint32_t *inc)
{
    uint32_t i;
    for (i = 0; i < 6u; i++)
        inc[i] = (uint32_t)(LD_METAL_F[k & 3u][i] * (4294967296.0f / LD_SR));
}

/* a choke: the closed hat on the open one (its envelope falls at once) */
static void ld_choke(ld_voice_t *v) { if (v->on) v->choked = 1; }
static int ld_live(const ld_voice_t *v) { return v->on || v->trig; }
static int32_t ld_env256(const ld_voice_t *v) { return v->on ? (int32_t)(fm_clampf(v->amp, 0.0f, 1.0f) * 256.0f) : 0; }

/* the hit's n samples into y (Q15, as dx_run's) */
LD_NI void ld_run(ld_voice_t *v, int32_t *y, uint32_t n)
{
    uint32_t j, inc_m[6];
    float a = v->amp, out[CTL], bp, s;
    if (!v->on) {
        for (j = 0; j < n; j++)
            y[j] = 0;
        return;
    }
    switch (v->type) {
    case LT_KICK: {
        float inc = v->inc, ie = v->inc_end, pd = v->pd, ph = v->ph, nm = v->nmp, drive = v->kit == 1u ? 1.8f : 1.1f;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            s = ld_sine(ph) * a;
            if (nm > 0.001f) {                     /* the click: a pulse and a breath of noise */
                s += nm * (0.6f + 0.4f * ld_white(v));
                nm *= v->nd;
            }
            out[j] = ld_sat(s * drive) * (1.0f / drive) * 1.25f;
            a *= v->ad;
        }
        v->inc = inc;
        v->ph = ph;
        v->nmp = nm;
        break;
    }
    case LT_SNARE: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, ph2 = v->ph2, a2 = v->amp2, nm = v->nmp, lp = v->lp, hl = v->hp;
        float ch = v->c1[0], cl = v->c2;
        for (j = 0; j < n; j++) {
            float nz = ld_white(v), hp;
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            if (v->kit == 1u)                       /* the 909's heads are triangles */
                s = (fm_fabsf(ph * 4.0f - 2.0f) - 1.0f) * a + (fm_fabsf(ph2 * 4.0f - 2.0f) - 1.0f) * a2;
            else
                s = ld_sine(ph) * a + ld_sine(ph2) * a2;
            lp += cl * (nz - lp);                   /* the snares: low-passed, then the lows out */
            hl += ch * (lp - hl);
            hp = (lp - hl) * 1.4f;
            out[j] = s * 0.8f + hp * nm;
            a *= v->ad;
            a2 *= v->ad2;
            nm *= v->nd;
        }
        v->inc = inc;
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        v->nmp = nm;
        v->lp = lp;
        v->hp = hl;
        break;
    }
    case LT_TOM: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, nm = v->nmp;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            s = ld_sine(ph) * a;
            if (nm > 0.001f) {
                s += ld_svf(v->s1, v->c1, ld_white(v), &bp) * nm;
                nm *= v->nd;
            }
            out[j] = s;
            a *= v->ad;
        }
        v->inc = inc;
        v->ph = ph;
        v->nmp = nm;
        break;
    }
    case LT_RIM: {
        float ph = v->ph, ph2 = v->ph2, a2 = v->amp2, nm = v->nmp;
        for (j = 0; j < n; j++) {
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            s = ld_sine(ph) * a + ld_sine(ph2) * a2 * 0.6f;
            if (nm > 0.001f) {
                ld_svf(v->s1, v->c1, ld_white(v), &bp);
                s += bp * nm;
                nm *= v->nd;
            }
            out[j] = ld_sat(s * 1.5f) * 0.8f;
            a *= v->ad;
            a2 *= v->ad2;
        }
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        v->nmp = nm;
        break;
    }
    case LT_CLAP: {
        int32_t t = v->t;
        float e = v->nmp;
        for (j = 0; j < n; j++, t++) {
            int32_t burst = t / v->gap;
            if (burst < v->bursts) {
                if (t % v->gap == 0)
                    e = 1.0f;
                e *= v->nd;
                s = e;
            } else {
                if (burst == v->bursts && t % v->gap == 0)
                    e = 0.0f;
                s = a * 0.75f;
                a *= v->ad;
            }
            ld_svf(v->s1, v->c1, ld_white(v), &bp);
            out[j] = s * bp * 2.2f;
        }
        v->nmp = e;
        break;
    }
    case LT_HAT:
    case LT_CYM:
    case LT_RIDE: {
        float nmix = v->nmp, ph = v->ph, a2 = v->amp2, hl = v->hp, k2 = v->c2;
        ld_metal_incs(v->kit, inc_m);
        for (j = 0; j < n; j++) {
            float x = ld_metal(inc_m, j) * (1.0f - nmix) + ld_white(v) * nmix;
            ld_svf(v->s1, v->c1, x, &bp);
            hl += k2 * (bp - hl);                   /* the band, high-passed */
            s = (bp - hl) * a * 1.9f;
            if (a2 > 0.001f) {                      /* the ride's bell */
                ph += v->inc;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                s += (ld_sine(ph) + 0.6f * ld_sine(ph * 2.0f + 0.13f)) * a2 * 0.25f;
                a2 *= v->ad2;
            }
            a *= v->ad;
            if (v->choked) {
                a *= 0.995f;
                a2 *= 0.995f;
            }
            out[j] = s;
        }
        v->ph = ph;
        v->amp2 = a2;
        v->hp = hl;
        break;
    }
    case LT_COWBELL: {
        float ph = v->ph, ph2 = v->ph2, a2 = v->amp2;
        for (j = 0; j < n; j++) {
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            ld_svf(v->s1, v->c1, (ld_sq(ph) + ld_sq(ph2)) * 0.5f, &bp);
            out[j] = bp * (0.35f * a + 0.65f * a2) * 1.5f;
            a *= v->ad;
            a2 *= v->ad2;
        }
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        break;
    }
    case LT_CLAVES: {
        float ph = v->ph;
        for (j = 0; j < n; j++) {
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            out[j] = ld_sine(ph) * a;
            a *= v->ad;
        }
        v->ph = ph;
        break;
    }
    case LT_GUIRO: {
        float ph = v->ph, tooth;
        for (j = 0; j < n; j++) {
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            tooth = ph < 0.35f ? 1.0f - ph * 2.5f : 0.1f;
            ld_svf(v->s1, v->c1, ld_white(v), &bp);
            out[j] = bp * tooth * a * 1.8f;
            a *= v->ad;
        }
        v->ph = ph;
        break;
    }
    case LT_TAMB: {
        float ph = v->ph;
        ld_metal_incs(v->kit, inc_m);
        for (j = 0; j < n; j++) {
            float x = ld_metal(inc_m, j) * 0.5f + ld_white(v) * 0.5f, lp, shake;
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            shake = 0.6f + 0.4f * ld_sine(ph);
            lp = ld_svf(v->s1, v->c1, x, &bp);
            out[j] = ((x - v->c1[3] * bp - lp) * 0.6f + bp) * a * shake;   /* (the high-pass and the band) */
            a *= v->ad;
        }
        v->ph = ph;
        break;
    }
    case LT_CKICK:
    case LT_CTOM: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph;
        int tri = v->type == LT_CTOM || v->tone < 0.5f;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            out[j] = (tri ? ld_tri32(ph) : (ph < 0.5f ? 0.8f : -0.8f)) * ld_step4(a);
            a *= v->ad;
        }
        v->inc = inc;
        v->ph = ph;
        break;
    }
    case LT_CSNARE: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, no = v->nout, nph = v->nph;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            nph += v->ninc;
            while (nph >= 1.0f) {
                nph -= 1.0f;
                no = ld_lfsr(v);
            }
            out[j] = (no * 0.75f + (ph < 0.5f ? 0.35f : -0.35f) * (a > 0.3f ? 1.0f : 0.0f)) * ld_step4(a);
            a *= v->ad;
        }
        v->inc = inc;
        v->ph = ph;
        v->nout = no;
        v->nph = nph;
        break;
    }
    case LT_CHAT:
    case LT_CCRASH: {
        float no = v->nout, nph = v->nph;
        for (j = 0; j < n; j++) {
            nph += v->ninc;
            while (nph >= 1.0f) {
                nph -= 1.0f;
                no = ld_lfsr(v);
            }
            s = no * ld_step4(a) * 0.6f;
            v->hp += 0.3f * (s - v->hp);           /* the hiss, high-passed a little */
            out[j] = s - v->hp;
            a *= v->ad;
            if (v->choked)
                a *= 0.995f;
        }
        v->nout = no;
        v->nph = nph;
        break;
    }
    case LT_BLIP:
    case LT_COIN: {
        float ph = v->ph;
        int32_t t = v->t;
        for (j = 0; j < n; j++, t++) {
            float inc = t < v->gap ? v->inc : (v->type == LT_COIN ? v->inc2 : v->inc * 2.0f);
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            out[j] = (ph < 0.5f ? 0.5f : -0.5f) * ld_step4(a);
            if (v->type == LT_COIN && t < v->gap)
                a = 1.0f;
            else
                a *= v->ad;
        }
        v->ph = ph;
        break;
    }
    case LT_ZAP: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            out[j] = (ph < 0.25f ? 0.6f : -0.2f) * ld_step4(a);
            a *= v->ad;
        }
        v->inc = inc;
        v->ph = ph;
        break;
    }
    default:
        a = 0.0f;
        for (j = 0; j < n; j++)
            out[j] = 0.0f;
        break;
    }
    {
        float g = v->g * 26000.0f;                 /* (Q15 as DRUM-X's hits; a beat at DRUM-X's level) */
        for (j = 0; j < n; j++)
            y[j] = (int32_t)fm_clampf(out[j] * g, -98000.0f, 98000.0f);
    }
    v->amp = fm_flush(a);
    v->t += (int32_t)n;
    if (v->amp < 0.001f && v->amp2 < 0.001f && ((v->type != LT_SNARE && v->type != LT_RIM) || v->nmp < 0.001f) &&
        (v->type != LT_CLAP || v->t > v->gap * (v->bursts + 1)))
        v->on = 0;
}
