# The big FM-1: the plan (2026-11-03)

Yehnah stays on the FM-1. The Lenovo tablet (USB) shows the FM-1's screen larger, plays its sound, and has its knobs,
buttons and keys; the FM-1's own panel works alongside. One app: Yehnah Big (android/big, Kotlin, grown from
android/linkprobe). Decided before this plan: the tablet draws the road scene itself; the page looks like the whole
FM-1; some lag is fine; an APK, not a Chrome page.

## What the probe told us (Lenovo TB373FU, Link Probe 0.3)

| | measured | what it means |
|---|---|---|
| Bulk, 1 packet a poll | 116 KB/s, 0 lost | enough: the screen without the scene needs ~2.5 KB for a knob turn and tens of KB for a page change |
| SysEx, 1 packet a poll | 116 KB/s wire, ~69 KB/s payload | bulk carries ~1.7x as much: the screen goes over bulk |
| 4 packets a poll | ~417 KB/s, but packets lost / reordered (bulk), extra ones (SysEx) and polls slowed to 1870/s | not used. One packet a poll needs no spin-wait and loses nothing |
| PING (SysEx round trip) | 46.5 ms | the main loop's frame time, not the link. Tablet knobs will feel like the FM-1's own |
| Audio (UAC, 48 kHz) | 0 underruns. USB lag 7.7 ms, speaker 62 ms | the 62 ms is AudioTrack's full 85 ms buffer. A smaller buffer should get it to ~20-30 ms |

The scene (the road, ~135 tiles, > 16 colours, changes every frame) costs 120-240 KB/s as pixels. That is why the
tablet draws it from a small state message.

## The design

**Link: a TABLET USB mode.** A new entry in MENU > SYSTEM > USB (after CONSOLE STEREO STEMS IN+OUT STEMS+IN):
STEREO's audio + MIDI, plus one vendor-class interface (class FF) with a bulk IN / OUT pair. Not CDC: the normal
build has CDC off (~7.1 KB), and macOS's CDC driver broke the audio before (#67). As its own mode, Macs and PCs in the
other modes see exactly what they see today. Framing on the bulk pipe: `type, len lo, len hi, payload`. The firmware
sends at most one 64-byte packet per 2 kHz poll from a TX ring (~116 KB/s), with no spin-waits and two RAM buffers
used in turn. The probe's burst bug looks like a buffer refilled while the SIE could still resend it.

**Screen: changed tiles, from the one tap point.** No framebuffer: everything reaches the panel through gfx.c's strip
blit (lcd_blit) and lcd_fill. While the tablet is connected, both also go through a tile pass:
- 16x16 tiles (15 x 15 over 240 x 240), one 16-bit hash each (450 B). A drawn tile whose hash changed is encoded and
  queued; an unchanged one costs only the hash.
- Encodings (as RemoteFM1's): FILL (one colour), PAL (<= 16 colours, 4 bits a pixel: the AA text's 16-step ramps
  fit), RAW (RGB565). The tablet decodes into a 240 x 240 canvas and draws it scaled.
- Backpressure: if a tile does not fit in the ring, its hash is not updated and `ui.force = 1` (the existing full
  repaint). The next frame resends only the tiles that are still missing. A slow trickle re-sends one tile row
  every ~0.5 s, which heals hash collisions and anything lost.
- On pages with the road, tiles wholly inside the scene's rectangle (24..161) are skipped; the edge tiles are
  sent clipped.

**Scene: drawn on the tablet.** Each frame on a road page the firmware sends a small ROAD message (~40-60 B): the
time and beat phase rd_draw takes (now, bp, dy, sh, way), the time of day, each track's level, the robot (track,
engine, pose), and the steps (note / empty mask, playhead, REC mask), the steps only when they change. The tablet runs
the JS road (today inside tools/robots/engine_robots.html), moved into one shared file, web/road/road.js, that the
robots page and the app both load. The existing rule stands: change ui_road.c and road.js together.

**Panel: the tablet's knobs, buttons and keys.** The app's page is web/emu/index.html's panel (knobs, buttons, keys
around the screen) in a WebView, bundled in the APK's assets. Touches go out as INPUT messages on bulk OUT (encoder
step +-n, button down / up, key down / up + velocity). In the firmware's usb_poll they are merged into fm1_in (the
same place the hardware's buttons and encoders land), so every UI path treats them as the panel's own. No echo is
needed: the screen stream shows the result. LED state goes out as a tiny message on change, so the tablet's LEDs
pulse as the FM-1's do.

**Sound.** TABLET mode's UAC stream (stereo 48 kHz) goes through linkusb.c's iso reaper (already working in the
probe) into an AudioTrack. The probe's buffer is cut from 85 ms with setBufferSizeInFrames, as low as it goes without
underruns. The probe's mic-based lag check is dropped; AudioTrack's timestamp is the measure.

**App plumbing.** Kotlin + linkusb.c (the probe's: one fd, iso + bulk URBs reaped together). Tiles, ROAD and LED
messages go to the WebView as ArrayBuffers over a WebMessagePort (androidx.webkit), not base64 strings.
INPUT comes back the same way. The app reconnects on unplug / replug and re-requests a full screen (HELLO ->
the firmware sets ui.force).

## Budgets (estimates, to be measured at each step)

| | free now | this plan |
|---|---|---|
| Flash | ~12.3 KB | ~3-4 KB: vendor IF + bulk ~1 KB, tiles + encoders ~1.5 KB, ROAD / LED / INPUT / HELLO ~0.8 KB |
| RAM (.data + .bss) | ~4.9 KB (probe ELF; recheck on the normal one) | ~2.6 KB: tile hashes 450 B, TX ring 2 KB, RX 64 B |
| LOWBUF | 992 B | 0 (nothing main-loop-only needs to go there) |
| CPU | | hashing ~33-57k px per frame plus encoding the changed tiles, only while connected. To measure: frame time with / without the tablet |

The probe code stays under YEHNAH_PROBE (not in the normal build).

## Steps (each one a session, each on the device before the next)

1. **The link.** Firmware: TABLET USB mode (the descriptors, the vendor IF, bulk IN / OUT on EP3 without CDC),
   the framed TX ring (one packet a poll, two buffers), HELLO / INFO (version, mode). App android/big: CONNECT,
   claim the vendor IF, show INFO and a counting test (a FLOOD as in the probe, through the real ring: 0 lost,
   ~116 KB/s). Check: Mac / PC in STEREO unchanged (descriptors byte for byte), flash / RAM measured.
   BUILT 2026-11-03, not yet on the devices: usb.c tab_* + tablet.c, android/big 0.1, tests/tablet_link_test.c. Cost
   +2.4 KB flash (FLOOD / STATS ~0.6 KB of it), +2.35 KB RAM (2.5 KB left).
   RESULTS 2026-11-03 (Lenovo TB373FU, Android 16, Big 0.1): INFO 2.5 ms after HELLO. FLOOD LINK 113.8 KB/s on the
   wire (108.4 payload), 10,174 messages, 0 gaps, 0 not received, 0 refused. FLOOD LOOP the same, 114.1 KB/s, 0 lost:
   the main loop's 15 ms frame wait calls tab_service ~27,000 times a second, so it keeps the ring full as well
   (frames 66.7/s, idle). So step 2's budget is the link's (~108 KB/s of payload), not the loop's, as long as frames
   finish inside 15 ms; a long render (a song playing) leaves the ring to drain. Not yet checked: a Mac / PC on
   STEREO as before.
2. **The screen as pixels.** Firmware: the tile pass in lcd_blit / lcd_fill, FILL / PAL / RAW, the backpressure and
   trickle. App: a plain canvas showing the stream (the scene comes as pixels for now, so it will be choppy). Host
   test: the emulator's web_fb vs a decoder fed the stream must match after every frame of a scripted session
   (tests/, under the usual suite). Measure the frame time cost.
   BUILT 2026-11-03, not yet on the devices: firmware/src/tab_screen.c (hooks TAB_SCR_BLIT in gfx.c cv_blit,
   TAB_SCR_FILL in lcd.c lcd_fill; usb.c tab_open / tab_byte / tab_close writes a message in place), android/big
   0.2 (the canvas, SCREEN 5 S), tests/tablet_screen_test.c. What changed from the design above, found by the test:
   - Strips share tiles (edges at y 24, 160, 162, 164, ...: up to three strips in one tile row), so one hash a
     tile thrashed: every forced repaint resent ~135 unchanged pieces, more than the 2 KB ring, and the resync
     looped. Now each tile column keeps the pieces the tablet shows (hash + rows, 20 a column, 1.2 KB in the pool);
     a piece sent drops only those its rows overlap.
   - A refused piece owes only its rows in its tile (one byte a tile), and the resync fills only those with the
     background: blanking whole tiles wiped their pieces already sent, which were resent every round (a livelock).
   - The resync runs once the ring is nearly empty (>= 1792 B free), at most once a frame: half-empty and every
     100 ms left the link idle (~20 KB/s).
   - "Spoiling" a hash by XOR 1 undid itself when the trickle and a resync both hit a tile; now entries are dropped.
   Test results (the emulator's firmware): an endless link: the canvas = web_fb after every one of 23,140 ms (pages,
   knobs, presets, the song). The 2 KB ring at 114 KB/s: on FX after the session, right again 101 ms after the
   page settles; a tile spoilt on the tablet heals in ~1.2 s (within the trickle's 7.5 s round). Bandwidth with an
   endless link: idle FX 2.5 KB/s, SEQ 0.8, ENV / LFO / GLO / EDIT / ARP 14-28 (the trickle's repaints and small
   animations), but HOME 146 and SCL 132 (the road); a knob turn 70-230, a preset change 250, a page change
   100-450 KB/s in bursts. So HOME / SCL stay choppy at 114 KB/s until step 4.
   Cost: .text +2,912 B against step 1 (ts_draw 610, ts_psend 702, tab_scr_service 532, the rest inlined);
   .data+.bss +48 B (95,780 of 98,304: 2.5 KB free); pool +1.44 KB (11.9 KB spare). Frame time: STATS now carries
   ui_draw's ticks and the tile pass's: SCREEN 5 S on the tablet shows both (to measure on the device).
   FIRST RESULTS 2026-11-03 (TB373FU, Big 0.2.1, firmware 3636543; the page not noted): the screen streams; SCREEN
   5 S: 10.5 KB/s, 311 tiles sent / 15,464 unchanged / 5 refused, 0 malformed; frames 56.3/s; ui_draw 6.2 ms a frame,
   the tile pass 0.8 ms of it (~13 %: ~5.4 ms without the tablet). The picture matches the FM-1's (the user).
   Not measured: HOME (the road), a knob turn, a page change.
3. **The panel.** The WebView with web/emu's panel around the stream canvas; INPUT -> fm1_in; LEDs out. Check: every
   button, encoder and key from the tablet, and the FM-1's own at the same time.
   BUILT 2026-11-03, not yet on the devices (the user chose WebView and sharp pixels): firmware/src/tab_panel.c
   (included by tablet.c): BUTTONS (4, u16 by label), KEYS (5, u32), ENC (6, role + int8) in; LEDS (4, 23 B: the
   browser page's eight masks, web_lit_buttons ..., sent when fm1_led / dim / breath / mid or DIM LO change, again
   after HELLO, retried if refused) out. What changed from the design above:
   - Not "merged into fm1_in" directly: hal/fm1_input.h fm1_ext_hold(buttons, notes) works out each column's held
     rows (fm1__ext[11]) and fm1__keys ORs them into what the matrix read (one OR a column in the ISR), so a tablet press debounces and makes its edges exactly as a
     press on the panel (fm1_in's state would be undone by the next scan otherwise); either panel holds a key.
     fm1_ext_turn adds encoder steps (by panel.enc / dir, as panel_enc reads them). The emulator has the same two
     (yehnah_web.c web_held: the page's and the tablet's ORed).
   - No velocity: the FM-1's keys have none.
   - No WebMessagePort (the app has no libraries; the platform port carries only strings): the page polls
     Big.take() each animation frame (base64 of whole TILE / LEDS messages) and calls Big.buttons / keys / enc; the
     page decodes the tiles itself (JS, into its canvas). A backlog over 2 MB (the page not polling) is dropped and
     HELLO sent again.
   - The page is web/emu/index.html itself, its "big" mode when window.Big is there (no copy to maintain): the panel
     alone, filling the screen; the canvas snapped to a whole number of device pixels a pixel (x3 on the TB373FU's
     wide layout, ~720 px). app/build.gradle's bigPage copies index.html + fonts into the APK's assets.
   - The app connects on start and on USB_DEVICE_ATTACHED; its old buttons and log are behind a ··· overlay.
   Tests: tests/tablet_panel_test.c (in run_tests.sh, ~1 s): the emulator's firmware; after HELLO one LEDS equal to
   the browser page's; through a session (FX, HOME, SELECT, a key, the song, SEQ) the last LEDS = the LEDs after
   every ms (3,980 ms, 9 messages); FX / HOME / SELECT from the tablet act as the panel's; both panels holding one
   button; the tablet gone lets go; a refused LEDS goes out once there is room. The page's decoder (bigTile /
   bigLeds, cut out of index.html and run in node on a recorded stream of 1,529 tiles): 0 of 57,600 pixels wrong
   against the emulator's screen, its LEDs equal. Cost: tab_pnl_msg 190 B, tab_pnl_led_masks 102 B, the LED
   compare inlined in tab_service (1,114 B now); .data+.bss +64 B (95,844 of 98,304).
4. **The scene on the tablet.** road.js split out of engine_robots.html; the ROAD message; scene tiles skipped on
   road pages. Check: bandwidth on HOME while playing (should fall from ~200 KB/s to a few KB/s), the tablet's road
   matches the FM-1's (a side-by-side photo).
   BUILT 2026-11-03, not yet on the devices (firmware + Big 0.4). What changed from the design above:
   - web/road/road.js holds the robots' design code (export_parts.mjs now reads it there: parts and refs unchanged)
     and the scene; tools/robots/engine_robots.html loads it and keeps only its demo and gallery. Road.decode reads a
     ROAD, Road.frame draws it (put(x, y, rgb) a pixel). Brought in line with ui_road.c on the way: the sky carried
     up (dy), the strip mode, the walk frames and the moonwalk (robot()'s s.walk), the pivot's shear, the levels
     from the message; the rim light mixed in RGB565 as rb_lit, the sky's dither in floats as the firmware.
   - ROAD carries the inputs as drawn, not the firmware's state: ui_road.c fills rdm (.lowbuf) in rd_draw / rd_steps /
     rd_robot (way, now, bp, dy, sh, pulse, vm, each track's level, the steps' colour kinds + the six step colours,
     each robot after its walk / slide: ri, cx, fy, sc and its rb_in_t). ~86 B a frame with one robot, 16 steps.
   - Not "tiles inside 24..161 skipped": the scene is one blit (draw_graph's cv_blit, 240 x 138 at 24), so
     tab_scr_blit sends ROAD in place of that whole blit (edge tiles included) when rdm.on (rb_home drew it in the
     knob-cell layout, no toast over it: a toast sends the scene as tiles), and forgets the pieces kept for those rows
     (the next page's panel goes out in full). A refused ROAD asks for a quiet repaint (ANIM OFF would never send
     another). The page draws ROAD into its canvas at row 24, in order with the tiles.
   - The app (Big 0.4) passes ROAD (5) to the page, counts it on the live line; the page loads road.js itself.
   Tests: tablet_screen_test (the tablet side paints the scene the emulator drew for each ROAD: the canvas = web_fb
   every ms still; HOME playing under 12 KB/s: 4.0 KB/s on the endless link, 6.0 on the 2 KB ring, was ~146);
   tests/tablet_road.mjs (in run_tests.sh): road.js on 53 recorded ROADs against the firmware's pictures: 1.3 pixels
   a frame differ (of 33,120; the worst 16: a robot edge pixel here and there), limit 40.
   Cost: .text +1,136 B (app 574,316 of 581,564), .bss +16 B, LOWBUF +104 B (888 B free).
   Still heavy, not the road: SCL's panel (graph_key / graph_harp) ~132 KB/s idle, 450 while turning.
5. **Sound.** UAC in the app, the smaller buffer, underruns and lag shown in a debug strip.
   BUILT 2026-11-03 (Big 0.6, app only: the firmware's TABLET mode already streams STEREO's audio), not yet on the
   tablet. On connect the app claims the audio streaming IF (the iso IN alt), asks 48 kHz, puts 8 iso URBs (2 ms each)
   in flight next to the bulk ones (bigusb.c, unchanged). The AudioTrack (LOW_LATENCY) plays on the tablet's own output
   (headphones > Bluetooth > speaker; never the FM-1, a USB output too) and is kept filled to a target: 12 ms to
   start, +2 ms on each underrun (80 at most), ··· SOUND − / + by hand; bufferSizeInFrames = target + 8 ms. The two
   clocks' drift: every 20 ms the fill (written − playbackHead, smoothed) steers one frame dropped or repeated; a fill
   under half the target is padded with silence, one over twice + 4 ms cut. The strip (bottom left, ··· STRIP) shows
   kHz in, the fill, the lag (AudioTimestamp: a frame written now to the speaker; USB adds ~3-8 ms), underruns, the
   drift count, padded / cut, lost (writes that did not fit), iso packets in error / empty. ··· SOUND: the track off.
   FIRST RESULTS 2026-11-03 (TB373FU, Big 0.6, run time not noted): sound plays, no clicks heard; kept 20 ms (4
   underruns raised it from 12), lag 35.7 ms + USB (the probe: 62); drift -5848 +0, padded / cut 4100, lost 3602, iso
   err 0, empty 76. Big 0.6.1: the room 8 -> 40 ms (lost = whole packets that did not fit after a stall; the room is
   not lag), padded and cut counted apart, and the long-term rates: the FM-1's Hz against the tablet's clock and the
   drift a second (to tell the clocks from a flapping steer).
   Big 0.6.1 on the tablet (139 s): the FM-1 48,016 Hz against the tablet's clock; kept 12 ms, lag 27.4 ms + USB,
   underruns 0, drift -28.9/s, cut 1442, padded 596, lost 504: ~39 frames/s to take out (the tablet's DAC slower than
   its clock too?), near the 50/s a 20 ms steer allows, so the fill crept up and was cut. Big 0.6.2: steered each URB
   (2 ms: up to 500/s, one frame an URB, the fill smoothed over ~130 ms); the strip shows the track's size as Android
   took it. If single dropped frames are ever heard: a small cubic resampler steered by the fill instead.
   Big 0.6.2 on the tablet (210 s): 48,020 Hz; kept 12, track 52 ms, lag 27.3 ms + USB, underruns 0, lost 0, cut ~1,
   drift -41.8/s steady (the clocks), but padded 3678 with no underrun: the reader's stalls dipped the fill under half
   the target, silence went in, then the stalled packets came. Big 0.6.3: padded only after a real underrun, cut only
   when the smoothed fill stays over 2 x target + 4 ms.
   Big 0.6.3 on the tablet (227 s): 48,026 Hz, kept 12 ms, lag 27.8 ms + USB, underruns 0, padded 0, drift -25.9/s,
   cut 866, lost 892 (~37 ms of audio in all: one or two events, the start's guessed: the output slow to pull the first
   frames). Big 0.6.4: the start's counts set aside 1 s after the track first moves (logged, and the strip's second
   line), the strip's counts from then on.
   DONE 2026-11-03: Big 0.6.4 on the tablet (97 s of a song): 48,022 Hz, kept 12 ms, lag 27.5 ms + USB (the probe:
   62 + USB), underruns 0, cut 0, lost 0, padded 0, drift -25.3/s (the clocks), iso err 0, no clicks heard. The start:
   the output moved 65 ms after play(); what piled up meanwhile (cut 866, lost 1004) is unheard.
6. **Living with it.** Reconnects, the tablet sleeping / waking, USB permission after a replug, the app's icon and
   name, a release build served at :8000/app/ as linkprobe is.
   BUILT 2026-11-04 (Yeh nah 0.7, the app only, NOT yet on the tablet). Reconnects: the receiver takes USB_DEVICE_ATTACHED
   too (an FM-1 back connects by itself after 500 ms, so a ticked-Always "open with" goes first: it brings the
   permission), a detach disconnects only when it is the connected device, a failed read reconnects after 300 ms;
   one permission dialog at a time, a refusal logged. Waking / coming back (onStart): a broken link or an FM-1 that
   came meanwhile connects; the track back if it went; the page catches up by itself (pageLost -> HELLO). The screen
   stays on while connected (FLAG_KEEP_SCREEN_ON); the sound keeps playing with the screen off or the app behind
   (the user's choice). Headphones / Bluetooth in or out: the track follows (AudioDeviceCallback; it stayed on the
   speaker before). Permission after a replug: tick "Always open Yeh nah" on Android's "open with" when plugging in.
   The name: "Yeh nah" (the user, 2026-11-04: the firmware's name, which they will change to it; the device filter
   already matches "Yeh nah"). The icon: ANALOG's head from road.js (android/big/make_icon.mjs, an adaptive vector).
   A release build (the same key: installs over 0.6.x) at build/site/app/yehnah-big.apk; CI builds the release too.
   DONE 2026-11-04 (Yeh nah 0.7.1 on the tablet): replug and MENU > SYSTEM > USB away and back reconnect by themselves;
   the screen locked mid-song: the sound kept playing, the screen caught up on unlocking (no headphone jack: untested).
   0.7.1: Android hid the "Always" tick (warning "could capture audio") without RECORD_AUDIO, so the app asks for it
   once at start (nothing is recorded); with it ticked, no dialog after a replug. "no TABLET interface" logged once.

## To decide (the user)

1. **The USB mode. DECIDED (the user, 2026-11-03): the TABLET entry.** A new TABLET entry in MENU > SYSTEM > USB (recommended: nothing changes for Macs / PCs), or the
   app switches the FM-1 into it by SysEx on connect (handier, but Android may ask for USB permission again after
   the re-enumeration). The second can be added on top of the first later.
2. **The panel: WebView or native. DECIDED (the user, 2026-11-03): WebView.** WebView (recommended) reuses web/emu's panel and the JS road. Native Kotlin
   would need a second port of ui_road.c to maintain.
3. **The screen's look on the tablet. DECIDED (the user, 2026-11-03): sharp.** Sharp pixels (nearest, integer scale: 240 -> 960 px on the tab's 2944 x
   1840 is x4) or smoothed.
