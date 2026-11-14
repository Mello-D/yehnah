/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM's MACHINE HORT: the drum models of DEADACTIVE's Hortator (github.com/deadactive/hortator, branch drum-v1.0.1,
 * GPL-3.0, built on Felucca; hm_models.c: 808 / 909 voices, Plaits-derived kicks / snares / metal, a membrane and a 2-op FM
 * (MIT ports), and the measured digital (TRX) and FM (EFM) drum voices of the Machinedrum kind). Each of DRUM's 8 lanes
 * plays ONE model of the kit (ht_model: the power-on TRX kit, or any lane set to any of the NMODELS), at the model's own
 * knob values (edit[].def) plus DRUM's TUNE / DECAY / TONE; the voice state is an instance of a small shared pool
 * (dvoice_t), the lane's handle the LCD-4 one (ld_voice_t: on, trig, rsv = instance + 1). Velocity, LEVEL, IMAGE and SEND
 * are DRUM's. Not taken from Hortator: SAMPL (the sample engine, the Hügelton sample pack) and ACID. */
#define HT_IS(m) ((uint32_t)((m) - DXK_HORT) < HT_NKIT)
#if YEHNAH_HORT
#include "hm_models.c"
#define HT_NINST 10u
#define HT_NKIT 7u
/* the kits (DRUM's presets "HORT ..."): a model a lane (KICK SNARE CLAP HATCL HATOP TOM RIM BELL) at the model's own knobs.
 * Only models hm_models.c switches on may be named (a missing one is a compile error). */
static const uint8_t HT_KITS[HT_NKIT][HM_NL] = {
    {DM_K808, DM_S808, DM_C808, DM_HATC, DM_HATO, DM_TOM, DM_RIM, DM_CYMB},                 /* 808 MODEL */
    {DM_K909, DM_S909, DM_C909, DM_HATC, DM_HATO, DM_TOM, DM_RIM, DM_CYMB},                 /* 909 MODEL */
    {DM_KTRX, DM_SDTRX, DM_C909, DM_HATC, DM_HATO, DM_TOM, DM_RIM, DM_CYMB},                /* DIGITAL KIT: the measured digital kick and snare */
    {DM_BDEFM, DM_SDEFM, DM_CPEFM, DM_HATC, DM_HATO, DM_XTEFM, DM_CLAVE, DM_FM},            /* FM KIT */
    {DM_KPUNC, DM_SSNAP, DM_C808, DM_HATC, DM_HATO, DM_CONGA, DM_RIM, DM_CYMB},             /* PUNCH KIT */
    {DM_KBOOM, DM_S808, DM_C909, DM_HATC, DM_HATO, DM_TOM, DM_CLAVE, DM_CYMB},              /* BIG BOOM */
    {DM_K909, DM_S909, DM_SSNAP, DM_HATC, DM_HATO, DM_TOM, DM_KBOOM, DM_CYMB},              /* BASIC KIT: see HT_ALT */
};
/* BASIC KIT has no clap, rim or cowbell: its lanes play SEVERAL models, the one the note picks (ht_pick, a lane's variant 0 is
 * the kit's model above, 1 and 2 these, +1; 0 none): KICK 36 K909, 35 K808; SNARE 38 S909, 40 S808; the CLAP lane (39) the
 * third snare SNAP; HAT CL 42 HATC, 44 METAL; HAT OP 46 HATO; TOM low XTEFM (the FM tom), mid TOM, high CONGA; the RIM lane
 * (37 ..) a third kick, BOOM; BELL (the cymbals, the cowbell ..) CYMB */
static const uint8_t HT_ALT[HM_NL][2] = {
    {DM_K808 + 1u, 0}, {DM_S808 + 1u, 0}, {0, 0}, {DM_HMETL + 1u, 0}, {0, 0}, {DM_XTEFM + 1u, DM_CONGA + 1u}, {0, 0}, {0, 0},
};
/* a kit's own lane SENDs (dx_snd: bus << 6 | amount; 2 DELAY): the track's delay send reaches every lane, the kick's too (it repeated
 * the kick's first-sample click: the user), so the presets that want an echo send only the snare (and the tom) to it */
static const uint8_t HT_SND[HT_NKIT][HM_NL] = {
    {0}, {0}, {0},
    {0, 138, 138, 0, 0, 0, 0, 0},                       /* FM KIT: the snare and the clap, 10 */
    {0}, {0, 138, 0, 0, 0, 138, 0, 0},                  /* BIG BOOM: the snare and the tom */
    {0},
};
static void ht_sends(uint32_t mach, uint8_t *snd)       /* the kit's lane sends into dx_snd (a machine preset clears them first) */
{
    if ((uint32_t)(mach - DXK_HORT) < HT_NKIT)
        memcpy(snd, HT_SND[mach - DXK_HORT], HM_NL);
}
static uint8_t ht_kit;
/* a model's level, %: a hit at its default knobs peaks from 2k to 77k; these bring them to about 32k (kicks), 30k (snares), 22k
 * (claps), 14k (hats), 28k (toms), 24k (rim, claves, FM), 20k (cymbal): measured on the host (a render of each model, the
 * hats' mean peak of several hits: the metal bank's phase moves them 3x) */
static const uint16_t HT_MG[NMODELS] = {
    [DM_K808] = 104,
    [DM_K909] = 105,
    [DM_KBOOM] = 170,
    [DM_KPUNC] = 126,
    [DM_KTRX] = 68,
    [DM_BDEFM] = 104,
    [DM_S808] = 78,
    [DM_S909] = 113,
    [DM_SSNAP] = 122,
    [DM_SDTRX] = 165,
    [DM_SDEFM] = 1760,
    [DM_C808] = 138,
    [DM_C909] = 133,
    [DM_CPEFM] = 394,
    [DM_HATC] = 450,
    [DM_HATO] = 420,
    [DM_HMETL] = 69,
    [DM_TOM] = 88,
    [DM_CONGA] = 77,
    [DM_XTEFM] = 35,
    [DM_RIM] = 61,
    [DM_CLAVE] = 68,
    [DM_FM] = 47,
    [DM_CYMB] = 253,
};
static uint32_t ht_age;
typedef struct { dvoice_t v; hmt_t t; ld_voice_t *owner; int32_t last, prev; uint8_t on, model, fresh; } ht_inst_t;
static ht_inst_t ht_inst[HT_NINST] __attribute__((section(".pool")));
static uint8_t ht_nogain;                              /* (the host's level measurement) */

/* the variant of the lane's models a note picks (0: the kit's), and what is left of the note's pitch */
static uint32_t ht_pick(uint32_t lane, uint32_t note, int32_t gm, int32_t *rest)
{
    uint32_t v = 0;
    *rest = gm;
    if (ht_kit != HT_NKIT - 1u)
        return 0;
    switch (lane) {
    case DV_KICK: v = note == 35u; break;
    case DV_SNARE: v = note == 40u; break;
    case DV_HATC: v = note == 44u; break;
    case DV_TOM:                                        /* the low, the mid and the high toms: three models */
        v = gm < -3 ? 1u : gm > 3 ? 2u : 0u;
        *rest = gm - (gm < -3 ? -6 : gm > 3 ? 6 : 0);
        break;
    default: break;
    }
    return v;
}
static uint32_t ht_model_of(uint32_t lane, uint32_t var)
{
    uint32_t m = HT_ALT[lane & 7u][var ? var - 1u : 0u];
    return var && ht_kit == HT_NKIT - 1u && m ? m - 1u : HT_KITS[ht_kit][lane & 7u];
}
static void ht_reset(void)
{
    uint32_t i;
    for (i = 0; i < HT_NINST; i++)
        ht_inst[i].on = 0;
}
static void ht_select(uint32_t mach)                    /* the MACHINE is a HORT kit: its models on the lanes */
{
    if ((uint32_t)(mach - DXK_HORT) < HT_NKIT)
        ht_kit = (uint8_t)(mach - DXK_HORT);
}
static void ht_block(void) { dblock++; }                /* the metal bank is made once a block (hm_metal.c) */
static ht_inst_t *ht_get(ld_voice_t *h)
{
    uint32_t i, best = 0;
    if (h->rsv && ht_inst[h->rsv - 1u].owner == h && ht_inst[h->rsv - 1u].on)
        return &ht_inst[h->rsv - 1u];
    for (i = 0; i < HT_NINST; i++)
        if (!ht_inst[i].on) {
            best = i;
            goto got;
        }
    for (i = 1; i < HT_NINST; i++)                      /* the oldest is stolen */
        if (ht_inst[i].v.age < ht_inst[best].v.age)
            best = i;
    if (ht_inst[best].owner) {
        ht_inst[best].owner->on = 0;
        ht_inst[best].owner->rsv = 0;
    }
got:
    ht_inst[best].owner = h;
    h->rsv = (uint8_t)(best + 1u);
    return &ht_inst[best];
}
static void ht_release(ld_voice_t *h)
{
    if (h->rsv && ht_inst[h->rsv - 1u].owner == h)
        ht_inst[h->rsv - 1u].on = 0;
    h->rsv = 0;
    h->on = 0;
}
static void ht_stop(ld_voice_t *h) { ht_release(h); h->trig = 0; }
static void ht_choke(ld_voice_t *h)                     /* the closed hat ends the open one */
{
    if (h->on)
        ht_release(h);
}
static int32_t ht_env256(const ld_voice_t *h)
{
    return h->on && h->rsv ? ht_inst[h->rsv - 1u].v.env[0] >> 16 : 0;
}
/* a hit of lane `lane` now (if trig; var: the variant of its models the note picked), then n (<= CTL) samples into y */
static void ht_run(ld_voice_t *h, uint32_t lane, uint32_t var, int32_t st16, int32_t dofs, int32_t tofs, int32_t *y, uint32_t n)
{
    uint32_t j, l = lane & (HM_NL - 1u);
    ht_inst_t *x;
    const dmodel_t *m;
    for (j = 0; j < n; j++)
        y[j] = 0;
    if (h->trig) {
        hmt_t *t;
        uint32_t k, mi = ht_model_of(l, var);
        x = ht_get(h);
        x->prev = x->on ? x->last : 0;                  /* a hit on a voice still sounding: its last sample joins the new hit */
        t = &x->t;
        m = &DMODELS[mi % NMODELS];
        for (k = 0; k < 8u; k++)
            t->p[k] = m->def[k];
        t->p[1] = (int16_t)clamp(t->p[1] + dofs, 0, 127);   /* DECAY */
        t->p[2] = (int16_t)clamp(t->p[2] + tofs, 0, 127);   /* TONE */
        t->nt = (int8_t)clamp((st16 + (st16 >= 0 ? 8 : -8)) / 16, -24, 24);
        t->lane = (uint8_t)l;
        memset(&x->v, 0, sizeof x->v);
        x->v.active = 1;
        x->v.vel = 127;
        x->v.age = ++ht_age;
        x->v.rng = (int32_t)(x->v.age * 2654435761u) | 1;
        x->model = (uint8_t)mi;
        x->fresh = 1;
        m->trigger(t, &x->v);
        x->owner = h;
        x->on = 1;
        h->trig = 0;
        h->on = 1;
    }
    if (!h->on || !h->rsv)
        return;
    x = &ht_inst[h->rsv - 1u];
    m = &DMODELS[x->model % NMODELS];
    m->render(&x->t, &x->v, y, n);
    if (!ht_nogain) {
        int32_t g = HT_MG[x->model];
        for (j = 0; j < n; j++)
            y[j] = clamp(y[j] * g / 100, -98000, 98000);
    }
    if (x->fresh) {                                     /* the hit's first block ramps in (a step of up to 31k at the first sample: the
                                                         * EFM kick starts at its peak, as the machine it was measured on: a click,
                                                         * "crackle" at every hit; 0.7 ms keeps the thump and drops the step) */
        for (j = 0; j < n; j++)
            y[j] = (y[j] * (int32_t)(j + 1u) + x->prev * (int32_t)(CTL - 1u - j)) / (int32_t)CTL;
        x->fresh = 0;
    }
    x->last = y[n - 1u];
    if (!x->v.active)
        ht_release(h);
}
#else
#define DXK_HORT 255u
#define HT_NKIT 7u
static inline void ht_reset(void) {}
static inline void ht_block(void) {}
static inline void ht_select(uint32_t m) { (void)m; }
static inline void ht_sends(uint32_t m, uint8_t *snd) { (void)m; (void)snd; }
static inline uint32_t ht_pick(uint32_t lane, uint32_t note, int32_t gm, int32_t *rest) { (void)lane; (void)note; *rest = gm; return 0; }
static inline void ht_choke(ld_voice_t *h) { (void)h; }
static inline void ht_stop(ld_voice_t *h) { (void)h; }
static inline int32_t ht_env256(const ld_voice_t *h) { (void)h; return 0; }
static inline void ht_run(ld_voice_t *h, uint32_t lane, uint32_t var, int32_t st16, int32_t dofs, int32_t tofs, int32_t *y, uint32_t n)
{ (void)h; (void)lane; (void)var; (void)st16; (void)dofs; (void)tofs; (void)y; (void)n; }
#endif
