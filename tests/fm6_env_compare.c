/* SPDX-License-Identifier: GPL-3.0-only
 * Differential test against cpu1's original amplitude envelope, including
 * long static stages, zero-length holds, release and retrigger mid-stage. */
#define main hostsim_main
#include "hostsim.c"
#undef main
typedef struct {                 /* msfa Env: level in Q24 log2 (2^24 = 6 dB), 16 more bits than the original */
    int32_t level, target, inc, statc;
    int16_t outlevel;            /* microsteps (99 * 32 = full) */
    uint8_t rscale, ix, rising, down;
} ref_env_t;

static const uint8_t REF_LEVELLUT[20] = {0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46};
static int32_t ref_scaleout(int32_t ol) { return ol >= 20 ? 28 + ol : REF_LEVELLUT[ol < 0 ? 0 : ol]; }

/* samples at 44.1 kHz a stage holds when its level does not move (msfa ACCURATE_ENVELOPE) */
static const int32_t REF_STATICS[77] = {
    1764000, 1764000, 1411200, 1411200, 1190700, 1014300, 992250, 882000, 705600, 705600, 584325, 507150, 502740,
    441000, 418950, 352800, 308700, 286650, 253575, 220500, 220500, 176400, 145530, 145530, 125685, 110250, 110250,
    88200, 88200, 74970, 61740, 61740, 55125, 48510, 44100, 37485, 31311, 30870, 27562, 27562, 22050, 18522, 17640,
    15435, 14112, 13230, 11025, 9261, 9261, 7717, 6615, 6615, 5512, 5512, 4410, 3969, 3969, 3439, 2866, 2690, 2249,
    1984, 1896, 1808, 1411, 1367, 1234, 1146, 926, 837, 837, 705, 573, 573, 529, 441, 441};

/* stage ix (0..3; 4 = done) of an envelope whose rates and levels are op[FP_R1..], op[FP_L1..] */
static void ref_env_advance(ref_env_t *e, const uint8_t *op, uint32_t ix)
{
    int32_t lv, actual, qrate;
    e->ix = (uint8_t)ix;
    if (ix >= 4u)
        return;
    lv = op[FP_L1 + ix];
    actual = ref_scaleout(lv) >> 1;
    actual = (actual << 6) + e->outlevel - 4256;
    actual = actual < 16 ? 16 : actual;
    e->target = actual << 16;
    e->rising = e->target > e->level;
    qrate = ((op[FP_R1 + ix] * 41) >> 6) + e->rscale;
    qrate = qrate > 63 ? 63 : qrate;
    if (e->target == e->level || (ix == 0u && lv == 0)) {
        int32_t sr = op[FP_R1 + ix] + e->rscale;
        sr = sr > 99 ? 99 : sr;
        e->statc = sr < 77 ? REF_STATICS[sr] : 20 * (99 - sr);
        if (sr < 77 && ix == 0u && lv == 0)
            e->statc /= 20;                     /* the attack is scaled faster (a stage change, not per sample) */
    } else {
        e->statc = 0;
    }
    e->inc = (4 + (qrate & 3)) << (2 + FM6_LG_N + (qrate >> 2));
}

static void ref_env_init(ref_env_t *e, const uint8_t *op, int32_t outlevel, int32_t rate_scaling)
{
    e->outlevel = (int16_t)outlevel;
    e->rscale = (uint8_t)rate_scaling;
    e->level = 0;
    e->down = 1;
    ref_env_advance(e, op, 0);
}

static int32_t ref_env_get(ref_env_t *e, const uint8_t *op)
{
    if (e->statc) {
        e->statc -= FM6_N;
        if (e->statc <= 0) {
            e->statc = 0;
            ref_env_advance(e, op, e->ix + 1u);
        }
    }
    if (e->ix < 3u || (e->ix < 4u && !e->down)) {
        if (e->statc) {
            ;
        } else if (e->rising) {
            if (e->level < (1716 << 16))
                e->level = 1716 << 16;
            e->level += (((17 << 24) - e->level) >> 24) * e->inc;
            if (e->level >= e->target) {
                e->level = e->target;
                ref_env_advance(e, op, e->ix + 1u);
            }
        } else {
            e->level -= e->inc;
            if (e->level <= e->target) {
                e->level = e->target;
                ref_env_advance(e, op, e->ix + 1u);
            }
        }
    }
    return e->level;
}

static void ref_env_key(ref_env_t *e, const uint8_t *op, int down)
{
    if (e->down != (uint8_t)down) {
        e->down = (uint8_t)down;
        ref_env_advance(e, op, down ? 0u : 3u);
    }
}


int main(void)
{
    uint32_t rng=12345, trial, block, differences=0;
    for (trial=0; trial<4096; trial++) {
        uint8_t op[FP_OP]={0}; fm6_env_t e={0}; ref_env_t r={0}; uint32_t j;
        for(j=0;j<8;j++) { rng=rng*1664525u+1013904223u; op[j]=(rng>>16)%100; }
        /* Equal targets, all rates, zero attack and sr=99 (zero hold). */
        if (!(trial&3)) for(j=4;j<8;j++) op[j]=(trial&4)?0:99;
        for(j=0;j<4;j++) if (!(trial&7)) op[j]=trial%100;
        fm6_env_init(&e,op,3200,trial%57); ref_env_init(&r,op,3200,trial%57);
        for(block=0;block<4096;block++) {
            if (!(block%257)) { fm6_env_key(&e,op,(block/257)&1); ref_env_key(&r,op,(block/257)&1); }
            if (fm6_env_get(&e,op)!=ref_env_get(&r,op) || e.target!=r.target || e.ix!=r.ix || e.rising!=r.rising || e.down!=r.down || (e.step<0 ? -e.step : 0)!=r.statc) differences++;
        }
    }
    for (trial=0; trial<256; trial++) {
        uint32_t expected=0, k;
        for(k=0;k<6;k++) expected |= ((FM6_ALG[trial&31][k]&3u)==0)<<k;
        if (fm6_carriers(trial)!=expected) differences++;
    }
    printf("FM6 envelope: 16777216 blocks, differences=%u, sizeof=%zu (was %zu), notes saved=%zu bytes\n",differences,sizeof(fm6_env_t),sizeof(ref_env_t),NTRK*FM6_POLY*6*(sizeof(ref_env_t)-sizeof(fm6_env_t)));
    return differences!=0;
}
