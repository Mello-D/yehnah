# SOUND page + row-list pages: design plan (2026-11-11, for discussion; nothing built)

Goal (the user): FMINI's PATCH list (KNOB 1 picks a row, KNOB 2..4 its settings) for every engine, as one page called
SOUND whose rows depend on the engine; then the same idea over the other pages; the pictures go except on HOME.
Inputs: docs/plans/ENGINE-SETTINGS-AUDIT.md.

## 1. What exists (params.c, ui_lzlist.c)
- A row = `{name, id[3], label[3]}`; `plist_page()` hands out a copy of the page with the row's ids in slots 1..3; the
  existing knob / lock / record / editor code already works on those slots. So a row list is a table plus a cursor.
- Ids >= 0xA0 (`VF(k)`) reach settings that are not track parameters (FMINI's 19, bit fields in P_LN0..6).
- Screen: panel ~232 px wide; a row is up to 14 px high (the AF_X font), 10 rows fit; a row shows 3 settings at x = 64 / 121 / 178.

## 2. The page
- **SOUND** (replaces EDIT 1, EDIT 2, FILTER, VOICE, VOICE 2, DRIFT, PATCH; not HOME). Rows per engine come from a table
  in the engine (`engine_t.rows`, a pointer + count), the generic rows (FILTER, VOICE, GLIDE) appended after.
- Layout: a window of ~8 rows (rows 16 px: a bit larger than FMINI's 14, readable), the cursor row lit, a thin scroll bar
  at the right edge. KNOB 1 moves the cursor (wraps) and the window follows. Section tags (OSC / FILTER / ENV / MOD /
  VOICE) as a dim label left of the first row of each group, so a 25-row engine still reads as 4 groups.
- Row: `NAME   LBL value  LBL value  LBL value` (as now); a row with 1 or 2 settings leaves the rest blank; a row may
  carry a 3-letter unit hint. Enum values (wave names) use the same 6 characters as the knob cards now.
- The knob cards (the four columns below) stay and show the cursor row's name and its three settings (as PATCH does).

## 3. Row counts (from the audit; 3 settings a row)
| Engine | Today | After (rows) |
|---|---|---|
| JUNO, ACID, STRINGS, VPS, TERRAIN, NOISE, LOFI, DRIFT, MODAL, KALIMBA, GUITAR, OMNI, PLAITS | 8 settings | 3-4 engine rows + generic (~8 rows) |
| ANALOG | 8 | + OSC 2 (RANGE WAVE LEVEL), PW, SYNC: ~7 engine rows |
| DIGITAL | 8 | 4 operator rows (RATIO LEVEL DETUNE), INDEX / MDEC / FB / ALG: ~7 |
| PHASE | 8 | + WAVE2 balance, DCW2, DCA envelope, CZ envelopes: ~8 |
| FORMANT | 8 | + F4, KEY TRK, per-formant Q: ~5 |
| WSEQ | 8 | + step editor (step n: WAVE SEMI LEVEL), 16 steps: ~22 |
| FMINI | 10 rows (done) | unchanged |
| HYBRID | 8 | 3 osc x 2 rows, 2 filters x 2, 2 env x 2, 3 LFO, matrix 16 slots / 4 per row: ~30 |
| FM6 | 8 + 3 op pages | 6 operators x 3 rows, algorithm, LFO, pitch env: ~25 |
So the largest list is ~30 rows: the scrolling window is needed. Matrix and step editors are rows with a step number, like
MOD's slot picker today (KNOB 1 the row, a long-press or a row-type "slot" for the index; to decide).

## 4. Storage (the real constraint)
- Today: P_E0..E7 + packed fields in P_LN0..6 (FMINI: 19 settings in 7 words). RAM .data+.bss 98,264 / 98,304.
- Option A (cheap, covers ANALOG / DIGITAL / PHASE / FORMANT / small engines): pack into unused slots per engine, as FMINI.
- Option B (needed for HYBRID, FM6, WSEQ, DIGITAL ops): a per-track "patch" block in the .pool, ~64-160 bytes x 6 tracks,
  saved with the project, locks / automation through the same virtual ids (motion ids already go through MOT_NID).
  Needs RAM freed first (move big .bss arrays into .pool; pool spare 23,360 B).
- Locks: motion ids are P_COUNT + k today (MOT_NID); the SOUND rows would add their own ids; MOT_W (bitset width) grows
  with them: check motion.c for the per-step record size before choosing.

## 5. Order of work
1. Make the PLIST table engine-driven and generic (move FMINI's table into eng_fmini.c; SOUND page = plist for any engine;
   engine_t.rows). Test with the 8 settings of every engine: no new settings yet, the EDIT 1 / EDIT 2 pages retire.
2. Scroll window + section tags (host render test: ui_render).
3. Fold the generic rows in (FILTER, VOICE, GLIDE, VOICE 2, DRIFT); remove their pages; fix page cycling and the breadcrumb.
4. The gaps, engine by engine (ANALOG, DIGITAL, PHASE, FORMANT first: no new storage); then Option B and HYBRID / FM6 / WSEQ.
5. Then the other pages, in the order of the walk below.

## 6. Other pages: first look (to walk through with the user, none off limits)
- ENV + ENV DEST + STAGE: one list (AMP / FILTER / PITCH / SHAPE rows, ADSR + destination per row). The ADSR picture goes.
- LFO + LFO DEST + MOD + MSEQ + MSEQ 2: one list: LFO rows (RATE WAVE PHASE / FADE / dest x4), MOD slots as rows, MSEQ steps.
- FX (SENDS, DIST, AUTO PAN, PHASER), BUSES, DLY 1..3, REVERB 1..2, CHORUS, SLICER, SL BUS 1..2, SL PITCH, PUNCH, MASTER, SCENE:
  one list per block; the pictures (spectrum, curves) are the first to go.
- ARP 1..3, AMBIENT: one list.
- SCL (SCL, CHORD, HARP, OMNI, GUITAR, TUNE, PROG): rows; HARP / GUITAR / TUNE's chord shapes are interactive pictures:
  ask before touching.
- SEQ (PATTERN, TRACK, BAR, STRETCH, STEP, NOTE, SPICE): STEP / NOTE / SPICE edit by the piano roll: ask before touching.
- GLO already uses lists (MAIN, DEVICE, CONTROL, SYSTEM). MIXER, PRESETS, PROJECT, ARRANGE, SONG EDIT, MORPH: ask.

## 7. Decided (user, 2026-11-11)
- Window: 8 rows at 16 px. EDIT 1 / EDIT 2 are retired at step 1 (no alternative view kept).
- DECIDED (user accepted my recommendations): (a) dim section tags in the left margin, KNOB 1 steps rows; (b) one row per copy
  for FM6's 6 operators, a picker row for the 16-slot / 16-step cases (HYBRID matrix, WSEQ steps); revisit at HYBRID / WSEQ / FM6.
- (older wording:) (a) groups (section tags vs a group step first); (b) slot-style rows (HYBRID matrix, WSEQ steps,
  FM6 operators). Default if the user has no preference: (a) section tags, (b) one row per slot.
