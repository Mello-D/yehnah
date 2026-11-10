#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests of the Felucca sources (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, the GM map on DRUM, voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             instructions / sample per preset and mix (tests/cpu_baseline.txt, +25 %), ns printed;
#                   target: loop instructions of the render functions in the pi32v2 disassembly
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# UI renders (tests/ui_render.c): every screen in every palette from the real drawing code: the layout lint (no text
#                   off the screen, cut, hidden, overlapping or spilling out of its cell / card; only free text
#                   ellipsised), the draw cost, the text audit (build/ui_new/text_audit.tsv);
#                   PNGs of every palette in build/ui_new (tests/ui_render.py), the findings in build/ui_new/report.txt;
#                   FM6's 32 algorithm charts as drawn (no box overlapping, no route through a box or crossing another);
#                   every frame of the rolling digits (lint), their filmstrips in build/ui_slot;
#                   the alignment: every text / icon / keycap meant to be centred, or on its neighbours' line, by its
#                   ink against its box (cells, chips, buttons, rows, knobs, the roll's strip, the keycaps' pills), every
#                   screen in FLAT and LINE and every palette, over every value it can show; 1 px off or more fails
#                   (build/ui_new/align.txt). (2026-11-02: MENU > LARGE's pass gone with it.)
# UI (tests/ui_test.c): the UI sources against stub display / buttons / knobs: sound loads keep the steps and
#                   the track's ARP / SCL / SLICER, the SEQ > PATTERNS loader and its REPLACE? dialog, the
#                   one-step undo of both (SAVE held), REC on TRACKS / SEQ / ARP, STEP and ARP while recording,
#                   MIDI IN ROUT, saves refused while playing and the OVERWRITE? dialog, MUTE on TRACKS KNOB 1,
#                   the DRUM grid (keys, knobs, LEDs, pages, live recording into it, BEAT from PATTERNS).
#                   (0.9.9) The power-on parts (T1 DRUM-X, T2 VOICE, T3 ANIMAL, T4 LOFI, T5 ANALOG, T6 FM6, dry, each
#                   sounding), the engines' display order (place <-> engine round trips; PROGRAM CHANGE in 0.9.8's), a
#                   project of 0.9.8's tracks unchanged; the other UI tests on 0.9.8's tracks (engines.c TRK_OLD).
# Audio / persistence / editor: the real C paths against simulated DMA and NOR flash: bounded overload
#                   fades, shared-voice limits, deferred settings and retries, failed-save rollback, the autosave
#                   (1.2: when it writes, the power-on restore, damaged copies, a write cut short, wear over a session),
#                   malformed transfers, transport-stop timeouts, MIDI and UART recovery; the MENU settings over the
#                   editor (MENU_DESC / MENU_SET: every item, clamping, unknown ids, saving, USB SERIAL applied later).
# RATCH (tests/ratchet_test.c): a step's ratchet (x1..x4): its parts in the sequencer (equal, gated, chords and drum
#                   hits whole, one chance roll, swing, no slide or tie out, STOP), FUN8 round trip and older projects x1,
#                   user preset patterns, SEQ > CHANCE KNOB 3 and the roll / grid drawing.
# MOD (tests/mod_test.c): the modulation matrix: slots that do nothing are bit-identical, every source on each
#                   kind of destination, clamping, MIDI CC1 / CC11 / aftertouch routing, the cost of 4 active
#                   slots (at most +5 %), demos in build/mod_demo/.
# PERFORM (tests/perform_test.c): the FX hold layer (src/perform.c): 1/16 starts, stereo buffer effects, the
#                   too-long REPEAT, the SLICER interplay, silent layer keys, idle bit-identical, cost; build/perform_demo/;
# REVERB (tests/reverb_test.c): REVERB TYPE (src/fx.c): ROOM bit for bit as before, SPRING's decay against SIZE,
#                   its chirp (group delay rising with frequency), stability at the corners, level, a model change
#                   without a click, its cost against ROOM (+30 % at most); demos in build/fx_demo/.
# DELAY (tests/delay_test.c, 0.8.4): the delay's line (src/fx.c dly_lo): no leak of the input at the longest TIME with MOD
#                   up (plain, PING-PONG, WIDE), every long division its own length, the line at half / a quarter rate past
#                   what it holds at the full one (the echo's timing and level, the response, aliases and images, the noise
#                   floor against the full rate), no click when the rate changes (TIME swept, the tempo, a load, an external
#                   clock wavering at a boundary), MOD / GRAIN / DIGITAL / ANALOG 120 / PING-PONG / TONE at those rates.
#                   sine (builds without the CC0 samples); demos in build/slice_demo/.
# INPUT (tests/input_test.c): the key / button debounce of hal/fm1_input.h against the TIMER5 scan and bouncing
#                   contacts: a press within 2 scans (<= 2.3 ms), one note per bouncy press, no early or hanging
#                   release, stray samples ignored, fast repeats, the encoders' detents; the LED scan: lit LEDs every
#                   frame, dim ones a short pulse (the second line write) every frame, each only on its own column;
#                   the breath (#119): dark .. ~60 % of lit (DIM LO ~30 %), smooth, no dark run over ~10 ms near its peak;
#                   the power-on sweep (1.1, hal/fm1_led_anim.h): its length, every LED every frame at its level, the
#                   head left to right with its tail, the buttons, the end on the idle glow, the tick as before after.
# CLICK (tests/click_test.c, 1.1): the metronome (src/click.c) and the count-in (src/seq.c) through audio.c: each beat at
#                   the first sample of its block, with a 1/16 track's steps 1 5 9 13 (internal and external clock, DIV 1/8
#                   and SWING too), the accent, the frequencies, length, LEVEL, MASTER, OFF / REC / ON, USB audio bit for bit
#                   without it; COUNT-IN 1 / 2 BARS (step 1 exactly N bars after PLAY, STOP cancels, PLAY again nothing,
#                   never with an external clock), notes in its last eighth onto step 1; build/click_demo/.
# USB audio (tests/uac_test.c): the UAC1 descriptors as a host parses them (with and without CDC; 44100 and
#                   48000 Hz), the ring and packetiser: 44.1 frames per packet, every frame in order (bit for bit),
#                   underrun / overrun, restart; SET_CUR; the 44.1 -> 48 kHz resampler (every frame against a
#                   double-precision one and bit for bit 0.8.8's, response, SNR, THD / aliasing spurs, cost); the stream
#                   at 48 kHz (47..49 frames per packet, fast and slow I2S clocks, switching rates mid-stream).
# USB STEMS (tests/stems_usb_test.c, 0.9.8): the 8- and 10-channel stream at 44.1 kHz against the FM-1's audio ISR and a
#                   host that reads it as the Yehnah Stems app does (each packet's buffer wMaxPacketSize; a longer packet is
#                   babble: cut, the endpoint halted 1..3 ms): every packet fits, every rendered frame reaches the app's lanes
#                   once, in order, in its own channel (2 min each, +-500 ppm); 0.9.7's 720-byte endpoint shown dropping
#                   blocks as a field take did; starvation's repeats made up with the frame spread over a packet.
# STEMS (tests/stems_test.c, 0.9.8): fx.c's stems on dense IDEAS songs (idea_song): each bit for bit its track's solo render (LOAD FULL,
#                   LITE, ECO with its tail trimming; one core and CORES 2), the six and the FX return summing to the mix,
#                   an empty track exact zeros, a track through the SLICER its own sound as it got through.
# Yehnah Stems app (android/stems, while it is in this repo): usbstream.c against a pretend usbfs (JDK headers), the :core
#                   JVM tests with gradle (offline: the export's length, the trim, the WAV recovery, the mix).
# USB audio scheduler (tests/uac_sched_test.c, 0.8.9): minutes of the stream against a simulated FM-1 (the audio ISR's
#                   order, renders as a heavy song's with late halves, starvation bursts, IRQs off) and host (48 kHz,
#                   +-500 ppm): every IN token finds a packet of n +- 1 frames, every rendered frame sent once (exact), no
#                   drift, underruns repeated and made up; one core and CORES 2 (core 1 resampling the half).
# web (web/test_web.mjs): the editor protocol against its mock device, whose tables must equal the
#                   firmware's (tests/descdump.c -> build/host/desc.json),
#                   the package builder, the updater.
# Browser emulator (web/emu, when emcc is there): the firmware in WebAssembly (build/emu) boots, plays keys and MIDI,
#                   draws, lights its LEDs, keeps a save across instances, plays a song bit for bit as the same file
#                   built with cc (web/emu/native_check.c); the cost of 1 s of a heavy song against real time.
# DRUM-X OCTAVES (tests/drumx_oct_test.c, 0.8.5): OCT 0 the kit as before (bit for bit); OCT +1 / +2 / +3 ratchets x2 / x4 /
#                   x8 in time while held (stopped and playing), -1 / -2 / -3 SHORT / GHOST / FLAM (length, level, tone,
#                   the grace hit measured); real-time REC: the step's RATCH / ECHO x8 EVEN / TOUCH, the pass after the
#                   very hits heard; MIDI's bands around 35..81, MIDI REC, a project round trip, the OCT message, COMP.
# DRUM-X (tests/drumx_test.c): the DRUM engine (src/drumx_voice.c): every lane sounds, stays under full scale and ends,
#                   MORPH A <-> B (also while a hit rings), PITCH, the hat choke, MRPH on the knob; demos in build/drumx_demo/.
# NOISE (tests/noise_test.c): the engine (src/eng_noise.c): COLR's slope (white, pink, brown), the filter and the
#                   register clock following the key, META periodic at the key, no DC, no clipping at the
#                   corners, a note from silence the same twice, the cost per voice; demos in build/noise_demo/.
# ANIMAL (tests/animal_test.c, 0.8.3): the engine (src/eng_animal.c): CMOS's relaxation oscillator on the key, the owner's
#                   video (chirps at 1.0 .. 1.3 kHz with rising hooks, 2.4 .. 4.5 a second; a trill of 8 .. 10), the cricket's
#                   carrier and chirp rate, TEMP (Dolbear), the Hopf whistle's onset by bifurcation and its pitch, every
#                   preset sounding, no DC, ending, the same twice, no clipping at any mode's corners; demos in build/animal_demo/.
# FM6 (tests/fm6_test.c): the 6-operator FM engine (src/eng_fm6.c, src/fm6_core.c): the 32 algorithms' carriers, the
#                   operator envelopes (stages, rates, the voice ending), bit-stable notes, a click-free retrigger, no DC /
#                   clipping over the factory patches, the macros' directions, PTCH, pack / unpack and the SysEx
#                   layouts, the 6-voice cap, the cost per voice; demos in build/fm6_demo/.
# FM6 BANK (tests/fm6bank_test.c, 0.6.11 from Felucca 1.4.1): the voice bank (src/fm6_vbank.c) and its editor
#                   commands against simulated NOR: write / read back, its sectors only, a power cut at any byte or erase
#                   (the old bank or the new one), a wrong CRC, SLOT B1..B32, the SLOT values stored before as before.
# DJ FILTER (tests/djf_test.c, 0.9.1): the master DJ FILTER (src/fx.c djf_block, GLO > MIXER KNOB 4): centred, a sequenced
#                   mix bit for bit as 0.9.0 rendered it (never run); the low-pass / high-pass responses (pass band, 12 dB an
#                   octave, the peak, the ends, stereo); no click crossing the middle, jumping sides, swept, automated; its
#                   automation recorded, played, stopped, saved apart ('M' 'G') and loaded; an old project loads it centred.
# MICRO (tests/micro_test.c, 0.9.1): a step's MICRO in 64ths (core.h ST_MICRO): every value, a project of before played
#                   sample for sample and saved byte for byte as 0.9.0 did; live recording at REC Q 0 % within 1/64 of a step
#                   of where played (72 / 120 / 174 BPM, 1/8 1/16 1/32 16T, swing) and played back there; REC Q 50 % half way;
#                   the block 'M' 'F' saved and loaded, without it the sixteenth below; SEQ > NOTE's knob in 64ths.
# ROBUST (tests/robust_test.c): damaged or crafted stored data and editor requests: engine numbers past the last
#                   refused, a retained older
#                   RAM project bounded, user preset patterns inside their fields, malformed requests never stop PLAY.
# Sanitizers (ASan + UBSan, when the compiler has them; SANITIZE=0 skips): the stored-data and protocol tests again
#                   (loader, M-UPGRADE entry, editor, projects, backup, ROBUST) and two short fuzz runs with fixed
#                   seeds: tests/fuzz_ed.c (editor SysEx and raw USB-MIDI packets), tests/fuzz_proj.c (mutated project
#                   stores -> import -> restore -> render). Longer runs: build/host/asan/fuzz_ed 300000 7 (iterations, seed), the same for the others.
#                   UBSan leaves out the DSP's intended wraps (signed overflow, shifts); bounds, object-size and
#                   pointer-overflow stay off as in Felucca (once for the user zones' XIP rebase).
# Change baseline entries only for reviewed, intentional differences in sound or cost;
# retain every unaffected golden / CPU / target entry. VERBOSE=1: every render.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
# (chord-harp fork) ANIMAL and NOISE are not built in this fork's firmware (core.h YEHNAH_ANIMAL / YEHNAH_NOISE); the
# host tests build them, as Yehnah's own tests do, and tests/retire_test.c checks the firmware as built (their numbers
# reserved, their sounds as ANALOG, T3 PHASE's HARP at power-on, IDEAS never on them)
ENGS="-DYEHNAH_ANIMAL=1 -DYEHNAH_NOISE=1"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function $ENGS"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

$CC -O2 -o "$OUT/core1_stop_test" tests/core1_stop_test.c
run "core 1 halt: delayed bus fault and restored protection" "$OUT/core1_stop_test"

$CC -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions, PHYS DRUM and SAMPLE PERC -> DRUM, grid records, DIGITAL kept)" "$OUT/upreset_test"

$CC -o "$OUT/input_test" tests/input_test.c
run "keys and buttons: fast press, long release, bouncy contacts (one note each), glitches, encoders" "$OUT/input_test"

$CC -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"

HALF=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h)
$CC -O2 -DT_CDC=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test" tests/uac_test.c -lm
run "USB audio input: descriptors (with CDC), ring and packets, 44.1 -> 48 kHz resampler, 48 kHz stream" "$OUT/uac_test"
$CC -O2 -DT_CDC=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_nocdc" tests/uac_test.c -lm
run "USB audio input: descriptors (without CDC), ring and packets, resampler, 48 kHz stream" "$OUT/uac_test_nocdc"
$CC -O2 -DT_C1=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_sched_test" tests/uac_sched_test.c -lm
run "USB audio scheduler: 3 min at +-500 ppm, late renders, starvation, IRQs off (one core)" "$OUT/uac_sched_test"
$CC -O2 -DT_C1=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_sched_test_c1" tests/uac_sched_test.c -lm
run "USB audio scheduler: the same, CORES 2 (core 1 resamples each half at 48 kHz)" "$OUT/uac_sched_test_c1"
$CC -O2 -DHALF_FRAMES=$HALF -o "$OUT/stems_usb_test" tests/stems_usb_test.c -lm
run "USB STEMS / STEMS+IN (0.9.8): minutes into the app's lanes, every frame once in its channel, no babble, slips crossfaded" "$OUT/stems_usb_test"
# USB descriptor layouts (#67): CDC UAC LAYOUT CDC-presented 48K; layout 0 and the console left out = 1.0's bytes
# (+ the 48 kHz rate with 48K = 1, the default; 48K = 0: byte for byte)
for v in 1.1.0.1.1 1.1.1.1.1 1.1.2.1.1 1.1.3.1.1 1.1.0.0.1 1.1.2.0.1 0.1.0.1.1 1.1.0.1.0 1.1.0.0.0 1.1.2.1.0 0.1.0.1.0 \
         1.0.0.1.1 1.0.1.1.1 1.0.0.0.1 0.0.0.1.1; do
    IFS=. read -r t_cdc t_uac t_lay t_on t_48 <<EOF
$v
EOF
    $CC -DT_CDC="$t_cdc" -DT_UAC="$t_uac" -DT_LAYOUT="$t_lay" -DT_ON="$t_on" -DT_48K="$t_48" -o "$OUT/usb_desc_test" \
        tests/usb_desc_test.c
    run "USB descriptors: CDC $t_cdc (presented $t_on), UAC $t_uac (48 kHz $t_48), layout $t_lay" "$OUT/usb_desc_test"
done
$CC -o "$OUT/tablet_link_test" tests/tablet_link_test.c
run "USB TABLET (the big FM-1): the vendor interface, the ring's messages, packets and ZLPs" "$OUT/tablet_link_test"

[ -f build/yehnah.fwsc ] || { echo "run ./build.sh first"; exit 1; }

$CC -DOWN_PKG=1 -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry (own loader)" "$OUT/ota_test" build/yehnah.fwsc

$CC -o "$OUT/boot_lzma_test" tests/boot_lzma_test.c
run "boot images (chord-harp fork, after Melodee): the four packed RAM images unpack as linked; bounds, malformed streams" \
    "$OUT/boot_lzma_test" build/yehnah.bin build/text.bin build/ramtext.bin build/c1text.bin build/hottext.bin build/data.bin

$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/res_test" tests/res_test.c -lm
run "resource region (chord-harp fork, after Melodee): whole and this build's only; begin / write / commit; VECTOR, TERRAIN, WSEQ, PLAITS, OMNI silent without it" "$OUT/res_test"

head -c 200000 build/yehnah.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/yehnah.fwsc

if [ -f build/gen/yehnah_tables.h ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim" tests/hostsim.c -lm
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/scale_test" tests/scale_test.c -lm
    run "scales: white-key mapping and note lifecycle" "$OUT/scale_test"
    $CC -O1 -w -o "$OUT/harp_test" tests/harp_test.c
    run "chord-harp: 16 sets x 12 chords whole in every ROOT, the slash basses, the plate, the confirmed readings" "$OUT/harp_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/harp_live_test" tests/harp_live_test.c -lm
    run "chord-harp live: chord keys whole, the plate follows, silent black keys, ROOT / octave, MONO, LATCH" "$OUT/harp_live_test"
    ${CC%% *} -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/harp_bench" tests/harp_bench.c -lm   # (as the firmware: no ENGS)
    run "chord-harp: voices and cost of a 6-note chord + 3 plate keys on each engine, LOAD FULL / LITE / ECO (build/harp_report.md)" \
        env HARP_REPORT=build/harp_report.md "$OUT/harp_bench"
    ${CC%% *} -O1 -w -Ibuild/gen -Ifirmware/src -DYEHNAH_RES_SIM=1 -o "$OUT/retire_test" tests/retire_test.c -lm   # (as the
                                                        # firmware: no ENGS)
    run "chord-harp fork: ANIMAL, NOISE and DIGITAL built, T3 PHASE CZ PLUCK, IDEAS on built engines" "$OUT/retire_test"
    $CC -O1 -w -fpermissive -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/fmini_test" tests/fmini_test.c -lm
    run "FMINI: its 200 patches survive the packing, a patch load writes only its own parameters (not the chord mode, HUMAN, KEY>F ..)" "$OUT/fmini_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/speaker_test" tests/speaker_test.c -lm
    run "SPEAKER EQ: FLAT / LOWCUT / BASS+ responses, BASS+ harmonics of the bass, the sub cut, no offset after" "$OUT/speaker_test"
    run "DSP render (ANALOG preset 0)" "$OUT/hostsim" 0 0 1 "$OUT/render.wav"
    mkdir -p build/tracks_demo
    run "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" env TRACKS=build/tracks_demo "$OUT/hostsim" 0 0 1 "$OUT/tracks.wav"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/project_test" tests/project_test.c -lm
    run "project formats (FUN1..FUN5 -> FUN6, the grid and song chain; DIGITAL tracks -> FM6, SAMPLE PERC -> DRUM)" "$OUT/project_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/motion_test" tests/motion_test.c -lm
    run "motion, whole-step chance, FUN7 migration, song restore, ARP repeat and the 1.2 ARP modes" "$OUT/motion_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/djf_test" tests/djf_test.c -lm
    run "DJ FILTER (0.9.1): centred bit for bit 0.9.0, LP / HP response, no click crossing the middle, automation, old projects" "$OUT/djf_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/micro_test" tests/micro_test.c -lm
    run "MICRO in 64ths (0.9.1): recorded within 1/64 of a step at any tempo / division, played there, REC Q 50 %, old projects, 'M' 'F'" "$OUT/micro_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -DYEHNAH_RES_SIM=1 -o "$OUT/stems_test" tests/stems_test.c -lm
    run "STEMS (0.9.8): IDEAS songs, each stem = its track's solo render (FULL LITE ECO, CORES 1 / 2), the sum = the mix, the SLICER" "$OUT/stems_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ratchet_test" tests/ratchet_test.c -lm
    run "RATCH: x1..x4 in a step (notes, chords, drum hits), gates, chance, swing, projects, user presets, CHANCE page" "$OUT/ratchet_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/hold_test" tests/hold_test.c -lm
    run "HOLD: a step's notes ring over the steps after it its steps are over (or the pattern comes round), the next HOLD or STOP; STEP's LENGTH past a note makes one" "$OUT/hold_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumx_oct_test" tests/drumx_oct_test.c -lm
    run "DRUM-X OCTAVES (0.8.5): ratchets above the kit, SHORT GHOST FLAM below, live, recorded and played back as heard, MIDI bands" "$OUT/drumx_oct_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_control_test" tests/midi_control_test.c -lm
    run "USB/TRS clock, bend, sustain, ownership and panic recovery" "$OUT/midi_control_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/theme_test" tests/theme_test.c -lm
    run "themes: contrast, text blending and font metrics" "$OUT/theme_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/text_ref_test" tests/text_ref_test.c -lm
    run "text: pens and pixels equal the reference renderer" "$OUT/text_ref_test"
    if python3 -c "import PIL" 2>/dev/null; then
        run "text spacing: glyph gaps against the font's own (16x supersampled), within a phase step (1 phase: 1 px)" \
            python3 tests/text_spacing_test.py build/gen/ui_fonts.h
    fi
    $CC -O1 -w -Ibuild/gen -o "$OUT/settings_test" tests/settings_test.c
    run "settings: PER1..PER4 migration, palette ids and preference preservation" "$OUT/settings_test"
    $CC -w -DYEHNAH_RES_SIM=1 -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test" tests/ui_test.c -lm
    run "UI: sounds keep steps, undo, recording, MIDI overflow, pending saves, panel recovery, drum grid, song chain" "$OUT/ui_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/tablet_screen_test" tests/tablet_screen_test.c -lm
    run "USB TABLET (the big FM-1): the screen as tiles (HOME's scene as ROAD), decoded = the emulator's screen every ms; the 2 KB ring's resync; the trickle" "$OUT/tablet_screen_test" "$OUT/road.rec"
    if command -v node >/dev/null 2>&1; then
        run "USB TABLET (the big FM-1): web/road/road.js draws ROAD's scene as the firmware does (ui_road.c)" node tests/tablet_road.mjs "$OUT/road.rec"
    fi
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/tablet_panel_test" tests/tablet_panel_test.c -lm
    run "USB TABLET (the big FM-1): the panel, the tablet's buttons / keys / encoders as the FM-1's own, LEDS = the LEDs every ms" "$OUT/tablet_panel_test"
    FV=$(sed -n 's/^#define YEHNAH_VERSION "\([^"]*\)".*/\1/p' firmware/src/yehnah.c)   # (the splash, ABOUT: the real version)
    $CC -O1 -w "-DYEHNAH_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render" tests/ui_render.c -lm
    mkdir -p build/ui_new/ppm build/ui_slot
    run "UI renders: layout lint (every screen and palette, every page, engine and column value), alignment by ink (1 px fails), draw cost" \
        "$OUT/ui_render" build/ui_new build/ui_slot
    if python3 -c "import PIL" 2>/dev/null; then python3 tests/ui_render.py build/ui_new build/ui_slot; fi
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/audio_test" tests/audio_test.c -lm
    run "audio: overload protection, bounded fades and DMA diagnostics" "$OUT/audio_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/click_test" tests/click_test.c -lm
    run "metronome and count-in: beats sample for sample (internal and external clock), accent, level, MASTER, not in USB, count-in timing, notes onto step 1" "$OUT/click_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/persistence_test" tests/persistence_test.c -lm
    run "persistence: deferred settings, retry and failed-save rollback" "$OUT/persistence_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/startup_test" tests/startup_test.c -lm
    run "startup: saved arrangement survives initialization and idle saving (by @workeffortwaste, #134)" "$OUT/startup_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/backup_test" tests/backup_test.c -lm
    run "full backup: CRC before writes, stale runtime, USB reset / timeout, malformed objects, older projects" "$OUT/backup_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/editor_test" tests/editor_test.c -lm
    run "editor: real C protocol, malformed transfers, queue recovery, MENU settings (writes build/host/menu.json)" \
        env MENU_JSON="$OUT/menu.json" "$OUT/editor_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/robust_test" tests/robust_test.c -lm
    run "robustness: engine numbers, retained old projects, preset patterns, malformed requests" \
        "$OUT/robust_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test" tests/mod_test.c -lm
    mkdir -p build/mod_demo
    run "modulation matrix: off = bit-identical, the math, MIDI CC1 / CC11 / aftertouch, cost, demos" "$OUT/mod_test" build/mod_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/slicer_test" tests/slicer_test.c -lm
    mkdir -p build/slicer_demo
    run "SLICER: no clicks, timing, sync with the sequencer, STUT, cost, demos" "$OUT/slicer_test" build/slicer_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/swing_test" tests/swing_test.c -lm
    run "SWING: track + global at most 100, sequencer and SLICER step lengths, the SWG display" "$OUT/swing_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/perform_test" tests/perform_test.c -lm
    mkdir -p build/perform_demo build/fx_demo
    run "FX layer effects: on the 1/16, stereo, too-long REPEAT, SLICER, silent keys, idle bit-identical, clicks, cost, demos" "$OUT/perform_test" build/perform_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/reverb_test" tests/reverb_test.c -lm
    run "REVERB TYPE: ROOM bit-identical, SPRING decay / chirp / stability / level, model change, cost, demos" "$OUT/reverb_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/delay_test" tests/delay_test.c -lm
    run "DELAY: no leak at the far end, the long times at half / quarter rate (timing, response, aliases, noise), no click on a rate change, MOD / GRAIN / FEEDBK there, demos" "$OUT/delay_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
    run "regression: golden renders, health, voices, CPU budget" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
    $CC -O2 -w -pthread -Ibuild/gen -Ifirmware/src -o "$OUT/mc_thread_test" tests/mc_thread_test.c -lm
    run "mixer: serial, deferred, real thread, delays and lost job" python3 tests/mc_thread_test.py "$OUT/mc_thread_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/descdump" tests/descdump.c -lm
    echo "== parameter and engine tables as JSON (for the editor mock test)"
    "$OUT/descdump" > "$OUT/desc.json" || fail=1
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumx_test" tests/drumx_test.c -lm
    mkdir -p build/drumx_demo
    run "DRUM-X: every lane sounds and ends, MORPH, PITCH, NOIS, hat choke, group mutes, demos" "$OUT/drumx_test" build/drumx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/noise_test" tests/noise_test.c -lm
    mkdir -p build/noise_demo
    run "NOISE: colour slopes, key-tracked filter and clock, META period, DC, clipping, retrigger, cost, demos" "$OUT/noise_test" build/noise_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/animal_test" tests/animal_test.c -lm
    mkdir -p build/animal_demo
    run "ANIMAL: CMOS on the key and the owner's video, cricket, TEMP, Hopf onset, presets, DC, clipping, retrigger, demos" "$OUT/animal_test" build/animal_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_env_compare" tests/fm6_env_compare.c -lm
    run "FM6 compact envelope: original state machine and all carrier masks" "$OUT/fm6_env_compare"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_test" tests/fm6_test.c -lm
    mkdir -p build/fm6_demo
    run "FM6: algorithms, envelopes, retrigger, DC, clipping, macros, patch formats, voices, cost, demos" "$OUT/fm6_test" build/fm6_demo
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6bank_test" tests/fm6bank_test.c -lm
    run "FM6 voice bank: write / read, power cuts, CRC, SLOT B1..B32, stored SLOT values as before" "$OUT/fm6bank_test"
else
    echo "== skip hostsim (run ./build.sh once)"
fi

# ASan + UBSan: the stored-data and protocol paths, and short fuzz runs (fixed seeds: the same inputs every run)
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow"
SAN="$SAN -fno-sanitize-recover=undefined -w"
san_ok() {
    printf 'int main(void){return 0;}\n' > "$OUT/san_probe.c" &&
        ${CC%% *} $SAN -o "$OUT/san_probe" "$OUT/san_probe.c" 2>/dev/null && "$OUT/san_probe"
}
if [ "${SANITIZE:-1}" = 0 ]; then
    echo "== skip the sanitizer runs (SANITIZE=0)"
elif [ ! -f build/gen/yehnah_tables.h ]; then
    echo "== skip the sanitizer runs (run ./build.sh once)"
elif ! san_ok; then
    echo "== skip the sanitizer runs (the compiler has no ASan / UBSan)"
else
    mkdir -p "$OUT/asan"
    A="$OUT/asan"
    SCC="${CC%% *} $SAN $ENGS -Ibuild/gen -Ifirmware/src"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
    $SCC -o "$A/ldr_test" tests/ldr_test.c
    run "ASan/UBSan: update loader (other app -> this build)" "$A/ldr_test" "$OUT/old.fwsc" build/yehnah.fwsc
    $SCC -DOWN_PKG=1 -o "$A/ota_test" tests/ota_test.c
    run "ASan/UBSan: M-UPGRADE entry (own loader)" "$A/ota_test" build/yehnah.fwsc
    for t in editor_test project_test backup_test robust_test fm6bank_test ui_test; do   # (chord-harp fork: ui_test, the lens chains' buffers)
        $SCC -o "$A/$t" tests/$t.c -lm
        run "ASan/UBSan: $t" "$A/$t"
    done
    $SCC -o "$A/fuzz_ed" tests/fuzz_ed.c -lm
    run "ASan/UBSan fuzz: editor SysEx and raw USB-MIDI packets (20000, seed 7)" "$A/fuzz_ed" 20000 7
    $SCC -o "$A/fuzz_proj" tests/fuzz_proj.c -lm
    run "ASan/UBSan fuzz: project stores -> import -> restore -> render (5000, seed 13)" "$A/fuzz_proj" 5000 13
fi

run "regression: target cost of the render loops (pi32v2 disassembly)" python3 tests/target_budget.py \
    build/yehnah.dis tests/target_budget.txt

# (0.6.83) the audio path's CPU where proc_pid_rusage is missing (Linux): callgrind's exact counts, section by
# section (each engine's heaviest preset, every REVERB TYPE, a full mix, the sequencer at its heaviest), against
# tests/cpu_baseline_vg.txt (x86-64 gcc; another compiler: BUDGET_UPDATE=1 once, then it watches from there)
if command -v valgrind >/dev/null 2>&1 && [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/cpu_vg" tests/cpu_vg.c -lm
    run "CPU under valgrind: engines, reverbs, full mix, the sequencer's heaviest (exact counts, +8 %)" \
        python3 tests/cpu_vg.py "$OUT/cpu_vg" tests/cpu_baseline_vg.txt
else
    echo "== skip CPU under valgrind (no valgrind, or not Linux x86-64)"
fi

run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py

if [ -d android/stems ]; then                         # (the app, while it lives here)
    JH="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac || command -v java || echo /x/x/x)")")")}"
    if [ -f "$JH/include/jni.h" ]; then
        cc -O1 -I"$JH/include" -I"$JH/include/linux" -o "$OUT/usbstream_test" android/stems/app/src/test/cpp/usbstream_test.c -lm
        run "Yehnah Stems: usbstream.c (every frame once, a cut frame left out whole, the mic's drift with no sample slipped)" "$OUT/usbstream_test"
    else
        echo "== skip Yehnah Stems usbstream.c (no JDK headers)"
    fi
    if command -v gradle >/dev/null 2>&1; then
        run "Yehnah Stems :core (JVM): WAV, layout, splitter, latency, mix, exports, the take's length and trim" \
            gradle -q -p android/stems -PcoreOnly --offline :core:test
    else
        echo "== skip Yehnah Stems :core (no gradle)"
    fi
fi
if command -v node >/dev/null 2>&1; then
    run "web pages: editor protocol, package builder, update protocol" node web/test_web.mjs
    run "web backup: capture, validation before writes, restore order" node web/test_backup.mjs
    run "web resources (chord-harp fork): the region sent through command 82 when missing, kept, refused for another build" node web/test_res.mjs build/yehnah-res.bin
else
    echo "== skip web tests (no node)"
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "== skip the browser emulator (no emcc: Emscripten builds web/emu)"
elif ! command -v node >/dev/null 2>&1 || [ ! -f build/gen/yehnah_tables.h ]; then
    echo "== skip the browser emulator (needs node and build/gen)"
else
    run "browser emulator: the firmware to WebAssembly (web/emu/build.sh -> build/emu)" sh web/emu/build.sh
    cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o "$OUT/emu_native" web/emu/native_check.c -lm
    run "browser emulator: the same song from the native build" "$OUT/emu_native" "$OUT/emu_native.f32"
    run "browser emulator: boot, keys, MIDI, screen, LEDs, PLAY, a save kept across instances, = native, cost" \
        node web/emu/emu_test.mjs build/emu/yehnah.wasm "$OUT/emu_native.f32"
        run "stems from the browser build (web/emu/stems.js): a backup in, each part alone, the song once round, WAV + ZIP" node web/emu/stems_test.mjs build/emu/yehnah.wasm
fi

[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }
