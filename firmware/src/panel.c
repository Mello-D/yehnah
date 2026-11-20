/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Physical panel: which matrix button / encoder carries which printed label.
 * The matrix ids are known, the printed labels are not, so
 * the mapping is a table with a best guess, overridable by HARDWARE CALIBRATION
 * (hold OCT- and OCT+ while powering on): YEHNAH asks for each label in turn.
 * The learned table lives in .noinit and, with YEHNAH_FLASH, in flash with
 * the settings (project.c); read it back with `fm1t memr` to bake it in. */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const char *const B_NAME[NB] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE",
                                        "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
static const char *const E_NAME[NE] = {"SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2",
                                        "KNOB 3", "KNOB 4"};
#define PANEL_MAGIC 0x50414E35u          /* "PAN5": bump when PANEL_DEFAULT changes */

typedef struct {
    uint32_t magic;
    uint8_t btn[NB];             /* matrix button id (0..13) per label */
    uint8_t enc[NE];             /* matrix encoder (0..6) per role */
    int8_t dir[NE];              /* +1 / -1 so that clockwise is + */
} panel_t;
panel_t panel __attribute__((section(".noinit")));

static const panel_t PANEL_DEFAULT = {
    PANEL_MAGIC,
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1},   /* PLAY = 12, REC = 13 (measured on hardware) */
    {0, 1, 6, 2, 3, 4, 5},                           /* SELECT = enc 0, ALGORITHM = enc 1, PRESETS = enc 6 (printed labels) */
    {1, 1, 1, 1, 1, 1, 1},
};

static void panel_init(void)                     /* also after a flash load: ids are used as array indexes and shifts */
{
    uint32_t i, buttons = 0, encoders = 0, ok = panel.magic == PANEL_MAGIC;
    for (i = 0; ok && i < NB; i++) {
        ok = panel.btn[i] < NB && !(buttons & (1u << panel.btn[i]));
        if (ok) buttons |= 1u << panel.btn[i];
    }
    for (i = 0; ok && i < NE; i++) {
        ok = panel.enc[i] < NE && !(encoders & (1u << panel.enc[i])) &&
             (panel.dir[i] == 1 || panel.dir[i] == -1);
        if (ok) encoders |= 1u << panel.enc[i];
    }
    if (!ok)
        panel = PANEL_DEFAULT;
}

static uint32_t panel_btn_of(uint32_t matrix_id)        /* label of a matrix button, NB if none */
{
    uint32_t b;
    for (b = 0; b < NB; b++)
        if (panel.btn[b] == matrix_id)
            return b;
    return NB;
}

/* steps of a role, + = clockwise */
static uint8_t panel_moved;                     /* a knob turned since ui.c scr_input last looked (SCREEN OFF) */
static int32_t panel_enc(uint32_t role)
{
    int32_t s = fm1_enc_take(panel.enc[role]) * panel.dir[role];
    if (s)
        panel_moved = 1;
    return s;
}

/* Keep the retained settings layout fixed. palette: the index into UI_PALETTES */
#define SETTINGS_MAGIC 0x53455434u              /* "SET4" */
struct { uint32_t magic, palette, lowcut, zoom; } settings __attribute__((section(".noinit")));

static void settings_save(void);              /* project.c: flash copy (YEHNAH_FLASH) */

/* HOLD (menu): how long a button is held before its layer opens (ui_input.c). Saved in the settings record's
 * retired bold field as HOLD_TAG | index; any other value there (0 or 1 from older firmware) is the default */
#define HOLD_TAG 0x484C4400u
#define HOLD_DEF 1u
static const uint16_t HOLD_MS[4] = {300, 400, 500, 600};
static uint8_t settings_hold = HOLD_DEF;
static uint32_t hold_from_stored(uint32_t v) { return (v & ~3u) == HOLD_TAG ? v & 3u : HOLD_DEF; }
static uint32_t hold_to_stored(uint32_t old, uint32_t i)
{
    return i % 4u != HOLD_DEF ? HOLD_TAG | (i & 3u) : old > 1u ? 0u : old;
}
static int hold_stored_ok(uint32_t v) { return v <= 1u || (v & ~3u) == HOLD_TAG; }

/* LEDS (menu, 2026-11-17): a number 0 .. 5, the brightness of the idle glow (the keys and buttons nothing is happening on); the lit
 * and breathing ones are always at full. 0 no glow (the grid's beats and a layer's mid keys still show), 1 the pulse alone (~1/60,
 * FM1_LED_DIM_LO_NS), 2 the idle glow dim (~1/30 pulse + the mid level 1 frame in 8, was DIM HI), 3 1 in 6, 4 1 in 5, 5 1 in 4 (was
 * HIGH, the default; hal/fm1_input.h fm1_led_dim_level: leds_hal). (INV, the stock firmware's inverted LEDs, is gone.)
 * Saved in the settings record's retired zoom field: 0 is the default (5), else LEDS_TAG | 8 | level. Older records: LEDS_TAG | mode
 * with DIM 0 INV 1 OFF 2 DIM LO 3 HIGH 4, read as 2, 5, 0, 1, 5 (leds_from_stored) */
#define LEDS_TAG 0x4C454400u                    /* "LED" */
#define LEDS_COUNT 6u
#define LEDS_DEF 5u
static const char *const LEDS_NAME[LEDS_COUNT] = {"0", "1", "2", "3", "4", "5"};
static uint8_t settings_leds = LEDS_DEF;
static uint32_t leds_hal(uint32_t lvl) { static const uint8_t H[LEDS_COUNT] = {0, 1, 0, 3, 4, 2}; return H[lvl % LEDS_COUNT]; }   /* the HAL's levels */
static uint32_t leds_from_stored(uint32_t v)
{
    static const uint8_t OLD[5] = {2, 5, 0, 1, 5};      /* DIM, INV, OFF, DIM LO, HIGH */
    uint32_t x = v & 15u;
    if ((v & ~15u) != LEDS_TAG)
        return LEDS_DEF;
    return x >= 8u ? (x - 8u < LEDS_COUNT ? x - 8u : LEDS_DEF) : x < 5u ? OLD[x] : LEDS_DEF;
}
static uint32_t leds_to_stored(uint32_t old, uint32_t m)
{
    (void)old;
    return m != LEDS_DEF && m < LEDS_COUNT ? LEDS_TAG | 8u | m : 0u;
}
static int leds_stored_ok(uint32_t v)
{
    uint32_t x = v & 15u;
    return v <= 1u || ((v & ~15u) == LEDS_TAG && (x < 5u || (x >= 8u && x - 8u < LEDS_COUNT)));
}

static void settings_init(void)
{
    if (settings.magic != SETTINGS_MAGIC || settings.palette >= NPALETTES) {
        settings.magic = SETTINGS_MAGIC;
        settings.palette = UI_DEFAULT_INDEX;   /* VAP DK (tools/gen_ui_palettes.py DEFAULT) */
        settings.lowcut = 0;
        settings.zoom = 0;                     /* (retired: the LEDS setting, settings_persist.c) */
    }
    palette_set(settings.palette);
    fx_lowcut = (uint8_t)(settings.lowcut % 3u);
}
