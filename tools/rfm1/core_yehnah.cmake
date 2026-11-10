# SPDX-License-Identifier: GPL-3.0-only
# Yehnah (https://github.com/Mello-D/JIANT, branch chord-harp): the yehnah_web family's core (fw_core.h over the
# firmware's own web/emu/yehnah_web.c; cmake/FM1Cores.cmake fm1_add_core), after FM1VST's core_yehnah.cmake.
# Copied into FM1VST's core-glue/yehnah_web/ by tools/rfm1/build_rfm1.sh, the tree linked as cores/YehNah.
include(${CMAKE_CURRENT_LIST_DIR}/fw_common.cmake)
fm1_add_core(yehnah ${CMAKE_CURRENT_LIST_DIR} SOURCE ${CMAKE_SOURCE_DIR}/cores/YehNah UNIT core_yehnah.c
             DESC core_yehnah_desc.c FAMILY yehnah_web LICENSE GPL-3.0-only
             CREDITS "VOICE's English words: a subset of The CMU Pronouncing Dictionary (cmudict 0.7b, Carnegie Mellon University), BSD-2-Clause:"
             CREDITS_FILES LICENSES/BSD-2-Clause-CMUdict.txt
             GEN ui_fonts.h ui_icons.h ui_keycaps.h ui_palettes.h ui_shapes.h yehnah_tables.h
                 yehnah_fm6.h ui_font_x.h dice_bars.h
             VERSION_FILE firmware/src/yehnah.c VERSION_REGEX "^#define YEHNAH_VERSION \"v?([^\"]*)\"")
fw_golden(yehnah ${CMAKE_SOURCE_DIR}/cores/YehNah web/emu/yehnah_web.c)
# the .rfm1 (cmake/FM1Rfm1.cmake: -DFM1_RFM1=ON; docs/RFM1.md)
fm1_add_rfm1(yehnah AUTHORS "Mello-D" "juanjiant-bit (JIANT)" "Leo Kuroshita (Hügelton Instruments) (Felucca)")
