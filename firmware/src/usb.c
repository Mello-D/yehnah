/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* USB full-speed device on USB0: a
 * class-compliant USB-MIDI interface. All SIE traffic happens in one place,
 * usb_poll(), called from the TIMER5 ISR at 2 kHz, so INDEX is never shared
 * and no USB IRQ is needed. MIDI in goes to a ring the audio ISR drains;
 * MIDI out (the panel keys) comes from a ring the audio ISR fills.
 * SysEx F0 22 24 35 7D F7 (the stock soft key, tools/fm1_softkey.py) asks
 * the main loop to enter UBOOT. VID 0x1209 / PID 0x0001 is the pid.codes
 * TEST id: fine for the bench, must be replaced before any release.
 * YEHNAH_CDC=1 adds a CDC-ACM serial function (IAD composite: EP2 notify,
 * EP3 bulk data) for the console in console.c; YEHNAH_USB_LAYOUT picks how
 * the composite presents itself, usb_cdc_switch / YEHNAH_CDC_DEFAULT=0 leave
 * the console out (descriptors, below).
 * YEHNAH_UAC=1 adds a USB audio input (UAC1, class compliant): the master
 * output as 16-bit stereo at 44.1 kHz on an isochronous asynchronous IN
 * endpoint (EP4), so a computer can record the FM-1 over the cable. audio.c
 * fills a ring (uac_render_start, uac_tap); uac_service sends one packet per
 * USB frame from the TIMER5 ISR, nested in the render too (main.c). The
 * update loader leaves both off: MIDI only.
 * YEHNAH_UAC_48K=1 (the default with YEHNAH_UAC) offers 48 kHz too: the host picks the rate with
 * SET_CUR (UAC1 sampling frequency control, two discrete rates in the one alt setting). The engines stay
 * at 44.1 kHz; at 48 kHz uac_tap resamples (polyphase FIR 160 / 147, uac_fir.h) into the ring. 44.1 kHz,
 * the rate until a host asks for another, sends the samples as before, bit for bit. (0.8.9) With CORES 2 the
 * second core resamples each half after the render (uac_render_end), off the first core's render; one core: each
 * block in the ISR as before (uac_rs_run, in RAM either way). An underrun never drops or shortens a packet. */
#include "../hal/fm1_usb.h"   /* registers; relative, so the loader and the host tests find it too */
#ifndef YEHNAH_CDC
#define YEHNAH_CDC 0
#endif
#ifndef YEHNAH_UAC
#define YEHNAH_UAC 0
#endif
#ifndef YEHNAH_UAC_48K
#define YEHNAH_UAC_48K 1
#endif
#ifndef YEHNAH_PROBE
#define YEHNAH_PROBE 0
#endif
#if !YEHNAH_UAC
#undef YEHNAH_UAC_48K
#define YEHNAH_UAC_48K 0
#endif
#ifndef YEHNAH_TABLET
#define YEHNAH_TABLET YEHNAH_UAC                         /* (the big FM-1) MENU > SYSTEM > USB TABLET: needs the audio */
#endif
#if !YEHNAH_UAC
#undef YEHNAH_TABLET
#define YEHNAH_TABLET 0
#endif
enum { S_FADDR = 0, S_POWER = 1, S_INTRTX1 = 2, S_INTRTX2 = 3, S_INTRRX1 = 4, S_INTRRX2 = 5, S_INTRUSB = 6,
       S_INTRTX1E = 7, S_INTRTX2E = 8, S_INTRRX1E = 9, S_INTRRX2E = 10, S_INTRUSBE = 11, S_FRAME1 = 12,
       S_FRAME2 = 13, S_INDEX = 14,
       S_TXMAXP = 16, S_CSR0 = 17, S_TXCSR1 = 17, S_TXCSR2 = 18, S_RXMAXP = 19, S_RXCSR1 = 20, S_RXCSR2 = 21,
       S_COUNT0 = 22, S_RXCOUNT1 = 22, S_RXCOUNT2 = 23 };

static uint8_t ep0buf[64 + 4] __attribute__((aligned(4)));
static uint8_t ep1tx[64] __attribute__((aligned(4)));
static uint8_t ep1rx[64 + 4] __attribute__((aligned(4)));
#if YEHNAH_CDC
static uint8_t ep2tx[8] __attribute__((aligned(4)));
static uint8_t ep3tx[64] __attribute__((aligned(4)));
static uint8_t ep3rx[64 + 4] __attribute__((aligned(4)));
/* serial rings: in = host -> console (ISR writes), out = console -> host (ISR reads) */
#define CI_N 256u
#define CO_N 2048u
static uint8_t cdc_in[CI_N], cdc_out[CO_N];
static volatile uint32_t ci_w, ci_r, co_w, co_r;
static struct {
    uint8_t line[7];             /* line coding: just stored and echoed back */
    volatile uint8_t dtr;        /* host has the port open */
    uint8_t e0_rx;               /* SET_LINE_CODING data stage pending */
    uint8_t rx_pend;             /* EP3 OUT packet seen, not yet taken (the ring was full) */
    uint32_t rx_pkts, tx_pkts;
} cdc = {{0x00, 0xC2, 0x01, 0x00, 0, 0, 8}};   /* 115200 8N1 */
#endif
#if YEHNAH_TABLET
/* (chord-harp: the big FM-1, docs/plans/BIG-FM1-PLAN.md step 1) MENU > SYSTEM > USB TABLET: the stereo device plus a
 * vendor interface (class FF, IF 3) with a bulk pair on EP3, for the tablet app (android/big). The main loop puts
 * framed messages (type, len lo, len hi, payload: tab_put, tablet.c) into tab_q, only once the tablet has said HELLO;
 * usb_poll sends at most one 64-byte packet a poll from it (~116 KB/s: no spin-waits), and the main loop's wait does too
 * as soon as the SIE took the last (tab_ep_more, interrupts off: 2-4 x the speed while it idles), the two buffers in
 * turn (the one the SIE may still be resending is never refilled), and a zero-length packet after a full one that emptied the ring
 * (the tablet's read ends there). A tablet packet (whole messages, <= 64 bytes) is copied to tab_rx for the main loop;
 * the next is taken once that one is done (NAKed meanwhile) */
#define TQ_N 2048u                                      /* (a power of two) */
static uint8_t tab_q[TQ_N];
static volatile uint32_t tq_w, tq_r;
static uint8_t tab_tx[2][64] __attribute__((aligned(4)));
static uint8_t tab_rxb[64 + 4] __attribute__((aligned(4)));
static uint8_t tab_rx[64];
static volatile uint8_t tab_rx_n;                       /* bytes in tab_rx for the main loop; 0 free */
static struct {
    volatile uint8_t live;                              /* the tablet said HELLO (bus reset: 0) */
    uint8_t buf, zlp, rx_pend;
    volatile uint32_t pkts, bytes, zlps, rx_pkts, full, max_fill;
} tab;
static volatile uint8_t usb_tablet;                     /* the device presents the vendor interface (usb_tablet_switch) */
#define USB_TABLET usb_tablet
#else
#define USB_TABLET 0
#endif

#if YEHNAH_UAC
/* USB audio input. The I2S clock is not locked to USB (about 44,117.6 Hz against the host's 44,100):
 * the endpoint is asynchronous, so each 1 ms packet carries what the ring holds, 44 or 45 frames
 * (the 44.1 pattern) and one more or one fewer while the ring's fill drifts out of its band.
 * The render writes HALF_FRAMES (128) frames every 2.9 ms; the fill measured when a render starts is
 * the low point of that saw tooth, and is kept in UA_LO..UA_HI. A frame waits in the ring for that fill
 * plus its place in the render: 80..256 frames, ~3.8 ms on average (256-frame halves and a 96..160
 * band: ~5.8 ms). Measured under load (65 % CPU): a 64..112 band started renders at
 * 48..119 with no glitch; one render was once seen 29 frames under the floor: a packet (46) must
 * still be in hand then. */
#define UA_N 512u                /* ring, frames (16-bit L | R << 16); a power of two */
/* (0.8.9) the band 0.7 ms higher: a render now catches up at once after a late one (audio.c), so the next one may
 * start up to ~1 ms late (a render of 131 %) and the ring must hold a packet beyond that (was 80..128, prime 104) */
#define UA_PRIME 136u            /* silence ahead of the first rendered frame */
#define UA_LO 112u               /* a render may start late: keep a packet and more in hand */
#define UA_HI 160u
/* (0.9.8) STEMS / STEMS+IN: the band 1.1 ms higher still (a repeated frame is a click in a soloed stem; a few renders
 * late in a row emptied the ring now and then); the app measures the latency it lines the voice up by in each take */
#define UA_PAD_STEMS 48u
#define UAC_RATE 44100u
#if YEHNAH_UAC_48K
/* 48 kHz: the same times in 48 kHz frames (x 160 / 147); a render of HALF_FRAMES gives 139..140 frames.
 * (0.8.9) With CORES 2 the half is resampled on core 1 after the render (uac_render_end) and enters the ring when the
 * next half starts: the fill measured then (before it) is the low point, the prime one half (UA_REND48) longer */
#define UA_PRIME48 152u
#define UA_LO48 128u
#define UA_HI48 176u
#define UA_REND48 ((HALF_FRAMES * 160u + 146u) / 147u)
#define UA_MAXF 49u              /* frames in the largest packet: 48 + 1 */
_Static_assert(UA_PRIME48 + 2u * UA_REND48 + UA_MAXF < UA_N, "the ring holds the band, two renders and a packet (48 kHz)");
#else
#define UA_MAXF 46u              /* frames in the largest packet */
#endif
#define UAC_MAXP (UA_MAXF * 4u)
_Static_assert(UA_HI + HALF_FRAMES + UA_MAXF < UA_N, "the ring holds the band, a render and a packet");
#define UA_W 5u                  /* (0.7.1) words a frame in the ring: 1 stereo (L | R << 16), 4 the stems (8 x 16 bit);
                                  * (0.8.0) 5 the stems with the host's audio in (10 x 16 bit: STEMS+IN) */
#define UA_NF10 409u             /* (0.8.0) STEMS+IN: the ring's frames, the 8-channel ring's words / 5 (the pool kept) */
_Static_assert(UA_HI + UA_PAD_STEMS + HALF_FRAMES + UA_MAXF < UA_NF10 && UA_NF10 * 5u <= UA_N * 4u, "the 10-channel ring");
static uint32_t ua_ring[UA_N * 4u] __attribute__((section(".pool")));   /* (8 KiB: in the pool, zeroed at boot) */
#ifndef UAC_STEMS
#define UAC_STEMS() 0            /* (a build without fx.c: the host tests of USB alone) */
#endif
#define UA_FW (UAC_STEMS() ? 3u + UAC_STEMS() : 1u) /* this mode's words a frame (UAC_STEMS: 1 the 8, 2 the 10) */
#define UA_NF (UA_FW == 5u ? UA_NF10 : UA_N)        /* this mode's frames in the ring */
/* a frame's slot in the ring: a power of two masked; STEMS+IN's 409 a modulo by a constant (a multiply, no divide;
 * the free-running index wraps every 2^32 frames, 27 hours of streaming: one glitch then) */
static inline uint32_t ua_slot(uint32_t f) { return UA_FW == 5u ? f % UA_NF10 : f & (UA_N - 1u); }
static volatile uint32_t ua_w, ua_r;
static uint32_t ep4tx[UA_MAXF * UA_W] __attribute__((aligned(4)));
static struct {
    volatile uint8_t alt;        /* streaming interface setting: 1 = the host records */
    volatile uint8_t r48;        /* the host's rate (SET_CUR): 1 = 48 kHz, 0 = 44.1 (bus reset) */
    uint8_t ring48;              /* the rate the ring was primed for (the render's; a change re-primes) */
    volatile uint8_t go;         /* the ring is primed: the consumer reads it (0: silence packets) */
    volatile uint8_t flowing;    /* the host takes our packets (it may set alt 1 well before it reads) */
    uint8_t queued;              /* a packet was queued since the stream (re)started */
    uint8_t feed;                /* this render feeds the ring (sampled when it starts) */
    uint8_t wait;                /* polls the queued packet has waited */
    uint8_t e0_rx;               /* SET_CUR sampling frequency: data stage pending */
    volatile uint32_t fill_min;  /* ring fill when the last render started */
    uint32_t acc;                /* 44.1 frames per ms: 0.1 frame steps */
    uint32_t last[UA_W];         /* the last frame sent, repeated on an underrun (0.7.1: up to 4 words) */
    uint32_t starts, pkts, frames;               /* frames in packets sent from the ring */
    uint32_t underruns, overruns, missed;        /* glitches: ring empty (repeat), ring full (drop), no packet ready */
    uint32_t stalls;                             /* the host stopped reading for > 20 ms (alt still 1) */
    uint32_t adj_up, adj_down;                   /* packets one frame longer / shorter than the pattern */
    uint32_t owe, repaid;                        /* (0.8.9) frames an underrun repeated, not yet made up; made up */
    uint32_t fill_lo, fill_hi;                   /* range of fill_min since the stream started */
    uint32_t rate_sets;                          /* SET_CUR sampling frequency requests */
} uac;
/* (0.7.5) MENU > SYSTEM > USB IN+OUT: the FM-1 is the host's audio output too (UAC1, 2 ch 16 bit 44.1 kHz, EP4 OUT
 * isochronous adaptive): TIMER5 takes each 1 ms packet into ui_ring (uin_rx), the audio ISR reads it at its own clock
 * (uac_in_fill, a linear interpolation whose step follows the ring's fill: the host's 44,100 Hz against the I2S's
 * ~44,117.6) into fx.c's uin_l / uin_r, which the last live track mixes in (its LEVEL, PAN, sends, MUTE) */
#define UI_N 512u                /* ring, frames (L | R << 16); a power of two */
#define UI_TARGET 160u           /* the fill the step steers to: ~3.6 ms (a render's 128 and a packet's 45 in hand) */
#define UI_MAXF 46u              /* frames in the largest packet a host sends at 44.1 kHz (45, one spare) */
#define UI_STEP_MAX 650          /* the step's range: +-1 % of 65536 (a host clock far off: the ring re-primes) */
static uint32_t ui_ring[UI_N] __attribute__((section(".pool")));   /* (2 KiB) */
static volatile uint32_t ui_w, ui_r;
static uint32_t ep4rx[UI_MAXF + 2u] __attribute__((aligned(4)));
static volatile uint8_t usb_duplex;                      /* the device presents the audio output too (usb_duplex_switch) */
static struct {
    volatile uint8_t alt;        /* the output's streaming setting: 1 = the host plays to us */
    uint8_t go;                  /* the reader runs (primed: UI_TARGET frames were in hand) */
    uint8_t e0_rx;               /* SET_CUR to EP4 OUT: its data stage pending (taken, ignored: one rate) */
    volatile uint8_t gen;        /* stream starts and stops (TIMER5); the reader drops the ring on a new one */
    uint8_t gen_seen;            /* (the reader's: only it moves ui_r) */
    uint32_t ph;                 /* the reader's phase between ring frames (Q16) */
    int32_t step;                /* frames a sample, Q16 (65536: the host's rate is ours) */
    int32_t fs;                  /* the fill, smoothed (Q8 frames) */
    int32_t g;                   /* the fade in / out, Q15 */
    int32_t last_l, last_r;      /* the last frame read (an underrun fades from it) */
    uint32_t pkts, frames, overruns, underruns, starts, bad;   /* bad: a packet not of whole frames / too long */
} uin;
#define USB_DUPLEX usb_duplex
#if YEHNAH_UAC_48K
#include "uac_fir.h"
/* the resampler's input history: the last 32 frames, written twice so a window of UAC_FIR_N never wraps */
#define RS_H 32u
_Static_assert(RS_H >= UAC_FIR_N && (RS_H & (RS_H - 1u)) == 0 && UAC_FIR_N % 4u == 0u, "the history holds a window");
static struct {
    int16_t l[2u * RS_H], r[2u * RS_H];   /* r right after l (the FIR loop reads both from one pointer) */
    uint32_t w;                  /* input frames written */
    uint32_t ph;                 /* the next output's phase, 0..159 (its newest input frame: the next written) */
#if YEHNAH_UAC_TONE
    uint32_t tone;
#endif
} rs;
/* (chord-harp fork, 2026-11-04) all 160 phases in the flash (uac_fir.h UAC_TAB, 15 KiB; 0.8.9 built them at boot into
 * the pool): one forward loop. Core 1 runs it outside the audio ISR, so the flash is never turned off while it does:
 * every flash access waits for its half first (uac_flash_quiet, fm1_flash.h FL_QUIET) */
#define rs_tab UAC_TAB
static uint32_t rs_in[HALF_FRAMES] __attribute__((section(".pool")));            /* a half (CORES 2) or a block, 16-bit L | R */
static struct {
    uint8_t c1;                  /* this stream resamples on core 1, a half at a time (chosen when it primes) */
    volatile uint8_t busy;       /* a half handed to core 1, not yet in the ring */
    uint32_t nin;                /* frames of this half in rs_in */
    uint32_t n, w0, job;         /* the half handed over: its frames, its place in the ring, core 1's job number */
    volatile uint32_t k;         /* core 1's: the frames it wrote */
} rq;
#if !defined(UAC_C1_LIVE) && YEHNAH_DUAL_CORE
/* core 1 (hal/fm1_multicore.h): one job at a time; mix_block's first job of the next half queues behind this one and
 * its wait covers it */
#define UAC_C1_LIVE() (fm1_mc_on && !mc_off)
#define UAC_C1_RUN(f) fm1_mc_run(f, 0)
#define UAC_C1_JOB() fm1_mc_box.job
#define UAC_C1_DONE(j) fm1_mc_done(j)
#define UAC_C1_WAIT() fm1_mc_wait()
#define UAC_C1_TEXT __attribute__((section(".c1_text"), noinline, used))
#endif
#ifndef UAC_C1_LIVE              /* one core (host builds, YEHNAH_DUAL_CORE=0): every block resampled at once */
#define UAC_C1_LIVE() 0
#define UAC_C1_RUN(f) 0
#define UAC_C1_JOB() 0u
#define UAC_C1_DONE(j) 1
#define UAC_C1_WAIT() 0
#endif
#ifndef UAC_C1_TEXT
#define UAC_C1_TEXT __attribute__((noinline))
#endif
#endif
#endif

static struct {
    uint8_t up, config, pend_addr, has_pend_addr, e0_tx, e0_zlp;
    const uint8_t *e0_src;
    uint16_t e0_left;
    uint32_t resets, setups, rx_pkts, tx_pkts, sof_seen, timeouts, no_sof;
    uint32_t suspends, max_gap, retries, frame_stalls;   /* diagnostics: suspend events, longest run of polls without SOF */
    uint16_t frame, frame_same;  /* last SOF frame number and how long it has not moved (x 10 ms) */
    uint8_t detached;            /* usb_detach() was called: no retry */
    volatile uint8_t suspended;  /* bus idle > 3 ms (unplugged or host asleep): the UI hides "USB" */
    uint8_t last_setup[8];
    uint8_t sysex[16];
    uint8_t sx_len, sx_on;
    uint8_t rx_pend;             /* EP1 OUT packet seen, not yet taken (the MIDI ring was too full) */
    volatile uint8_t uboot_req;
    volatile uint8_t ota_req;    /* F0 22 24 35 7F F7: M-UPGRADE upgrade command (YEHNAH_OTA) */
} usb;

#if YEHNAH_OTA
/* M-UPGRADE SysEx (ota.c): one received frame at a time (7-bit bytes between
 * F0 and F7; frames arriving while one is pending are dropped, the host
 * retries), and a TX ring of SysEx event packets sent before any MIDI. */
static uint8_t sx_frame[640];
static uint32_t sx_pos;
static volatile uint32_t sx_frame_len;
static volatile uint8_t sx_ready, sx_collect, sx_busy;
#define SXQ 64u
static uint32_t sx_out_q[SXQ];
static volatile uint32_t so_w, so_r;
#endif

/* MIDI rings: 4-byte USB-MIDI event packets */
#define MQ 64u
static uint32_t midi_in_q[MQ], midi_out_q[MQ];
static uint32_t midi_in_ms[MQ];
static uint8_t midi_in_source[MQ];                    /* 1 USB, 2 TRS */
static volatile uint32_t fm1_ms;                    /* also declared by core.h; parser-only host tests */
static volatile uint32_t mi_w, mi_r, mo_w, mo_r;
static volatile uint8_t midi_in_overflow;               /* audio ISR discards a broken stream and releases notes */

static int midi_enqueue(uint32_t pkt, uint32_t source)
{
    uint32_t at = mi_w % MQ;
    if (midi_in_overflow)
        return 0;
    if (mi_w - mi_r >= MQ) {
        midi_in_overflow = 1;
        return 0;
    }
    midi_in_q[at] = pkt;
    midi_in_ms[at] = fm1_ms;
    midi_in_source[at] = (uint8_t)source;
    RING_PUBLISH();
    mi_w++;
    return 1;
}

static void midi_out_event(uint32_t pkt)            /* from the audio ISR */
{
    if (usb.config && mo_w - mo_r < MQ) {
        midi_out_q[mo_w % MQ] = pkt;
        RING_PUBLISH();
        mo_w++;
    }
}

/* ------------------------------------------------------- descriptors --- */
/* Two functions: audio + MIDI (0 audio control, 1 MIDI streaming, 2 audio streaming with YEHNAH_UAC)
 * and, with YEHNAH_CDC, the CDC-ACM console (communication + data). The endpoints never move:
 * EP1 MIDI, EP2 CDC notify, EP3 CDC data, EP4 audio.
 * bcdDevice: 3.00 MIDI, +0.10 the audio input (+0.20 since 1.1: 44.1 and 48 kHz, YEHNAH_UAC_48K), +0.01 CDC
 * (+0.02 per YEHNAH_USB_LAYOUT step, so a host never reuses what it learnt about another layout).
 * YEHNAH_USB_LAYOUT, how the CDC build presents itself (#67: macOS 13-15 attach Apple's CDC composite
 * driver to a misc / IAD device that has a CDC function, and their kernel audio driver then never
 * takes the audio interfaces; macOS 26 / 27 (usbaudiod, matched per interface) and iOS / iPadOS work):
 *   0  1.0: device class EF 02 01 (misc / IAD), audio + MIDI IF 0.., CDC last         (the default)
 *   1  CDC first: CDC IF 0-1, then audio + MIDI, both with IADs, EF 02 01
 *   2  as 0 with device class 00 00 01: outside AppleUSBCDC's device personalities
 *      (02 * *, EF 02 01, 00 00 00); the IADs stay for Windows / Linux (test build)
 *   3  as 0 with device class 00 00 00 (test build: does AppleUSBCDC's "Vendor" personality take it?)
 * usb_cdc_on (YEHNAH_CDC_DEFAULT at boot; usb_cdc_switch re-enumerates): 0 presents the device without
 * the console, byte for byte as a YEHNAH_CDC=0 build: device class 0, audio + MIDI, no IAD. */
#ifndef YEHNAH_USB_LAYOUT
#define YEHNAH_USB_LAYOUT 0
#endif
#ifndef YEHNAH_CDC_DEFAULT
#define YEHNAH_CDC_DEFAULT 1
#endif
#ifndef YEHNAH_USB_PID
#define YEHNAH_USB_PID 0x0001   /* the update loader is 0x0002 */
#endif
#if YEHNAH_USB_LAYOUT < 0 || YEHNAH_USB_LAYOUT > 3
#error "YEHNAH_USB_LAYOUT is 0..3"
#endif

#define D_CFG(len, nif) 9, 2, (len) & 0xFF, (len) >> 8, (nif), 1, 0, 0x80, 50
#define D_IAD(first, n, cls, sub, proto) 8, 0x0B, (first), (n), (cls), (sub), (proto), 0
#if YEHNAH_UAC_48K
#define D_FMT 14, 0x24, 2, 1, 2, 2, 16, 2, 0x44, 0xAC, 0x00, 0x80, 0xBB, 0x00,   /* 44100, 48000 */
#define FMT_LEN 14
#elif YEHNAH_UAC
#define D_FMT 11, 0x24, 2, 1, 2, 2, 16, 1, UAC_RATE & 0xFF, (UAC_RATE >> 8) & 0xFF, UAC_RATE >> 16,
#define FMT_LEN 11
#else
#define FMT_LEN 0
#endif
#if YEHNAH_UAC
#define D_AC_CH(a, ch, cfg)                                                                          \
    9, 4, (a), 0, 0, 1, 1, 0, 0,                                                                     \
    10, 0x24, 1, 0x00, 0x01, 31, 0, 2, (a) + 1, (a) + 2,   /* AC header 1.00: MIDI + audio streaming */ \
    12, 0x24, 2, 1, 0x03, 0x06, 0, (ch), (cfg), 0x00, 0, 0, /* input terminal 1: line, ch channels */    \
    9, 0x24, 3, 2, 0x01, 0x01, 0, 1, 0,                    /* output terminal 2: USB streaming, from 1 */
#define D_AC(a) D_AC_CH(a, 2, 0x03)                            /* 2 ch (L R) */
#define D_AS_FMT(s, fmt, maxp)                                                                       \
    9, 4, (s), 0, 0, 1, 2, 0, 0,                           /* audio streaming, alt 0: no bandwidth */    \
    9, 4, (s), 1, 1, 1, 2, 0, 0,                           /* alt 1: the stream */                      \
    7, 0x24, 1, 2, 1, 0x01, 0x00,                          /* AS general: terminal 2, delay 1, PCM */   \
    fmt                                                    /* type I: the channels, 16 bit, the rates */ \
    9, 5, 0x84, 0x05, (maxp) & 0xFF, (maxp) >> 8, 1, 0, 0,   /* EP4 IN isochronous async, 1 ms */       \
    7, 0x25, 1, 0x01, 0, 0, 0,                             /* CS endpoint: sampling frequency control */
#define D_AS(s) D_AS_FMT(s, D_FMT, UAC_MAXP)
/* (0.7.1) MENU > USB STEMS: 8 channels (the six parts mono, the FX return L R), 44.1 kHz only, 46 frames a packet at
 * most: 736 bytes. (0.9.8) It said 45 (720 bytes) while uac_packet sends 46 whenever it holds the ring's fill (+1 on a
 * 45-frame millisecond): the host took each such packet as babble (the FM-1's clock is fast, so +1 comes often), dropped
 * it and stalled the endpoint, the ring filled and whole blocks were dropped, ~100 times a second: stems full of clicks */
#define D_FMT8 11, 0x24, 2, 1, 8, 2, 16, 1, 0x44, 0xAC, 0x00,
#define FMT_LEN8 11
#define UAC_MAXP8 (46u * 16u)
#define D_AC8(a) D_AC_CH(a, 8, 0x00)
#define D_AS8(s) D_AS_FMT(s, D_FMT8, UAC_MAXP8)
/* (0.8.0) MENU > USB STEMS+IN: 10 channels (the stems, then the host's audio as the mix took it and as it came), 46
 * frames a packet at most: 920 bytes, within a full-speed isochronous packet's 1023 */
#define D_FMT10 11, 0x24, 2, 1, 10, 2, 16, 1, 0x44, 0xAC, 0x00,
#define UAC_MAXP10 (46u * 20u)
#define D_AS10(s) D_AS_FMT(s, D_FMT10, UAC_MAXP10)
/* (0.9.8) every packet uac_packet can send fits the endpoint's wMaxPacketSize, in every mode: 44.1 kHz's 45 + 1 frames */
_Static_assert(UAC_MAXP8 >= (45u + 1u) * 16u && UAC_MAXP10 >= (45u + 1u) * 20u && UAC_MAXP >= UA_MAXF * 4u &&
               UAC_MAXP8 <= 1023u && UAC_MAXP10 <= 1023u, "the largest packet fits wMaxPacketSize (full speed: 1023)");
/* (0.7.5) MENU > USB IN+OUT: the input as D_AC's, plus the host's output: input terminal 3 (USB streaming, L R) ->
 * output terminal 4 (speaker), and its streaming interface (EP4 OUT isochronous adaptive, 44.1 kHz only) */
#define UIN_MAXP (UI_MAXF * 4u)
#define D_AC_DUP_CH(a, ch, cfg)                                                                      \
    9, 4, (a), 0, 0, 1, 1, 0, 0,                                                                     \
    11, 0x24, 1, 0x00, 0x01, 53, 0, 3, (a) + 1, (a) + 2, (a) + 3,   /* AC header: MIDI, input, output */ \
    12, 0x24, 2, 1, 0x03, 0x06, 0, (ch), (cfg), 0x00, 0, 0, /* input terminal 1: line, L R (or the stems) */ \
    9, 0x24, 3, 2, 0x01, 0x01, 0, 1, 0,                    /* output terminal 2: USB streaming, from 1 */ \
    12, 0x24, 2, 3, 0x01, 0x01, 0, 2, 0x03, 0x00, 0, 0,    /* input terminal 3: USB streaming, L R */   \
    9, 0x24, 3, 4, 0x01, 0x03, 0, 3, 0,                    /* output terminal 4: speaker, from 3 */
#define D_AC_DUP(a) D_AC_DUP_CH(a, 2, 0x03)
#define D_AS_OUT(s)                                                                                  \
    9, 4, (s), 0, 0, 1, 2, 0, 0,                           /* the output's streaming, alt 0 */          \
    9, 4, (s), 1, 1, 1, 2, 0, 0,                           /* alt 1: the host plays */                  \
    7, 0x24, 1, 3, 1, 0x01, 0x00,                          /* AS general: terminal 3, PCM */            \
    11, 0x24, 2, 1, 2, 2, 16, 1, 0x44, 0xAC, 0x00,         /* type I: 2 ch, 16 bit, 44100 */            \
    9, 5, 0x04, 0x09, UIN_MAXP & 0xFF, UIN_MAXP >> 8, 1, 0, 0,   /* EP4 OUT isochronous adaptive, 1 ms */ \
    7, 0x25, 1, 0x00, 0, 0, 0,                             /* CS endpoint: no controls */
#else
#define D_AC(a) 9, 4, (a), 0, 0, 1, 1, 0, 0, 9, 0x24, 1, 0x00, 0x01, 9, 0, 1, (a) + 1,
#define D_AS(s)
#endif
#define D_MIDI(m)                                                                                    \
    9, 4, (m), 0, 2, 1, 3, 0, 0,                                                                     \
    7, 0x24, 1, 0x00, 0x01, 0x41, 0x00,                                                              \
    6, 0x24, 2, 1, 1, 0,                                                                             \
    6, 0x24, 2, 2, 2, 0,                                                                             \
    9, 0x24, 3, 1, 3, 1, 2, 1, 0,                                                                    \
    9, 0x24, 3, 2, 4, 1, 1, 1, 0,                                                                    \
    9, 5, 0x01, 2, 64, 0, 0, 0, 0,                                                                   \
    5, 0x25, 1, 1, 1,                                                                                \
    9, 5, 0x81, 2, 64, 0, 0, 0, 0,                                                                   \
    5, 0x25, 1, 1, 3,
#define D_AUDIO(a) D_AC(a) D_MIDI((a) + 1) D_AS((a) + 2)   /* audio + MIDI from interface a */
#define AUD_NIF (2 + YEHNAH_UAC)
#define AUD_LEN (92 + (63 + FMT_LEN) * YEHNAH_UAC)
#if YEHNAH_UAC
#define D_AUDIO8(a) D_AC8(a) D_MIDI((a) + 1) D_AS8((a) + 2)   /* (0.7.1) the stems: 8 channels */
#define AUD_LEN8 (92 + 63 + FMT_LEN8)
#define D_AUDIO_DUP(a) D_AC_DUP(a) D_MIDI((a) + 1) D_AS((a) + 2) D_AS_OUT((a) + 3)   /* (0.7.5) in + out */
#define AUD_LEN_DUP (92 + 63 + FMT_LEN + 22 + 52)
#define D_AUDIO_STIN(a) D_AC_DUP_CH(a, 10, 0x00) D_MIDI((a) + 1) D_AS10((a) + 2) D_AS_OUT((a) + 3)   /* (0.8.0) */
#define AUD_LEN_STIN (92 + 63 + 11 + 22 + 52)
#endif
#define D_CDC(c)                                                                                     \
    D_IAD((c), 2, 2, 2, 1),                                /* IAD: CDC ACM */                           \
    9, 4, (c), 0, 1, 2, 2, 1, 0,                           /* communication */                          \
    5, 0x24, 0x00, 0x10, 0x01,                             /* header 1.10 */                            \
    5, 0x24, 0x01, 0x00, (c) + 1,                          /* call management: the data IF */           \
    4, 0x24, 0x02, 0x02,                                   /* ACM: line coding + state */               \
    5, 0x24, 0x06, (c), (c) + 1,                           /* union */                                  \
    7, 5, 0x82, 3, 8, 0, 16,                               /* EP2 IN interrupt (never sent) */          \
    9, 4, (c) + 1, 0, 2, 0x0A, 0, 0, 0,                    /* data */                                   \
    7, 5, 0x03, 2, 64, 0, 0,                               /* EP3 OUT bulk */                           \
    7, 5, 0x83, 2, 64, 0, 0,                               /* EP3 IN bulk */
#define CDC_LEN 66                                         /* with its IAD */
#define BCD_LO(cdc) (0x10 * (YEHNAH_UAC + YEHNAH_UAC_48K) + ((cdc) ? 0x01 + 2 * YEHNAH_USB_LAYOUT : 0))
#define D_DEV_LO(bcdusb, cls, sub, proto, lo)                                                        \
    18, 1, (bcdusb) & 0xFF, (bcdusb) >> 8, (cls), (sub), (proto), 64, 0x09, 0x12, YEHNAH_USB_PID & 0xFF, \
    YEHNAH_USB_PID >> 8, (lo), 0x03, 1, 2, 0, 1
#define D_DEV(bcdusb, cls, sub, proto, cdc) D_DEV_LO(bcdusb, cls, sub, proto, BCD_LO(cdc))

/* without the console (YEHNAH_CDC=0, or usb_cdc_on = 0): class 0, no IAD, as a plain USB-MIDI / audio device */
#define CFG_LEN_PLAIN (9 + AUD_LEN)
static const uint8_t DEV_DESC_PLAIN[18] = {D_DEV(0x0110, 0, 0, 0, 0)};
static const uint8_t CFG_DESC_PLAIN[] = {D_CFG(CFG_LEN_PLAIN, AUD_NIF), D_AUDIO(0)};
typedef char cfg_plain_len_ok[sizeof CFG_DESC_PLAIN == CFG_LEN_PLAIN ? 1 : -1];
#if YEHNAH_UAC
/* (0.7.1) MENU > USB STEMS: the plain device with the 8-channel audio function (bcdDevice + 0x40: a host never reuses
 * what it learnt about the stereo one). No console with it (usb_stems_switch turns it off) */
#define CFG_LEN_PLAIN8 (9 + AUD_LEN8)
static const uint8_t DEV_DESC_PLAIN8[18] = {D_DEV_LO(0x0110, 0, 0, 0, BCD_LO(0) + 0x40)};
static const uint8_t CFG_DESC_PLAIN8[] = {D_CFG(CFG_LEN_PLAIN8, AUD_NIF), D_AUDIO8(0)};
typedef char cfg_plain8_len_ok[sizeof CFG_DESC_PLAIN8 == CFG_LEN_PLAIN8 ? 1 : -1];
static volatile uint8_t usb_stems;                         /* the device presents the 8 channels (fx.c fx_stems feeds them) */
#define USB_STEMS usb_stems
/* (0.7.5) MENU > USB IN+OUT: the plain device with the audio output as well (bcdDevice + 0x80), no console */
#define CFG_LEN_DUP (9 + AUD_LEN_DUP)
static const uint8_t DEV_DESC_DUP[18] = {D_DEV_LO(0x0110, 0, 0, 0, BCD_LO(0) + 0x80)};
static const uint8_t CFG_DESC_DUP[] = {D_CFG(CFG_LEN_DUP, AUD_NIF + 1), D_AUDIO_DUP(0)};
typedef char cfg_dup_len_ok[sizeof CFG_DESC_DUP == CFG_LEN_DUP ? 1 : -1];
/* (0.8.0) MENU > USB STEMS+IN: the stems (10 channels) and the host's audio output, both flags set (bcdDevice + 0xC0) */
#define CFG_LEN_STIN (9 + AUD_LEN_STIN)
static const uint8_t DEV_DESC_STIN[18] = {D_DEV_LO(0x0110, 0, 0, 0, BCD_LO(0) + 0xC0)};
static const uint8_t CFG_DESC_STIN[] = {D_CFG(CFG_LEN_STIN, AUD_NIF + 1), D_AUDIO_STIN(0)};
typedef char cfg_stin_len_ok[sizeof CFG_DESC_STIN == CFG_LEN_STIN ? 1 : -1];
#if YEHNAH_TABLET
/* (the big FM-1) MENU > USB TABLET: the plain stereo device and a vendor interface after it, IF 3: EP3 OUT / IN bulk
 * (bcdDevice + 0x08: no other mode sets it), no console */
#define D_VENDOR(v)                                                                                  \
    9, 4, (v), 0, 2, 0xFF, 0, 0, 0,                        /* vendor class, two endpoints */            \
    7, 5, 0x03, 2, 64, 0, 0,                               /* EP3 OUT bulk: the tablet's input */       \
    7, 5, 0x83, 2, 64, 0, 0,                               /* EP3 IN bulk: the screen, INFO .. */
#define CFG_LEN_TAB (9 + AUD_LEN + 23)
static const uint8_t DEV_DESC_TAB[18] = {D_DEV_LO(0x0110, 0, 0, 0, BCD_LO(0) + 0x08)};
static const uint8_t CFG_DESC_TAB[] = {D_CFG(CFG_LEN_TAB, AUD_NIF + 1), D_AUDIO(0) D_VENDOR(AUD_NIF)};
typedef char cfg_tab_len_ok[sizeof CFG_DESC_TAB == CFG_LEN_TAB ? 1 : -1];
#endif
#else
#define USB_STEMS 0
#define USB_DUPLEX 0
#endif
#if YEHNAH_CDC
#define CFG_LEN (9 + 8 + AUD_LEN + CDC_LEN)
#define USB_NIF (AUD_NIF + 2)
#if YEHNAH_USB_LAYOUT == 1
#define AUD_IF_CDC 2u                                      /* the audio function's first interface */
#define CDC_IF_CDC 0u
#else
#define AUD_IF_CDC 0u
#define CDC_IF_CDC AUD_NIF
#endif
#if YEHNAH_USB_LAYOUT == 2
static const uint8_t DEV_DESC[18] = {D_DEV(0x0200, 0x00, 0x00, 0x01, 1)};
#elif YEHNAH_USB_LAYOUT == 3
static const uint8_t DEV_DESC[18] = {D_DEV(0x0200, 0x00, 0x00, 0x00, 1)};
#else
static const uint8_t DEV_DESC[18] = {D_DEV(0x0200, 0xEF, 0x02, 0x01, 1)};   /* misc / IAD */
#endif
static const uint8_t CFG_DESC[] = {
    D_CFG(CFG_LEN, USB_NIF),
#if YEHNAH_USB_LAYOUT == 1
    D_CDC(CDC_IF_CDC)
#endif
    D_IAD(AUD_IF_CDC, AUD_NIF, 1, 1, 0),                  /* IAD: audio + MIDI */
    D_AUDIO(AUD_IF_CDC)
#if YEHNAH_USB_LAYOUT != 1
    D_CDC(CDC_IF_CDC)
#endif
};
typedef char cfg_len_ok[sizeof CFG_DESC == CFG_LEN ? 1 : -1];
static uint8_t usb_cdc_on = YEHNAH_CDC_DEFAULT != 0;      /* the console is presented (read by the TIMER5 ISR) */
#define USB_CDC_ON usb_cdc_on
#define AUD_IF (usb_cdc_on ? AUD_IF_CDC : 0u)
#else
#define DEV_DESC DEV_DESC_PLAIN
#define CFG_DESC CFG_DESC_PLAIN
#define USB_CDC_ON 0
#define AUD_IF 0u
#endif
#define UAC_AS_IF (AUD_IF + 2u)
static const uint8_t STR0[4] = {4, 3, 0x09, 0x04};
static const uint8_t STR1[] = {16, 3, 'Y', 0, 'e', 0, 'h', 0, ' ', 0, 'n', 0, 'a', 0, 'h', 0};   /* Yeh nah (2026-11-04, the user's; was Yeh<>naH); the editor and the installer match /yeh/ */
#ifdef YEHNAH_LOADER
static const uint8_t STR2[] = {30, 3, 'Y', 0, 'e', 0, 'h', 0, ' ', 0, 'n', 0, 'a', 0, 'h', 0, ' ', 0, 'U', 0, 'p', 0, 'd', 0, 'a', 0, 't', 0, 'e', 0};
#else
static const uint8_t STR2[] = {16, 3, 'Y', 0, 'e', 0, 'h', 0, ' ', 0, 'n', 0, 'a', 0, 'h', 0};
#endif

static int get_desc(uint32_t wvalue, const uint8_t **d, uint16_t *l)
{
    switch (wvalue >> 8) {
    case 1:
#if YEHNAH_TABLET
        if (USB_TABLET) { *d = DEV_DESC_TAB; *l = 18; return 1; }
#endif
#if YEHNAH_UAC
        if (USB_STEMS && USB_DUPLEX) { *d = DEV_DESC_STIN; *l = 18; return 1; }
        if (USB_STEMS) { *d = DEV_DESC_PLAIN8; *l = 18; return 1; }
        if (USB_DUPLEX) { *d = DEV_DESC_DUP; *l = 18; return 1; }
#endif
        *d = USB_CDC_ON ? DEV_DESC : DEV_DESC_PLAIN;
        *l = 18;
        return 1;
    case 2:
#if YEHNAH_TABLET
        if (USB_TABLET) { *d = CFG_DESC_TAB; *l = (uint16_t)sizeof CFG_DESC_TAB; return 1; }
#endif
#if YEHNAH_UAC
        if (USB_STEMS && USB_DUPLEX) { *d = CFG_DESC_STIN; *l = (uint16_t)sizeof CFG_DESC_STIN; return 1; }
        if (USB_STEMS) { *d = CFG_DESC_PLAIN8; *l = (uint16_t)sizeof CFG_DESC_PLAIN8; return 1; }
        if (USB_DUPLEX) { *d = CFG_DESC_DUP; *l = (uint16_t)sizeof CFG_DESC_DUP; return 1; }
#endif
        *d = USB_CDC_ON ? CFG_DESC : CFG_DESC_PLAIN;
        *l = (uint16_t)(USB_CDC_ON ? sizeof CFG_DESC : sizeof CFG_DESC_PLAIN);
        return 1;
    case 3:
        switch (wvalue & 0xFFu) {
        case 0:
            *d = STR0;
            *l = sizeof STR0;
            return 1;
        case 1:
            *d = STR1;
            *l = sizeof STR1;
            return 1;
        case 2:
            *d = STR2;
            *l = sizeof STR2;
            return 1;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

/* ---------------------------------------------------------- the SIE --- */
static void sie_wr(uint32_t r, uint32_t v)
{
    uint32_t n = 20000;
    if (!fm1_usb_sie_on())
        return;
    fm1_usb_sie_wr_start(r, v);
    while (!fm1_usb_sie_done() && --n)
        ;
    if (n)
        usb.timeouts = 0;                               /* consecutive failures only */
    else if (++usb.timeouts > 50u)
        usb.up = 0;                                     /* SIE dead (no clock?): stop polling it */
}

static uint32_t sie_rd(uint32_t r)
{
    uint32_t n = 20000;
    if (!fm1_usb_sie_on())
        return 0;
    fm1_usb_sie_rd_start(r);
    while (!fm1_usb_sie_done())
        if (!--n) {
            if (++usb.timeouts > 50u)
                usb.up = 0;
            return 0;
        }
    usb.timeouts = 0;
    return fm1_usb_sie_data();
}

#if YEHNAH_UAC
static void uac_ep4_reset(void)                         /* drop a queued packet; isochronous mode */
{
    sie_wr(S_INDEX, 4);
    sie_wr(S_TXCSR1, 0x48);                             /* ClrDataTog + FlushFIFO (SDK usb_g_ep_config) */
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_TXCSR1, 0);
    sie_wr(S_TXCSR2, 0x40);                             /* ISO */
}

static void uac_stream(uint32_t alt)                    /* SET_INTERFACE, configuration, bus reset */
{
    uac.go = 0;                                         /* the next render primes the ring again */
    uac.flowing = 0;                                    /* ... once the host has taken a packet */
    uac.queued = 0;
    uac.wait = 0;
    RING_PUBLISH();
    uac.alt = (uint8_t)alt;
    uac.acc = 0;
    memset(uac.last, 0, sizeof uac.last);
    if (alt) {                                          /* (the fill range of the last stream stays readable) */
        uac.starts++;
        uac.fill_lo = 0xFFFFFFFFu;
        uac.fill_hi = 0;
    }
}

/* (0.7.5) MENU > USB IN+OUT: EP4 OUT's buffer, isochronous mode, a fresh toggle (configuration; out of line: the SIE
 * writes' waits are not in the TIMER5 body) */
static __attribute__((noinline)) void uin_ep_config(void)
{
    fm1_usb_ep4_rxbuf(ep4rx);
    sie_wr(S_INDEX, 4);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);
    sie_wr(S_RXCSR2, 0x40);
}
/* (0.7.5) MENU > USB IN+OUT: the host starts (alt 1) or stops (alt 0) playing to us: the ring empties, the reader
 * waits for UI_TARGET frames again (uac_in_fill) */
static __attribute__((noinline)) void uin_stream(uint32_t alt)   /* (its SIE writes: out of the TIMER5 body) */
{
    uin.alt = 0;
    RING_PUBLISH();
    uin.gen++;                                          /* the reader drops what the ring holds (ui_r is its own) */
    if (alt) {
        uin.starts++;
        sie_wr(S_INDEX, 4);
        sie_wr(S_RXCSR1, 0x90);                         /* ClrDataTog + flush, as EP1 / EP3 (ep1_config) */
        sie_wr(S_RXCSR2, 0x40);                         /* ISO */
    }
    RING_PUBLISH();
    uin.alt = (uint8_t)(alt != 0u);
}

/* SET_CUR sampling frequency (any time: before or after SET_INTERFACE; a bus reset goes back to 44.1).
 * 48 kHz when built with it and asked for (the nearer of the two), else 44.1. A change restarts the
 * stream: silence packets until the next render primes the ring at the new rate. */
static void uac_rate_set(uint32_t hz)
{
    uint8_t r48 = YEHNAH_UAC_48K && !USB_STEMS && hz >= (UAC_RATE + 48000u) / 2u;   /* (0.7.1: the stems at 44.1 only) */
    if (r48 == uac.r48)
        return;
    uac.go = 0;
    RING_PUBLISH();
    uac.r48 = r48;
    uac.acc = 0;
    uac.pkts = uac.frames = 0;                          /* the measured rate (console) starts again */
}
#endif

static void ep1_config(void)
{
    fm1_usb_ep_txbuf(1, ep1tx);
    fm1_usb_ep_rxbuf(1, ep1rx);
    sie_wr(S_INDEX, 1);
    sie_wr(S_TXMAXP, 0xFF);
    sie_wr(S_TXCSR1, 0x48);
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);
    sie_wr(S_RXCSR2, 0);
    sie_wr(S_INTRRX1E, 0x02);
    fm1_usb_ep_enable(1u << 1);
#if YEHNAH_CDC
    if (!usb_cdc_on)
        goto no_cdc;                                    /* presented without the console: EP2 / EP3 unused */
    fm1_usb_ep_txbuf(2, ep2tx);
    sie_wr(S_INDEX, 2);
    sie_wr(S_TXMAXP, 0xFF);
    sie_wr(S_TXCSR1, 0x48);
    sie_wr(S_TXCSR2, 0);
    fm1_usb_ep_txbuf(3, ep3tx);
    fm1_usb_ep_rxbuf(3, ep3rx);
    sie_wr(S_INDEX, 3);
    sie_wr(S_TXMAXP, 0xFF);
    sie_wr(S_TXCSR1, 0x48);
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);
    sie_wr(S_RXCSR2, 0);
    sie_wr(S_INTRRX1E, 0x0A);
    fm1_usb_ep_enable((1u << 2) | (1u << 3));
    cdc.rx_pend = 1;                                    /* look once: a packet may already wait */
no_cdc:;
#endif
#if YEHNAH_TABLET
    if (usb_tablet) {                                   /* (the big FM-1) EP3 bulk, as the console's (no EP2) */
        fm1_usb_ep_txbuf(3, tab_tx[0]);
        fm1_usb_ep_rxbuf(3, tab_rxb);
        sie_wr(S_INDEX, 3);
        sie_wr(S_TXMAXP, 0xFF);
        sie_wr(S_TXCSR1, 0x48);
        sie_wr(S_TXCSR2, 0);
        sie_wr(S_RXMAXP, 0xFF);
        sie_wr(S_RXCSR1, 0x90);
        sie_wr(S_RXCSR2, 0);
        sie_wr(S_INTRRX1E, 0x0A);
        fm1_usb_ep_enable(1u << 3);
        tab.rx_pend = 1;
        tab.zlp = 0;
    }
#endif
#if YEHNAH_UAC
    fm1_usb_ep4_txbuf(ep4tx);
    sie_wr(S_INDEX, 4);
    sie_wr(S_TXMAXP, 0xFF);
    uac_ep4_reset();
    if (USB_DUPLEX)                                     /* (0.7.5) EP4 OUT: the host's audio, isochronous */
        uin_ep_config();
    fm1_usb_ep_enable(1u << 4);
    uac_stream(0);
    uin_stream(0);
#endif
}

static void e0_chunk(void)
{
    uint32_t n = usb.e0_left > 64u ? 64u : usb.e0_left, i;
    int last;
    for (i = 0; i < n; i++)
        ep0buf[i] = usb.e0_src[i];
    fm1_usb_ep0_send(ep0buf, n);
    usb.e0_src += n;
    usb.e0_left = (uint16_t)(usb.e0_left - n);
    last = usb.e0_left == 0 && !(n == 64u && usb.e0_zlp);
    if (usb.e0_left == 0 && n == 64u)
        usb.e0_zlp = 0;
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, last ? 0x0A : 0x02);
    if (last)
        usb.e0_tx = 0;
}

static void e0_send(const uint8_t *p, uint16_t len, uint16_t wlen)
{
    if (len > wlen)
        len = wlen;
    usb.e0_src = p;
    usb.e0_left = len;
    usb.e0_zlp = (len < wlen) && (len % 64u == 0);
    usb.e0_tx = 1;
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x40);
    e0_chunk();
}

static void ep0_service(void)
{
    static const uint8_t zero2[2];
    uint32_t csr, i;
    uint16_t wvalue, wlength;
    uint8_t *s = usb.last_setup;
    if (usb.has_pend_addr) {
        sie_wr(S_FADDR, usb.pend_addr);
        usb.has_pend_addr = 0;
    }
    sie_wr(S_INDEX, 0);
    csr = sie_rd(S_CSR0);
    if (csr & 0x04u) {                                  /* SentStall */
        sie_wr(S_CSR0, 0);
        usb.e0_tx = 0;
#if YEHNAH_CDC
        cdc.e0_rx = 0;
#endif
#if YEHNAH_UAC
        uac.e0_rx = 0;
        uin.e0_rx = 0;
#endif
        return;
    }
    if (csr & 0x10u) {                                  /* SetupEnd: the host abandoned the transfer */
        sie_wr(S_CSR0, 0x80);
        usb.e0_tx = 0;
#if YEHNAH_CDC
        cdc.e0_rx = 0;                                  /* else the next SETUP is taken as line coding */
#endif
#if YEHNAH_UAC
        uac.e0_rx = 0;
        uin.e0_rx = 0;
#endif
    }
    if (usb.e0_tx) {
        if (!(csr & 0x02u))
            e0_chunk();
        return;
    }
    if (!(csr & 0x01u))
        return;
    fm1_usb_rx_sync();
#if YEHNAH_CDC
    if (cdc.e0_rx) {                                    /* SET_LINE_CODING data stage */
        uint32_t n = sie_rd(S_COUNT0);
        for (i = 0; i < 7u && i < n; i++)
            cdc.line[i] = ep0buf[i];
        cdc.e0_rx = 0;
        goto ack;
    }
#endif
#if YEHNAH_UAC
    if (uac.e0_rx) {                                    /* SET_CUR sampling frequency: 3 bytes, Hz */
        uac.rate_sets++;
        if (sie_rd(S_COUNT0) >= 3u)
            uac_rate_set(ep0buf[0] | (uint32_t)ep0buf[1] << 8 | (uint32_t)ep0buf[2] << 16);
        uac.e0_rx = 0;
        goto ack;
    }
    if (uin.e0_rx) {                                    /* (0.7.5) SET_CUR to EP4 OUT: one rate, taken and ignored */
        uin.e0_rx = 0;
        goto ack;
    }
#endif
    for (i = 0; i < 8u; i++)
        s[i] = ep0buf[i];
    usb.setups++;
    wvalue = (uint16_t)(s[2] | s[3] << 8);
    wlength = (uint16_t)(s[6] | s[7] << 8);
    switch ((uint32_t)s[0] << 8 | s[1]) {
    case 0x0005:                                        /* SET_ADDRESS */
        usb.pend_addr = s[2] & 0x7Fu;
        usb.has_pend_addr = 1;
        goto ack;
    case 0x8006: {                                      /* GET_DESCRIPTOR */
        const uint8_t *d;
        uint16_t l;
        if (get_desc(wvalue, &d, &l)) {
            if (!wlength)
                goto ack;
            e0_send(d, l, wlength);
            return;
        }
        goto stall;
    }
    case 0x0009:                                        /* SET_CONFIGURATION: 0 or 1 only */
        if (s[2] > 1u)
            goto stall;
        usb.config = s[2];
        if (usb.config == 1u)
            ep1_config();
        else
            sie_wr(S_INTRRX1E, 0);
#if YEHNAH_UAC
        if (usb.config != 1u) {
            uac_stream(0);
            uin_stream(0);
        }
        uac_rate_set(UAC_RATE);                         /* a new configuration starts at 44.1 */
#endif
        goto ack;
    case 0x8008:
        e0_send(&usb.config, 1, wlength);
        return;
    case 0x8000:
    case 0x8100:
    case 0x8200:
        e0_send(zero2, 2, wlength);
        return;
    case 0x010B:                                        /* SET_INTERFACE: alt 0 (alt 1 for the audio stream) */
#if YEHNAH_UAC
        if (s[4] == UAC_AS_IF && wvalue <= 1u && usb.config) {
            uac_ep4_reset();
            uac_stream(wvalue);
            goto ack;
        }
        if (USB_DUPLEX && s[4] == UAC_AS_IF + 1u && wvalue <= 1u && usb.config) {   /* (0.7.5) the output */
            uin_stream(wvalue);
            goto ack;
        }
#endif
        if (wvalue == 0)
            goto ack;
        goto stall;
    case 0x810A:
#if YEHNAH_UAC
        if (s[4] == UAC_AS_IF || (USB_DUPLEX && s[4] == UAC_AS_IF + 1u)) {
            static uint8_t alt;
            alt = s[4] == UAC_AS_IF ? uac.alt : uin.alt;
            e0_send(&alt, 1, wlength);
            return;
        }
#endif
        e0_send(zero2, 1, wlength);
        return;
    case 0x0201: {                                      /* CLEAR_FEATURE(ENDPOINT_HALT): data toggle reset */
        uint32_t ep = s[4] & 0x0Fu, last = USB_CDC_ON || USB_TABLET ? 3u : 1u;
#if YEHNAH_UAC
        if (wvalue == 0 && (s[4] == 0x84u || (USB_DUPLEX && s[4] == 0x04u)))
            goto ack;                                   /* isochronous: no halt, no toggle */
#endif
        if (wvalue != 0 || ep > last)
            goto stall;                                 /* not an endpoint we have */
        if (ep) {                                       /* (EP0 has no halt to clear) */
            sie_wr(S_INDEX, ep);
            if (s[4] & 0x80u)
                sie_wr(S_TXCSR1, 0x40);
            else
                sie_wr(S_RXCSR1, 0x80);
        }
        goto ack;
    }
#if YEHNAH_CDC
    case 0x2120:                                        /* SET_LINE_CODING: 7 bytes follow */
        if (!usb_cdc_on)
            goto stall;
        if (wlength) {
            cdc.e0_rx = 1;
            sie_wr(S_INDEX, 0);
            sie_wr(S_CSR0, 0x40);                       /* ServicedRxPktRdy, no DataEnd yet */
            return;
        }
        goto ack;
    case 0xA121:                                        /* GET_LINE_CODING */
        if (!usb_cdc_on)
            goto stall;
        e0_send(cdc.line, 7, wlength);
        return;
    case 0x2122:                                        /* SET_CONTROL_LINE_STATE */
        if (!usb_cdc_on)
            goto stall;
        cdc.dtr = s[2] & 1u;
        goto ack;
    case 0x2123:                                        /* SEND_BREAK */
        if (!usb_cdc_on)
            goto stall;
        goto ack;
#endif
#if YEHNAH_UAC
    case 0x2201:                                        /* SET_CUR, endpoint: sampling frequency */
        if ((s[4] != 0x84u && !(USB_DUPLEX && s[4] == 0x04u)) || s[3] != 1u)
            goto stall;
        if (wlength) {
            if (s[4] == 0x04u)                          /* (0.7.5) the output: 44.1 only, the value ignored */
                uin.e0_rx = 1;
            else
                uac.e0_rx = 1;
            sie_wr(S_INDEX, 0);
            sie_wr(S_CSR0, 0x40);                       /* the 3 bytes follow */
            return;
        }
        goto ack;
    case 0xA281:                                        /* GET_CUR / MIN / MAX: the rate / the range */
    case 0xA282:
    case 0xA283: {
        static const uint8_t RATE[3] = {UAC_RATE & 0xFF, (UAC_RATE >> 8) & 0xFF, UAC_RATE >> 16};
#if YEHNAH_UAC_48K
        static const uint8_t RATE48[3] = {48000u & 0xFF, (48000u >> 8) & 0xFF, 48000u >> 16};
#endif
        if ((s[4] != 0x84u && !(USB_DUPLEX && s[4] == 0x04u)) || s[3] != 1u)   /* (0.7.5: the output, 44.1) */
            goto stall;
#if YEHNAH_UAC_48K
        if (s[4] == 0x84u && (s[1] == 0x83u || (s[1] == 0x81u && uac.r48))) {
            e0_send(RATE48, 3, wlength);
            return;
        }
#endif
        e0_send(RATE, 3, wlength);
        return;
    }
#endif
    default:
        goto stall;
    }
ack:
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x48);
    return;
stall:
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x60);
}

/* SysEx assembly: only short commands matter here */
static void sysex_byte(uint8_t b)
{
    static const uint8_t UBOOT_KEY[6] = {0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7};
    if (b >= 0xF8u)
        return;                                        /* realtime may occur anywhere in SysEx */
    if ((b & 0x80u) && b != 0xF0u && b != 0xF7u) {
        usb.sx_on = 0;
#if YEHNAH_OTA
        sx_collect = 0;
#endif
        return;                                        /* another status aborts the unfinished frame */
    }
    if (b == 0xF0) {
        usb.sx_on = 1;
        usb.sx_len = 0;
#if YEHNAH_OTA
        sx_collect = !sx_ready;
        sx_pos = 0;
#endif
    }
    if (!usb.sx_on)
        return;
#if YEHNAH_OTA
    if (sx_collect && b != 0xF0 && b != 0xF7) {
        if (sx_pos < sizeof sx_frame)
            sx_frame[sx_pos++] = b;
        else
            sx_collect = 0;                             /* too long: not ours */
    }
#endif
    if (usb.sx_len < sizeof usb.sysex)
        usb.sysex[usb.sx_len++] = b;
    if (b == 0xF7) {
        uint32_t i, ok = usb.sx_len == 6u;
        for (i = 0; ok && i < 6u; i++)
            ok = usb.sysex[i] == UBOOT_KEY[i];
        if (ok)
            usb.uboot_req = 1;
#if YEHNAH_OTA
        else if (usb.sx_len == 6u && usb.sysex[1] == 0x22 && usb.sysex[2] == 0x24 && usb.sysex[3] == 0x35 &&
                 usb.sysex[4] == 0x7F)
            usb.ota_req = 1;
        else if (sx_collect && sx_pos) {
            sx_frame_len = sx_pos;
            RING_PUBLISH();
            sx_ready = 1;                               /* main loop: ota_take() / ed_service() */
        }
        sx_collect = 0;
#endif
        usb.sx_on = 0;
    }
}

static void midi_in_event(uint32_t pkt)                 /* one USB-MIDI event packet */
{
    uint32_t cin = pkt & 15u, st = (pkt >> 8) & 0xFFu;
    if (cin >= 4u && cin <= 7u) {
        uint32_t k, nb = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (k = 0; k < nb; k++)
            sysex_byte((uint8_t)(pkt >> (8u * (k + 1u))));
        return;
    }
    if (st >= 0x80u && st < 0xF8u) {
        usb.sx_on = 0;                                 /* channel/system common also ends SysEx */
#if YEHNAH_OTA
        sx_collect = 0;
#endif
    }
    if (cin == 0xFu && (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)) {
        midi_enqueue(0xFu | st << 8, 1u);
        return;
    }
    if (cin >= 8u && cin <= 0xEu && (st >> 4) == cin && !((pkt >> 16) & 0x80u) &&
        ((cin == 0xCu || cin == 0xDu) || !(pkt & 0x80000000u))) {
        midi_enqueue(pkt, 1u);
    }
}

/* one EP1 OUT packet (<= 16 events) into the MIDI ring, or 0: fewer than 16 + 8 slots free, so the packet
 * stays (the host is NAKed) and is retried next poll; a burst never overflows the ring (that panics), and
 * 8 slots stay for TRS MIDI, which shares the ring and cannot wait (31250 baud: about 3 messages a half).
 * The update loader never drains the ring (it wants SysEx only): no back-pressure there. */
#define EP1_ROOM (16u + 8u)
static int ep1_take(const uint8_t *b, uint32_t n)
{
    uint32_t i;
#ifndef YEHNAH_LOADER
    if (MQ - (mi_w - mi_r) < EP1_ROOM)
        return 0;
#endif
    for (i = 0; i + 3u < n; i += 4u)
        midi_in_event((uint32_t)b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16 | (uint32_t)b[i + 3] << 24);
    return 1;
}

static void ep1_rx(void)                                /* leaves the packet (NAK) while the ring is too full */
{
    uint32_t csr, n;
    sie_wr(S_INDEX, 1);
    csr = sie_rd(S_RXCSR1) | (sie_rd(S_RXCSR2) << 8);
    if (!(csr & 1u)) {
        usb.rx_pend = 0;
        return;
    }
    n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
    if (n > 64u)
        n = 64u;
    fm1_usb_rx_sync();
    if (!ep1_take(ep1rx, n))
        return;                                         /* rx_pend stays: retried next poll */
    usb.rx_pend = 0;
    usb.rx_pkts++;
    csr = (csr & ~0x164u) | 0x10u;
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

#if YEHNAH_OTA
/* ---- SysEx frames for the main loop (ota.c / editor.c hooks; yehnah.c and the
 * update loader supply ota_now_ms / ota_idle) ---- */
static uint32_t ota_now_ms(void);
static void ota_idle(void);

static int ota_wire_send(const uint8_t *p, uint32_t n)   /* F0..F7 -> USB-MIDI SysEx packets */
{
    uint32_t i = 0, t0 = ota_now_ms();
    sx_busy = 1;
    while (i < n) {
        uint32_t k = n - i >= 3u ? 3u : n - i, pkt;
        uint32_t cin = k == 3u && i + 3u < n ? 4u : k == 3u ? 7u : 4u + k;   /* 4 continues; 5/6/7 end */
        pkt = cin | (uint32_t)p[i] << 8 | (k > 1u ? (uint32_t)p[i + 1] << 16 : 0u) |
              (k > 2u ? (uint32_t)p[i + 2] << 24 : 0u);
        while (so_w - so_r >= SXQ) {
            if (!*(volatile uint8_t *)&usb.config || ota_now_ms() - t0 > 200u) {
                sx_busy = 0;
                return -1;
            }
            ota_idle();
        }
        sx_out_q[so_w % SXQ] = pkt;
        RING_PUBLISH();
        so_w++;
        i += k;
    }
    sx_busy = 0;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!sx_ready)
        return 0;
    RING_PUBLISH();                                     /* read the frame only after the flag */
    *p = sx_frame;
    *n = sx_frame_len;
    return 1;
}
static void ota_frame_done(void)
{
    RING_PUBLISH();                                     /* done with the frame before the ISR may refill it */
    sx_ready = 0;
}
#endif

#if YEHNAH_PROBE
/* (chord-harp: the big FM-1's step 1, a test build only, YEHNAH_PROBE=1) the link probe: editor.c's PROBE FLOOD
 * fills EP1 (SysEx, 48-byte messages of 16 USB-MIDI events) or EP3 (CDC bulk, 64-byte packets) with numbered packets
 * for a time, `burst` packets a poll (after the first, each waits for the last to go: the SIE holds one); the tablet
 * counts them and the gaps. PROBE CLICK puts a burst into the master (audio.c) for the USB audio's lag. */
static struct {
    volatile uint8_t ep, burst;                         /* 0: off */
    volatile uint32_t left;                             /* polls to go (2 kHz) */
    uint32_t seq, pkts, waits, polls;                   /* waits: a burst cut short (the last packet not gone) */
} pr;
static volatile uint32_t pr_click;                      /* frames of click to render (audio.c) */
static uint32_t pr_fill(uint32_t ep, uint8_t *b, uint32_t last)   /* one numbered packet, its length */
{
    uint32_t s = pr.seq++, i;
    if (ep == 3u) {                                     /* (the last one short: it ends the tablet's read) */
        b[0] = (uint8_t)s;
        b[1] = (uint8_t)(s >> 8);
        b[2] = (uint8_t)(s >> 16);
        b[3] = (uint8_t)(s >> 24);
        for (i = 4; i < 64u; i++)
            b[i] = (uint8_t)(s + i);
        return last ? 32u : 64u;
    }
    {   /* F0 7D 46 4C 64 05 s0 s1 s2, 38 bytes, F7: 48 bytes, 16 events (CIN 4, the last CIN 7) */
        uint8_t m[48];
        m[0] = 0xF0; m[1] = 0x7D; m[2] = 0x46; m[3] = 0x4C; m[4] = 100; m[5] = 5;
        m[6] = (uint8_t)(s & 0x7Fu);
        m[7] = (uint8_t)(s >> 7 & 0x7Fu);
        m[8] = (uint8_t)(s >> 14 & 0x7Fu);
        for (i = 9; i < 47u; i++)
            m[i] = (uint8_t)((s + i) & 0x7Fu);
        m[47] = 0xF7;
        for (i = 0; i < 16u; i++) {
            b[4u * i] = i == 15u ? 7u : 4u;
            b[4u * i + 1u] = m[3u * i];
            b[4u * i + 2u] = m[3u * i + 1u];
            b[4u * i + 3u] = m[3u * i + 2u];
        }
        return 64u;
    }
}
static void pr_flood(uint32_t ep, uint8_t *buf)         /* from ep1_tx / ep3_tx with INDEX = ep, the FIFO free */
{
    uint32_t b, k;
    pr.polls++;
    if (!--pr.left)
        pr.ep = 0;
    for (b = 0; b < pr.burst; b++) {
        if (b) {
            for (k = 0; k < 400u && (sie_rd(S_TXCSR1) & 0x01u); k++)
                ;
            if (sie_rd(S_TXCSR1) & 0x01u) {
                pr.waits++;
                return;
            }
        }
        fm1_usb_ep_send(ep, buf, pr_fill(ep, buf, !pr.ep && b + 1u == pr.burst));
        sie_wr(S_TXCSR1, sie_rd(S_TXCSR1) | 0x01u);
        pr.pkts++;
    }
}
#endif

static void ep1_tx(void)
{
    uint32_t csr, n = 0;
#if YEHNAH_PROBE
    if (pr.ep == 1u && mo_w == mo_r && so_w == so_r && !sx_busy) {
        sie_wr(S_INDEX, 1);
        csr = sie_rd(S_TXCSR1);
        if (csr & 0x01u)
            return;
        if (csr & 0x80u)
            sie_wr(S_TXCSR1, csr & ~0x80u);
        pr_flood(1, ep1tx);
        return;
    }
#endif
#if YEHNAH_OTA
    if (mo_w == mo_r && so_w == so_r)
        return;
#else
    if (mo_w == mo_r)
        return;
#endif
    sie_wr(S_INDEX, 1);
    csr = sie_rd(S_TXCSR1);
    if (csr & 0x01u)
        return;                                         /* previous packet still pending */
    if (csr & 0x80u)
        sie_wr(S_TXCSR1, csr & ~0x80u);
#if YEHNAH_OTA
    while (so_r != so_w && n < 64u) {                   /* SysEx first, never split by notes */
        uint32_t pkt = sx_out_q[so_r % SXQ];
        ep1tx[n] = (uint8_t)pkt;
        ep1tx[n + 1] = (uint8_t)(pkt >> 8);
        ep1tx[n + 2] = (uint8_t)(pkt >> 16);
        ep1tx[n + 3] = (uint8_t)(pkt >> 24);
        n += 4u;
        so_r++;
    }
    while (!sx_busy && so_r == so_w && mo_r != mo_w && n < 64u) {
#else
    while (mo_r != mo_w && n < 64u) {
#endif
        uint32_t pkt;
        RING_PUBLISH();                                 /* the audio ISR (producer) can preempt us: */
        pkt = midi_out_q[mo_r % MQ];                    /* slot read strictly between the index checks */
        ep1tx[n] = (uint8_t)pkt;
        ep1tx[n + 1] = (uint8_t)(pkt >> 8);
        ep1tx[n + 2] = (uint8_t)(pkt >> 16);
        ep1tx[n + 3] = (uint8_t)(pkt >> 24);
        n += 4u;
        RING_PUBLISH();
        mo_r++;
    }
    if (!n)
        return;
    fm1_usb_ep_send(1, ep1tx, n);
    sie_wr(S_TXCSR1, sie_rd(S_TXCSR1) | 0x01u);
    usb.tx_pkts++;
}

#if YEHNAH_CDC
static void ep3_rx(void)                                /* leaves the packet (NAK) while the ring is full */
{
    uint32_t csr, n, i;
    sie_wr(S_INDEX, 3);
    csr = sie_rd(S_RXCSR1) | (sie_rd(S_RXCSR2) << 8);
    if (!(csr & 1u)) {
        cdc.rx_pend = 0;
        return;
    }
    n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
    if (n > 64u)
        n = 64u;
    if (CI_N - (ci_w - ci_r) < n)
        return;                                         /* rx_pend stays: retried next poll */
    cdc.rx_pend = 0;
    fm1_usb_rx_sync();
    for (i = 0; i < n; i++)
        cdc_in[(ci_w + i) % CI_N] = ep3rx[i];
    RING_PUBLISH();
    ci_w += n;
    cdc.rx_pkts++;
    csr = (csr & ~0x164u) | 0x10u;
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

static void ep3_tx(void)                                /* <= 63 bytes per packet: never needs a ZLP */
{
    uint32_t csr, n = 0;
#if YEHNAH_PROBE
    if (co_w == co_r && pr.ep != 3u)
        return;
#else
    if (co_w == co_r)
        return;
#endif
    sie_wr(S_INDEX, 3);
    csr = sie_rd(S_TXCSR1);
    if (csr & 0x01u)
        return;
    if (csr & 0x80u)
        sie_wr(S_TXCSR1, csr & ~0x80u);
#if YEHNAH_PROBE
    if (pr.ep == 3u && co_w == co_r) {
        pr_flood(3, ep3tx);
        return;
    }
#endif
    while (co_r != co_w && n < 63u)
        ep3tx[n++] = cdc_out[co_r++ % CO_N];
    fm1_usb_ep_send(3, ep3tx, n);
    sie_wr(S_TXCSR1, sie_rd(S_TXCSR1) | 0x01u);
    cdc.tx_pkts++;
}
#endif

#if YEHNAH_TABLET
/* one message into tab_q whole, or nothing (0: not live, or no room; counted). The main loop's only */
static int tab_put(uint32_t type, const uint8_t *p, uint32_t n)
{
    uint32_t w = tq_w, fill = w - tq_r, i;
    if (!tab.live || TQ_N - fill < n + 3u) {
        tab.full++;
        return 0;
    }
    tab_q[w++ % TQ_N] = (uint8_t)type;
    tab_q[w++ % TQ_N] = (uint8_t)n;
    tab_q[w++ % TQ_N] = (uint8_t)(n >> 8);
    for (i = 0; i < n; i++)
        tab_q[w++ % TQ_N] = p[i];
    if (fill + n + 3u > tab.max_fill)
        tab.max_fill = fill + n + 3u;
    RING_PUBLISH();
    tq_w = w;
    return 1;
}
/* (step 2) a message written in place, for the screen's tiles (tab_screen.c: no buffer of their own): tab_open checks
 * its room and writes the head as tab_put does, then exactly n tab_byte calls, then tab_close publishes it. The main
 * loop's only */
static uint32_t tab_wp;
static int tab_open(uint32_t type, uint32_t n)
{
    uint32_t w = tq_w;
    if (!tab.live || TQ_N - (w - tq_r) < n + 3u) {
        tab.full++;
        return 0;
    }
    tab_q[w++ % TQ_N] = (uint8_t)type;
    tab_q[w++ % TQ_N] = (uint8_t)n;
    tab_q[w++ % TQ_N] = (uint8_t)(n >> 8);
    tab_wp = w;
    return 1;
}
static inline void tab_byte(uint32_t b) { tab_q[tab_wp++ % TQ_N] = (uint8_t)b; }
static void tab_close(void)
{
    if (tab_wp - tq_r > tab.max_fill)
        tab.max_fill = tab_wp - tq_r;
    RING_PUBLISH();
    tq_w = tab_wp;
}
static uint32_t tab_room(void) { return TQ_N - (tq_w - tq_r); }

static void tab_ep_rx(void)                             /* leaves the packet (NAK) while the main loop has the last */
{
    uint32_t csr, n, i;
    if (tab_rx_n)
        return;                                         /* rx_pend stays: retried next poll */
    sie_wr(S_INDEX, 3);
    csr = sie_rd(S_RXCSR1) | (sie_rd(S_RXCSR2) << 8);
    tab.rx_pend = 0;
    if (!(csr & 1u))
        return;
    n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
    if (n > 64u)
        n = 64u;
    fm1_usb_rx_sync();
    for (i = 0; i < n; i++)
        tab_rx[i] = tab_rxb[i];
    RING_PUBLISH();
    tab_rx_n = (uint8_t)n;                              /* (a zero-length packet: nothing to hand over) */
    tab.rx_pkts++;
    csr = (csr & ~0x164u) | 0x10u;
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

static uint32_t tab_pack(uint8_t *b)                    /* the next packet from tab_q into b (tests/tablet_link_test.c) */
{
    uint32_t n = 0, r = tq_r, w = tq_w;
    RING_PUBLISH();
    while (r != w && n < 64u)
        b[n++] = tab_q[r++ % TQ_N];
    RING_PUBLISH();
    tq_r = r;
    tab.zlp = n == 64u;                                 /* a full packet: a ZLP next if nothing follows it */
    return n;
}

static void tab_ep_tx(void)                             /* one packet a call at most: <= 64 bytes, or a ZLP */
{
    uint32_t csr, n;
    uint8_t *b;
    if (tq_w == tq_r && !tab.zlp)
        return;
    sie_wr(S_INDEX, 3);
    csr = sie_rd(S_TXCSR1);
    if (csr & 0x01u)
        return;                                         /* the last packet not yet gone */
    if (csr & 0x80u)
        sie_wr(S_TXCSR1, csr & ~0x80u);
    b = tab_tx[tab.buf];
    tab.buf ^= 1u;
    n = tab_pack(b);
    fm1_usb_ep_send(3, b, n);
    sie_wr(S_TXCSR1, sie_rd(S_TXCSR1) | 0x01u);
    tab.pkts++;
    tab.bytes += n;
    if (!n)
        tab.zlps++;
}

/* (the big FM-1, page changes b) the main loop's: the next packet once the SIE took the last, between the ISR's polls.
 * Interrupts off around it (the caller's): the SIE and INDEX stay one owner's at a time (core 1 takes no interrupts and
 * no USB) */
static void tab_ep_more(void)
{
    if (usb.config && usb_tablet)
        tab_ep_tx();
}
#endif

#if YEHNAH_UAC
/* ---- the audio input: audio.c produces (render start, then each block), TIMER5 consumes ---- */
#if YEHNAH_UAC_48K
static void uac_collect(int wait);
static void uac_rs_init(void)                           /* the resampler from silence */
{
    uint32_t i;
    for (i = 0; i < 2u * RS_H; i++)
        rs.l[i] = rs.r[i] = 0;
    rs.w = rs.ph = 0;
}
#endif
static __attribute__((noinline)) void uac_render_start(void)   /* audio ISR, before a half buffer renders */
{
    uint32_t fill;
    uac.feed = uac.alt && usb.config && uac.flowing;
#if YEHNAH_UAC_48K
    rq.nin = 0;
    fill = ua_w - ua_r;                                 /* (CORES 2: the low point, before the last half comes in) */
    uac_collect(0);
    if (uac.ring48 && rq.c1 != UAC_C1_LIVE())           /* (core 1 given up mid-stream, or back) */
        uac.go = 0;
#else
    fill = ua_w - ua_r;
#endif
    if (!uac.feed) {
        uac.go = 0;                                     /* not read (yet / any more): prime when it is */
        return;
    }
    if (!uac.go || uac.ring48 != uac.r48) {             /* (re)start: silence ahead, then this render */
        uint32_t r = ua_r, i, prime = UA_PRIME + (UA_FW > 1u ? UA_PAD_STEMS : 0u);
        uac.go = 0;
        uac.owe = 0;
#if YEHNAH_UAC_48K
        if (rq.busy)                                    /* (a half still on core 1: prime when it is in) */
            return;
        uac.ring48 = uac.r48;
        rq.c1 = 0;
        if (uac.ring48) {                               /* the resampler starts from silence */
            rq.c1 = (uint8_t)UAC_C1_LIVE();
            prime = UA_PRIME48 + (rq.c1 ? UA_REND48 : 0u);
            uac_rs_init();
        }
#endif
        for (i = 0; i < prime * UA_FW; i++)
            ua_ring[ua_slot(r + i / UA_FW) * UA_FW + i % UA_FW] = 0;
        RING_PUBLISH();
        ua_w = r + prime;
        uac.fill_min = prime;
        RING_PUBLISH();
        uac.go = 1;
        return;
    }
    uac.fill_min = fill;
    if (fill < uac.fill_lo)
        uac.fill_lo = fill;
    if (fill > uac.fill_hi)
        uac.fill_hi = fill;
}

static inline int32_t uac_clip(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
#ifndef UAC_GAIN
#define UAC_GAIN() 4096                /* (a build without fx.c: the host tests of USB alone) */
#endif
static inline int32_t uac_s16(int32_t v)
{
    v = (v * UAC_GAIN()) >> 12;                       /* (0.7.0) MENU > USB LEVEL -6 .. -24 dB (fx.c fx_usb_gain) */
    return v > 32767 ? 32767 : v < -32768 ? -32768 : v;
}

#if YEHNAH_UAC_48K
#define UAC_CLIP(v) ((v) > 32767 ? 32767 : (v) < -32768 ? -32768 : (v))
/* 44.1 -> 48 kHz (x 160 / 147): n input frames (16-bit L | R << 16) into the ring from frame w; each input frame enters
 * the history, then the 1 or 2 output frames whose newest input it is: phase ph, the window of UAC_FIR_N frames ending
 * at it, phase ph's taps (rs_tab), Q30 into 64-bit sums, four taps a pass. Returns the frames written. (0.8.9) In RAM
 * and calling nothing: core 1 runs it outside the audio ISR (uac_rs_job), core 0 with one core; bit for bit the
 * resampler of 0.7 .. 0.8.8 */
static UAC_C1_TEXT uint32_t uac_rs_run(const uint32_t *in, uint32_t n, uint32_t w)
{
    uint32_t i, j, k = 0, h = rs.w, ph = rs.ph;
    if (ph >= UAC_FIR_L)                                /* (core 1 may misread a word: never past the table) */
        ph = 0;
    for (i = 0; i < n; i++) {
        uint32_t f = in[i], s = h & (RS_H - 1u);
        rs.l[s] = rs.l[s + RS_H] = (int16_t)f;
        rs.r[s] = rs.r[s + RS_H] = (int16_t)(f >> 16);
        h++;
        s = (h - UAC_FIR_N) & (RS_H - 1u);              /* the window: oldest .. this frame */
        do {
            const int16_t *x = &rs.l[s];                /* R: x[2 * RS_H] */
            const int32_t *c = rs_tab[ph];
            int64_t al = 0, ar = 0;
            int32_t yl, yr;
            for (j = UAC_FIR_N / 4u; j; j--, x += 4, c += 4) {
                int32_t c0 = c[0], c1 = c[1], c2 = c[2], c3 = c[3];
                al += (int64_t)c0 * x[0];
                ar += (int64_t)c0 * x[2u * RS_H];
                al += (int64_t)c1 * x[1];
                ar += (int64_t)c1 * x[2u * RS_H + 1u];
                al += (int64_t)c2 * x[2];
                ar += (int64_t)c2 * x[2u * RS_H + 2u];
                al += (int64_t)c3 * x[3];
                ar += (int64_t)c3 * x[2u * RS_H + 3u];
            }
            yl = (int32_t)((al + (1 << (UAC_FIR_Q - 1))) >> UAC_FIR_Q);
            yr = (int32_t)((ar + (1 << (UAC_FIR_Q - 1))) >> UAC_FIR_Q);
            yl = UAC_CLIP(yl);
            yr = UAC_CLIP(yr);
#if YEHNAH_UAC_TONE
            {   /* bench: the triangles at 48 kHz (not resampled), one per output frame */
                uint32_t t = rs.tone, q = t < 50u ? t : 100u - t, q2 = t < 50u ? 50u - t : t - 50u;
                rs.tone = t == 99u ? 0u : t + 1u;
                yl = (int32_t)q * 1000 - 25000;
                yr = (int32_t)q2 * 1000 - 25000;
            }
#endif
            ua_ring[(w + k++) & (UA_N - 1u)] = (uint32_t)(uint16_t)yl | (uint32_t)(uint16_t)yr << 16;
            ph += UAC_FIR_M;
        } while (ph < UAC_FIR_L);
        ph -= UAC_FIR_L;
    }
    rs.w = h;
    rs.ph = ph;
    return k;
}
static UAC_C1_TEXT void uac_rs_job(uint32_t a)          /* core 1: the half core 0 handed over (uac_render_end) */
{
    uint32_t n = rq.n;
    (void)a;
    rq.k = uac_rs_run(rs_in, n <= HALF_FRAMES ? n : 0u, rq.w0);
}
/* Publish core 1's half only after completion. The mix can skip its job (silent parts / buses), so its wait is not
 * guaranteed. Before reusing rs_in, wait here if necessary; only a stopped core can relinquish an unfinished half. */
static void uac_collect(int wait)
{
    uint32_t k;
    if (!rq.busy)
        return;
    if (!UAC_C1_DONE(rq.job) && UAC_C1_LIVE()) {
        if (!wait)
            return;
        (void)UAC_C1_WAIT();                            /* bounded, stops core 1 on a fault / timeout (fm1_multicore.h) */
        if (!UAC_C1_DONE(rq.job) && UAC_C1_LIVE())
            return;                                    /* keep ownership if a backend did not finish or stop it */
    }
    if (!UAC_C1_DONE(rq.job)) {
        rq.busy = uac.go = 0;                           /* core 1 stopped: the partial half is discarded */
        return;
    }
    k = rq.k;                                          /* after DONE's publication barrier, never before it */
    rq.busy = 0;
    if (k <= UA_REND48 + 1u) {                          /* (a wrong word from core 1: the half dropped) */
        RING_PUBLISH();
        ua_w = rq.w0 + k;
    }
}
/* (chord-harp fork, 2026-11-04) interrupts off, before the flash goes off (fm1_flash.h FL_QUIET): core 1's half done
 * first, as it reads rs_tab from the flash (no new half starts: the audio ISR cannot run) */
static void uac_flash_quiet(void)
{
    uac_collect(1);
}
/* the audio ISR, after each block: 16-bit frames, then (one core) resampled into the ring at once, or (CORES 2) kept
 * for the half's end */
static __attribute__((noinline)) void uac_tap48(const int32_t *out, uint32_t n)
{
    uint32_t i, *d;
    if (rq.c1) {
        uac_collect(1);                                 /* the mix may have skipped its core-1 job and wait */
        if (rq.busy || !uac.go || rq.nin + n > HALF_FRAMES)
            return;
        d = &rs_in[rq.nin];
        rq.nin += n;
    } else {
        if (UA_N - (ua_w - ua_r) < n + n / 8u + 2u) {   /* nobody reads: drop the block (<= n * 160 / 147 + 1) */
            uac.overruns++;
            return;
        }
        d = rs_in;
    }
    for (i = 0; i < n; i++)
        d[i] = (uint32_t)(uint16_t)uac_s16(out[2u * i]) | (uint32_t)(uint16_t)uac_s16(out[2u * i + 1u]) << 16;
    if (!rq.c1) {
        uint32_t w = ua_w, k = uac_rs_run(rs_in, n, w);
        RING_PUBLISH();
        ua_w = w + k;
    }
}
#endif

/* (0.8.9) the audio ISR, after the render: CORES 2 at 48 kHz, the half to core 1 (it resamples while core 0 goes back
 * to the main loop; the next half takes it in); core 1 gone, here */
static __attribute__((noinline)) void uac_render_end(void)
{
#if YEHNAH_UAC_48K
    uint32_t n = rq.nin, w = ua_w;
    rq.nin = 0;
    if (!n || rq.busy || !uac.feed)
        return;
    if (UA_N - (w - ua_r) < n + n / 8u + 2u) {          /* nobody reads: drop the half */
        uac.overruns++;
        return;
    }
    rq.n = n;
    rq.w0 = w;
    RING_PUBLISH();
    if (UAC_C1_LIVE() && UAC_C1_RUN(uac_rs_job)) {
        rq.job = UAC_C1_JOB();
        rq.busy = 1;
    } else {
        uac.go = 0;                                     /* (core 1 given up: one core from the next half) */
    }
#endif
}

static __attribute__((noinline)) void uac_tap(const int32_t *out, uint32_t n)   /* audio ISR: one block (Q15) */
{
    uint32_t w = ua_w, i;
    if (!uac.feed || !uac.go)                           /* ((0.8.9) not primed: a half on core 1 to wait for) */
        return;
#if YEHNAH_UAC_48K
    if (uac.ring48) {
        uac_tap48(out, n);
        return;
    }
#endif
    if (UA_N - (w - ua_r) < n) {                        /* nobody reads: drop the block */
        uac.overruns++;
        return;
    }
#if YEHNAH_UAC_TONE
    {   /* bench: triangles of period 100 frames instead of the music (L, R in antiphase): every
         * recorded frame must follow the one before, so a lost or repeated frame shows exactly */
        static uint32_t ph;
        for (i = 0; i < n; i++, ph = ph == 99u ? 0u : ph + 1u) {
            uint32_t q = ph < 50u ? ph : 100u - ph, q2 = ph < 50u ? 50u - ph : ph - 50u;
            ua_ring[(w + i) & (UA_N - 1u)] = (uint32_t)(uint16_t)(int16_t)((int32_t)q * 1000 - 25000) |
                                             (uint32_t)(uint16_t)(int16_t)((int32_t)q2 * 1000 - 25000) << 16;
        }
        (void)out;
    }
#else
    for (i = 0; i < n; i++)
        ua_ring[(w + i) & (UA_N - 1u)] = (uint32_t)(uint16_t)uac_s16(out[2u * i]) |
                                         (uint32_t)(uint16_t)uac_s16(out[2u * i + 1u]) << 16;
#endif
    RING_PUBLISH();
    ua_w = w + n;
}
#ifdef UAC_STEMS_SRC
/* (0.7.1) MENU > USB STEMS: a block as 8 channels (fx.c stem_buf: the six parts mono, stem_fx: the FX return L R),
 * 16 bit each, four words a frame; the ring, the priming and the packets as the stereo stream's (44.1 kHz only).
 * (0.8.0) STEMS+IN: two more, a fifth word: the host's audio as the mix took it (stem_buf[NPART]: its LEVEL, PAN and
 * MUTE, in its own channel so the host can move it by the latency) and as it came (uin_l / uin_r halved: the
 * reference the host finds its latency against) */
static __attribute__((noinline)) void uac_tap8(uint32_t n)
{
    uint32_t w = ua_w, i, k;
    if (!uac.feed)
        return;
    if (UA_NF - (w - ua_r) < n) {
        uac.overruns++;
        return;
    }
    for (i = 0; i < n; i++) {
        uint32_t *f = &ua_ring[ua_slot(w + i) * UA_FW];
        for (k = 0; k < 3u; k++)
            f[k] = (uint32_t)(uint16_t)uac_s16(stem_buf[2u * k][i]) | (uint32_t)(uint16_t)uac_s16(stem_buf[2u * k + 1u][i]) << 16;
        f[3] = (uint32_t)(uint16_t)uac_s16(stem_fx[0][i]) | (uint32_t)(uint16_t)uac_s16(stem_fx[1][i]) << 16;
        if (UA_FW == 5u)
            f[4] = (uint32_t)(uint16_t)uac_s16(stem_buf[6][i]) |
                   (uint32_t)(uint16_t)(uin_on ? (int16_t)((uin_l[i] + uin_r[i]) >> 1) : 0) << 16;
    }
    RING_PUBLISH();
    ua_w = w + n;
}
#define UAC_TAP8 1                 /* (audio.c: a build with fx.c but without usb.c, audio_test, has no stems tap) */
#endif

/* (0.9.8) a frame made up after an underrun (uac_packet): the ring's n + 1 frames from r read into the packet's n, each
 * channel linearly between its two neighbours (the step 1 + 1 / (n + 1) frame), so the frame left out is spread over
 * the millisecond instead of skipped (a skip is a step in a soloed stem: a click). Each 16-bit channel of each word */
static __attribute__((noinline)) void uac_squeeze(uint32_t *d, uint32_t r, uint32_t n)
{
    uint32_t i, j, fw = UA_FW, st = (16384u << 8) / (n + 1u), q = st;
    for (i = 0; i < n; i++, q += st) {
        const uint32_t *a = &ua_ring[ua_slot(r + i) * fw], *b = &ua_ring[ua_slot(r + i + 1u) * fw];
        int32_t w = (int32_t)(q >> 8);                  /* (i + 1) / (n + 1), Q14 */
        for (j = 0; j < fw; j++) {
            int32_t al = (int16_t)a[j], ah = (int16_t)(a[j] >> 16);
            int32_t l = al + ((((int32_t)(int16_t)b[j] - al) * w + 8192) >> 14);
            int32_t h = ah + ((((int32_t)(int16_t)(b[j] >> 16) - ah) * w + 8192) >> 14);
            d[i * fw + j] = (uint32_t)(uint16_t)l | (uint32_t)(uint16_t)h << 16;
        }
    }
}

/* the next packet into d: 44 / 45 frames (+-1 to hold the ring's fill), from the ring or, until it is
 * primed, silence. An empty ring repeats the last frame. Returns the frame count. */
static uint32_t uac_packet(uint32_t *d)
{
    uint32_t n = 44u, r, fill, k, i, pad = UA_FW > 1u ? UA_PAD_STEMS : 0u, lo = UA_LO + pad, hi = UA_HI + pad;
#if YEHNAH_UAC_48K
    if (uac.r48) {                                      /* 48 frames a ms, no pattern */
        n = 48u;
        lo = UA_LO48;
        hi = UA_HI48;
    } else
#endif
    {
        uac.acc += UAC_RATE % 1000u;
        if (uac.acc >= 1000u) {
            uac.acc -= 1000u;
            n++;
        }
    }
    if (!uac.go || uac.ring48 != uac.r48) {             /* not primed (at this rate) yet */
        for (i = 0; i < n * UA_FW; i++)
            d[i] = 0;
        return n;
    }
    if (uac.fill_min > hi) {
        n++;
        uac.adj_up++;
    } else if (uac.fill_min < lo) {
        n--;
        uac.adj_down++;
    }
    r = ua_r;
    fill = ua_w - r;
    RING_PUBLISH();                                     /* the slots after the index */
    if (uac.owe && uac.fill_min >= lo && fill > n) {    /* (0.8.9) a frame an underrun repeated, made up: one */
        uac.owe--;                                      /* frame more of the ring taken, so the stream keeps the */
        uac.repaid++;                                   /* render's time and length (a packet still carries n). */
        uac_squeeze(d, r, n);                           /* (0.9.8) n + 1 frames read into n, not one skipped: a */
        RING_PUBLISH();                                 /* skip was a step in every stem (a click soloed) */
        ua_r = r + n + 1u;
        uac.pkts++;
        uac.frames += n;
        for (i = 0; i < UA_FW; i++)
            uac.last[i] = d[(n - 1u) * UA_FW + i];
        return n;
    }
    k = fill < n ? fill : n;
    {
        uint32_t fw = UA_FW, j;
        for (i = 0; i < k; i++)
            for (j = 0; j < fw; j++)
                d[i * fw + j] = ua_ring[ua_slot(r + i) * fw + j];
        if (k < n) {
            uac.underruns++;
            uac.owe += n - k;
            for (; i < n; i++)
                for (j = 0; j < fw; j++)
                    d[i * fw + j] = k ? d[(k - 1u) * fw + j] : uac.last[j];
        }
        for (j = 0; j < fw; j++)
            uac.last[j] = d[(n - 1u) * fw + j];
    }
    RING_PUBLISH();
    ua_r = r + k;
    uac.pkts++;
    uac.frames += n;
    return n;
}

/* TIMER5, 2 kHz, also nested in the render: the host takes one packet per 1 ms frame; queue the
 * next one as soon as the last has gone (TxPktRdy clear). Not paced on SOF-pending (see usb_poll).
 * The ring is fed only while the host reads: it often selects alt 1 long before its first IN token,
 * and a recorder may stop reading without going back to alt 0 (a packet waiting > 20 ms). */
static void uac_service(void)
{
    uint32_t csr, n;
    if (!usb.up || !usb.config || !uac.alt)
        return;
    sie_wr(S_INDEX, 4);
    csr = sie_rd(S_TXCSR1);
    if (csr & 0x01u) {                                  /* still queued */
        if (uac.wait < 255u && ++uac.wait == 40u && uac.flowing) {
            uac.flowing = 0;
            uac.stalls++;
        }
        return;
    }
    if (uac.queued) {
        if (csr & 0x04u)
            uac.missed++;                               /* UnderRun: an IN token found nothing queued */
        uac.flowing = 1;
    }
    uac.queued = 1;
    uac.wait = 0;
    n = uac_packet(ep4tx);
    fm1_usb_ep4_send(ep4tx, n * 4u * UA_FW);
    sie_wr(S_TXCSR1, 0x01);                             /* TxPktRdy (and UnderRun cleared) */
}

/* (0.7.5) MENU > USB IN+OUT, TIMER5: a packet the host played to us (n bytes of L | R << 16 frames) into ui_ring;
 * a full ring drops it (the reader has stopped: it re-primes) */
static __attribute__((noinline)) void uin_take(const uint32_t *p, uint32_t n)   /* (not in the TIMER5 body: its copy loop) */
{
    uint32_t w = ui_w, i, f = n / 4u;
    if ((n & 3u) || f > UI_MAXF) {
        uin.bad++;
        if (f > UI_MAXF)
            f = UI_MAXF;
    }
    if (UI_N - (w - ui_r) < f) {
        uin.overruns++;
        return;
    }
    for (i = 0; i < f; i++)
        ui_ring[(w + i) & (UI_N - 1u)] = p[i];
    RING_PUBLISH();
    ui_w = w + f;
    uin.pkts++;
    uin.frames += f;
}
static __attribute__((noinline)) void uin_rx(void)
{
    uint32_t csr, n;
    sie_wr(S_INDEX, 4);
    csr = sie_rd(S_RXCSR1) | (sie_rd(S_RXCSR2) << 8);
    if (!(csr & 1u))                                    /* RxPktRdy */
        return;
    n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
    if (n > sizeof ep4rx)
        n = sizeof ep4rx;
    fm1_usb_rx_sync();
    uin_take(ep4rx, n);
    csr = (csr & ~0x164u) | 0x10u;                      /* serviced, as EP1 / EP3 (ep1_rx); ISO kept */
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

#ifdef UIN_SRC
/* (0.7.5) the audio ISR, before the mix: n frames of the host's audio into fx.c's uin_l / uin_r (Q15), read at the
 * I2S clock: a linear interpolation between ring frames, its step steered by the ring's fill (smoothed over ~0.4 s)
 * toward UI_TARGET: the host's rate, whatever the clocks. Primed: UI_TARGET frames in hand; it fades in. Short of
 * frames (the host stopped, a glitch): it fades out from the last frame and primes again. uin_on: this block holds it */
static __attribute__((noinline)) void uac_in_fill(uint32_t n)
{
    uint32_t r = ui_r, fill = ui_w - r, i, need, ph = uin.ph;
    int32_t step = uin.step, g = uin.g;
    if (!uin.alt || !usb.config) {
        uin.go = 0;
        uin_on = 0;
        return;
    }
    if (uin.gen_seen != uin.gen) {                      /* a new stream: nothing of the last one */
        uin.gen_seen = uin.gen;
        uin.go = 0;
        RING_PUBLISH();
        ui_r = r = ui_w;
        fill = 0;
    }
    if (!uin.go) {
        if (fill < UI_TARGET) {
            uin_on = 0;
            return;
        }
        uin.go = 1;
        ph = 0;
        step = 65536;
        g = 0;
        uin.fs = (int32_t)fill << 8;
    }
    need = (uint32_t)(((uint64_t)ph + (uint64_t)(uint32_t)step * n) >> 16) + 2u;
    RING_PUBLISH();                                     /* the slots after the index */
    if (fill < need) {                                  /* short: fade out from the last frame, prime again */
        int32_t d = g / (int32_t)n + 1;
        uin.underruns++;
        uin.go = 0;
        for (i = 0; i < n; i++) {
            g = g > d ? g - d : 0;
            uin_l[i] = (uin.last_l * g) >> 15;
            uin_r[i] = (uin.last_r * g) >> 15;
        }
        uin.g = 0;
        uin_on = 1;
        return;
    }
    for (i = 0; i < n; i++) {
        uint32_t a = ui_ring[r & (UI_N - 1u)], b = ui_ring[(r + 1u) & (UI_N - 1u)], f = ph & 0xFFFFu;
        int32_t al = (int16_t)a, ar = (int16_t)(a >> 16), bl = (int16_t)b, br = (int16_t)(b >> 16);
        int32_t l = al + (((bl - al) * (int32_t)f) >> 16), rr = ar + (((br - ar) * (int32_t)f) >> 16);
        if (g < 32767)
            g = g + 1024 < 32767 ? g + 1024 : 32767;    /* (a fade in over ~32 frames) */
        uin_l[i] = (l * g) >> 15;
        uin_r[i] = (rr * g) >> 15;
        uin.last_l = l;
        uin.last_r = rr;
        ph += (uint32_t)step;
        r += ph >> 16;
        ph &= 0xFFFFu;
    }
    RING_PUBLISH();
    ui_r = r;
    fill = ui_w - r;
    uin.fs += (((int32_t)fill << 8) - uin.fs) >> 7;     /* (a block a ~0.7 ms: ~90 ms; the packets' saw tooth averaged) */
    {
        int32_t e = (uin.fs >> 8) - (int32_t)UI_TARGET;   /* more in hand than wanted: read faster */
        step = 65536 + (e > UI_STEP_MAX ? UI_STEP_MAX : e < -UI_STEP_MAX ? -UI_STEP_MAX : e);
    }
    uin.ph = ph;
    uin.step = step;
    uin.g = g;
    uin_on = 1;
}
#define UAC_UIN 1                  /* (audio.c: a build with fx.c and usb.c reads the host's audio before the mix) */
#endif
#endif

static void usb_poll(void)                              /* TIMER5 ISR, 2 kHz */
{
    uint32_t iu, it, ir;
    if (!usb.up)
        return;
    if (fm1_usb_sof_take()) {                           /* SOF pending: NOT a reliable "host is there" */
                                                        /* (it keeps firing with the cable out) */
        usb.sof_seen++;
        usb.no_sof = 0;
    } else if (++usb.no_sof > usb.max_gap) {
        usb.max_gap = usb.no_sof;
    }
    if (usb.config) {                                   /* the frame number only advances with a real host */
        static uint32_t tick;
        if (++tick >= 20u) {                            /* every 20 polls = 10 ms */
            uint16_t f;
            tick = 0;
            f = (uint16_t)(sie_rd(S_FRAME1) | (sie_rd(S_FRAME2) & 7u) << 8);
            if (f != usb.frame) {
                usb.frame = f;
                usb.frame_same = 0;
                usb.suspended = 0;
            } else if (++usb.frame_same >= 10u && !usb.suspended) {   /* 100 ms frozen: unplugged / host asleep */
                usb.suspended = 1;
                usb.frame_stalls++;
            }
        }
    }
    iu = sie_rd(S_INTRUSB);
#ifdef YEHNAH_LOADER
    it = sie_rd(S_INTRTX1) | sie_rd(S_INTRTX2) << 8;    /* the update loader reads both */
    ir = sie_rd(S_INTRRX1) | sie_rd(S_INTRRX2) << 8;
#else
    it = sie_rd(S_INTRTX1);                             /* EP0..7 only: skipping INTR*2 saves */
    ir = sie_rd(S_INTRRX1);                             /* 2 SIE round trips per poll */
#endif
    if (iu & 0x01u) {                                   /* suspend: the bus went idle */
        usb.suspends++;
        usb.suspended = 1;
    }
    if (iu & 0x02u)                                     /* resume */
        usb.suspended = 0;
    if (iu & 0x04u) {                                   /* bus reset */
        usb.resets++;
        sie_wr(S_FADDR, 0);
        usb.config = 0;
        usb.suspended = 0;
        mo_r = mo_w;                                    /* nothing stale for the next host */
        usb.sx_on = 0;
#if YEHNAH_PROBE
        pr.ep = 0;
#endif
#if YEHNAH_TABLET
        tab.live = 0;                                   /* the next tablet says HELLO afresh */
        tab.zlp = 0;
        tab.rx_pend = 0;
        tq_r = tq_w;
#endif
#if YEHNAH_OTA
        so_r = so_w;
        sx_collect = 0;
#endif
        usb.e0_tx = 0;
        usb.has_pend_addr = 0;
        usb.rx_pend = 0;
#if YEHNAH_CDC
        cdc.dtr = 0;
        cdc.e0_rx = 0;
        cdc.rx_pend = 0;
#endif
#if YEHNAH_UAC
        uac.e0_rx = 0;
        uac_stream(0);
        uac_rate_set(UAC_RATE);
        uin.e0_rx = 0;
        uin_stream(0);
#endif
        fm1_usb_ep0_buf(ep0buf);
        sie_wr(S_INTRUSBE, 0x07);
        sie_wr(S_INTRTX1E, 0x01);
        sie_wr(S_INTRRX1E, 0);
    }
    if (it & 0x01u)
        ep0_service();
    if (ir & 0x02u)
        usb.rx_pend = 1;
    if (usb.rx_pend)                                    /* not every poll: 3 SIE round trips each */
        ep1_rx();
    if (usb.config)
        ep1_tx();
#if YEHNAH_CDC
    if (usb.config && usb_cdc_on) {
        if (ir & 0x08u)
            cdc.rx_pend = 1;
        if (cdc.rx_pend)                                /* not every poll: 3 SIE round trips each */
            ep3_rx();
#if YEHNAH_PROBE
        if (cdc.dtr || pr.ep == 3u)                     /* (the probe: no port opened on the tablet) */
#else
        if (cdc.dtr)
#endif
            ep3_tx();
    }
#endif
#if YEHNAH_TABLET
    if (usb.config && usb_tablet) {                     /* (the big FM-1) EP3: the tablet's link */
        if (ir & 0x08u)
            tab.rx_pend = 1;
        if (tab.rx_pend)
            tab_ep_rx();
        tab_ep_tx();
    }
#endif
#if YEHNAH_UAC
    uac_service();
    if (usb.config && uin.alt)                          /* (0.7.5) the host's audio: every poll (a 1 ms packet each */
        uin_rx();                                       /* other poll: an isochronous one not taken is lost) */
#endif
}

static void usb_start(void)                             /* boot, or main-loop retry while usb.up == 0 */
{
    usb.timeouts = 0;
    fm1_usb_reset();                                    /* reset whatever the ROM left */
    fm1_delay_ms(25);
    fm1_usb_attach(ep0buf);
    sie_wr(S_POWER, 0x60);
    sie_wr(S_INTRUSBE, 0x07);                           /* suspend, resume, reset (polled) */
    sie_wr(S_INTRTX1E, 0x01);
    sie_wr(S_INTRTX2E, 0);
    sie_wr(S_INTRRX1E, 0);
    sie_wr(S_INTRRX2E, 0);
    fm1_usb_ep_enable(0x1Fu);
    usb.up = usb.timeouts < 3u;                         /* a dead SIE leaves USB off */
}

/* Powered on without a cable, the SIE does not answer and usb_start leaves
 * USB off: retry once a second from the main loop so a cable plugged in
 * later still enumerates. */
static void usb_retry(uint32_t now_ms)
{
    static uint32_t t;
    if (usb.up || usb.detached || now_ms - t < 1000u)
        return;
    t = now_ms;
    usb.retries++;
    usb_start();
}

static void usb_detach(void)
{
    usb.detached = 1;
    usb.up = 0;
    fm1_usb_off();
}

#if YEHNAH_CDC
/* Present the device with or without the serial console (main loop; at boot before usb_start, or later
 * from a setting). A change while attached drops off the bus; usb_retry attaches again within a second
 * with the other descriptors, and the host enumerates the device afresh. */
static __attribute__((unused)) void usb_cdc_switch(uint32_t on)
{
    on = on != 0;
    if (on == usb_cdc_on)
        return;
    if (usb.up) {
        usb.up = 0;                                     /* usb_poll (TIMER5) stops before the SIE goes */
        RING_PUBLISH();
        fm1_usb_off();
    }
    usb.config = 0;
    cdc.dtr = 0;                                        /* the console stops writing */
    cdc.rx_pend = 0;
    usb_cdc_on = (uint8_t)on;
}
#endif
#if YEHNAH_UAC
/* (0.7.1) MENU > USB STEMS: the device re-enumerates with the 8-channel audio function (on) or the stereo one (off);
 * as usb_cdc_switch, the console off with the stems (menu_items.c usb_serial_apply picks the three) */
static __attribute__((unused)) void usb_stems_switch(uint32_t on)
{
    on = on != 0;                                       /* ((0.8.0) STEMS+IN: this and usb_duplex_switch both on) */
    if (on == usb_stems)
        return;
    if (usb.up) {
        usb.up = 0;
        RING_PUBLISH();
        fm1_usb_off();
    }
    usb.config = 0;
    uac.go = 0;                                         /* the ring restarts in the new frame size */
    usb_stems = (uint8_t)on;
}
/* (0.7.5) MENU > USB IN+OUT: re-enumerates with the audio output as well (on) or without it, as usb_stems_switch */
static __attribute__((unused)) void usb_duplex_switch(uint32_t on)
{
    on = on != 0;
    if (on == usb_duplex)
        return;
    if (usb.up) {
        usb.up = 0;
        RING_PUBLISH();
        fm1_usb_off();
    }
    usb.config = 0;
    uac.go = 0;
    uin.alt = 0;
    uin.gen++;
    usb_duplex = (uint8_t)on;
}
#endif
#if YEHNAH_TABLET
/* (the big FM-1) MENU > USB TABLET: re-enumerates with the vendor interface (on) or without it, as usb_stems_switch;
 * the console, the stems and IN+OUT off with it (menu_items.c usb_serial_apply) */
static __attribute__((unused)) void usb_tablet_switch(uint32_t on)
{
    on = on != 0;
    if (on == usb_tablet)
        return;
    if (usb.up) {
        usb.up = 0;
        RING_PUBLISH();
        fm1_usb_off();
    }
    usb.config = 0;
    tab.live = 0;
    tq_r = tq_w;
    usb_tablet = (uint8_t)on;
}
#endif
