/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* YEHNAH boot and main loop. Boot order: WDT first, boot-loop guard, fatal vectors,
 * guards; then LCD, input (TIMER5 IRQ, 10 kHz), audio (ALNK0 IRQ). */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
#if YEHNAH_DUAL_CORE
extern uint32_t _c1t_start[], _c1t_end[], _c1t_load[];   /* (0.6.71) the second core's loop (app.ld .c1_text) */
extern uint32_t _hot_start[], _hot_end[], _hot_load[];   /* (0.6.78) core 0's hottest audio code (app.ld .hot_text) */
extern uint32_t _low_start[], _low_end[];                /* (0.6.78) main-loop buffers below the RAM code (app.ld) */
#endif
extern uint32_t _xip_end[];
#include "boot_lzma.h"


/* TIMER5 outranks ALNK0, so the scan keeps its 100 us pace while a half buffer renders: before,
 * the ticks stopped for the whole render (0.7 ms idle, several ms loaded), the column lit when it
 * began stayed lit that long (a ~16 Hz flicker over all LEDs, beating with the scan) and the
 * encoders lost frames. Nested in ALNK0 it only scans (GPIO + fm1_in, nothing the audio ISR touches)
 * and counts ms; USB and UART polls wait for the first tick after the render, as they always did,
 * and the time spent nested is handed to the audio ISR so its load figures stay render-only.
 * The USB audio stream cannot wait for the render (one packet per 1 ms frame, a render takes up to
 * ~5 ms): uac_service also runs nested. It touches only EP4 (INDEX is set on every access) and the
 * consumer side of the audio ring, and usb_poll never runs nested, so the two never interleave. */
void fm1_timer5_irq(void)
{
    static uint32_t sub, owed;
    uint32_t t0 = fm1_ticks(), usb_due = sub % 5u == 0u;
    fm1_timer5_ack();
    yehnah_dbg.timer_irqs++;
    fm1_input_tick();
    {   /* milliseconds from the 24 MHz TIMER4 (robust to a late tick) */
        static uint32_t last, acc;
        acc += t0 - last;
        last = t0;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
    if (usb_due)
        owed |= 1u;                             /* 2 kHz: all USB SIE traffic lives here */
#if YEHNAH_UART
    if (sub % 5u == 2u)
        owed |= 2u;                             /* 2 kHz: <= ~7 bytes per call at 31250 baud */
#endif
    if (++sub == 10u)
        sub = 0;
    if (yehnah_dbg.in_audio) {
        yehnah_dbg.nested++;
#if YEHNAH_UAC
        if (usb_due)
            uac_service();
#endif
        t5_nested_ticks += fm1_ticks() - t0;
        return;
    }
    if (owed & 1u)
        usb_poll();
#if YEHNAH_UART
    if (owed & 2u)
        uart_midi_poll();
#endif
    owed = 0;
}
extern void isr_timer5(void);

static void timer5_start(void)                 /* OSC /4 = 6 MHz, PRD 600 -> 10 kHz */
{
    fm1_timer5_start(isr_timer5, 4);   /* above ALNK0 (3): the scan nests into the render (see fm1_timer5_irq) */
}

static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
#if YEHNAH_DUAL_CORE
    if (fm1_mc_on)
        fm1_mc_bar_set();                             /* (0.6.71) a crash beside the second core: one core until power-off */
    fm1_mc_hold();
#endif
    fm1_audio_stop();
    scr_wake_now();                                   /* (MENU > SCREEN OFF: the panel on first) */
    lcd_fill(0, 0, 240, 240, UI_CRASH_BG);            /* fixed, outside the palettes */
    draw_text_line(0, 8, 240, &AF_M, "Yehnah CRASH", UI_CRASH_INK, UI_CRASH_BG, 1);
    hexs(b, c->vec);
    draw_text_line(10, 40, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    hexs(b, c->pc);
    draw_text_line(10, 60, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    hexs(b, c->emu);
    draw_text_line(10, 84, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    hexs(b, c->dbg);
    draw_text_line(10, 102, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    hexs(b, c->rets);
    draw_text_line(10, 120, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

/* power-on: the parts with their power-on sounds (0.9.9: T1 DRUM-X, T2 VOICE, T3 ANIMAL, T4 LOFI, T5 ANALOG, T6 FM6,
 * engines.c TRK_DEF; as INIT SYSTEM: ui.c power_sound_of); the sequencers empty. RESTORE LAST ON brings the last session back after (autosave_boot) */
static void yehnah_init(void)
{
    uint32_t i;
    if (song_idx.magic == SONG_MAGIC && song_cur < NSONG && chain_valid(&song_idx.rows[song_cur]))
        chain_config = song_idx.rows[song_cur];       /* (0.6.81) the song's rows as persist_boot read them (it
                                                       * runs first): reset here, the main loop then saved the empty
                                                       * rows over them, the ARRANGE lost at every power-on */
    else
        chain_defaults(&chain_config);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    load_depth++;                             /* (the power-on loads: one outer load) */
    fm6_init();                               /* every track's FM6 patch: the init voice */
    power_on_parts();                         /* (0.9.9: ui.c, TRK_DEF's sounds; the tests run it too) */
    load_depth--;
    song.sel = 0;
    song.master_q12 = 2048;                   /* (the pot's level replaces it before the audio starts: master_boot) */
    ui.home = 1;
    ui.force = 1;
}

/* The power-on LED sweep (1.1, hal/fm1_led_anim.h: a soft light over the keys left to right, then the buttons swell
 * and settle on the glow; ~0.70 s), started before the scan and run by it: nothing waits for it, the splash's ~2 s (0.5: the signature written, then held)
 * and the first ~0.27 s of the UI go by under it (the UI's LEDs from its end, or at once on a key or a button down).
 * Every LEDS setting: it is no idle glow; with DIM HI / DIM LO it ends on that glow (and is drawn over that glow
 * level), with OFF / INV dark. Not with MENU > ANIM OFF (no motion: the LEDs as the UI has them at once), not after a crash (a crash record newer than the last boot saw: the crash screen's
 * reset), a hang (a WDT reset) or a failed boot (bootguard: the last one died within its first 30 s): straight to the
 * UI. The UBOOT paths never get here. */
static uint32_t crash_seen __attribute__((section(".noinit")));   /* fm1_crash.count at the last boot */
static uint8_t boot_clean;                    /* no crash, hang (WDT) or failed boot before this one (boot_leds):
                                               * the sweep runs and the autosave is restored (project.c autosave_boot) */
static void boot_leds(void)
{
    uint32_t crashed = fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != crash_seen;
    crash_seen = fm1_crash.count;
    boot_clean = !crashed && !bootguard.failed && !(fm1_boot.p3_rst & 4u);
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO ? 1u : settings_leds == LEDS_HIGH ? 2u : 0u);
    if (boot_clean)
        fm1_led_anim_start(settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO || settings_leds == LEDS_HIGH);
}

static void fm1_main(void)
{
    uint32_t ms;
    persist_boot();
#if YEHNAH_RESREGION
    res_boot();                                         /* (chord-harp fork) the resource region installed? (res.c) */
#endif
#if YEHNAH_OTA
    if (flash_ok)
        ota_boot_cleanup();                             /* staging area left by an update */
#endif
    settings_init();
    prefs_update();                                     /* (0.6.44) FX LATCH ON by default, once */
    usb_serial_apply();                                 /* (#67: the saved USB SERIAL before usb_start) */
    lcd_init();
    spl_anim = 1;                                       /* (0.5) the signature written: splash_write */
    draw_splash();                                      /* ui_draw.c (1.1.5) */
    if (yehnah_dbg.magic != DBG_MAGIC) {
        memset(&yehnah_dbg, 0, sizeof yehnah_dbg);
        yehnah_dbg.magic = DBG_MAGIC;
    }
    yehnah_dbg.boots++;
    yehnah_dbg.max_us = 0;
    yehnah_dbg.in_audio = 0;                       /* .noinit: a reset inside the audio ISR left it set, and
                                                       TIMER5 would treat every tick as nested (no USB poll) */
    yehnah_dbg.prev_stage = yehnah_dbg.stage;     /* a WDT reset leaves the last breadcrumb here */
    yehnah_dbg.prev_page = yehnah_dbg.page;
    yehnah_dbg.prev_home = yehnah_dbg.home;
    yehnah_dbg.prev_frames = yehnah_dbg.ui_frames;
    yehnah_dbg.prev_rst = fm1_boot.p3_rst;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    yehnah_init();
    master_boot();
    kb_boot_hold = 1;                         /* #137: no notes from the keys before the UI (seq.c keyboard_block) */
    audio_init();
    usb_start();
#if YEHNAH_UART
    uart_midi_init();
#endif
    boot_leds();
    timer5_start();
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(30);
    if ((fm1_in.buttons & 3u) == 3u) {
        panel_setup();                        /* OCT- + OCT+ held at power-on */
        settings_save();
    }
#if YEHNAH_DUAL_CORE
    /* (0.6.71) the second core, unless REC is held at power-on (one core this session: mc_off), a boot failed (a second
     * would enter UBOOT), or it was given up / beside a crash since power-on (hal/fm1_multicore.h fm1_mc_bar). Its entry
     * word is in the protected top of RAM: opened for it, interrupts off (up to 20 ms: under the splash) */
    mc_off = (uint8_t)((fm1_in.buttons >> panel.btn[B_REC]) & 1u);
    if (!mc_off && !bootguard.failed && !fm1_mc_barred()) {
        fm1_irq_off();
        fm1_guard_unlock_top();
        fm1_mc_start();
        fm1_guard_lock_top();
        fm1_irq_on();
    }
#endif
    autosave_boot(boot_clean);                /* (1.2) the last session's music, under the splash */
    session_dice(fm1_ticks() ^ (uint32_t)fm1_adc_read(FM1_ADC_BATT) << 16);   /* (0.4) this session's macros */
    {                                         /* the splash, its signature written (0.5: was 400 ms); the pot
                                               * followed (MIDI IN plays under it). (0.5.2) On the clock: drawing the
                                               * beings does not make it longer */
        uint32_t t0 = fm1_ms, n;
        for (n = 0; n < 600u && (ms = fm1_ms - t0) < SPL_WRITE + SPL_HOLD; n++) {   /* (600: a stalled clock still
                                                                                   * ends it, in ~2.4 s) */
            splash_write(ms);
            fm1_delay_ms(4);
            master_poll();
        }
    }
    kb_boot_hold = 0;
    lcd_fill(0, 0, 240, 240, T_BG);
#if YEHNAH_RESREGION
    if (flash_ok && !res_ok)
        ui_message("SOUNDS MISSING: INSTALL AGAIN");   /* (res.c: VECTOR, TERRAIN, WSEQ silent until then) */
#endif
    scrn.idle = fm1_ms;                        /* (MENU > SCREEN OFF: from here) */

    for (;;) {
        uint32_t m = fm1_ms;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (fm1_ms > 30000u && bootguard.pending) {     /* a crash or hang in the first 30 s counts */
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        {
            int32_t b = fm1_adc_read(FM1_ADC_BATT);     /* battery: slow IIR */
            if (b > 0)
                song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
        }
        master_poll();
        {   /* OCT- + OCT+ held 5 s: enter UBOOT with RAM intact (debug / update); a countdown
             * shows from 2 s over the whole screen (the menu and the dialogs too: ui_draw), letting
             * go cancels it */
            static uint32_t t0;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both) {
                if (ui.uboot) {
                    ui.uboot = 0;
                    ui.force = 1;                       /* what the countdown covered */
                    ui_message("UPDATE CANCELLED");
                }
                t0 = fm1_ms;
            } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
                uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
                if (left != ui.uboot) {
                    ui.uboot = (uint8_t)left;
                    ui.force = 1;
                }
            } else if (fm1_ms - t0 > 5000u) {
                fm1_audio_stop();
                scr_wake_now();
                lcd_fill(0, 0, 240, 240, T_BG);
                draw_text_box(0, 110, 240, &AF_M, "UBOOT", T_THEME, 1);
                usb_detach();
                fm1_delay_ms(30);
                bootguard.pending = 0;                  /* intentional reset: not a failed boot */
                fm1_enter_uboot();
            }
        }
#if YEHNAH_OTA
        ed_service();                                   /* web editor SysEx */
        tab_service();                                  /* (the big FM-1) MENU > USB TABLET's messages */
        ota_service();                                  /* M-UPGRADE handshake */
        if (usb.ota_req) {                              /* M-UPGRADE upgrade command */
            usb.ota_req = 0;
            panic_req = (1u << NTRK) - 1u;               /* every track (bit per track) */
            scr_wake_now();                             /* (its progress on the screen) */
            if (flash_ok)
                ota_session();                          /* returns only if nothing was committed */
            lcd_fill(0, 0, 240, 240, T_BG);
            ui.force = 1;
        }
#endif
        if (usb.uboot_req) {                            /* SysEx F0 22 24 35 7D F7 from the host */
            fm1_audio_stop();
            scr_wake_now();
            lcd_fill(0, 0, 240, 240, T_BG);
            draw_text_box(0, 110, 240, &AF_M, "UBOOT (USB)", T_THEME, 1);
            fm1_delay_ms(20);
            usb_detach();
            fm1_delay_ms(30);
            bootguard.pending = 0;
            fm1_enter_uboot();
        }
#if YEHNAH_CDC
        cdc_task();
#endif
        yehnah_dbg.ui_frames++;
        yehnah_dbg.page = ui.page;
        yehnah_dbg.home = ui.home;
        yehnah_dbg.stage = 1;
        ui_input();
        settings_poll();                              /* queued settings save: only while stopped */
        autosave_poll();                              /* (1.2) the music, stopped and idle: project.c */
        song_poll();                                  /* the song: the next section staged, the music back after it */
        yehnah_dbg.stage = 2;
        ui_leds();
        ui_draw();
        yehnah_dbg.stage = 9;
        while (fm1_ms - m < 15u) {                               /* ~60 UI frames/s at most */
            ui_input();
#if YEHNAH_OTA
            ed_service();                       /* editor replies without waiting for the next frame */
            tab_service();
#endif
        }
    }
}

/* (chord-harp fork, 2026-11-04, after Melodee) one boot-packed load image at rec (tools/boot_lzma.py: MAGIC, packed
 * bytes, raw bytes, 0, the stream) into start .. end; the next record. The decoder's tables borrow LOWBUF, cleared
 * after. One that does not unpack exactly: update mode */
static __attribute__((noinline)) const uint32_t *boot_image(const uint32_t *rec, uint32_t *start, uint32_t *end)
{
    uint32_t bytes = (uint32_t)((uintptr_t)end - (uintptr_t)start), packed;
    if ((uintptr_t)(rec + 4) > (uintptr_t)_xip_end || rec[0] != BOOT_LZMA_MAGIC || rec[2] != bytes || rec[3] != 0)
        fm1_enter_uboot();
    packed = rec[1];
    if (packed > (uintptr_t)_xip_end - (uintptr_t)(rec + 4) ||
        !boot_lzma((uint8_t *)start, bytes, (const uint8_t *)(rec + 4), packed, (uint16_t *)_low_start))
        fm1_enter_uboot();
    return rec + 4 + (packed + 3u) / 4u;
}
_Static_assert(BOOT_LZMA_PROBS * 2u <= 0x7000u, "the boot decoder's tables fit LOWBUF (app.ld)");

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    fm1_core1_stop();                           /* (0.6.71) a second core a reset left running would read the RAM
                                                 * cleared below */
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 2u) {
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    if (_rt_load[0] == BOOT_LZMA_MAGIC) {      /* (chord-harp fork) packed, one after another (tools/boot_lzma.py) */
        const uint32_t *at = _rt_load;
        at = boot_image(at, _rt_start, _rt_end);
#if YEHNAH_DUAL_CORE
        at = boot_image(at, _c1t_start, _c1t_end);
#else
        at += 4 + (at[1] + 3u) / 4u;            /* (no second core: its loop's record skipped) */
#endif
        at = boot_image(at, _hot_start, _hot_end);
        (void)boot_image(at, _data_start, _data_end);
    } else {                                    /* YEHNAH_PACK=0: each at its load address */
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;                                /* flash driver code that must run from RAM */
#if YEHNAH_DUAL_CORE
    for (s = _c1t_load, d = _c1t_start; d < _c1t_end; s++, d++)
        *d = *s;                                /* (0.6.71) the second core's waiting loop (hal/fm1_multicore.h) */
#endif
    for (s = _hot_load, d = _hot_start; d < _hot_end; s++, d++)
        *d = *s;                                /* (0.6.78) core 0's hottest audio code (core.h HOTRAM) */
    }
    for (d = _low_start; d < _low_end; d++)
        *d = 0;                                 /* (after: the decoder's tables were here) */
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    fm1_main();
    for (;;)
        ;
}
