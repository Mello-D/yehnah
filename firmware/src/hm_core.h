/* SPDX-License-Identifier: GPL-3.0-only
 * The drum models of DEADACTIVE's Hortator (github.com/deadactive/hortator, branch drum-v1.0.1, GPL-3.0; built on Felucca by
 * Leo Kuroshita, Hügelton Instruments), as DRUM's MACHINE DXK_HORT (drum_hort.c). The model files (hm_*.c) are Hortator's
 * dm_*.c with track_t -> hmt_t: a lane's knobs. Not taken: SAMPL (its sample engine and the Hügelton sample pack) and ACID
 * (we have ACID).  Taken: the structures of drum_core.c's dvoice_t / dmodel_t (no sample state, no glide / gate). */
#pragma once
#define HM_NL 8u                                     /* DRUM's lanes */
#define HPD(l, f, mn, mx, df) (df)                        /* (params.c PD, which comes later: only the default is kept: no picker yet, no labels) */
#define HPE(l, n, df) (df)
typedef struct { int32_t a1, a2, a3, k; } dsvf_t;    /* hm_dsp.c: SVF with LP / BP / HP */
#include "hm_state.h"
typedef struct {                                     /* one drum voice; the fields' meaning is the model's */
    uint8_t active, vel;
    uint32_t age;
    uint32_t t;
    uint32_t ph[3], inc[3];
    int32_t env[3];
    uint32_t k[3];
    dsvf_t c[2];
    int32_t f[4];
    int32_t x[4];
    int32_t rng;
    int32_t last;                                    /* last sample added to the output */
    dm_state_t ms;
} dvoice_t;
typedef struct {                                     /* what a model reads of its track: the knobs, the note, the lane */
    int16_t p[8];                                    /* the model's knobs: TUNE DECAY TONE CHAR + 4 extras */
    int8_t nt;                                       /* the note's semitones from the lane's pitch */
    uint8_t lane;
    int32_t dtail;                                   /* (SKIN: the declick tail it takes back) */
} hmt_t;
typedef struct {
    const char *name;
    uint8_t voices, choke;
    int16_t def[8];                                  /* the 8 knobs' defaults (Hortator: param_desc_t edit[8] with names and ranges) */
    void (*trigger)(hmt_t *t, dvoice_t *v);
    void (*render)(hmt_t *t, dvoice_t *v, int32_t *out, uint32_t n);
    uint8_t weight;
} dmodel_t;
/* 2^(d16 / 192), Q16 (d16: 1/16 semitones; Hortator: from its pitch table, here the float, as the models only call it at a hit) */
static uint32_t pow2_q16(int32_t d16)
{
    return (uint32_t)(fm_exp2f((float)d16 * (1.0f / 192.0f)) * 65536.0f);
}
static hmt_t vg_trk;                                 /* (hm_metal.c / hm_memb.c: the rendered hit's scratch track: unused here) */
static uint32_t dblock;                              /* blocks rendered (drum_hort.c ht_block): the metal bank is made once a block */
