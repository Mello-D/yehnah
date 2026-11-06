/* SPDX-License-Identifier: GPL-3.0-only */
/* Yehnah (the chord-harp fork of JIANT) as an FM1VST core: the firmware's own browser unit, web/emu/yehnah_web.c,
 * driven by FM1VST's yehnah_web family glue (fw_core.h). Copied into FM1VST's core-glue/yehnah_web/ by
 * tools/rfm1/build_rfm1.sh; after FM1VST's core_yehnah.c (ea5e604), whose notes hold here too:
 *   - the session's random macros (session_dice) take the web unit's default seed, 1 (no host clock in a core);
 *   - a fifth LED plane, fm1_led_hi (a brighter steady level than mid);
 *   - MENU > RESTORE LAST is OFF by default (PREF_RESTORE_ON). */
#ifndef FM1CORE_YEHNAH_SEED
#define FM1CORE_YEHNAH_SEED 1u
#endif
#define FM1CORE_FW_P yn
#define FM1CORE_FW_WEB_C "../../cores/YehNah/web/emu/yehnah_web.c"
#define FM1CORE_FW_BEFORE_BOOT() web_seed(FM1CORE_YEHNAH_SEED)
#define FM1CORE_FW_RESTORE_ON() ((ui_prefs & PREF_RESTORE_ON) != 0)
#define FM1CORE_YEHNAH_HI_LEVEL 160u       /* fm1_led_hi: lit on a third of the mid phases, between MID and lit */
#define FM1CORE_FW_LED(c, r, lv)                                                                             \
    do {                                                                                                     \
        if (((fm1_led_hi[c] >> (r)) & 1u) && (lv) < FM1CORE_YEHNAH_HI_LEVEL)                                 \
            (lv) = FM1CORE_YEHNAH_HI_LEVEL;                                                                  \
    } while (0)
#define FM1CORE_FW_EDITOR_C "../../cores/YehNah/firmware/src/editor.c"  /* the web editor's SysEx */
#define FM1CORE_FW_OTA_C "../../cores/YehNah/firmware/src/ota.c"        /* the M-UPGRADE identity */
#define YEHNAH_ID "FM-1_900"                                             /* (yehnah.c) */
#include "fw_core.h"
