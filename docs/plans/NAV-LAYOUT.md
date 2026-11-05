# Navigation layout: every button's tabs and pages (draft 2, 2026-11-01: every choice answered)

Built from NAV-MAP.md sections 6, 8, 9, 10 (the button map and the holds you decided) and the chains as they are now
(firmware/src/ui_lens.c CE_* / CG_*). Planning only: no code changed.

**(2026-11-03) A done plan, kept for its reasons.** Built 2026-11-02 / 03; the firmware has moved on since: no tabs (one
page row per button), and the holds of 3e58851: only FX, SCL and REC have a layer. SEQ's tools are a page (SEQ > TOOLS;
G5 clear step gone, the NOTE knob's NONE instead), SAVE's song keys are on REC held (G4 A4 B4 C5 PLAY A-D, D5 LOOP / SONG,
E5 SONG REC, F5 NAME; STORE A-D and OCT+ / OCT- gone), and every other button held does nothing (not a tap).
The "Holds and combinations" table below is updated to that; the sections' own hold lines are not.

How to read it: a **tab** is a group (tapping the button again goes to the next tab); the **pages** are what SELECT walks,
in order, one chain from the first tab's first page to the last tab's last page. **Opens on** = the page shown the first time
the button is tapped after power-on (after that, the page last shown). **(2026-11-02, held notes a b: superseded. A
button always opens its chain's first page; pressed again on its own pages it goes to HOME > TRACKS. The column below is
stale.)** `(?)` = a choice for you; `NEW` = a page that does not
exist yet; `MOVED` = comes from another button. The four rules stay: PRESETS loads, ALGORITHM picks the track, SELECT walks
pages, KNOB 1 picks the thing the page edits.

## Summary
| Button | Tabs | Pages (now -> new) | Hold layer | Opens on |
|---|---|---|---|---|
| HOME | TRACKS > PRESETS > MIX > IDEAS | 9 -> 11 | none (MENU moves to GLO) | TRACKS |
| EDIT | synth: SOUND > TONE > FOCUS; FM6: SOUND > TONE > OPERATOR; DRUM: KIT > SOUND > LEVELS > TONE | 8 / 9 / 13 (unchanged; VOICE 2 gains KEY>F, MW>) | none (INIT / RECALL / drum mutes go) | EDIT 1 (KIT on DRUM) |
| ENV | TRACKS > ENV > STAGE | 5 (unchanged) | none | ENV . ENV |
| LFO | MOD > LFO > MSEQ | 5 (unchanged) | none (the MACRO layer goes) | LFO . LFO |
| FX | TRACK > BUSES > DELAY > REVERB > CHORUS > SLICER > MASTER > PUNCH | 17 (unchanged) | punch-in FX (stays) | SENDS |
| SCL | KEY > CHORD > HARP > OMNI > PROG | 5 (unchanged) | keys set the root (stays) | SCL |
| ARP | ARP | 2 -> 3 (NEW ARP 3: SPICE, LOOP) | none | ARP |
| SEQ | PATTERN > TRACK > BAR > STEP > AUTO | 12 (unchanged) | SEQ TOOLS (stays) | STEP |
| GLO | PROJECT > DISPLAY > CONTROL > AUDIO > SYSTEM | 4 pages + the MENU's 26 rows -> 10 pages | none (mutes / solos / UNDO move) | PROJECT . TEMPO |
| SAVE | USER > PROJECT > LIBRARY | 7 -> 5 | song controls, sections A-D (stays) | USER |
| PLAY | no pages: start / stop | | none (GLO + PLAY restart goes) | |
| REC | no pages: arm recording | | REC SET (stays) + **UNDO** | |

Pages in all (a synth track): 75 now (the MENU counted as one) -> 81: HOME +2, GLO +5 (the MENU opened out), ARP +1, SAVE -2. Dead pages deleted on top of that: OP1-4
ENV, OP LEVEL (and the whole DIGITAL chain CE_DIG / CG_DIG: DIGITAL is a plain synth since step 2, so it is never used), RETRIG,
ROTOR.

## HOME (the instrument's face)
| Tab | Pages in order | Knobs (KNOB 1 .. 4) | Notes |
|---|---|---|---|
| TRACKS | 1 TRACKS | the four HOME knobs of the selected track's engine (ANALOG: CUT RES ATK REL) | The robot scene. ENGINE page removed (same knobs). The engine pictures stay: EDIT, SAVE, the USER slots and IDEAS draw them too |
| PRESETS | 2 PRESETS (MOVED from SAVE), 3 MORPH (MOVED from SAVE) | PRESETS as now (KNOB 2 ENG changes the engine) | PRESETS + MORPH side by side, as decided |
| MIX | 4 MIXER (MOVED from GLO), 5 SENDS (MOVED from GLO), 6 MACRO (today's ROUTES screen, renamed) | MIXER: LEVEL, PAN, **OFF / SOLO / MUTE** (NEW, was REV), DJ FILTER; MACRO: M1-M4 (0 = off) | MIXER KNOB 3 per NAV-MAP 9; OCT+ on MIXER = clear all mutes / solos |
| IDEAS | 7 TRACK, 8 SOUND, 9 LFO, 10 FX, 11 TEXTURE | as now | Reached by HOME taps only (HOME + SELECT goes) |

MACRO (decided): today's MACRO and ROUTES pages both put M1-M4 on the four knobs; MACRO draws the tracks' triangles behind
them, ROUTES lists what each macro moves on every track ("T1 CUT+40"). One page stays, named MACRO, with ROUTES' screen.

Tab names (decided): HOME's second tab is PRESETS, SAVE's first is USER, EDIT's keeps SOUND.

## EDIT (designing the sound; the chain follows the selected track's engine)
| Engine | Tab | Pages in order |
|---|---|---|
| synth (all but FM6 / DRUM) | SOUND | 1 EDIT 1, 2 EDIT 2 |
| | TONE | 3 FILTER, 4 DRIFT, 5 VOICE, 6 VOICE 2, 7 WORDS (VOICE engine only) |
| | FOCUS | 8 FOCUS |
| FM6 | SOUND | 1 EDIT 1, 2 EDIT 2 |
| | TONE | 3 FILTER, 4 DRIFT, 5 VOICE, 6 VOICE 2 |
| | OPERATOR | 7 OPERATOR, 8 OP ENV, 9 OPERATOR 2 |
| DRUM | KIT | 1 KIT, 2 KIT 2, 3 KIT 3 |
| | SOUND | 4 SOUND, 5 SOUND 2, 6 SOUND 3, 7 SOUND 4 |
| | LEVELS | 8 LEVELS, 9 LEVELS 2 |
| | TONE | 10 X-MOD, 11 FILTER, 12 STEREO, 13 VOICE |

EDIT notes:
- **VOICE 2** gains the two settings with no page (decided: my call): ALLOC, DETUNE, **KEY>F** (higher notes brighter: the
  filter follows the keyboard), **MW>** (how much a MIDI keyboard's mod wheel moves the sound). Its knobs 3 and 4 are free
  now; FILTER's four are full. Both are "how playing shapes the sound", as VOICE 2's are.
- DRUM sound mutes, RECALL: go with the EDIT hold (decided).
- FOCUS stays (you want every page) but repeats EDIT 1/2 + MOD: still a ditch candidate.

## ENV (unchanged)
| Tab | Pages |
|---|---|
| TRACKS | 1 ENV (all six tracks), 2 ENV DEST (all six tracks) |
| ENV | 3 ENV, 4 ENV DEST (opens here) |
| STAGE | 5 STAGE (repeats ENV large: ditch candidate) |

## LFO (unchanged pages; no hold now)
| Tab | Pages |
|---|---|
| MOD | 1 MOD (four slots: source, target, amount) |
| LFO | 2 LFO (opens here), 3 LFO DEST |
| MSEQ | 4 MSEQ, 5 MSEQ 2 |

## FX (unchanged; hold = punch-in FX)
| Tab | Pages |
|---|---|
| TRACK | 1 SENDS, 2 DIST, 3 AUTO PAN, 4 PHASER |
| BUSES | 5 BUSES |
| DELAY | 6 DLY, 7 DLY 2, 8 DLY 3 |
| REVERB | 9 REVERB, 10 REVERB 2 |
| CHORUS | 11 CHORUS |
| SLICER | 12 SLICER, 13 SL BUS, 14 SL BUS 2, 15 SL PITCH |
| MASTER | 16 MASTER |
| PUNCH | 17 PUNCH |

FX . SENDS (this track's four sends) and HOME . MIX . SENDS (all tracks' sends) both stay: one per track, one overview. Eight
tabs is NAV_GMAX (8), the most a chain may have.

## SCL (unchanged; hold = keys set the root)
| Tab | Pages |
|---|---|
| KEY | 1 SCL |
| CHORD | 2 CHORD |
| HARP | 3 HARP |
| OMNI | 4 OMNI |
| PROG | 5 PROG |

## ARP (unchanged)
| Tab | Pages |
|---|---|
| ARP | 1 ARP, 2 ARP 2, 3 ARP 3 NEW: SPICE, LOOP (the TURING mode's random melody and its repeat), -, - (ARP 2's four knobs are full) |

## SEQ (unchanged; hold = SEQ TOOLS, G5 clear step, REC = clear all)
| Tab | Pages |
|---|---|
| PATTERN | 1 PATTERN, 2 STRETCH, 3 PLAYHEADS |
| TRACK | 4 TRACK, 5 MUTATE |
| BAR | 6 BAR, 7 STEP (opens here), 8 NOTE |
| STEP | 9 COMP, 10 COMP 2 |
| AUTO | 11 AUTOMATION, 12 AUTO LIST |

Not in the chain (reached from code, as now): SONG, PHRASES, CHANCE.

## GLO (settings: the project's first, then the device's)
The MENU's rows become ordinary four-knob pages, so the rules hold here too (today the MENU uses ALGORITHM for its tabs and
PRESETS for its rows, which breaks rules 1 and 2). The PROJECT tab is kept first and apart: everything after it is the
device's and survives NEW PROJECT / INIT SYSTEM.
| Tab | Pages in order | KNOB 1 .. 4 | Saved in |
|---|---|---|---|
| PROJECT | 1 TEMPO | BPM, SWING, -, - | the project |
| DISPLAY | 2 SCREEN | COLOR, SKIN, STYLE, LARGE | device |
| | 3 MOTION | ANIM, SCOPE, SCREEN OFF, - | device |
| | 4 LEDS | LEDS, SCALE LEDS, -, - | device |
| CONTROL | 5 CONTROL | HOLD, KNOB ACCEL, FX LATCH, BPM LOCK | device |
| | 6 MIDI | CLOCK (sync, from GLOBAL), MIDI IN ROUTE (from SYSTEM), MIDI OUT, - | device |
| AUDIO | 7 AUDIO | SPEAKER EQ, USB LEVEL, TUNE (from GLOBAL), - | device |
| | 8 CLICK | CLICK, CLICK LEVEL, COUNT-IN, - | device |
| SYSTEM | 9 SYSTEM | TRACKS 4 / 6, LOAD (ECO / LITE / FULL), CPU (read-out), USB | device |
| | 10 TOOLS | RESTORE LAST, CALIBRATION, INIT SYSTEM, ABOUT | device (actions) |

Where the 34 old rows went: GLOBAL's 4 (BPM SWING -> TEMPO; CLOCK -> MIDI; TUNE -> AUDIO), SYSTEM's 4 (TRACKS LOAD CPU ->
SYSTEM; ROUTE -> MIDI), the MENU's 26 (TRACKS and LOAD were in both: one row each now). 32 settings in 10 pages, 8 knobs empty.

GLO notes (decided):
- Actions (CALIBRATION, INIT SYSTEM, ABOUT, RESTORE LAST): turning the knob opens a YES / NO dialog, OCT+ = yes, OCT- = no.
- TEMPO's knobs 3 and 4 and the other empty knobs stay empty for now.
- Pop-out / card menus: the later visual-style topic.

## SAVE (keeping your work)
| Tab | Pages in order | Notes |
|---|---|---|
| USER (was SOUND) | 1 USER (opens here) | Your own sounds: save the track's sound to a slot, load one back. PRESETS and MORPH have left for HOME |
| PROJECT | 2 SECTIONS, 3 ARRANGE | |
| LIBRARY | 4 PROJECTS, 5 TOOLS | INIT (a sound) stays in TOOLS |

Hold (stays): play / store sections A-D, LOOP / SONG, SONG REC, C5 NAME; + OCT+ / OCT- store / recall the playing section.

## PLAY and REC
- **PLAY**: start / stop. GLO + PLAY goes; restart from the top moves to the REC layer (E4).
- **REC**: tap arms (ONE PASS / LOOP). Held, the SET layer (white keys from F3): F3 CLEAR, G3 CLICK, A3 MODE, B3 TAP, C4
  COUNT-IN; KNOB 1-4 the same rows + BPM. **UNDO NEW** on D4 and **RESTART NEW** (from the top, was GLO + PLAY) on E4,
  the next two white keys (decided). UNDO again = redo; what it covers: NAV-MAP.md "What UNDO covers".
- REC + a section button (clear that part's automation) goes; SEQ held F4 CLR AUTO and AUTO LIST cover it.

## Holds and combinations, after the redesign (all of them)
| Gesture | Does |
|---|---|
| FX held | Punch-in FX keys |
| SCL held | Keys set the root |
| REC held | CLEAR, CLICK, MODE, TAP, COUNT-IN, BPM, UNDO, RESTART, REVERT ALL; PLAY A-D, LOOP / SONG, SONG REC, NAME (2026-11-03) |
| A layer button + ALGORITHM | Layer view MAP / STATE |
| A step held + KNOB 1-4 | Parameter lock |
| OCT- + OCT+ at power-on / held 5 s | Calibration / update mode |
| Gone | HOME held (MENU), HOME + SELECT, EDIT held, LFO held (MACRO, roll, clear), GLO held, every button + SELECT, GLO + PLAY, REC + a section button |

Three buttons keep a layer (FX SCL REC, 2026-11-03); the others held do nothing when let go (was: act as a tap).

## Your choices (all answered, 2026-11-01)
| # | Choice | Answer |
|---|---|---|
| 1 | ROUTES | One page, **MACRO**, with ROUTES' screen (M1-M4 + what each moves); today's MACRO page goes |
| 2 | Three "SOUND" tabs | HOME's is **PRESETS**; SAVE's (now only USER) is **USER**; EDIT's stays SOUND |
| 3 | Engine pictures | **Kept**: only the ENGINE page goes; EDIT, SAVE, USER and IDEAS draw them too |
| 4 | MIXER OCT+ | **Clears all mutes / solos** |
| 5 | DRUM sound mutes | **Go** |
| 6 | MW>, KEY>F, SPICE / LOOP | KEY>F, MW> on **VOICE 2**; SPICE, LOOP on a NEW **ARP 3** |
| 7 | RECALL | **Goes** |
| 8 | GLO actions; empty knobs | **YES / NO dialog** (OCT+ / OCT-); empty knobs stay empty |
| 9 | UNDO, restart | **D4** UNDO, **E4** RESTART on the REC layer |
| 10 | Holds on buttons with no layer | **Act as a tap** |

Nothing open in the layout. Next topic: the visual style (knob squares, tabs, page frames), one page rendered first.

## Page and knob names for the scrolling page row (suggested 2026-11-02, for you to edit)
With no tabs, each page's name must stand on its own, and every name must fit at the legible sizes (mockups/legible.py):
the page shown 14 px bold in 68 px, the pages beside it 12 px in 56 px, knob names 13 px in 56 px.

**Knob names:** the firmware's own 216 labels all fit (the widest, AMOUNT, is 45 px). The long words in the mock-ups were
mine. Suggested rule: a label may be the full word where it fits in 56 px (CUTOFF 38, ATTACK 36, RELEASE 43, SUSTAIN 41,
FEEDBACK 52, DETUNE 40), else a short form: RESONANCE (60) -> RESO.

**Pages renamed** (the rest keep their names; all fit):
| Button | Now | Suggested | Why |
|---|---|---|---|
| HOME | TRACK, SOUND, LFO, FX, TEXTURE (the IDEAS pages) | IDEA TRK, IDEA SND, IDEA LFO, IDEA FX, IDEA TEX | without the IDEAS tab they read as other pages (TRACK beside TRACKS, LFO and FX as buttons) |
| ENV | ENV, ENV DEST (all six tracks), ENV, ENV DEST (this track) | ALL ENV, ALL DEST, ENV, ENV DEST | the same two names twice |
| FX | DLY, DLY 2, DLY 3 | DELAY, DELAY 2, DELAY 3 | DLY was short for a tab; DELAY fits now |
| FX | SL BUS, SL BUS 2, SL PITCH | (kept: SL BUS, SL BUS 2, SL PITCH) | (2026-11-02) SLICE ... did not fit the middle pill in its bold face (B, <= 62 px) |
| SEQ | PLAYHEADS | PLAYHEAD | (2026-11-02) PLAYHEADS (69 px in B) did not fit the middle pill |
| SEQ | AUTOMATION, AUTO LIST | AUTO, AUTO LIST | AUTOMATION (73 px shown, 62 beside) does not fit |
| EDIT (FM6) | OPERATOR, OP ENV, OPERATOR 2 | OP 1, OP ENV, OP 2 | OPERATOR 2 only just fits; the three then read as a set |
| SCL | SCL | SCALE | the page, named as a word (34 px) |

Optional: EDIT 1 / EDIT 2 could take the engine's own section names (ANALOG: OSC, FLT; desc.json ENG titles), so the
row says what is on the page; they differ per engine.
