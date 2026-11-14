/* SPDX-License-Identifier: GPL-3.0-only
 * Hortator's model table (dmodels.c of github.com/deadactive/hortator, GPL-3.0), without SAMPL and ACID. Each model has a
 * switch HM_M_<NAME> (0 / 1; -D to change): only the switched-on ones are in the table, and the code of the others goes (static
 * functions nothing refers to). The app slot's room (2026-11-14: 27.8 KB for all of HORT) decides the defaults; the kits in
 * drum_hort.c may name only models that are on. */
#include "hm_core.h"
#include "hm_dsp.c"
#ifndef HM_M_K808
#define HM_M_K808 1
#endif
#ifndef HM_M_K909
#define HM_M_K909 1
#endif
#ifndef HM_M_KBOOM
#define HM_M_KBOOM 1
#endif
#ifndef HM_M_KPUNC
#define HM_M_KPUNC 1
#endif
#ifndef HM_M_S808
#define HM_M_S808 1
#endif
#ifndef HM_M_S909
#define HM_M_S909 1
#endif
#ifndef HM_M_C808
#define HM_M_C808 1
#endif
#ifndef HM_M_C909
#define HM_M_C909 1
#endif
#ifndef HM_M_SSNAP
#define HM_M_SSNAP 1
#endif
#ifndef HM_M_SCRAK
#define HM_M_SCRAK 0
#endif
#ifndef HM_M_HATC
#define HM_M_HATC 1
#endif
#ifndef HM_M_HATO
#define HM_M_HATO 1
#endif
#ifndef HM_M_CYMB
#define HM_M_CYMB 1
#endif
#ifndef HM_M_COWB
#define HM_M_COWB 0
#endif
#ifndef HM_M_HMETL
#define HM_M_HMETL 1
#endif
#ifndef HM_M_HNOIS
#define HM_M_HNOIS 0
#endif
#ifndef HM_M_TOM
#define HM_M_TOM 1
#endif
#ifndef HM_M_CONGA
#define HM_M_CONGA 1
#endif
#ifndef HM_M_RIM
#define HM_M_RIM 1
#endif
#ifndef HM_M_CLAVE
#define HM_M_CLAVE 1
#endif
#ifndef HM_M_MEMB
#define HM_M_MEMB 0
#endif
#ifndef HM_M_FM
#define HM_M_FM 1
#endif
#ifndef HM_M_KTRX
#define HM_M_KTRX 1
#endif
#ifndef HM_M_SDTRX
#define HM_M_SDTRX 1
#endif
#ifndef HM_M_XTTRX
#define HM_M_XTTRX 0
#endif
#ifndef HM_M_XCTRX
#define HM_M_XCTRX 0
#endif
#ifndef HM_M_CPTRX
#define HM_M_CPTRX 0
#endif
#ifndef HM_M_MATRX
#define HM_M_MATRX 0
#endif
#ifndef HM_M_RSTRX
#define HM_M_RSTRX 0
#endif
#ifndef HM_M_CBTRX
#define HM_M_CBTRX 0
#endif
#ifndef HM_M_CHTRX
#define HM_M_CHTRX 0
#endif
#ifndef HM_M_OHTRX
#define HM_M_OHTRX 0
#endif
#ifndef HM_M_CYTRX
#define HM_M_CYTRX 0
#endif
#ifndef HM_M_CLTRX
#define HM_M_CLTRX 0
#endif
#ifndef HM_M_B2TRX
#define HM_M_B2TRX 0
#endif
#ifndef HM_M_BDEFM
#define HM_M_BDEFM 1
#endif
#ifndef HM_M_XTEFM
#define HM_M_XTEFM 1
#endif
#ifndef HM_M_SDEFM
#define HM_M_SDEFM 1
#endif
#ifndef HM_M_CPEFM
#define HM_M_CPEFM 1
#endif
#ifndef HM_M_RSEFM
#define HM_M_RSEFM 0
#endif
#ifndef HM_M_CBEFM
#define HM_M_CBEFM 0
#endif
#ifndef HM_M_HHEFM
#define HM_M_HHEFM 0
#endif
#ifndef HM_M_CYEFM
#define HM_M_CYEFM 0
#endif
#if HM_M_MEMB
#include "hm_phys.c"
#include "hm_memb.c"
#endif
#include "hm_kick.c"
#include "hm_snare.c"
#include "hm_metal.c"
#include "hm_perc.c"
#include "hm_fm.c"
#include "hm_ktrx.c"
#include "hm_sdtrx.c"
#include "hm_xttrx.c"
#include "hm_trxn.c"
#include "hm_rstrx.c"
#include "hm_trxm.c"
#include "hm_trxcb.c"
#include "hm_cltrx.c"
#include "hm_b2trx.c"
#include "hm_bdefm.c"
#include "hm_efmn.c"
#include "hm_efmm.c"
enum {
#if HM_M_K808
    DM_K808,
#endif
#if HM_M_K909
    DM_K909,
#endif
#if HM_M_KBOOM
    DM_KBOOM,
#endif
#if HM_M_KPUNC
    DM_KPUNC,
#endif
#if HM_M_S808
    DM_S808,
#endif
#if HM_M_S909
    DM_S909,
#endif
#if HM_M_C808
    DM_C808,
#endif
#if HM_M_C909
    DM_C909,
#endif
#if HM_M_SSNAP
    DM_SSNAP,
#endif
#if HM_M_SCRAK
    DM_SCRAK,
#endif
#if HM_M_HATC
    DM_HATC,
#endif
#if HM_M_HATO
    DM_HATO,
#endif
#if HM_M_CYMB
    DM_CYMB,
#endif
#if HM_M_COWB
    DM_COWB,
#endif
#if HM_M_HMETL
    DM_HMETL,
#endif
#if HM_M_HNOIS
    DM_HNOIS,
#endif
#if HM_M_TOM
    DM_TOM,
#endif
#if HM_M_CONGA
    DM_CONGA,
#endif
#if HM_M_RIM
    DM_RIM,
#endif
#if HM_M_CLAVE
    DM_CLAVE,
#endif
#if HM_M_MEMB
    DM_MEMB,
#endif
#if HM_M_FM
    DM_FM,
#endif
#if HM_M_KTRX
    DM_KTRX,
#endif
#if HM_M_SDTRX
    DM_SDTRX,
#endif
#if HM_M_XTTRX
    DM_XTTRX,
#endif
#if HM_M_XCTRX
    DM_XCTRX,
#endif
#if HM_M_CPTRX
    DM_CPTRX,
#endif
#if HM_M_MATRX
    DM_MATRX,
#endif
#if HM_M_RSTRX
    DM_RSTRX,
#endif
#if HM_M_CBTRX
    DM_CBTRX,
#endif
#if HM_M_CHTRX
    DM_CHTRX,
#endif
#if HM_M_OHTRX
    DM_OHTRX,
#endif
#if HM_M_CYTRX
    DM_CYTRX,
#endif
#if HM_M_CLTRX
    DM_CLTRX,
#endif
#if HM_M_B2TRX
    DM_B2TRX,
#endif
#if HM_M_BDEFM
    DM_BDEFM,
#endif
#if HM_M_XTEFM
    DM_XTEFM,
#endif
#if HM_M_SDEFM
    DM_SDEFM,
#endif
#if HM_M_CPEFM
    DM_CPEFM,
#endif
#if HM_M_RSEFM
    DM_RSEFM,
#endif
#if HM_M_CBEFM
    DM_CBEFM,
#endif
#if HM_M_HHEFM
    DM_HHEFM,
#endif
#if HM_M_CYEFM
    DM_CYEFM,
#endif
    NMODELS };
static const dmodel_t DMODELS[NMODELS] = {
#if HM_M_K808
    DM_K808_DEF,
#endif
#if HM_M_K909
    DM_K909_DEF,
#endif
#if HM_M_KBOOM
    DM_KBOOM_DEF,
#endif
#if HM_M_KPUNC
    DM_KPUNC_DEF,
#endif
#if HM_M_S808
    DM_S808_DEF,
#endif
#if HM_M_S909
    DM_S909_DEF,
#endif
#if HM_M_C808
    DM_C808_DEF,
#endif
#if HM_M_C909
    DM_C909_DEF,
#endif
#if HM_M_SSNAP
    DM_SSNAP_DEF,
#endif
#if HM_M_SCRAK
    DM_SCRAK_DEF,
#endif
#if HM_M_HATC
    DM_HATC_DEF,
#endif
#if HM_M_HATO
    DM_HATO_DEF,
#endif
#if HM_M_CYMB
    DM_CYMB_DEF,
#endif
#if HM_M_COWB
    DM_COWB_DEF,
#endif
#if HM_M_HMETL
    DM_HMETL_DEF,
#endif
#if HM_M_HNOIS
    DM_HNOIS_DEF,
#endif
#if HM_M_TOM
    DM_TOM_DEF,
#endif
#if HM_M_CONGA
    DM_CONGA_DEF,
#endif
#if HM_M_RIM
    DM_RIM_DEF,
#endif
#if HM_M_CLAVE
    DM_CLAVE_DEF,
#endif
#if HM_M_MEMB
    DM_MEMB_DEF,
#endif
#if HM_M_FM
    DM_FM_DEF,
#endif
#if HM_M_KTRX
    DM_KTRX_DEF,
#endif
#if HM_M_SDTRX
    DM_SDTRX_DEF,
#endif
#if HM_M_XTTRX
    DM_XTTRX_DEF,
#endif
#if HM_M_XCTRX
    DM_XCTRX_DEF,
#endif
#if HM_M_CPTRX
    DM_CPTRX_DEF,
#endif
#if HM_M_MATRX
    DM_MATRX_DEF,
#endif
#if HM_M_RSTRX
    DM_RSTRX_DEF,
#endif
#if HM_M_CBTRX
    DM_CBTRX_DEF,
#endif
#if HM_M_CHTRX
    DM_CHTRX_DEF,
#endif
#if HM_M_OHTRX
    DM_OHTRX_DEF,
#endif
#if HM_M_CYTRX
    DM_CYTRX_DEF,
#endif
#if HM_M_CLTRX
    DM_CLTRX_DEF,
#endif
#if HM_M_B2TRX
    DM_B2TRX_DEF,
#endif
#if HM_M_BDEFM
    DM_BDEFM_DEF,
#endif
#if HM_M_XTEFM
    DM_XTEFM_DEF,
#endif
#if HM_M_SDEFM
    DM_SDEFM_DEF,
#endif
#if HM_M_CPEFM
    DM_CPEFM_DEF,
#endif
#if HM_M_RSEFM
    DM_RSEFM_DEF,
#endif
#if HM_M_CBEFM
    DM_CBEFM_DEF,
#endif
#if HM_M_HHEFM
    DM_HHEFM_DEF,
#endif
#if HM_M_CYEFM
    DM_CYEFM_DEF,
#endif
};
static const char *const N_MODEL[NMODELS] = {
#if HM_M_K808
    "K808",
#endif
#if HM_M_K909
    "K909",
#endif
#if HM_M_KBOOM
    "BOOM",
#endif
#if HM_M_KPUNC
    "PUNCH",
#endif
#if HM_M_S808
    "S808",
#endif
#if HM_M_S909
    "S909",
#endif
#if HM_M_C808
    "C808",
#endif
#if HM_M_C909
    "C909",
#endif
#if HM_M_SSNAP
    "SNAP",
#endif
#if HM_M_SCRAK
    "CRACK",
#endif
#if HM_M_HATC
    "HATC",
#endif
#if HM_M_HATO
    "HATO",
#endif
#if HM_M_CYMB
    "CYMB",
#endif
#if HM_M_COWB
    "COWB",
#endif
#if HM_M_HMETL
    "METAL",
#endif
#if HM_M_HNOIS
    "HISS",
#endif
#if HM_M_TOM
    "TOM",
#endif
#if HM_M_CONGA
    "CONGA",
#endif
#if HM_M_RIM
    "RIM",
#endif
#if HM_M_CLAVE
    "CLAVE",
#endif
#if HM_M_MEMB
    "SKIN",
#endif
#if HM_M_FM
    "FM",
#endif
#if HM_M_KTRX
    "BDTRX",
#endif
#if HM_M_SDTRX
    "SDTRX",
#endif
#if HM_M_XTTRX
    "TMTRX",
#endif
#if HM_M_XCTRX
    "CGTRX",
#endif
#if HM_M_CPTRX
    "CPTRX",
#endif
#if HM_M_MATRX
    "SHTRX",
#endif
#if HM_M_RSTRX
    "RSTRX",
#endif
#if HM_M_CBTRX
    "CBTRX",
#endif
#if HM_M_CHTRX
    "CHTRX",
#endif
#if HM_M_OHTRX
    "OHTRX",
#endif
#if HM_M_CYTRX
    "CYTRX",
#endif
#if HM_M_CLTRX
    "CLTRX",
#endif
#if HM_M_B2TRX
    "B2TRX",
#endif
#if HM_M_BDEFM
    "BDEFM",
#endif
#if HM_M_XTEFM
    "TMEFM",
#endif
#if HM_M_SDEFM
    "SDEFM",
#endif
#if HM_M_CPEFM
    "CPEFM",
#endif
#if HM_M_RSEFM
    "RSEFM",
#endif
#if HM_M_CBEFM
    "CBEFM",
#endif
#if HM_M_HHEFM
    "HHEFM",
#endif
#if HM_M_CYEFM
    "CYEFM",
#endif
};
