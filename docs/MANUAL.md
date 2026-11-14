# Yehnah — User Manual

**Firmware v0.1.0 for the M-VAVE FM-1**

This manual is for musicians who have just installed Yehnah. It names every button, knob and page as the screen shows
it. It was rewritten from the firmware's source and checked against it: if you find something here that the unit does
differently, please [open an issue](https://github.com/Mello-D/yehnah/issues).

For a picture tour of what Yehnah is, read the [README](../README.md) first.

## Contents

1. [Quick start](#1-quick-start)
2. [The panel](#2-the-panel)
3. [Pages: how navigation works](#3-pages-how-navigation-works)
4. [HOME, sounds and IDEAS](#4-home-sounds-and-ideas)
5. [Engines and the SOUND page](#5-engines-and-the-sound-page)
6. [Drums](#6-drums)
7. [The sequencer](#7-the-sequencer)
8. [Scales, chords and the arpeggiator](#8-scales-chords-and-the-arpeggiator)
9. [Songs: projects, sections and the arrangement](#9-songs-projects-sections-and-the-arrangement)
10. [Effects](#10-effects)
11. [Envelopes, LFOs and modulation](#11-envelopes-lfos-and-modulation)
12. [The layers](#12-the-layers)
13. [GLO: tempo, tracks and settings](#13-glo-tempo-tracks-and-settings)
14. [MIDI and USB](#14-midi-and-usb)
15. [Web tools: installer, editor, emulator](#15-web-tools-installer-editor-emulator)
16. [Troubleshooting](#16-troubleshooting)

---

## 1. Quick start

### Install

1. Open the **web installer** in Chrome or Edge: <https://mello-d.github.io/yehnah/webapp/installer/>
2. Connect the FM-1 **directly** to the computer with a USB data cable.
3. **Coming from another custom firmware?** Return to the official V15 firmware first, with the installer's **Return to
   official V15** button (it also saves a complete backup). Writing Yehnah straight over another custom firmware has
   left an FM-1 unable to start.
4. Press **Install**. It first checks the FM-1's firmware and writes only over official V15 or Yehnah. Don't unplug the
   cable while it writes.

Yehnah has no user samples: it synthesises everything.

### Your first sound

1. Power on. The name draws itself, then you land on **HOME**: the selected track as a small robot on a city street, its
   four knobs under it.
2. Turn **ALGORITHM** to pick a track. With the default **TRACKS 4** you can reach T1–T4; T5 and T6 appear with
   **TRACKS 6** (GLO > MAIN).
3. Turn **PRESETS** to load a sound into the track, and play the keys.
4. Turn **KNOB 1–4**: on HOME they are the four most useful controls of the track's engine.

### Your first idea and your first save

1. On HOME turn **SELECT** past SENDS to the **IDEAS** pages (**I-TRK**, **I-SND**, **I-LFO**, **I-FX**, **I-TEX**). On
   **I-TRK**, turn **KNOB 4** (IDEA): a new sequence for the selected track, heard at once. Turn it again for
   another. **OCT+** keeps what you hear, **OCT−** puts back what was there.
2. Press **PLAY**. To record what you play, arm **REC** and play the keys while it runs.
3. Like it? Press **SAVE**: its first page is **PROJECT** (project number, section **A–D**, **LOAD**, **SAVE**). Turn
   **KNOB 4** (SAVE) to the right until its card says **OCT+**, then press **OCT+**. Choose another section with
   **KNOB 2** and save a variation into it.
4. Turn **SELECT** to **ARRANGE** (the song's rows): set **SECT** (KNOB 2) and **TIMES** (KNOB 3, the bars), then press
   **OCT+** to add that row. Add a row for A and one for B, and press **PLAY** to hear the song.

That is the loop: **make music → save it into a section → chain sections in ARRANGE**. Stopped music is not saved by
itself unless **RESTORE LAST** is on (GLO > SYSTEM): see [section 9](#9-songs-projects-sections-and-the-arrangement).

---

## 2. The panel

### Encoders and knobs

| Control | What it does |
| --- | --- |
| **SELECT** (encoder) | Walks the pages of the section you are in, one page a detent, round: after the last page comes the first. |
| **ALGORITHM** (encoder) | Selects the track (T1–T4, or T1–T6 with TRACKS 6), from any page. |
| **PRESETS** (encoder) | Loads sounds into the selected track, from any page: the next or previous sound of the PRESETS list. It never changes page, track or row. |
| **KNOB 1–4** | The four cells on screen. A page that picks something (the step, the bar, the slot, the kit's sound, the row) picks it with **KNOB 1**. |
| **MASTER** | The output volume. |

### Buttons

| Button | Tap | Hold |
| --- | --- | --- |
| **HOME** | HOME's first page, TRACKS. | nothing |
| **EDIT** | The **SOUND** page of the selected track. | a list of the sound's named parameters, or the **EDIT layer** (see [12](#12-the-layers)) |
| **ENV** | The envelopes: **ENV**, then **ALL ENV** and **ALL DEST**. | nothing |
| **LFO** | The **LFO** page: the LFO, the step modulator, the modulation matrix. | nothing |
| **FX** | The **FX** page: every effect's rows. | the **FX layer**: punch-in effects |
| **SCL** | The scale, then the chord pages. | the **SCL layer**: set the root with the keys |
| **ARP** | The **ARP** page. | nothing |
| **SEQ** | The sequencer pages, starting at **STEP**, the piano roll. | nothing |
| **GLO** | The **GLO** page: tempo, tracks and the settings. | the GLO lists, as a menu that stays open |
| **SAVE** | **PROJECT**, **ARRANGE**, **TRIM**. | nothing |
| **PLAY** | Start / stop. | |
| **REC** | Arms recording on the selected track; while it is lit, turns recording off. | the **REC layer** |
| **OCT− / OCT+** | Keyboard octave (−3 to +3; both together: back to 0). On action pages and in dialogs: **OCT−** = back / NO, **OCT+** = do it / YES. | |

A section's button always opens that section's first page. Pressed again while you are inside it, it goes back to
**HOME > TRACKS**. A button held past the hold time (**GLO > CONTROL > HOLD**) is its layer instead.

### The keys

27 keys from F3 to G5 (16 white, 11 black). They play the selected track. With a layer held they become that layer's
shortcuts; on the drum grid they are steps and lanes; on the NAME screen they type.

### Other combinations

| Combination | Result |
| --- | --- |
| **GLO** held + **SELECT** | nothing: the tempo is on GLO > MAIN (BPM) |
| **REC** held + **D4** | **UNDO** the last big change (again: redo) |
| **OCT− + OCT+** held at power-on | HARDWARE CALIBRATION |
| **REC** held at power-on | play on one core (see [16](#16-troubleshooting)) |

---

## 3. Pages: how navigation works

Yehnah has **no tabs**. Each section's pages are one chain, shown as a **row of page names at the foot of the screen**,
the page you are on lit. The rules are the same everywhere:

1. **SELECT** walks the section's pages, round.
2. A section's **button** opens its first page; pressed again inside it, it goes to HOME's first page, TRACKS.
3. **PRESETS** loads a sound: in every section, never a page or track.
4. A page that picks something picks it with **KNOB 1**; the track is **ALGORITHM**'s.
5. A button **held** is its layer.
6. The screen says where you are: the page row at the foot, the page's picture in the middle.

Most pages are **lists of rows**: **KNOB 1** scrolls the rows, the other knobs set the row's values. Each value is a cell
with its name, a small picture and its value; a choice between names is drawn as a grid with the picked one lit.

### The chains

| Section (button) | Pages |
| --- | --- |
| **HOME** | TRACKS · PRESETS · MORPH · MIXER · SENDS · I-TRK · I-SND · I-LFO · I-FX · I-TEX |
| **EDIT** | SOUND (the selected engine's rows; a drum track has its own, see [6](#6-drums)) |
| **SEQ** | STEP · NOTE · SPICE · AUTO · PATTERN · STRETCH · TRACK |
| **FX** | FX (one page, every effect's rows) |
| **GLO** | GLO (four lists of rows) |
| **ENV** | ENV · ALL ENV · ALL DEST |
| **LFO** | LFO |
| **SCL** | SCALE · CHORD · HARP · OMNI · PROG · GUITAR · TUNE |
| **ARP** | ARP |
| **SAVE** | PROJECT · ARRANGE (STRUCT) · TRIM |

---

## 4. HOME, sounds and IDEAS

### TRACKS

The selected track is drawn as a robot on a city street, at dusk or by night. It walks, and dances to the notes the
track plays. The four knobs under it are the track's own: the engine chooses which four (for ANALOG: CUTOFF, RESO,
ATTACK, RELEASE). The scene moves with the tempo whether the song plays or not.

### PRESETS

One list over every engine's factory sounds, then your user presets. The columns are **#**, **NAME**, **TYPE** (bass,
lead, pad…) and **ENGINE**. The knobs filter it: **KNOB 1 TYPE**, **KNOB 2 ENG** (an engine, or ALL), **KNOB 3 FAV**
(favourites only) and **KNOB 4 LIST**. Turning the PRESETS encoder loads the next sound into the track on any page.
Switching a track to another engine fades the old one out, and the notes you hold go on in the new one.

### MORPH

Blends two tracks into one part: **TRACK A**, **TRACK B**, **BLEND** and **MOVE**.

### MIXER and SENDS

**MIXER:** six strips, one per track. **KNOB 1 LEVEL**, **KNOB 2 PAN**, **KNOB 3 MUTE**, **KNOB 4 DJ FILT** (a DJ
filter: low-pass to the left of the middle, high-pass to the right). **SENDS:** the same strips with each track's sends
to the distortion, chorus, delay and reverb (**DIST**, **CHORUS**, **DELAY**, **REVERB**).

### IDEAS

Five pages that throw ideas for the selected track, each heard at once. **KNOB 4** and the other knobs throw; **OCT+**
keeps what you heard, **OCT−** puts back what was there before the page's first throw. A throw on another page or track
keeps the one before: **UNDO** (REC held + D4) still brings it back.

| Page | What it throws |
| --- | --- |
| **I-TRK** (TRACK) | A sequence. **GENRE** (RANDOM, or a genre's progression, voicing and figures), **RHYTHM** (RANDOM, the genre's figure, or one of twelve rhythm cells such as TRESILLO, OFFBEAT or CALL), **ROLE** (AUTO, BASS, CHORDS, MELODY, ARP) and **IDEA** (a turn throws). Only the track's length and key are kept: the sound, level and mix stay. |
| **I-SND** (SOUND) | **PRESET** (another sound of the engine), **VARY** and **WILD** (the sound varied a little or a lot), **ENGINE** (SAME, or ANY engine's sound that suits the role). |
| **I-LFO** | An LFO: **ALL**, **RATE**, **WAVE**, **TARGET**. |
| **I-FX** | Effects: **ALL**, **DRIVE**, **SPACE**, **MOVE**. |
| **I-TEX** (TEXTURE) | What the track plays, made ambient: **SOUND** (KEEP, PAD, CHOIR, HISS, SEA, CRACKLE), **BLUR** (KEEP, SOFT, LONG, HAZE: softer, then each note held to the next) and **SPACE** (KEEP, NEAR, WIDE, VAST: level down, pan aside, reverb up). **BACK** puts back all three. |

---

## 5. Engines and the SOUND page

**25 engines**, all synthesis, any on any track. Pick one by loading one of its sounds with PRESETS (or KNOB 2 ENG on
the PRESETS page). There are over 700 factory sounds and 32 user preset slots.

| Engine | What it is |
| --- | --- |
| **ANALOG** | two oscillators (SAW, SQR, TRI, SIN, PWM, SYNC, RING, SAW3) into a per-voice filter |
| **DIGITAL** | a four-operator FM voice |
| **FM6** | six-operator FM from Dexed's core: 32 algorithms and a patch of its own on every track |
| **FMINI** | a Minimoog: a 24 dB ladder filter, oscillators, mixer and 200 factory patches |
| **PHASE** | phase distortion, after the CZ synths |
| **JUNO** | a Juno-106-style voice with its factory bank |
| **ACID** | a TB-303-style mono bass line |
| **VPS** | variable phase shaping |
| **WSEQ** | a sequence of up to 16 steps, each with its own wave, pitch and level |
| **TERRAIN** | eight wavetable surfaces you scan over |
| **VECTOR** | vector synthesis, after the Prophet VS and the Wavestation |
| **HYBRID** | three oscillators, two filters, two envelopes and two LFOs a voice |
| **LOFI** | chip and bytebeat sounds |
| **WHEEL** | a tonewheel organ with drawbars and a rotary speaker |
| **STRINGS** | a 1970s string machine with divide-down registers |
| **GUITAR** | a plucked string (Karplus-Strong); MODEL NYLON, STEEL, ELEC or BASS |
| **KALIMBA** | a kalimba by modal synthesis |
| **MODAL** | struck tines and bars, electric-piano tines and mallets |
| **PLAITS** | four of Mutable Instruments Plaits' models in fixed point |
| **DRIFT** | an ambient voice: six sine partials, each wandering on its own |
| **NOISE** | noise from white to coloured, through a resonant filter |
| **OMNI** | the Omnichord's voices |
| **VOICE** | sung vowels and typed words: formant synthesis after Klatt |
| **ANIMAL** | bird, mammal and insect calls from four physical primitives |
| **DRUM** | the DRUM-X, HORT and LCD-4 drum machines |

### The SOUND page

**EDIT** opens **SOUND**: the selected track's engine as one scrollable list of rows. Each group starts with a header
and its rows are four cells each. Every engine has its own first groups, then these common ones:

| Group | Rows |
| --- | --- |
| **FILTER** | **TYPE** (LP, HP, BP, COMB), **CUT**, **RES**, **DIST** · **VEL>F** (how velocity opens the filter). ANALOG's own filter has **MODL** (SVF, MOOG, MS20, DIODE, SEM, NOTCH, BP, HP) and **DRV**, **KTR** |
| **VOICE** | **VOICE** (POLY, MONO, LEG, UNI, UNI2, UNI4), **PRIO** (LAST, LOW, HIGH), **ALLOC** (ROT, REUSE), **GLIDE** · **MODE** (RATE, TIME), **DETUNE**, **KEY>F**, **MW>** (what the mod wheel does: OFF, VIB, CUT, SHP, WAH, TREM) |
| **DRIFT** | **DRIFT**, **VINTG** |

The engine groups, as they appear on the unit:

| Engine | Groups and rows |
| --- | --- |
| **ANALOG** | OSC: WAVE, DTN, MIX, RANGE · WAVE2, PW, SYNC, NOIS |
| **VOICE** | VOWEL: VOWEL, VOWEL2, TALK · SHIFT, BUZZ, BREATH · Q, F4, KEYTRK, RAND |
| **FM6** | ALGO: ALG, FB, MOD · RATIO, MEG, VMOD · DETUNE, SLOT; then OP 1 … OP 6, each three rows (the operator's pitch, level and envelope) |
| **JUNO** | DCO: WAVE (SAW, PLS, S+P), PW, SUB · NOIS, HPF, CUT, RES · CHOR (OFF, I, II, I+II) |
| **GUITAR** | STRING: MODEL, PICK, PLUCK · DECY, TONE, BODY · MUTE, DAMP |

The other engines follow the same pattern with their own groups of rows. The sound's settings are **kept in the
section** and in user presets; the editor shows them as lists too.

---

## 6. Drums

**DRUM-X** is a synthesised kit of eight sounds, **BD SD CP CH OH TM RS CB**, generated in real time.

- Every sound has two sides, **A** and **B**. **MORPH** (KIT, KNOB 1) moves the whole kit between them.
- **Kit presets:** the DRUM engine has twenty: DRUM-X (the factory kit), 808 BOOM, LOFI DUST, INDUSTRIAL, ACOUSTIC, DUB
  ROOTS, TECHNO RAW, ROCK KIT, JAZZ KIT, 808 CIRCUIT, 909 SWEEP, ARCADE CHIP, RHYTHM BOX, 808 MODEL, 909 MODEL, DIGITAL
  KIT, FM KIT, PUNCH KIT, BIG BOOM and BASIC KIT. Loading one loads the whole kit into the section.
- **Machines:** DRUM-X, **HORT** (drum models from Hortator, with seven kit presets) and **LCD-4**.
- **SOUND on a drum track** begins with the kit: **KIT** (MORPH, TUNE, TONE · DECAY, NOISE, FM ENV · FOLD, DRIVE),
  **LEVELS** (KICK, SNARE, CLAP, HATCL · HATOP, TOM, RIM, BELL), then **KIT 3** (IMAGE), **X-MOD**, and one group per
  sound (**BD, SD, CP, CH, OH, TM, RS, CB**), each with four rows (**SOUND**, **SOUND 2**, **SOUND 3**, **SOUND 4**:
  the sound's own parameters, its LAYER and SEND), and **FILTER**.
- **The grid.** On a drum track, **STEP** is a grid: eight lanes, the keys are the steps of the lane you pick. Every hit
  keeps its own level and ratchet.
- **Hit modes on the keys.** The octave below plays **SHORT** (half the decay), **GHOST** (short, softer, darker) and
  **FLAM**; the octave above plays **RATCHET ×2, ×4, ×8**, in time with the step grid. They are recorded with the step.
- **EDIT layer:** on a drum track the black keys mute the drum sounds.

---

## 7. The sequencer

**SEQ** opens **STEP**. The seven pages:

| Page | Knobs | What it is |
| --- | --- | --- |
| **STEP** | STEP · NOTE · LENGTH · MOVE | The piano roll of the pattern (a drum track: the grid). **KNOB 1** the step, **KNOB 2** its note, **KNOB 3** its length, **KNOB 4** moves it. |
| **NOTE** | NOTE · FLAG 1 · FLAG 2 · VELOCITY | The selected note of the step: its flags (ACC, SLD …) and its velocity. |
| **SPICE** | NOTE · TYPE · value · which component | A step's two components: ARP NOTES, TRIG, OCT, BEND, REV, KILL … |
| **AUTO** | PLAY · EVENT · LOCK · CLEAR | The motion lane: knob moves and incoming MIDI CCs recorded as automation. |
| **PATTERN** | LEN · DIV · SWING · GATE | The pattern: its length in steps (up to 64), its division (1/32 to 2 bars), its swing and its gate. The bars are shown as thumbnails. |
| **STRETCH** | STRETCH · REPEAT · NUDGE · ROTATE | Stretch, repeat, nudge or rotate the sequence. |
| **TRACK** | PITCH · CHANCE · HUMAN · REC Q | The track's pitch offset, how much of it plays, humanise, and the record quantise. |

- **64 steps per track.** Each track has its own length and division, so patterns of different lengths drift in and out
  of phase.
- **Notes** can be longer than a step (ties); chords keep all their notes; a note can be lengthened or shortened
  afterwards on **STEP**.
- **Parameter locks.** Hold a step and turn a knob: that step plays its own value of that parameter.
- **Per step:** ratchets, probability, fill-only steps (the step plays only while FX is held) and micro timing.
- **Recording.** Arm **REC** and play while the transport runs; with **REC Q** below 100 % the timing you played is kept
  as a nudge of up to a half step. **COUNT-IN** and the **CLICK** are in GLO > MAIN.
- **Clearing.** The REC layer has **CLEAR** for the selected track.

---

## 8. Scales, chords and the arpeggiator

### SCALE

**ROOT**, **SCALE**, **QUANT** and **ALL TR**. There are 16 scales (CHR, MAJ, MIN, DOR, MIX, PEN, MPEN, HARM, PHRY, LYD,
LOC, MEL, BLUES, WHOLE, DIMHW, DIMWH). **ROOT** and **SCALE** are global: one key for the song. A **section keeps its
own key**, so a song can change key between sections. **QUANT:** OFF, SNAP (the keys snap to the scale), WHITE (the white
keys walk the scale), SEQ (the sequencer snaps too). **ALL TR** moves every sequence up or down the scale.

### CHORD

A key plays a chord on the track. **CHORD:** OFF; in key: **TRIAD, 7TH, ADD9, SUS2, SUS4**; on any key: **MAJ, MIN, 7,
MAJ7, MIN7, POWER**; and the handheld modes **HARP, OMNI, PROG, GUITAR**. **VOICING:** CLOSE, LEAD (voice leading), OPEN,
+BASS. **PLATE** is the strum plate. The chords record into steps, arpeggiate, and light the keys.

### HARP, OMNI, PROG, GUITAR, TUNE

- **HARP:** a strip of chord keys. **KNOB 2 GENRE** picks a family of chord sets, **KNOB 3 SET** the set within it.
- **OMNI:** the Omnichord's chord keys and strum plate: **SET**, **AUTO**.
- **PROG:** chord progressions: **PROG** (1–32), **RATE**; **OCT+** writes the progression onto the track.
- **GUITAR:** 50 chord styles: **GENRE**, **STYLE**. **TUNE** sets the tuning (14) and the chord shape.
- **STRUM** spreads the notes of any chord mode low to high; it is a row of the **ARP** page.

### ARP

One page of rows: **ARP** (MODE, RATE, OCT, ORDER), **FEEL** (GATE, SWING, PROB, HOLD) and **SPICE** (SPICE, LOOP,
STRUM).

- **MODE** has 27 values: UP, DN, UPDN, RND, ORD, REPEAT, DNUP, UP+8, CONV, DIVG, PINKY, THUMB, WALK, CHORD, TRNS, SKIP,
  HOLD, TURNG, STRDN, STRUP, STRALT, BLOCK, BS+CH, SHUF, HALF, 2/3 and FAST. **ORDER** says how the held notes are
  ordered.
- In GUITAR chord mode a separate **strum player** has its own pattern, division and sweep.

---

## 9. Songs: projects, sections and the arrangement

- A **project** is a song. There are **6 projects**.
- Each project has **4 sections, A–D**. A section is a full snapshot of the music: every track's sound, steps and
  automation, the drum kit and its mutes, the punch-in lane, the slicer, the tempo and the global effects, and the key.
- Each project has one **arrangement**: up to **16 rows**, each a section played for **1–64 bars**, with an optional
  **scene** and an **END** rule.

### SAVE: PROJECT, ARRANGE, TRIM

| Page | Knobs | |
| --- | --- | --- |
| **PROJECT** | PROJECT · SECT · LOAD · SAVE | The project (1–6) and the section (A–D) you are on, drawn as four squares: **USED**, **EMPTY** or **NOW** (the one playing). **LOAD** and **SAVE** act on the section picked. |
| **ARRANGE** (STRUCT) | PROJECT · SECT · TIMES · ROW | The song's rows as chips (the section and its bars; a scene is a dot) and the song as a strip with its length and END. **SECT** and **TIMES** (bars) are what **OCT+** inserts as a new row; **ROW** picks the row you are on (past the last row: **END**, set with **KNOB 2**: STOP, ×2 … ×8 or LOOP); **TIMES** turned below ×1 asks to delete the row. |
| **TRIM** | TRACK · PART · CUT START · CUT END | Cut a track's part of the song. |

**To save:** stopped or playing, turn **KNOB 4 (SAVE)** right until its card says **OCT+**, press **OCT+**, name it with
the keys or keep the name offered, and press **OCT+** again. **To load:** the same with **KNOB 3 (LOAD)**.

- **PLAY** on ARRANGE plays the song from the top. When the song ends, the music you had before comes back.
- **END:** STOP plays the song once, ×2 … ×8 that many times, LOOP forever (the last row of ARRANGE).
- A row that points to an empty section stops the song and the screen says which.
- **Scenes:** a scene stored on a row recalls the track mutes, the drum group mutes, the macros and the punch-in effects
  held, when the row starts.
- **Live sections** (REC layer): tap **A–D** to jump to a section on the next bar; tap several in one hold for a **quick
  chain**; **LOOP / SONG** and **SONG REC** record the song as you play it.
- **Autosave.** With **GLO > SYSTEM > RESTORE LAST** set to ON, the music is kept as you stop and put back at power-on.
  It is **OFF** by default.
- **User presets:** 32 slots, saved from the PRESETS page; **favourites** mark sounds in the list.

---

## 10. Effects

**FX** opens one page of rows, grouped:

| Group | Rows |
| --- | --- |
| **SENDS** | **DIST**, **CHORUS**, **DLY**, **REV**: the track's sends |
| **DIST** | **DIST** amount, **TYPE** (SOFT, HARD, FOLD, CRUSH, RECT, AMP, FUZZ), **TONE** |
| **AUTO PAN** | **DEPTH**, **RATE** (4BAR … 1/32), **SYNC** (FREE, BEAT), **WAVE** (SIN, TRI, SAW, SQR, S&H) |
| **PHASER** | **AMOUNT**, **RATE**, **FDBK** |
| **DELAY** | **TIME** (note values, or 5 ms upward), **FDBK**, **TONE**, **MIX** · **WIDE**, **PITCH**, **SPRAY**, **MOD** · **TYPE** (ANALOG, DIGITAL), **PING** |
| **REVERB** | **TYPE** (ROOM, SPRING, SHIMR, RESO, CLOUD, REVRS), **SIZE**, **DAMP**, **PRE** · **MOD**, **RATE**, **FILT**, **WIDE** |
| **CHORUS** | **RATE**, **DEPTH**, **STEREO** |
| **SLICER** | **MODE** (OFF, GATE, STUT), **PAT**, **RATE**, **DEPTH** · which tracks go in (**T1–T6, FX**) · **SL PITCH**: **PITCH**, **LEN**, **QUANT** (CHR, SCL) |
| **MASTER** | **CLIP**, **PUNCH** (the drum bus), **DUCK**, **REL** |
| **PUNCH** | **LEN**, **RES**: the punch-in lane |

Each effect's row shows a small picture that follows its settings. The DJ filter is on the MIXER. **LOAD ECO**
leaves out the chorus and the slicer.

### Punch-in effects (the FX layer)

Hold **FX**: the white keys are effects that act while held and go back exactly when you let go: repeats (1/8, 1/16,
1/32), low-pass and high-pass sweeps, an octave down or up, half tempo, short or long decays, stutters (1/8, 1/16), a
slow attack, an arpeggio or a fill, and random notes. They come in on the next 1/16. The note effects are **recorded
into the punch-in lane** (REC armed and playing): the lane holds 64 steps of 1/16, saved with the section. The knobs
are **FILTER**, **CRUSH**, **THROW** and **DEPTH**. See [12](#12-the-layers).

---

## 11. Envelopes, LFOs and modulation

### ENV

**ENV** is a list: **ENV** (ATK, DEC, SUS, REL · LOOP) and **TARGETS** (FILTER, PITCH, SHAPE). **ALL ENV** shows the six
tracks' envelopes side by side, **ALL DEST** their destinations.

### LFO

**LFO** is one page of rows:

| Group | Rows |
| --- | --- |
| **LFO** | **RATE**, **WAVE** (SIN, TRI, SAW, SQR, S&H), **PHASE**, **FADE** |
| **TARGETS** | **AMP**, **PITCH**, **FILTER**, **SHAPE**: how much the LFO moves each |
| **MSEQ** | a step modulator: **LEN**, **DIV**, **SLEW**, and 16 **LEVEL** steps in four rows |
| **MOD** | four slots, each a **SRC** and a **DEST** |

**Sources:** LFO, ENV, VEL, KEY, RAND, MODW (mod wheel), AT (aftertouch), EXPR (expression), M1–M4 (the macros), STEP
(the MSEQ) and NOISE (a smooth wander). **Destinations** include PITCH, CUT, SHP, AMP, PAN, DIST, the sends, the engine's
eight values (E1–E8), CLIP, PUNCH and the delay and reverb settings.

---

## 12. The layers

Hold a button past the hold time and the keys and the four knobs change job, while the screen draws a map of what they
do. Press a key or turn a knob while holding for the map at once. Keys pressed under a layer are the layer's: silent,
never recorded. Four buttons have a layer.

### FX: punch-in effects

The white keys are the effects (see [10](#10-effects)); the black keys **mute T1–T6** while held; **A#4** chooses
which tracks the effects touch (ALL, SYN, DRM). The knobs are **FILTER** (low-pass to the left, high-pass to the right),
**CRUSH**, **THROW** (sends the dry mix into the delay and reverb) and **DEPTH**. Hold FX until its map opens and let go
without touching anything: the layer stays open (**[FX] LOCKED**), with both hands free; press FX again to close it.

### REC

The white keys from F3: **CLEAR** (F3: the selected track's steps and automation), **LOOP** (A3: the record mode, also
on the **MODE** knob), **TAP** (B3: tap tempo), **UNDO** (D4: the last big change; again: redo), **RESTART** (E4: from the
top, playing on), **CLR FX** (F4), **PLAY A–D** (G4–C5: the section on the next bar; two or more in one hold: a quick
chain), **LOOP** (D5: loop or song), **SONG REC** (E5), **NAME** (F5) and **CLEAR ALL** (G5).

### SCL

The twelve keys C4–B4 set the **root**: the root lit, the scale's notes breathing. **KNOB 1** is **STRUM** for every chord
mode.

### EDIT

**INIT** (F3) puts the track's sound back to its engine's first preset (a drum track's kit to the factory one); **RECALL**
(G3) puts it back as the section playing stored it. Each asks first. **KNOB 1–4** are the first four of the SOUND page.
On a drum track the black keys mute each drum sound.

### Held menus: EDIT and GLO

Hold **EDIT** or **GLO** past the hold time and a **list menu** opens and stays open after you let go. **EDIT** lists the
sound's named parameters (E1–E8); **GLO** shows the four GLO lists (MAIN, DEVICE, CONTROL, SYSTEM; **KNOB 3** picks the
list). **KNOB 1** picks the row, **KNOB 2** sets its value. Leave with **OCT−** or by tapping the button.

---

## 13. GLO: tempo, tracks and settings

**GLO** is one page of four lists. **KNOB 1** scrolls, the other knobs set the row.

| List | Rows |
| --- | --- |
| **MAIN** | **BPM**, **SWING**, **TRACKS** (1–6), **USB LEVEL** (MASTER: follows the MASTER knob; FIXED; or −6 to −24 dB, for a phone or computer whose input clips) · **CLICK** (OFF, REC, ON), **CLICK LEVEL**, **COUNT-IN** (OFF, 1 BAR, 2 BARS) |
| **DEVICE** | **THEME** (the colour theme: VAP DK, VAP LT, NOIRE, DUSK or CREAM), **SCREEN OFF**, **FILL** (RANDOM or SNAKE), **LEDS** · **SCALE LEDS** |
| **CONTROL** | **HOLD** (the layer threshold), **KNOB ACCEL**, **FX LATCH**, **CLOCK** (INT, USB, TRS) · **ROUTE** (CH1-6, SEL), **MIDI OUT** (KEYS, CLOCK, ALL, OFF), **SPEAKER EQ** (FLAT, LOWCUT, BASS+), **TUNE** |
| **SYSTEM** | **LOAD** (ECO, LITE, FULL), **CPU**, **USB** (the USB mode), **RESTORE LAST** · **CALIBRATION**, **INIT SYSTEM**, **ABOUT** |

- **TRACKS 4** (the default) or **6**: how many tracks play. A note sent to an asleep track says so on screen.
- **LOAD:** ECO, LITE or **FULL**. Voices share a budget of 8 (up to 12 on FULL).
- **USB** (the mode): CONSOLE (stereo audio with the serial console), STEREO (audio and MIDI only), STEMS, IN+OUT, STEMS+IN and
  TABLET. See [14](#14-midi-and-usb).
- **CALIBRATION** opens the hardware calibration (teach each button and knob); **INIT SYSTEM** asks, then puts everything back
  to zero for a new song; **ABOUT** shows the version, the audio load (NOW, PEAK, LATE, SHED since power-on) and the
  credits (scroll with SELECT, OCT− back).

If you hear clicks, open ABOUT and look at **LATE** (audio blocks that came too late) and **SHED** (voices the guard
faded): a photo of that screen helps when you report a problem.

---

## 14. MIDI and USB

- **USB** is class-compliant MIDI and audio: the master output reaches the computer as a USB audio input.
- **MIDI IN** over USB and the **TRS** jack. **ROUTE** (GLO > CONTROL) decides which track each channel plays: **CH1-6**
  plays tracks 1–6 and ignores channels 7–16, **SEL** plays every channel on the selected track. Incoming notes on an
  armed track are recorded like the keys.
- **MIDI OUT** (over USB; GLO > CONTROL > **MIDI OUT**), each track on its channel (T1 = channel 1 … T6 = channel 6):
  **KEYS** (the default: the keys send their notes), **CLOCK** (and the clock, 24 a beat, with START / STOP, while the
  tempo is the FM-1's own), **ALL** (and every note the FM-1 plays: the sequencer, the arp and the punch-in repeats; each
  note gets its note-off) and **OFF**.
- **Clock:** **GLO > CONTROL > CLOCK** USB or TRS follows that input's clock and Start / Continue / Stop. With an external
  clock, the tap tempo and restart are disabled.
- **PROGRAM CHANGE:** program *n* on the channel a track plays loads the PRESETS list's sound number *n* + 1 into that
  track. **Channel 16** is the song's: it never loads a sound. Programs **0–3** jump to section **A–D** (playing: on the
  next bar; stopped: at once) and programs **16–21** load project **1–6** (stopped only).
- **Controllers** (on the track the channel plays): CC5 glide, CC7 level, CC10 pan, CC71 resonance and CC74 cutoff (the
  engine's own, where it has them), CC72 release, CC73 attack, CC75 decay, CC91 reverb send, CC93 chorus send, CC94
  delay send; **CC16–23** the engine's own eight values (E1–E8), **CC102 / CC103** the post filter's cutoff and
  resonance, **CC104** drive, **CC105** sustain. CC1 (mod wheel), CC11 (expression) and channel aftertouch are sources
  of the modulation matrix (MODW, EXPR, AT). Pitch bend, sustain (CC64), All Sound Off (CC120) and All Notes Off (CC123)
  work. A CC is recorded as automation like a knob move.
- **Not possible:** MIDI out on the TRS jack (the FM-1's jack is an input only) and Bluetooth MIDI.
- The USB MIDI and audio name is "Yehnah" ("Yehnah Update" while a package installs).

### USB audio modes (GLO > SYSTEM > USB)

| Mode | What you get |
| --- | --- |
| **CONSOLE** | stereo audio, and the serial console |
| **STEREO** | stereo audio and MIDI only: helps macOS see the audio input |
| **STEMS** | an 8-channel audio input at 44.1 kHz, 16 bit: channels 1–6 the six tracks (mono, with their level, drive, slicer and mutes), channels 7–8 the effects return in stereo (delay, reverb, chorus), all before the master |
| **IN+OUT** | the FM-1 is a USB audio output of the computer or phone as well as an input: what the host plays comes into the last track and is mixed like its sound (its LEVEL, PAN, sends and DUCK apply) |
| **STEMS+IN** | the stems and IN+OUT together, for a phone app: ten channels come back, the voice as the track mixed it and as it came |
| **TABLET** | the mode for the tablet app (the "big FM-1") |

The serial console is off in STEMS, IN+OUT and STEMS+IN; USB MIDI, the editor and the installer still work. Stems are what
each track adds to the mix, so things that depend on other tracks are in them, as in the mix: **DUCK**, and the **shared
voices** (a note another track's note took the voice from ends early with a short fade). Record stems at **LOAD FULL**.

---

## 15. Web tools: installer, editor, emulator

All three run in the browser and are rebuilt with every change to the firmware.

### Installer — <https://mello-d.github.io/yehnah/webapp/installer/>

Chrome or Edge, the FM-1 connected straight to the computer.

- **Install** Yehnah (it checks the FM-1 first: official V15 or Yehnah only).
- **Return to official V15:** give it M-VAVE's unmodified **FM-1.fwsc** (checked locally in the browser). It first saves
  a complete backup. Use this before installing Yehnah if your FM-1 runs **another custom firmware**, and whenever you
  want to go back to stock. Official V15 cannot open Yehnah music; to restore the backup later, reinstall Yehnah and use
  the editor.
- It can also offer earlier versions.

### Editor — <https://mello-d.github.io/yehnah/webapp/editor/>

Parameters from the computer, over USB. Tabs: **Sound**, **FM6**, **Sequencer**, **Tracks**, **Library**, **Projects**,
**Settings**.

- **FM6:** edit patches, import `.syx`, and send them to the device's voice bank.
- **Sound** on a VOICE track: the **Lyric** box (language and text; **Send** writes it to the track).
- **Projects:** the project slots, and the **complete backup**: **Save backup** downloads one file with the current
  music, all **6 projects** with their sections, the arrangements and scenes, the 32 user presets and the settings;
  **Restore backup** replaces all of that on the device. The FM6 voice bank is not included: keep your `.syx` files.

Back up before updating, before returning to stock, and whenever you finish something you love.

### Emulator — <https://mello-d.github.io/yehnah/emu/>

The same code as the FM-1, with sound, in the browser. Play it with the mouse, a touch screen or the computer keyboard:
**Z–'** and **Q–O** are the keys, **1** to **=** the buttons from FX to REC, **[ ]** OCT− / OCT+, **↑ ↓** SELECT,
**← →** ALGORITHM; **Shift-click** holds a button. It also renders **stems** from a backup file, offline.

---

## 16. Troubleshooting

**T5 or T6 is silent.** TRACKS is 4 (the default): T5 and T6 are asleep. Set GLO > MAIN > TRACKS to 6. The screen tells
you when you play into an asleep track.

**Two cores.** The second core renders some synth tracks; GLO > SYSTEM > ABOUT says **CORES 2**, or **CORES 1** and why.
Hold **REC** while powering on to play on one core.

**Clicks, stutters, the load meter full.** Keep TRACKS on 4, try **LOAD LITE** or **ECO**, use fewer voices (MONO, fewer
FM6 tracks), lighter reverbs (CLOUD and SHIMR are the heaviest). Check GLO > SYSTEM > ABOUT > AUDIO LOAD.

**A song won't start.** A row points to an empty section, or there are no rows: save music into that section, or change
the row. **STOP TO EDIT** means a song or quick chain is playing: stop it to edit steps or use UNDO.

**The keys feel inverted or a button does the wrong thing.** Run the hardware calibration: hold **OCT−** and **OCT+**
while powering on (or GLO > SYSTEM > CALIBRATION) and teach each button and knob. A piano key cancels.

**USB audio input doesn't appear on macOS.** GLO > SYSTEM > USB **STEREO** (or STEMS).

**SAVED (NO FLASH).** The flash chip wasn't detected; saves stay in RAM only. Power-cycle the device. If it persists, back
up what you can with the editor and reinstall.

**The FM-1 stays black after an interrupted update.** Check whether the computer sees it as **WL80UBOOT** (a USB device
4C4A:8057): that is the chip's boot mode and the installer can write it again (try another cable). To enter update mode
deliberately from a running device, hold **OCT− + OCT+** for 5 seconds. If that isn't enough,
[FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) reads and writes the flash with a Seeed XIAO RP2040.

**I want the factory firmware back.** Use the installer's **Return to official V15** (it backs up first), or M-VAVE's own
updater.

**I came from another custom firmware. Can I install directly?** No: return to official V15 first with the installer,
then install Yehnah. Going straight from another custom firmware has bricked an FM-1.
