# Yehnah — User Manual

**Firmware v0.2.0 for the M-VAVE FM-1**

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

1. Power on. The name draws itself, then you land on your **home page**. There are two to choose from, **DANCE** (the
   selected track as a small robot on a city street, its four knobs under it; the default) and **TRACKS** (every track's
   sound, length, key and pattern at a glance). You pick which one the unit opens on in **GLO > DEVICE > HOME PAGE**.
2. Turn **ALGORITHM** to pick a track. With the default **TRACKS 4** you can reach T1–T4; T5 and T6 appear with
   **TRACKS 6** (GLO > MAIN).
3. Turn **PRESETS** to load a sound into the track, and play the keys.
4. Turn **KNOB 1–4**: on DANCE they are the four most useful controls of the track's engine.

### Your first idea and your first save

1. On HOME turn **SELECT** right of PRESETS to **IDEAS**: one page whose sub-pages (**TRACK**, **SOUND**, **LFO**, **FX**,
   **TEXTURE**) SELECT scrolls, the name in a header chip at the foot of the panel. On **TRACK**, turn **KNOB 4** (IDEA): a new sequence for the selected track, heard at once. Turn it again for
   another. **OCT+** keeps what you hear, **OCT−** puts back what was there.
2. Press **PLAY**. To record what you play, arm **REC** and play the keys while it runs.
3. Like it? Press **SAVE**: at power-on it opens on **PROJECT** (later, on the SAVE page you were last on; SELECT goes back to PROJECT) (project number, section **A–D**, **LOAD**, **SAVE**). Turn
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
| **SELECT** (encoder) | Walks the pages of the section you are in, one page a detent, round: after the last page comes the first. On a **page of rows** (SOUND, FX, ENV, LFO, ARP, GLO) it scrolls the rows instead: a slow turn one row, a fast turn a whole group; past the last row it goes on to the section's next page. |
| **ALGORITHM** (encoder) | Selects the track (T1–T4, or T1–T6 with TRACKS 6), from any page. |
| **PRESETS** (encoder) | Loads sounds into the selected track, from any page: the next or previous sound of the PRESETS list. It never changes page, track or row. |
| **KNOB 1–4** | The four cells on screen. On a page of rows they set the four values of the row the cursor is on. On a page that picks something (the step, the bar, the slot, the kit's sound, the row) **KNOB 1** picks it. |
| **MASTER** | The output volume. |

### Buttons

| Button | Tap | Hold |
| --- | --- | --- |
| **HOME** | The page of HOME you were last on (at power-on, the one you chose in **HOME PAGE**: DANCE by default). | the **HOME layer**: SWARM's and TRACE's actions, and IDEA ALL, on any page |
| **EDIT** | The **SOUND** page of the selected track. | a list of the sound's named parameters, as a menu that stays open (see [12](#12-the-layers)) |
| **ENV** | The envelopes: **ENV** (one page of rows). | nothing |
| **LFO** | The **LFO** page: the LFO, the step modulator, the modulation matrix. | nothing |
| **FX** | The **FX** page: every effect's rows. | the **FX layer**: punch-in effects |
| **SCL** | The scale, then the chord pages. | nothing |
| **ARP** | The **ARP** page. | nothing |
| **SEQ** | The sequencer pages, starting at **STEP**, the piano roll. | the **SEQ layer**: pattern tools |
| **GLO** | The **GLO** page: tempo, tracks and the settings. | the GLO lists, as a menu that stays open |
| **SAVE** | **PROJECT**, **ARRANGE**, **TRIM**. | the **SAVE layer**: clear, erase, new, save sound |
| **PLAY** | Start / stop. | |
| **REC** | Arms recording on the selected track; while it is lit, turns recording off. | the **REC layer** |
| **OCT− / OCT+** | Keyboard octave (−3 to +3; both together: back to 0). On action pages and in dialogs: **OCT−** = back / NO, **OCT+** = do it / YES. | |

A section's button opens the page of that section you were last on (its first page after power-on, until you have been on another). Pressed again while you are inside it, it goes back to
the page of HOME you were last on (at power-on, your home page: **DANCE** or **TRACKS**, as you have chosen). A button held past the hold time (**GLO > CONTROL > HOLD**) is its layer instead.

### The keys

27 keys from F3 to G5 (16 white, 11 black). They play the selected track. With a layer held they become that layer's
shortcuts; on the drum grid they are steps and lanes; on the NAME screen they type.

In a layer the white keys sit four to a row (F3 to B3, C4 to F4, G4 to C5, D5 to G5), and the on-screen map shows the same rows.
The key LEDs follow the map: the **first key of each row pulses**, the other keys of the row glow, a key that cannot do
anything right now (the screen shows it dim) glows least, a key that is on or held is lit, and a key with no function in
that layer is dark. FX's mutes and tracks, and SEQ's drum lanes, are on black keys, which glow the same way.

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
2. A section's **button** opens the page you were last on in that section (its first page at power-on); pressed again inside it, it goes to the page of HOME you were last on. The **HOME** button goes to that page too (every button remembers its page; only at power-on do they open the first, HOME's being your **HOME PAGE**), and a second tap stays there.
3. **PRESETS** loads a sound: in every section, never a page or track.
4. A page that picks something picks it with **KNOB 1**; the track is **ALGORITHM**'s.
5. A button **held** is its layer.
6. The screen says where you are: the page row at the foot, the page's picture in the middle.

Most pages are **lists of rows**: **SELECT** scrolls the rows, and **KNOB 1–4** set the values of the row you are on
(up to four of them). Each value is a cell with its name, a small picture and its value; a choice between names is
drawn as a grid with the picked one lit. SOUND, FX, ENV, LFO, ARP and GLO are pages of rows. Pages that pick something
(the step on STEP, the slot on PROJECT, the part on TRIM) are not lists: **KNOB 1** picks.

### The chains

| Section (button) | Pages |
| --- | --- |
| **HOME** | DANCE · TRACKS · PRESETS · I-TRK · I-SND · I-LFO · I-FX · I-TEX · MORPH · SWARM · TRACE · XYZ · SENDS · MIXER |
| **EDIT** | SOUND (the selected engine's rows; a drum track has its own, see [6](#6-drums)) |
| **SEQ** | STEP · NOTE · SPICE · AUTO · PATTERN · STRETCH · TRACK |
| **FX** | FX (one page, every effect's rows) |
| **GLO** | GLO (four lists of rows) |
| **ENV** | ENV |
| **LFO** | LFO |
| **SCL** | HARP · OMNI · GUITAR · CHORD · SCALE · TUNE · PROG · USER |
| **ARP** | ARP |
| **SAVE** | PROJECT · ARRANGE (STRUCT) · TRIM |

---

## 4. HOME, sounds and IDEAS

HOME is a row of pages: **DANCE**, **TRACKS**, **PRESETS**, **IDEAS**, **MORPH**, **SWARM**, **TRACE**, **XYZ**, **SENDS** and **MIXER**.
Two of them, **DANCE** and **TRACKS**, are the ones you can make your own **home page**: the page the unit opens at power-on.
**You choose which is your home screen** in **GLO > DEVICE > HOME PAGE** (**DANCE**, the default, or **TRACKS**); the choice is saved with the settings.
After that the **HOME** button (and a section's button pressed again inside it) returns to the page of HOME you were last on, like every other button; a second tap of HOME stays where it is.

### DANCE

One of the two home pages (the default). The selected track is drawn as a robot on a city street, at dusk or by night. It walks, and dances to the notes the
track plays. The four knobs under it are the track's own: the engine chooses which four (for ANALOG: CUTOFF, RESO,
ATTACK, RELEASE). The scene moves with the tempo whether the song plays or not.

### TRACKS

The other home page: choose it in **HOME PAGE** if you would rather see the whole song than the robot. One row per track,
to see the whole song at a glance: its **sound** and **engine**, its **length** in steps, its **key**
(root and scale; a kit has none), the **pattern** written to it drawn as a small roll of its steps (a kit's hits in three
lanes; EMPTY when nothing is written) with the playhead running along it, **ARP** or **REC** after the length, **MUTE**
in place of the key (**SOLO** the same way), and the **level** in dB. The selected track's row is framed, and a muted track's is dimmed. Only the
tracks that play show: four with **TRACKS 4**, six with **TRACKS 6**. **ALGORITHM** picks the track; the knobs then act on it: **KNOB 1 LEVEL**, **KNOB 2 OFF / SOLO / MUTE** (as MIXER's knob 3), **KNOB 3 LEN** (its length in steps) and **KNOB 4 BPM** (the tempo for everything, shown in the header).

### PRESETS

One list over every engine's factory sounds, then your user presets. The columns are **#**, **NAME**, **TYPE** (bass,
lead, pad…) and **ENGINE**. **KNOB 1 TYPE** filters it (ALL, the types A to Z, INIT, USER and **FAV**, your favourites); **KNOB 2 ENG** jumps to the next or
previous engine; **KNOB 3 LETTER** jumps to the next or previous first letter; **KNOB 4 SORT** orders the list by **ENGINE**
(the engines A to Z, each its INIT then A to Z), **TYPE** (the types, each A to Z) or **A-Z** (the whole list); the sort is saved.
Hold **HOME** and press **E4** to mark or unmark the track's sound as a favourite. Turning the PRESETS encoder loads the next sound into the track on any page.
Switching a track to another engine fades the old one out, and the notes you hold go on in the new one. Types, engines and each engine's sounds are all in A to Z order (ALL first on KNOB 1 and INIT, USER last; INIT first in each engine's sounds).

**Every engine has an INIT sound**, its plain starting point, first in its list (a drum track's is the plain DRUM-X
kit). To build a sound from scratch, set **KNOB 1 TYPE** to **INIT** and **KNOB 2 ENG** to an engine, load it, and shape
it on the SOUND page; then save it as a user sound with **SAVE SND** on the SAVE layer.

### MORPH

Blends two tracks into one part: **TRACK A**, **TRACK B**, **BLEND** and **MOVE**. There are three morphs on one page: **SELECT**
scrolls them (**MORPH**, **MORPH 2**, **MORPH 3**, then on to the next page), so up to three pairs can be blended at once
(three pairs use all six tracks; two pairs and two single tracks, or any other grouping, work too). The page shows all three
morphs as rows and frames the one SELECT is on, which its four knobs set. A track can be in one morph only: a morph that names a track an earlier morph has is off (the
page says so). A note played on a morph's B plays its A (and so both), as before.

### MIXER and SENDS

**MIXER:** six strips, one per track, each in its track's own colour (the number, the sound's name, the level column with its meter, PAN and REV; the selected track has a frame round it). **KNOB 1 LEVEL**, **KNOB 2 PAN**, **KNOB 3 MUTE**, **KNOB 4 DJ FILT** (a DJ
filter: low-pass to the left of the middle, high-pass to the right). **SENDS:** the same strips with each track's sends
to the distortion, chorus, delay and reverb (**DIST**, **CHORUS**, **DELAY**, **REVERB**).

### SWARM

Four dots that wander by themselves and move the sound for you: a way to find a sound, best begun from an engine's INIT
sound (see PRESETS above). Each dot has a quarter of the screen as its home (dot 1 top left, 2 top right, 3 bottom left, 4 bottom right) but, with SIZE up, roams all of it. Each dot has its own
colour (the colour of the same-numbered knob on the other pages) and an icon for how it moves: a ring (ORBIT), a dot with a wavy tail (DRIFT), a bow-tie (CHAOS), a puck with a tick along its path (THROW). Every dot moves **two** parameters of the selected track's engine, one with its position across the screen
(X) and one with its position down it (Y); their names and values, in the dot's colour, are in its corner of the screen. They are the engine's livelier
ones, chosen per engine (DRIFT: TILT and DRIFT, SPEED and FLICK, DETUNE and BLOOM, BREATH and PARTS). A few of an engine's
parameters step instead of gliding (a waveform, a ratio, an algorithm), and a dot there jumps between the settings.

A parameter is its own value plus how far its dot is from the middle of its box, so the sound plays as loaded until a
dot moves. Nudge a parameter by hand and its starting point moves with it; load another sound and that sound is the new starting point.

**KNOB 1–4 are always dots 1–4.** **SELECT** scrolls the row they set (past the first or last row it goes on to the
next page):

| Row | What the four knobs set |
| --- | --- |
| **POSITION** | The dot's centre in its box, 0–100 %. 50 % is the middle. Below 50 % the centre slides down the box (25 % is the bottom, where it pops back to the top and slides down to the middle at 50 %); above 50 % it slides left to right (75 % is the right, where it pops to the left and slides to the middle at 100 %). A quick turn throws a THROW dot. |
| **SHAPE** | How the dot moves round its centre: **ORBIT** (a loop), **DRIFT** (a wandering point), **CHAOS** (a path that never repeats and never leaves the screen), **THROW** (a puck that bounces and slows, and is thrown again when it stops). |
| **SIZE** | How far it moves. At 100 % any shape roams the whole screen, not only its box. |
| **SPEED** | How fast it moves. |
| **GAIN** | How much its distance from the middle of its box adds to a parameter: at 50 % the edge of the box is ±50 % of the parameter's range, at 100 % ±100 % (a new SWARM starts at 25 %). Towards either end of a range the movement slows, so a dot seldom pins a parameter at its minimum or maximum. |

**FREEZE, REVERSE, THROW, CAPTURE** and turning SWARM off are keys on the **HOME layer** (hold HOME, [12](#12-the-layers)), so they work on any page. While frozen the parameters stay where they are, so you can still edit them on their own pages. SWARM starts the first time you open its page and keeps running on every page until you turn it off. It drives the track
that was selected when it started; opening its page on another track moves it there. Save a sound you like with
**CAPTURE** first: SWARM is not saved in a project, and a project saved while it runs keeps the parameters as they sound.

### TRACE

SWARM's second page, for playing by hand. The same four dots drive the same two parameters each (SWARM's pairings for the
engine, as the corner text shows), but nothing moves by itself: each dot rides a **shape**, and you turn its knob to take
it round. Every position on the shape is a different pair of values, so turning a knob walks the sound along the shape's
outline. The shape is drawn faintly where the dot walks, in the dot's colour, and stays inside the dot's own quarter of the
screen (so the shape itself shows where the dot is; there is no centre mark). The dot goes round at a constant speed, so
equal turns are equal distances (about seven detents round), and it follows the knob with a short glide (no zipper noise).
The row's name is in the bar at the foot, as on the rows pages.

**KNOB 1–4** are dots 1–4. **SELECT** scrolls the rows (TRAVEL first):

| Row | What the knob sets |
| --- | --- |
| **TRAVEL** | How far round its shape the dot is, 0–100 % (it wraps; a MOBIUS strip's two laps are one round). |
| **SHAPE** | One of 33 shapes, stepping round the whole list. |
| **SIZE** | How big the shape is (at 100 % it fills its quarter of the screen). Shared with SWARM. |
| **ROTATE** | Turns the shape, 0–100 % of a turn (it wraps). |
| **POSITION** | The shape's centre, as in SWARM. Shared with SWARM. |
| **GAIN** | How much the dot's distance from the middle of its quarter adds to a parameter, as in SWARM. Shared with SWARM. |

The shapes: TRIANGLE, SQUARE, TRAPEZOID, DIAMOND, PENTAGON, HEXAGON; the stars PENTAGRAM, HEXAGRAM, HEPT 7/2, HEPT 7/3 and
OCTAGRAM; CIRCLE, LEMNISCATE, CARDIOID, ROSE 3, ROSE 4, LISSAJOUS, SPIRO, WAVE, SPIRAL and HEART; the knots TRIQUETRA and
**MOBIUS** (a lemniscate the dot walks twice: the second lap is on the other side of the band, so the sound comes back to
where it began only after two laps); CROSS and BOLT; and eight constellations, drawn with the stars marked: CASSIOPEIA, BIG DIPPER,
CRUX, ORION, SCORPIUS, LEO, CYGNUS and LYRA. Each is one trail through its stars, and after the last star the dot goes straight
back to the first (a shape is never walked back over itself), so the corners of a figure are sudden changes of direction, and
so of sound. The constellations' positions are approximate.

On the HOME layer TRACE's keys are **F3 FREEZE**, **G3 RANDOM** (all four dots get another shape, any of the 33, at a random
rotation), **A3 REROLL** (only the dot whose knob you turned last, shown in white), **B3 CAPTURE** and **C4 TRACE** (on / off).
SWARM and TRACE share the running sound: the page you open takes over (the dots go on from where they are), and CAPTURE or
off acts on whichever is running. TRACE is not saved in a project either.

### XYZ

One dot you move yourself. **KNOB 1** moves it left and right, **KNOB 2** up and down, anywhere in the rectangle that fills the
panel. Seven invisible lines cross the pad's centre, and each is one parameter: a line's value is where the dot's shadow
falls on it, **100 % where the line meets the edge of the pad, 0 % at the opposite edge**, 50 % at the centre. Each line's
100 % end is a bright dot on the edge with the parameter's name and value just inside it (the faint dot opposite is its 0 %),
so turning KNOB 3 carries the texts round the edge with their dots. The fourteen ends are spread so that, going round the
edge, a 100 % end of one parameter is always next to a 0 % end of another (seven lines can do it exactly; six could not).

| Parameter | Its 100 % end is at the start (going round from the right) |
| --- | --- |
| **PAN** | right |
| the engine's first own setting (the first parameter SWARM's first dot uses on that engine; DETUNE on an engine with none) | up and right |
| **CUT** (the part's filter cutoff) | up (a little left) |
| **DIST** | left and up |
| **RES** (the part's filter resonance) | down and left |
| the engine's second own setting (VINTAGE on an engine with none) | down |
| **PHASER** (its amount) | down and right |

- **KNOB 3 ROTATES** the line set (100 is a whole turn): turned clockwise the lines turn clockwise (and the value goes up). The lines then cut the dot another way, so the sound moves.
- **KNOB 4 is Z**, a third axis for one more parameter: **REV** to begin with. It is drawn as flying down a tunnel of six
  frames: turning Z up zooms the camera forward, so the frames grow towards the edge one after another, the outermost first, and are
  gone once they reach it; at 100 % you are on the floor: the last, outermost rectangle is there, the cross in the middle has grown and
  Z's name is written on it (faint at first, clear at the bottom). Turning back down brings the frames back. KNOB 4 moves Z two
  units a click. The dot throws a longer shadow as Z rises.
  The foot of the panel
  shows X, Y, ROT and Z with its name. On the **HOME layer** the first four keys choose what Z is: **F3 REV, G3 DLY, A3 CUT,
  B3 LEVEL** (the chosen one is lit).
- **One dot moves several lines at once.** A line at right angles to your move stays where it is; the lines along it follow, by
  their share. XYZ is a pad that morphs the sound, not seven faders: rotate it to change which parameters go together.
- **The sound does not change when you open the page.** The dot starts at the place nearest to the sound's six values (two
  positions cannot match six, so each line also keeps an offset that holds its value). It moves from there. A parameter you change
  by hand becomes that line's new starting point. **RE-FIT** (E4 on the HOME layer) puts the dot back to the best place for the sound as it is now.
- **Each track remembers its dot** (and ROTATE and the Z choice): change the selected track with the page open and the dot goes to
  that track's remembered place, or to the best fit for its sound if you have not used it; the track you left keeps the sound the dot
  gave it. **OFF** on a track puts its own sound back and forgets its dot.
- **C4 XYZ** turns it on or, off, puts the sound back as it was. **D4 CAPTURE** keeps the sound as it is and stops XYZ.
- XYZ and SWARM / TRACE never run together (they move the same parameters): opening one page turns the other off and puts its sound back.
  XYZ is not saved in a project: CAPTURE first to keep a sound.

### IDEAS

One page with five sub-pages (**TRACK**, **SOUND**, **LFO**, **FX**, **TEXTURE**: turn **SELECT**; the ends go on along HOME) that throw ideas for the selected track, each heard at once. **KNOB 4** and the other knobs throw; **OCT+**
keeps what you heard, **OCT−** puts back what was there before the page's first throw. A throw on another page or track
keeps the one before: **UNDO** (REC held + A3) still brings it back.

| Page | What it throws |
| --- | --- |
| **I-TRK** (TRACK) | A sequence. **GENRE** (RANDOM, or a genre's progression, voicing and figures), **RHYTHM** (RANDOM, the genre's figure, or one of twelve rhythm cells such as TRESILLO, OFFBEAT or CALL), **ROLE** (AUTO, BASS, CHORDS, MELODY, ARP) and **IDEA** (a turn throws). Only the track's length and key are kept: the sound, level and mix stay. |
| **I-SND** (SOUND) | **PRESET** (another sound of the engine), **VARY** and **WILD** (the sound varied a little or a lot), **ENGINE** (SAME, or ANY engine's sound that suits the role). |
| **I-LFO** | An LFO: **ALL**, **RATE**, **WAVE**, **TARGET**. |
| **I-FX** | Effects: **ALL**, **DRIVE**, **SPACE**, **MOVE**. |
| **I-TEX** (TEXTURE) | What the track plays, made ambient: **SOUND** (KEEP, PAD, CHOIR, HISS, SEA, CRACKLE), **BLUR** (KEEP, SOFT, LONG, HAZE: softer, then each note held to the next) and **SPACE** (KEEP, NEAR, WIDE, VAST: level down, pan aside, reverb up). **BACK** puts back all three. |

---

## 5. Engines and the SOUND page

**25 engines**, all synthesis, any on any track (one exception: the section has **one drum kit**, see [6](#6-drums)). Pick one by loading one of its sounds with PRESETS (or KNOB 2 ENG on
the PRESETS page). There are over 700 factory sounds and 32 user preset slots.

| Engine | What it is |
| --- | --- |
| **ANALOG** | two oscillators (SAW, SQR, TRI, SIN, PWM, SYNC, RING, SAW3) into a per-voice filter |
| **DIGITAL** | a four-operator FM voice |
| **FM6** | six-operator FM from Dexed's core: 32 algorithms and a patch of its own on every track |
| **FMINI** | a Minimoog: a 24 dB ladder filter, oscillators, mixer and 200 factory patches, then INIT and nine more of ours (an acid and a pedal bass, two funk basses, a unison lead, a wide sync-style lead, a sequence, a trill lead and **MINI KIT**, a drum kit: the key picks the drum, C and C# kick, D and D# snare, E, F, A, A# and B toms tuned by the key, F#, G and G# hat) |
| **PHASE** | phase distortion, after the CZ synths (ten more tones after INIT: SYNC LEAD, FLUTE, VIOLIN, SAX, VIBES, CLAV, SLAP BASS, GLASS, VOICE, RING) |
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
- **Kit presets:** the DRUM engine has thirty-two: DRUM-X (the factory kit), 808 BOOM, LOFI DUST, INDUSTRIAL, ACOUSTIC, DUB
  ROOTS, TECHNO RAW, ROCK KIT, JAZZ KIT, 808 CIRCUIT, 909 SWEEP, ARCADE CHIP, RHYTHM BOX, 808 MODEL, 909 MODEL, DIGITAL
  KIT, FM KIT, PUNCH KIT, BIG BOOM and BASIC KIT, then twelve more DRUM-X kits: SUB DEEP, DARK ROOM, THUD, FAT BEAT, BODY, TIGHT, PUNCHY, SNAPPY and CRISP (new kicks, snares, claps, hats and toms: a deep sub kick, a snare with body, dark and bright hats), then three kits aimed at other styles: SPARK GAP (a tight, metallic kit), RAGGA CUT (a short punchy kick with long snares and toms) and SUB TAIL (a very long sub kick). Loading one loads the whole kit into the section.
- **One kit per section.** The kit is the section's, not a track's, so only one track can be on the DRUM engine. Trying to put
  a second track on it (a drum sound in PRESETS, the engine knob, a program change) says TRACK n ALREADY HAS THE KIT and the track
  keeps the sound it had; browsing with the PRESETS knob or the engine knob goes on past the kits.
- **Machines:** DRUM-X, **HORT** (drum models from Hortator, with seven kit presets) and **LCD-4**.
- **SOUND on a drum track** begins with the kit: **KIT** (MORPH, TUNE, TONE · DECAY, NOISE, FM ENV · FOLD, DRIVE),
  **LEVELS** (KICK, SNARE, CLAP, HATCL · HATOP, TOM, RIM, BELL), then **KIT 3** (IMAGE), **X-MOD**, and one group per
  sound (**BD, SD, CP, CH, OH, TM, RS, CB**), each with four rows (**SOUND**, **SOUND 2**, **SOUND 3**, **SOUND 4**:
  the sound's own parameters, its LAYER and SEND), and **FILTER**.
- **The grid.** On a drum track, **STEP** is a grid: eight lanes, the keys are the steps of the lane you pick. Every hit
  keeps its own level and ratchet.
- **Hit modes on the keys.** The octave below plays **SHORT** (half the decay), **GHOST** (short, softer, darker) and
  **FLAM**; the octave above plays **RATCHET ×2, ×4, ×8**, in time with the step grid. They are recorded with the step.

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

### The chord modes

SCL has chord pages **HARP**, **OMNI**, **GUITAR**, **CHORD**, **TUNE**, **PROG** and **USER** (and **SCALE**, between CHORD and TUNE). They are one idea in five
flavours: a key plays a whole chord, and every flavour uses the **same layout**, so what you learn on one carries to the
others. They suit holding the FM-1 in two hands like a console controller, **one thumb on the chord keys, the other
thumb strumming the plate**, with the octave and bend keys under the strumming thumb.

| Mode | What the chord keys play | Chord sets | From |
| --- | --- | --- | --- |
| **CHORD** | The chord on each scale degree, built by the mode you pick (TRIAD, 7TH, ADD9, SUS2, SUS4 in key; MAJ, MIN, 7, MAJ7, MIN7, POWER on any key) | one set: the scale's degrees I–VII, then I an octave up | built in |
| **HARP** | Whole chords from a set of twelve | 128 sets: HARP's own **MAJ1–MAJ8** and **MIN1–MIN8**, then the **J-6's 100 sets** in 16 genres (POP, JAZZ, BOSSA, BLUES, FUNK, R&B, NEOSOUL, TRAD, CLASSIC, CINEMA, SYNTH, HOUSE, EDM, MODERN, STACKS, UTILITY), then **DARK** (6 sets: JUNGLE, LIQUID, NEURO power chords, PHRYGIAN, ANDALUSIAN, RAVE) and **USER** (6 sets you build, below) | HARP's own sets come from Native Instruments' chord sets; the J-6 sets after benbadio's hera-fm1; DARK after TwistedGenetics' FM1-Chord |
| **OMNI** | The Omnichord's chord keys: major, minor and seventh chords | 5 sets: **FOMNI** (the Omnichord's own layout), **MAJ**, **MIN**, **7**, **M7** | FoMni by Charles Vestal |
| **PROG** | The chords of a progression, in order, one a key | 32 progressions (I V vi IV, vi IV I V, I vi IV V and so on) | built in |
| **GUITAR** | Chords as a guitar plays them, up to six strings | 50 chord styles in 9 families (GT ORIG, JAZZ, LOFI, ROCK, BLUES, POP, SOUL, FOLK, AMB), in 14 tunings and 6 shapes | guitchordi by stoogs |

**The layout.** The 27 keys, F3 to G5, are the same in every mode (CHORD uses fewer chord keys, see below):

| Keys | Job |
| --- | --- |
| **F3 – E4** (the lowest twelve, black keys included) | The **chord keys**: chord 1 on F3 up to chord 12 on E4 |
| **F4, G4, A4, B4, C5, D5, E5, F5, G5** (the nine white keys above) | The **plate**: the notes of the chord you struck last, one string a key. A sweep of the other thumb strums it. If the chord has more notes than keys it goes round again an octave up |
| **G#4** and **A#4** | **Octave bend** down and up, while held |
| **C#5** and **D#5** | **Whole-tone bend** down and up, while held |
| **F#5** (the top black key) | **Latch**: the chord keeps sounding after you let go (with the ARP off; with the ARP on it is the arp's HOLD). Press the same chord key again to stop it. The plate's notes never latch |
| **F#4** | Silent, except on PROG, where it picks **WRITE** |

In **CHORD** the chord keys are the lowest eight (F3 to C4: the scale's degrees I to VII and I an octave up), and the plate is
the eleven white keys from D4. The keys light to show what they are, and a chord struck records into the sequencer.

**Octave and bends.** There are four ways to move the notes while you play:

- **OCT− / OCT+** moves the whole keyboard, chords and plate together, up or down by octaves (−3 to +3).
- **PLATE** (KNOB 4 on every chord page; **STRINGS** on OMNI's page) sets the plate an octave down (**−1**), at the chord (**0**) or an octave up (**+1**), so the strumming thumb can sit above or below the chord.
- **G#4 / A#4** bend the whole track an octave down or up for as long as you hold them, gliding there and back; **C#5 / D#5** do the same by a whole tone. Hold two and they add.
- **ROOT** (KNOB 1) moves every set to another key, so a set built on C plays in any key. On a MONO, LEGATO or UNISON voice a chord plays its root alone.

**The pages.** KNOB 1 is always ROOT and KNOB 4 PLATE.

- **CHORD:** KNOB 2 CHORD (the mode: OFF, the in-key and any-key chords, or HARP, OMNI, PROG, GUITAR), KNOB 3 **VOICING** (CLOSE, LEAD for voice leading, OPEN, +BASS).
- **HARP:** KNOB 2 **GENRE** picks a family of sets, KNOB 3 **SET** the set within it.
- **USER:** your own chord sets, written from a list of the chords on a root, as the PRESETS browser lists sounds. It is the last page of SCL (turn SELECT left from HARP). The track plays one of six user sets (**U1–U6**, HARP's USER genre), so the keys play it as you write. Each row is a chord: its number, the chord and the first set that has it (each chord shows once; a J-6 chord as the nearest of the 35 shapes, its root and slash bass kept). **KNOB 1 CHORD** scrolls the list (nothing sounds as you scroll), **KNOB 2 ROOT** picks the root (only the chords on it show), **KNOB 3 SET** the user set written to (with how many of its twelve keys hold a chord). **The top F# key** (always lit) plays the chord picked, as a preview. **OCT+** does not write at once: it asks **HIT A KEY** and lights the twelve chord keys, the ones that already hold a chord solid and the empty ones breathing. The key you hit takes the chord. **OCT-**, a knob or leaving the page cancels. The other keys' lights are off on this page. A new set is empty (twelve C major chords until you write). The sets are saved in flash when you leave the page (the transport stopped: otherwise it says STOP TO SAVE and tries again on the next page change); they sit beside the user presets (the editor's backup does not carry them yet).
- **OMNI:** KNOB 2 **SET**, KNOB 3 **AUTO**, how loud the chord voice is under the strings.
- **PROG:** KNOB 2 **PROG** (1–32) picks the progression, KNOB 3 **RATE** (2 BAR, 1 BAR, 1/2 BAR) how long each chord lasts when written. **OCT+** (or F#4 then OCT+) **writes the progression onto the track** as chord steps; it says TOO LONG if it would not fit in 64 steps.
- **GUITAR:** KNOB 2 **GENRE** picks a family of styles, KNOB 3 **SET** the style. The voicings are a guitar's in standard tuning; **TUNE** changes that.
- **TUNE:** KNOB 1 **TUNING** (STD, DROP D, DBL DD, OPEN D, OPEN G, OPEN E, OPEN A, DADGAD, OPN Dm, OPN Gm, OPEN C, OPN D6, −1/2, −1), KNOB 2 **SHAPE** (1–6: other ways to finger the same chord in that tuning, found by a search of the fretboard).

**Strumming.** The ARP's **STRDN, STRUP, STRALT, BLOCK, BS+CH, SHUF, HALF, 2/3** and **FAST** modes strum the notes of any chord mode low to high (or the reverse, together, or a random half), and the **STRUM** row of the ARP page sets how long a stroke takes. They are modelled on guitchordi's strums, with a small human variation in timing and strength.

**Recording.** A chord you play records into the steps and plays back, and arpeggiates. HARP, OMNI and PROG record the chord itself, so a recorded step can show its name; the J-6 sets, GUITAR and CHORD record the notes. Chord modes do not apply to a drum kit.

### ARP

One page of rows: **ARP** (MODE, RATE, OCT, HOLD), **FEEL** (GATE, SWING, PROB, ORDER) and **SPICE** (SPICE, LOOP,
STRUM). With **HOLD** on, the arp keeps playing when you let the keys go; the next chord replaces it, and hitting a key
the arp is already playing stops it.

- **MODE** has 27 values: UP, DN, UPDN, RND, ORD, REPEAT, DNUP, UP+8, CONV, DIVG, PINKY, THUMB, WALK, CHORD, TRNS, SKIP,
  HOLD, TURNG, STRDN, STRUP, STRALT, BLOCK, BS+CH, SHUF, HALF, 2/3 and FAST. **ORDER** says how the held notes are
  ordered.
- The **STR…** modes strum a chord's notes like a guitar (see [The chord modes](#the-chord-modes)).

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
| **TRIM** | TRACK · PART · CUT START · CUT END | Per-track adjustment across the song: see below. |

**TRIM: per-track adjustment across the song.** The song is a list of parts (the ARRANGE rows). On TRIM, **KNOB 1**
picks a track, **KNOB 2** a part (A 3/6: the third of six), and **KNOB 3 CUT START** and **KNOB 4 CUT END** set how many
bars of that part the track sits out: from its start and from its end. Do it for every track and part and the same
sections play differently each time round: the bass comes in after four bars, the lead drops out for the last two, a
drum track rests through a verse. The picture is the song as one lane per track, with the picked part framed and its
cut shown, and a line under it spells it out (PART 3 · CUT 8 / 4). The cuts belong to the song, not to the section, so
a section reused in two parts can be trimmed differently in each. Cut start and cut end together may take every bar of the part: the
track then sits the whole part out (the line under the picture says **MUTED**, and anything it was playing is released). You
can set cuts while the song plays: they take effect from the next bar. **DEL ROW** on the SAVE layer deletes the part
TRIM is on.

**To save:** stopped or playing, turn **KNOB 4 (SAVE)** right until its card says **OCT+**, press **OCT+**, name it with
the keys or keep the name offered, and press **OCT+** again. **To load:** the same with **KNOB 3 (LOAD)**.

- **PLAY** on ARRANGE plays the song from the top. When the song ends, the music you had before comes back.
- **END:** STOP plays the song once, ×2 … ×8 that many times, LOOP forever (the last row of ARRANGE).
- A row that points to an empty section stops the song and the screen says which.
- **Scenes:** a scene stored on a row recalls the track mutes, the drum group mutes, the macros and the punch-in effects
  held, when the row starts.
- **The SAVE layer** (hold **SAVE**) holds the clear and erase tools for projects, see [12](#12-the-layers).
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

Hold **FX**: the white keys are effects that act while held and go back exactly when you let go. They sit four to a row:
**F3 G3 A3 B3** repeats 1/8, 1/16, 1/32 and half tempo; **C4 D4 E4 F4** stutters 1/8, 1/16, an arpeggio and random notes;
**G4 A4 B4 C5** low-pass and high-pass sweeps, an octave down and up; **D5 E5 F5 G5** a slow attack, short and long decays,
and **CLR PUNCH** (held: erases the punch-in lane where it passes). They come in on the next 1/16. The note effects are **recorded
into the punch-in lane** (REC armed and playing): the lane holds 64 steps of 1/16, saved with the section. The knobs
are **FILTER**, **CRUSH**, **THROW** and **DEPTH**. See [12](#12-the-layers).

---

## 11. Envelopes, LFOs and modulation

### ENV

**ENV** is one page of rows: **ENV** (ATK, DEC, SUS, REL · LOOP) and **TARGETS** (FILTER, PITCH, SHAPE), with a picture of
the envelope.

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
never recorded. Five buttons have a layer: **FX**, **REC**, **SAVE**, **SEQ** and **HOME**. Every other button, held, does
nothing (EDIT opens a menu, below). Only one layer is open at a time.

### FX: punch-in effects

The white keys are the effects (see [10](#10-effects)); the black keys **mute T1–T6** while held; **A#4** chooses
which tracks the effects touch (ALL, SYN, DRM). The knobs are **FILTER** (low-pass to the left, high-pass to the right),
**CRUSH**, **THROW** (sends the dry mix into the delay and reverb) and **DEPTH**. Hold FX until its map opens and let go
without touching anything: the layer stays open (**[FX] LOCKED**), with both hands free; press FX again to close it.

### REC

White keys, packed into rows of four. **F3 G3 A3 B3**: **MODE** (what a REC tap records, one pass or loop; also **KNOB 2**),
**TAP** (tap tempo), **UNDO** (the last big change; again: redo) and **RESTART** (from the top, playing on). **C4 D4 E4 F4**:
**PLAY A–D** (the section on the next bar; two or more in one hold: a quick chain). **G4 A4 B4**: **LOOP / SONG** (what PLAY
plays), **SONG REC** and **NAME** (the project or section on the PROJECT page, or the user sound slot). **D5 E5 F5**:
**CLEAR** (the selected track's steps and automation), **CLR FX** (the punch-in lane) and **CLEAR ALL** (every track and the
lane); the last two ask first. CLICK and COUNT-IN are in GLO > MAIN.

### SAVE: clear, erase, new

White keys from F3. Each tool asks first, and the keys that would do nothing right now stay dim. Several refuse while
the arrangement or the transport is running (the screen says STOP TO EDIT).

| Key | Tool | What it does |
| --- | --- | --- |
| **F3** | **CLEAR PAT** | Empties the selected track's pattern and automation. |
| **G3** | **CLEAR SONG** | Empties the project's arrangement (the ARRANGE rows). |
| **A3** | **DEL ROW** | Deletes the arrangement row ARRANGE or TRIM is on. |
| **B3** | **ERASE SECT** | Erases the section the PROJECT page is on (not while playing). |
| **C4** | **ERASE PROJ** | Erases the whole project: its four sections and its song. No undo. |
| **D4** | **NEW PROJ** | Everything back as at power-on. Undo: REC layer, A3. |
| **E4** | **SAVE SND** | Saves the selected track's sound: over the user slot it came from, or into the first free slot (you name it). |
| **F4** | **ERASE SND** | Erases that user sound slot. |

On XYZ the keys are **F3 Z REV, G3 Z DLY, A3 Z CUT, B3 Z LEVEL** (what KNOB 4 turns), **C4 XYZ** (on / off), **F4 CAPTURE** and **G4 RE-FIT**. **D4 IDEA ALL** and **E4 FAVORITE** are the same in every mode. On TRACE the keys change: **F3** FREEZE, **G3 RANDOM** (a new shape for every dot), **A3 REROLL** (a new shape for the dot you
turned last), **B3** CAPTURE, **C4** TRACE (on / off).

The four knobs do nothing on this layer.

### SEQ: pattern tools

Tools for the selected track's pattern. Each works at once, with a message, and none has an undo of its own (the REC
layer's UNDO covers big changes).

| Key | Tool | What it does |
| --- | --- | --- |
| **F3** | **REVERSE** | Plays the pattern backwards. |
| **G3 / A3** | **SHIFT <  /  SHIFT >** | Moves the pattern one step earlier or later. |
| **B3** | **CLR STP** | Clears the step under the cursor and its parameter locks, and moves on to the next. |
| **C4** | **BEAT** | (drum track) replaces the pattern with a basic beat: kick on the beat, snare on the backbeat, closed hats on the eighths. |
| **D4** | **CLR LN** | (drum track) empties the lane. |
| **E4** | **REV LN** | (drum track) reverses the lane. |
| **F4** | **FILL LN** | (drum track) fills the lane. |

On a drum track the **black keys** pick the lane (1–8) the lane tools act on. The tools refuse while the arrangement plays.

### HOME: SWARM's and TRACE's actions, IDEA ALL

Hold **HOME** on any page (a tap is still HOME) and the keys run **SWARM** (see [4](#swarm)), or **TRACE** (see [TRACE](#trace)) if that was the last of the two you opened, whatever page you are on.
The key is lit while the action is on.

| Key | Action | What it does |
| --- | --- | --- |
| **F3** | **FREEZE** | The dots hold, and the parameters stay where they are. Press again to let them move. |
| **G3** | **REVERSE** | The dots retrace their last 30 seconds backwards, then forwards, then carry on live. Press again to stop where they are: they go on from there. |
| **A3** | **THROW** | Throws every THROW dot at once. |
| **B3** | **CAPTURE** | The sound as it is now becomes the track's own, and SWARM stops. |
| **C4** | **SWARM** | Turns SWARM on; off puts the sound back as it was. |
| **D4** | **IDEA ALL** | The [I-TRK](#ideas) throw on every track that plays: its **GENRE** and **RHYTHM** settings, one genre and one chord progression for all of them (so they fit together) and each track its own role. Only the steps, length and key change. One **UNDO** (REC held + A3) puts it back. Refused while a song plays. |
| **E4** | **FAVORITE** | Marks or unmarks the track's sound as a favourite. |

The four knobs do nothing on this layer.

EDIT and GLO have no held menu: holding them does nothing (the sound's named parameters are on the EDIT page, and GLO's page has the four lists, MAIN, DEVICE, CONTROL and SYSTEM).

---

## 13. GLO: tempo, tracks and settings

**GLO** is one page of four lists (MAIN, DEVICE, CONTROL, SYSTEM). **SELECT** scrolls the rows, and **KNOB 1–4** set the row's values.

| List | Rows |
| --- | --- |
| **MAIN** | **BPM**, **SWING**, **TRACKS** (1–6), **USB LEVEL** (MASTER: follows the MASTER knob; FIXED; or −6 to −24 dB, for a phone or computer whose input clips) · **CLICK** (OFF, REC, ON), **CLICK LEVEL**, **COUNT-IN** (OFF, 1 BAR, 2 BARS) |
| **DEVICE** | **THEME** (the colour theme: VAP DK, VAP LT, NOIRE (greys only), DUSK, CREAM, POP (black with red, blue, green and yellow), YELLOW, RED or PURPLE (black with shades of one colour)), **SCREEN OFF**, **FILL** (RANDOM or SNAKE), **LEDS** (0 to 5: how brightly the idle keys and buttons glow; 0 is no glow, 5 the brightest and the default; lit and pulsing LEDs are always at full) · **SCALE LEDS** · **HOME PAGE** (your home screen, **DANCE** (the default) or **TRACKS**: the page power-on opens; after that the HOME button returns to the page of HOME you were last on) |
| **CONTROL** | **HOLD** (the layer threshold), **KNOB ACCEL**, **FX LATCH**, **CLOCK** (INT, USB, TRS) · **ROUTE** (CH1-6, SEL), **MIDI OUT** (KEYS, CLOCK, ALL, OFF), **SPEAKER EQ** (FLAT, LOWCUT, BASS+), **TUNE** |
| **SYSTEM** | **LOAD** (ECO, LITE, FULL), **CPU**, **USB** (the USB mode), **RESTORE LAST** · **CALIBRATION**, **INIT SYSTEM**, **ABOUT**, **MIDI LEARN** (OFF, LEARN, CLEAR) |

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
  track. **Bank Select** (CC0, or CC32 when CC0 is 0) before it picks a page of 128 sounds: bank 1, program 5 is sound 134. **Channel 16** is the song's: it never loads a sound. Programs **0–3** jump to section **A–D** (playing: on the
  next bar; stopped: at once) and programs **16–21** load project **1–6** (stopped only).
- **Remote buttons:** the **notes of channel 16** press panel buttons and never sound (even with ROUTE SEL): **36** PLAY
  (starts, or stops a running transport), **37** STOP, **38** REC (arms or disarms the selected track), **40–43** section
  **A–D**, **48–53** select track 1–6, **56–61** mute or unmute track 1–6.
- **Step entry by MIDI:** with a track armed on SEQ > STEP and the transport stopped, the notes a MIDI keyboard holds go
  into the cursor step like the keys do: held together they are a chord, and when the last is let go the cursor moves
  on.
- **MIDI LEARN** (GLO > SYSTEM > **MIDI LEARN**, or MENU > CONTROL): set it to **LEARN**, leave the menu, turn the knob of
  the parameter you want (the message says NOW MOVE A CC), then move a controller on your keyboard. That CC now drives
  the parameter over its whole range (on the track the channel plays) and is recorded as automation like a knob move.
  Eight CCs are kept in the settings; a ninth replaces the oldest, and a parameter has one CC. A learned CC wins over the
  list below. **CLEAR** forgets them all. Not learnable: CC0, 1, 6, 32, 38, 64, 98–101 and 120 up.
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
