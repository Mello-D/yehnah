/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MENU's rows and their settings: what ui_menu.c draws and steps, and what the editor reads and sets (editor_menu.c
 * MENU_DESC / MENU_SET). One apply path for both: menu_put. Grouped: the screen (COLOR, SKIN (0.8.9), STYLE, LARGE, ANIM, LEDS; 1.1.5:
 * SCREEN OFF), the
 * controls (HOLD: the layer threshold, KNOB ACCEL, FX LATCH, BPM LOCK; 1.2: SCALE LEDS, the keys), the sound (SPEAKER EQ: FLAT LOWCUT BASS+,
 * USB LEVEL; 1.1: CLICK, CLICK LEVEL, COUNT-IN), USB SERIAL, RESTORE LAST (1.2), then CALIBRATION (the setup screen: HARDWARE CALIBRATION) and ABOUT, the two rows with no
 * value (MI_VALUES: the rows before them hold one). 1.0.5: in four tabs (MI_TAB). */
/* (the rows' enum MI_*: params.c, GLO's pages name them) */
#define MI_VALUES MI_PANEL
static const char *const MI_NAME[MI_COUNT] = {"THEME", "LEDS", "SCREEN OFF", "FILL", "HOLD", "KNOB ACCEL", "FX LATCH",
                                              "SCALE LEDS", "MIDI OUT",
                                              "SPEAKER EQ", "USB LEVEL", "CLICK", "CLICK LEVEL", "COUNT-IN", "TRACKS",
                                              "USB",
                                              "RESTORE LAST", "LOAD", "CALIBRATION", "INIT SYSTEM", "ABOUT"};
/* 1.0.5: the MENU's tabs (ui_menu.c: ALGORITHM steps between them, PRESETS among one tab's rows; the editor gets a
 * row's tab after its MENU_DESC reply). A tab's rows follow each other in MI order (tests/ui_test.c checks it); at
 * most MTAB_ROWS each (the page does not scroll: ui_menu.c fits them). A new row joins a tab here, a new tab is
 * appended (its index is what the editor is told) */
enum { MTAB_DISPLAY, MTAB_CONTROL, MTAB_AUDIO, MTAB_SYSTEM, MTAB_COUNT };
static const char *const MTAB_NAME[MTAB_COUNT] = {"DISPLAY", "CONTROL", "AUDIO", "SYSTEM"};
static const uint8_t MI_TAB[MI_COUNT] = {
    MTAB_DISPLAY,                                                           /* THEME (was COLOR; 2026-11-02: BIG VIEW, was
                                                                             * LARGE, gone) */
    MTAB_DISPLAY,                                                           /* LEDS */
    MTAB_DISPLAY,                                                           /* SCREEN OFF (1.1.5) */
    MTAB_DISPLAY,                                                           /* FILL (2026-11-02) */
    MTAB_CONTROL, MTAB_CONTROL, MTAB_CONTROL,                               /* HOLD KNOB ACCEL FX LATCH */
    MTAB_CONTROL,                                                           /* SCALE LEDS (1.2) */
    MTAB_CONTROL,                                                           /* MIDI OUT (0.6.72) */
    MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO, MTAB_AUDIO,             /* SPEAKER EQ, USB LEVEL, CLICK, CLICK LEVEL,
                                                                             * COUNT-IN */
    MTAB_AUDIO,                                                             /* TRACKS (0.6.18) */
    MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM, MTAB_SYSTEM,        /* USB SERIAL, RESTORE LAST, LOAD (Yehnah
                                                                             * 0.6.30), CALIBRATION, INIT SYSTEM (0.6.58) */
    MTAB_SYSTEM,                                                            /* ABOUT */
};
/* the two-valued rows: a bit of ui_prefs (PREF_REC: of ui_rec_prefs, ui.c PREF_SCALE_LEDS) and its names (bit clear =
 * the default, bit set). A step up (a knob right,
 * OCT+) = ON on the ON / OFF rows (the switch's knob to the right; ANIM, USB SERIAL: their bit clears), else the
 * second name (STYLE LINE, USB LEVEL FIXED); a step down the other (menu_step) */
typedef struct { uint8_t row; uint16_t bit; const char *name[2]; } menu_flag_t;
static const menu_flag_t MENU_FLAGS[] = {
    {MI_ACCEL, PREF_ACCEL_OFF, {"ON", "OFF"}},        /* #52: ui_input.c accel (Yehnah: ON by default) */
    {MI_LATCH, PREF_LATCH, {"OFF", "ON"}},
    {MI_FILL, PREF_SNAKE, {"RANDOM", "SNAKE"}},       /* (2026-11-02) the knob cells' pixels (ui_draw.c kn_px) */
    {MI_RESTORE, PREF_RESTORE_ON, {"OFF", "ON"}},      /* 1.2, #130; 0.6.40: OFF (the default), no autosave, power-on as
                                                        * INIT SYSTEM; ON, the last session back (project.c) */
    {MI_SCLLED, PREF_SCALE_LEDS, {"OFF", "ON"}},      /* 1.2, Discussion #127: ON, the keys show the scale (ui_input.c) */
};
/* SPEAKER EQ (settings.lowcut, fx.c fx_lowcut): an EQ on the master for the small speaker, not a speaker switch
 * (#42: "OFF" read as the speaker off). FLAT is the old OFF (0, stored as before). The built-in speaker cannot be
 * turned off from the firmware (no amp enable or mute line is known); the EQ reaches
 * the headphone / line out and USB audio too (one DAC, uac_tap reads the master) */
static const char *const SPK_EQ[3] = {"FLAT", "LOWCUT", "BASS+"};
/* (0.7.0) USB LEVEL: MASTER (USB follows the knob, the default), FIXED (full level, the knob after: #42), then fixed
 * and attenuated (-6 .. -24 dB: a phone recording over USB clipped at the full level). ui.c usb_lvl, fx.c fx_usb_gain */
static const char *const USBLVL_N[6] = {"MASTER", "FIXED", "-6 dB", "-12 dB", "-18 dB", "-24 dB"};
/* (0.7.1) MENU > SYSTEM > USB (was USB SERIAL ON / OFF, #67): CONSOLE, the serial console with the stereo audio (the
 * default, PREF_SERIAL_OFF clear: the descriptors as before); STEREO, audio + MIDI only (class 0: the old OFF); STEMS,
 * audio as 8 channels (the six parts mono, the FX return L R; fx.c, usb.c), no console; (0.7.5) IN+OUT, the stereo
 * audio and the host's audio output too (usb.c usb_duplex: the computer or the phone plays into the last track), no
 * console; (0.8.0) STEMS+IN, the stems with the host's audio output too (10 channels in: the stems, then the host's
 * audio as the last track mixed it and as it came; the phone app records a voice through the FM-1 and moves it by
 * the latency); (the big FM-1, docs/plans/BIG-FM1-PLAN.md) TABLET, the stereo audio and a vendor bulk interface for the
 * tablet app (usb.c usb_tablet, tablet.c), no console. ui.c usb_out */
static const char *const USBOUT_N[6] = {"CONSOLE", "STEREO", "STEMS", "IN+OUT", "STEMS+IN", "TABLET"};
static const char *const HOLD_N[4] = {"0.3 s", "0.4 s", "0.5 s", "0.6 s"};   /* (the editor's names; the menu draws its own) */
/* 1.1 (Discussion #131): the metronome (click.c) and the count-in (seq.c); ui.c ui_rec_prefs holds them */
static const char *const CLICK_N[3] = {"OFF", "REC", "ON"};          /* REC: while a track is armed and playing */
static const char *const CLKLVL_N[3] = {"LOW", "MID", "HIGH"};
static const char *const COUNTIN_N[3] = {"OFF", "1 BAR", "2 BARS"};
/* 1.1.5: SCREEN OFF (ui.c scr_*: the screen dark after this long without panel input; NEVER the default since 1.1.5.1) */
static const char *const SCROFF_N[5] = {"NEVER", "5 MIN", "15 MIN", "30 MIN", "60 MIN"};
/* (0.6.72) MIDI OUT over USB (ui.c midi_out_mode, seq.c MO_*): the keys; and the clock + START / STOP; and every
 * note the FM-1 plays; nothing */
static const char *const MIDIOUT_N[4] = {"KEYS", "CLOCK", "ALL", "OFF"};

/* MENU > USB SERIAL (#67). The serial console is a developer tool (README: YEHNAH_CDC). ON (the default) presents it,
 * the descriptors byte for byte as before; OFF re-enumerates as audio + MIDI only, device class 0 (the bytes of a
 * YEHNAH_CDC=0 build): macOS 13-15 then attach their USB audio driver (with the console Apple's CDC composite driver
 * takes the device and the audio input never appears). USB-MIDI stays, and with it the editor, the update installer
 * and the soft key (SysEx on EP1). Applied at boot before USB starts (main.c: enumerated with it from the start, never
 * on then off) and while the menu is closed (ui_input: one re-enumeration when it closes, not one per KNOB 1 step).
 * A change from the editor (MENU_SET) waits 200 ms (menu_serial_at) so its reply leaves before the device drops off
 * the bus: the whole device re-enumerates, USB-MIDI and audio too */
static uint32_t menu_serial_at;                        /* fm1_ms | 1 of the editor's change; 0 none */
static void usb_serial_apply(void)
{
    if (menu_serial_at && (int32_t)(fm1_ms - menu_serial_at) < 200)   /* (chord-harp fork: signed: the | 1 puts it a
                                                         * ms ahead on an even ms, which unsigned read as long past) */
        return;
    menu_serial_at = 0;
#if YEHNAH_CDC
    usb_cdc_switch(YEHNAH_CDC_DEFAULT && !(ui_prefs & PREF_SERIAL_OFF) && !usb_out);   /* (a YEHNAH_CDC_DEFAULT=0 build: off) */
#endif
#if YEHNAH_UAC
    usb_stems_switch(usb_out == 1u || usb_out == 3u);  /* (0.7.1) MENU > USB STEMS: the 8-channel device */
    usb_duplex_switch(usb_out == 2u || usb_out == 3u); /* (0.7.5) MENU > USB IN+OUT: the audio output too */
#endif                                                 /* ((0.8.0) STEMS+IN: both, the 10-channel device) */
#if YEHNAH_TABLET
    usb_tablet_switch(usb_out == 4u);                  /* (the big FM-1) MENU > USB TABLET: the vendor interface too */
#endif
    fx_stems = (uint8_t)(usb_out == 1u ? 1u : usb_out == 3u ? 2u : 0u);   /* (the parts write their stems from the next block) */
}
static const menu_flag_t *menu_flag(uint32_t row)
{
    uint32_t i;
    for (i = 0; i < NELEM(MENU_FLAGS); i++)
        if (MENU_FLAGS[i].row == row)
            return &MENU_FLAGS[i];
    return 0;
}

/* a row's value as 0..menu_n(row) - 1 in the order the menu steps it (LEDS: OFF DIM LO DIM HI INV, LEDS_MENU) */
static uint32_t menu_n(uint32_t row)
{
    return row == MI_COLOR ? NPALETTES : row == MI_LOWCUT || (row >= MI_CLICK && row <= MI_COUNTIN) || row == MI_LOAD ? 3u :
           row == MI_HOLD || row == MI_MIDIOUT ? 4u : row == MI_USB || row == MI_TRACKS ? 6u : row == MI_SERIAL ? 5u + YEHNAH_TABLET : row == MI_LEDS ? LEDS_COUNT : row == MI_SCROFF ? NELEM(SCROFF_N) : 2u;
}
static uint32_t menu_get(uint32_t row)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t i = 0;
    if (f)
        return ((ui_prefs | (uint32_t)ui_rec_prefs << 8) & f->bit) != 0;
    switch (row) {
    case MI_COLOR: return settings.palette % NPALETTES;
    case MI_LOWCUT: return settings.lowcut % 3u;
    case MI_USB: return (ui_prefs & PREF_USB_FIXED) ? 1u + usb_lvl % 5u : 0u;
    case MI_SERIAL: return usb_out == 4u ? 5u : usb_out == 3u ? 4u : usb_out == 2u ? 3u : usb_out ? 2u :
                           (ui_prefs & PREF_SERIAL_OFF) || !YEHNAH_CDC ? 1u : 0u;   /* (no console built: CONSOLE reads STEREO) */
    case MI_HOLD: return settings_hold % 4u;
    case MI_SCROFF: return scr_get();
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: return rp_get(row - MI_CLICK);
    case MI_LOAD: return load_get();
    case MI_TRACKS: return trk_live - 1u;              /* (chord-harp fork: 1..6) */
    case MI_MIDIOUT: return midi_out_mode & 3u;
    case MI_LEDS:
        while (i + 1u < LEDS_COUNT && LEDS_MENU[i] != settings_leds)
            i++;
        return i;
    }
    return 0;
}
static const char *menu_vname(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f)
        return f->name[v & 1u];
    switch (row) {
    case MI_COLOR: return UI_PALETTES[v % NPALETTES].name;
    case MI_LOWCUT: return SPK_EQ[v % 3u];
    case MI_USB: return USBLVL_N[v % 6u];
    case MI_SERIAL: return USBOUT_N[v % 6u];
    case MI_HOLD: return HOLD_N[v & 3u];
    case MI_CLICK: return CLICK_N[v % 3u];
    case MI_CLKLVL: return CLKLVL_N[v % 3u];
    case MI_COUNTIN: return COUNTIN_N[v % 3u];
    case MI_LOAD: return N_LOAD[v % 3u];
    case MI_TRACKS: return N_TRKS[v % NTRK];
    case MI_MIDIOUT: return MIDIOUT_N[v & 3u];
    case MI_SCROFF: return SCROFF_N[v % NELEM(SCROFF_N)];
    case MI_LEDS: return LEDS_NAME[LEDS_MENU[v % LEDS_COUNT]];
    }
    return "";
}
/* set a row (v below menu_n(row)) and do what the menu does on a change: the palette, the EQ and USB LEVEL at once;
 * STYLE (ui_draw.c style_apply), LARGE, ANIM, LEDS, HOLD, the flags are read where they are used, every frame;
 * USB SERIAL when the menu has closed (usb_serial_apply). Saved by the caller (menu_close, the editor) */
static void menu_put(uint32_t row, uint32_t v)
{
    const menu_flag_t *f = menu_flag(row);
    if (f) {
        if (f->bit & PREF_REC)
            ui_rec_prefs = (uint8_t)(v ? ui_rec_prefs | f->bit >> 8 : ui_rec_prefs & ~(f->bit >> 8));
        else
            ui_prefs = (uint8_t)(v ? ui_prefs | f->bit : ui_prefs & ~f->bit);
        fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;   /* (at once; every frame too: ui_input) */
        return;
    }
    switch (row) {
    case MI_COLOR:
        settings.palette = v;
        palette_set(v);                                /* (the menu signature redraws) */
        ui.force = 1;
        break;
    case MI_LOWCUT: settings.lowcut = v; fx_lowcut = (uint8_t)v; break;
    case MI_USB:                                   /* (at once; every frame too: ui_input) */
        ui_prefs = (uint8_t)(v ? ui_prefs | PREF_USB_FIXED : ui_prefs & ~PREF_USB_FIXED);
        usb_lvl = (uint8_t)(v ? (v - 1u) % 5u : 0u);
        fx_usb_fixed = v != 0u;
        fx_usb_gain = USB_GAIN[usb_lvl];
        break;
    case MI_SERIAL:                                /* (0.7.1) applied when the menu closes (usb_serial_apply) */
        ui_prefs = (uint8_t)(v ? ui_prefs | PREF_SERIAL_OFF : ui_prefs & ~PREF_SERIAL_OFF);
        usb_out = (uint8_t)(v == 2u ? 1u : v == 3u ? 2u : v == 4u ? 3u : v == 5u ? 4u : 0u);   /* (0.7.5: 2 IN+OUT; 0.8.0: 3
                                                         * STEMS+IN; the big FM-1: 4 TABLET) */
        break;
    case MI_HOLD: settings_hold = (uint8_t)v; break;
    case MI_SCROFF: scr_put(v); break;                 /* (read every frame: ui.c scr_frame) */
    case MI_LEDS: settings_leds = LEDS_MENU[v]; break;
    case MI_CLICK: case MI_CLKLVL: case MI_COUNTIN: rp_put(row - MI_CLICK, v); break;   /* (at once: click.c, seq.c) */
    case MI_LOAD: load_put(v % 3u); break;
    case MI_TRACKS: trks_put(v % NTRK); break;
    case MI_MIDIOUT: midi_out_mode = (uint8_t)(v & 3u); break;   /* (seq.c reads it every frame: ui_input) */         /* (at once: voice.c sys_load) */
    }
}
/* the menu's step, any of KNOB 1..4 or OCT+ (s > 0) / OCT- (s < 0): the next / previous value, stopping at the ends;
 * COLOR and a row of 2..32 values (a pick: 2026-11-03, the user's) wrap. On an ON / OFF row up is ON (MENU_FLAGS) */
static uint32_t menu_step(uint32_t row, int32_t s)
{
    const menu_flag_t *f = menu_flag(row);
    uint32_t v = menu_get(row), n = menu_n(row);
    if (f && str_eq(f->name[0], "ON"))                 /* (ANIM, USB SERIAL, RESTORE LAST: ON is their value 0) */
        s = -s;
    if (s && n >= 2u && n <= 32u)
        return (uint32_t)wrap_add((int32_t)v, s > 0 ? 1 : -1, (int32_t)n);
    if (s > 0)
        return v + 1u < n ? v + 1u : row == MI_COLOR ? 0u : v;
    if (s < 0)
        return v ? v - 1u : row == MI_COLOR ? n - 1u : 0u;
    return v;
}
