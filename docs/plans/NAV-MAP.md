# Navigation map: what each control does now (for the redesign)

Sources: docs/MANUAL.md ("The panel", "The layers") and the chains in firmware/src/ui_lens.c. The right-hand column of
each table is empty for **your proposal**. The four rules to keep: **PRESETS** always loads presets, **ALGORITHM** always
picks the track, **SELECT** always walks pages, **KNOB 1** picks the thing a page edits.

## 1. The encoders and knobs (keep)
| Control | Does now |
|---|---|
| SELECT | Walks the section's pages, one chain, stops at the ends. Held button + SELECT: see section 3 |
| ALGORITHM | Picks the track T1-T6 on any page. With a button held: switches the layer view (MAP / KNOBS / STATE) |
| PRESETS | Always loads presets (sounds; in SEQ, patterns; in GEN, the genre) |
| KNOB 1-4 | The four cards on screen. KNOB 1 picks the thing the page edits |
| MASTER | Output volume |

## 2. The section buttons (tap = go there, tap again = next group; hold = a layer of key shortcuts)
| Button | Tap: groups (tabs) it cycles | Pages | Hold: the layer (the keys become...) | Your proposal |
|---|---|---|---|---|
| HOME | ECO > BEING > GEN | TRACKS, MACRO, ENGINE, ROUTES, IDEAS (TRACK, SOUND, LFO, FX, TEXTURE) | ~0.7 s opens the MENU | |
| EDIT | SOUND > TONE > FOCUS (FM6: SOUND > TONE > OPERATOR; DRUM: KIT > SOUND > LEVELS > TONE) | EDIT 1/2, FILTER, DRIFT, VOICE, VOICE 2, WORDS, FOCUS; OPERATOR pages; DRUM pages | INIT, RECALL, DRUM sound mutes | |
| ENV | TRACKS > ENV > STAGE | ENV, ENV DEST (all tracks, then the track), STAGE | none | |
| LFO | MOD > LFO > MSEQ | MOD, LFO, LFO DEST, MSEQ, MSEQ 2 | MACRO: KNOB 1-4 are M1-M4 (can latch) | |
| FX | TRACK > BUSES > DELAY > REVERB > CHORUS > SLICER > MASTER > PUNCH | 17 pages (SENDS, DIST, AUTO PAN, PHASER, BUSES, DLY 1-3, REVERB 1-2, CHORUS, SLICER x4, MASTER, PUNCH) | Punch-in FX: keys trigger repeat, filter, stutter... | |
| SCL | KEY > CHORD > HARP > OMNI > PROG | SCL, CHORD, HARP, OMNI, PROG | Keys set the root | |
| ARP | one group | ARP, ARP 2 | none | |
| SEQ | PATTERN > TRACK > BAR > STEP > AUTO | PATTERN, STRETCH, PLAYHEADS, TRACK, MUTATE, BAR, STEP, NOTE, COMP, COMP 2, AUTOMATION, AUTO LIST | SEQ TOOLS: clear, reverse, shift, random... G5 clears a step | |
| GLO | MIX > GLOBAL > SYSTEM | MIXER, SENDS, GLOBAL, SYSTEM | Mutes, solos, levels, tap tempo, UNDO | |
| SAVE | SOUND > PROJECT > LIBRARY | PRESETS, MORPH, USER; SECTIONS, ARRANGE; PROJECTS, TOOLS | Play and store sections A-D live; C5 NAME | |
| PLAY | start / stop | | none | |
| REC | arm recording (ONE PASS) | | REC: clear, click, mode, tap tempo | |
| OCT- / OCT+ | keyboard octave; also NO / YES in dialogs | | | |

## 3. Combinations (button held + SELECT, or two buttons)
| Combination | Does now | Your proposal |
|---|---|---|
| HOME + SELECT | GEN (the song generator) | |
| EDIT / SEQ / LFO / FX + SELECT | The "dice": random sound / sequence / LFO / effects (with KEEP / CANCEL) | |
| GLO + SELECT | Tempo from any page | |
| ENV / SCL / ARP / SAVE + SELECT | Jump into that section and along its pages | |
| GLO + PLAY | Restart from the top | |
| GLO + G4 | UNDO (again: redo) | |
| SEQ + REC | Clear all sequences? | |
| REC + a section button | Clear that part's automation | |
| SAVE + OCT+ / OCT- | Store / recall the playing section | |
| SEQ + G5 / SAVE + C5 | Clear step / rename | |
| OCT- + OCT+ at power-on | Hardware calibration | |
| OCT- + OCT+ held 5 s | Update mode | |

## 4. Things I noticed (for you to react to)
- **Uneven sections:** FX has 17 pages in 8 groups; ARP has 2 pages and no layer; ENV has 5; SEQ has 12.
- **Two buttons are really "shortcut" buttons:** ARP (2 pages) and ENV / LFO could be one "SHAPE" or "MOD" section.
- **SCL is two jobs:** the scale (root, scale, quantize) and the chord harp (CHORD, HARP, OMNI, PROG), which is the fork's
  signature and has no section of its own.
- **Save is three jobs:** sounds (presets, morph, user), the song (sections, arrange) and storage (projects, tools).
- **GLO is three jobs:** the mixer (+ sends), tempo, and system settings. Its layer holds mutes, solos and UNDO.
- **LFO's hold is the MACRO layer,** not an LFO shortcut. HOME's hold is the MENU. The hold actions are not consistent.
- **The dice** (random sound, sequence, LFO, effects) sit on four different buttons as hold+SELECT, so they are hard to find.
- **Holds are uneven:** ENV and ARP have no layer of their own (only hold + SELECT to jump); the others do.
- **Where the user's own areas would go:** HOME, the engine designs and the chord harp are what make this instrument
  distinct, so they might deserve their own buttons.

## 5. Questions to settle before any code
1. How many section buttons does the panel have? (The list above has 10 plus PLAY, REC, OCT-/+; the hardware is fixed, so
   the question is which job each button does, not how many.)
2. Should the chord harp get its own button (so SCL goes back to key / scale only)?
3. Should the dice live in one place (a DICE layer, or on HOME) instead of four buttons?
4. Should ENV + LFO + ARP + MACRO be one "MOD / SHAPE" section, or stay separate?
5. Should SAVE be split (SOUND vs SONG), or GLO split (MIX vs SYSTEM)?

## 6. Your proposal, draft 2 (2026-10-31)
| Button | Holds | Moves in / out compared with now |
|---|---|---|
| HOME | TRACKS; SOUND (PRESETS + MORPH, side by side); MIX (MIXER, SENDS, and MACRO if it fits: see below); IDEAS | In: PRESETS, MORPH (SAVE > SOUND), MIXER, SENDS (GLO), MACRO. Out: ENGINE page (marked for removal: it is TRACKS' robot, large, with the four HOME knobs as gauges at the corners), ROUTES page (undecided) |
| EDIT | Everything that designs a sound: EDIT 1/2, FILTER, DRIFT, VOICE, VOICE 2, WORDS, FOCUS, FM6 operators, DRUM pages | MORPH leaves (to HOME) |
| ENV, LFO, FX, SCL, ARP, SEQ | All of their own pages | Unchanged (but see the holds, section 8) |
| GLO | The MENU's tabs (DISPLAY, CONTROL, AUDIO, SYSTEM) + GLOBAL, SYSTEM merged | In: the MENU. Out: MIXER, SENDS |
| SAVE | USER (save a sound to a slot) + SECTIONS, ARRANGE, PROJECTS, TOOLS | Out: PRESETS, MORPH |
| PLAY, REC | As now | |

## 7. What I found for your five answers
1. **ENGINE vs TRACKS: yes, a duplicate.** HOME has two views (code: `home_level`), each with two sets of knobs.
   TRACKS and ENGINE both edit the same four "HOME knobs" of the selected track's engine (e.g. ANALOG: CUT RES ATK REL);
   TRACKS draws the robot scene, ENGINE the engine's pixel picture. ENGINE can go. (An engine is changed on the PRESETS
   page, KNOB 2 ENG, not on ENGINE, so nothing is lost.) What you'd lose is the engine pictures (ui_pixart.c); they
   could be kept as TRACKS' picture, if you want them.
   **MACRO and ROUTES are global in effect.** M1-M4 are one set for the whole project (`macro_v`, the same for every
   track, saved in the project). Each track decides in its MOD slots what each macro moves, and ROUTES lists those
   routes across all six tracks ("T1 CUT+40"). So MACRO (the four dials) fits next to MIX on HOME: dials that move
   several tracks at once. ROUTES is a read-out of the MOD pages, so it could join MACRO as a second view, or go.
2. **MORPH next to PRESETS on HOME** (HOME > SOUND). Done in the table.
3. **Holds:** the full list is section 8. Several are big (the FX punch-in keys, SEQ TOOLS, the SAVE live-section keys).
4. **MENU SYSTEM + GLO SYSTEM:** merge cleanly. Both already show the same TRACKS and LOAD rows (one setting, two
   places). One thing to watch: **GLOBAL (BPM, swing) is saved in the project; CLOCK, TUNE and the MENU's settings are the
   device's** and survive NEW PROJECT / INIT SYSTEM. Putting both in GLO is fine, but BPM and swing should stay visibly
   apart from device settings, or "reset project" and "reset device" get confusing.
5. **GLO with five groups is fine as a chain.** A pop-out or card-style menu would be a later UI topic (visual style).

## 8. Everything reached by holding, with your decisions (2026-10-31)
Code check: the dice on EDIT / SEQ / LFO / FX + SELECT are **already gone** (the manual is out of date). Those buttons held +
SELECT now only jump into the section's pages (`nav_jump`, ui_input.c), and HOME + SELECT opens IDEAS. The MACRO layer's
F3 "roll routes" is the only dice left (ui_layer.c `macro_dice`).
| Where | What it does now | Decision |
|---|---|---|
| HOME held | Opens the MENU | goes (MENU moves to GLO) |
| HOME + SELECT | Opens IDEAS | goes (IDEAS is a HOME page) |
| EDIT held | INIT, RECALL, DRUM-X sound mutes | **goes.** Lost: RECALL (the sound as stored; INIT exists in SAVE > TOOLS); the 8 DRUM sound mutes (could be a mute switch on DRUM's LEVELS pages) |
| EDIT / SEQ / LFO / FX / ENV / SCL / ARP / SAVE + SELECT | Jump into the section and walk its pages (no dice now) | goes with the holds |
| LFO held | MACRO layer: M1-M4 on the four knobs from **any page**, latched on release; F3 roll routes, G3 clear routes | **goes**, with roll and clear. MACRO becomes a HOME > MIX page (M1-M4 set to 0 = off) |
| FX held | Punch-in FX keys | **stays** |
| SCL held | Keys set the root | **stays** |
| GLO held | Mutes, solos, tap tempo, UNDO, levels, drum-group mutes | **mostly goes:** mutes / solos to MIXER KNOB 3, levels are in MIXER, tap tempo is in REC; **UNDO moves to the REC held layer** (a key; to choose) |
| GLO + SELECT, GLO + PLAY | Tempo; restart from the top | goes |
| SEQ held | SEQ TOOLS, G5 clear step | **stays** |
| SEQ + REC | Clear all sequences | stays (SEQ held) |
| REC held | CLEAR, CLICK, MODE, TAP, BPM | **stays** |
| REC + EDIT / ENV / LFO / FX / SCL / ARP / GLO | Clear that part's automation | **goes** (duplicate: SEQ held F4 CLR AUTO clears the track's automation and locks; AUTO LIST deletes single records). Lost: clearing one part (e.g. only FX) |
| SAVE held (+ OCT+/OCT-) | Play / store sections A-D, LOOP / SONG, SONG REC, NAME | **stays** |
| A step held + KNOB 1-4 | Parameter lock | stays |
| A layer button + ALGORITHM | Layer view MAP / KNOBS / STATE | stays with the layers |
| OCT- + OCT+ combos | Calibration / update | stays |
| MENU > CONTROL > HOLD | The hold time | stays (holds remain) |

## 9. New design: MIXER KNOB 3 = OFF / SOLO / MUTE
- MIXER is LEVEL, PAN, REV, DJ FILTER now (KNOB 3 the track's reverb send). KNOB 3 becomes a three-state switch per track:
  OFF (plays), SOLO, MUTE. The reverb send stays on FX > SENDS and on the sends page (nothing lost).
- Mute exists already (`P_MUTE`, per track) but **no page edits it now**: it was MIXER KNOB 4 until 0.9.1 (replaced by the DJ FILTER), and today only the GLO held layer's black keys 1-6 toggle it (ui_layer.c). **Solo does not exist as a stored state:** today a solo is "the key held
  while GLO is held". A stored solo needs one flag per track and a rule: if any track is SOLO, all tracks that are not
  SOLO are silent; it should not be saved in a song. MUTE and SOLO each need a way to be seen on every page (a coloured
  header chip, as the muted-track pale colour now).
- Lost with it: unmute-all (E4), the drum-group mutes KICK / SNARE / HAT / PERC. Unmuting everything is then six turns;
  an option is OCT+ on the MIXER page = "clear all".
- Fits the HOME > MIX move: the mixer page on HOME carries mute and solo.

## 10. Decided / still open
- **Macros:** the LFO hold, ROLL and CLEAR all go. MACRO is a HOME > MIX page. Setting a macro to 0 stops it moving
  CUT, pitch, sends and engine parameters, but a route to the **AMP** destination with a positive amount is silent at
  macro 0 (mod.c: "silent at 0, full at 1"), so such a route must be edited in its MOD slot, not zeroed.
- **UNDO:** into the REC held layer (one key). What it can undo: see the box below.
- **REC + EDIT / ENV / LFO / FX / SCL / ARP / GLO:** goes. SEQ held F4 CLR AUTO clears all of a track's automation; AUTO LIST
  deletes single records.
- **Open:** none. Next: the new button map as a whole (section 6 + 8 + 9), then the page/tab plan under it.

### What UNDO covers (project.c undo_mark, ui_layer.c)
One step, the **whole music** (all six tracks' steps, sounds, mix, effects, tempo, macros) saved just before a big change; UNDO
swaps it back, UNDO again swaps forward. Marks are taken before: loading a project; NEW PROJECT / INIT SYSTEM; the clear
dialogs (clear track, clear all, INIT, RECALL); loading a pattern (PHRASES); the first STRETCH / shape change; an IDEAS throw;
T1 BACK TO ANCHOR (MUTATE); the GEN generator; CLEAR on the REC / SEQ layers. **Not** undone: single knob turns, step edits, a
new preset on its own. A new mark replaces the old one (only the last change can be undone).
