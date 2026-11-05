# Hiding rarely used pages: first cut (MENU > PAGES: ALL / COMMON), in plain words

Where the information comes from: the page list (params.c PAGES[]) and the section chains (ui_lens.c). The breadcrumb is what
the screen shows at the foot: SECTION . GROUP . page. "J" = Yehnah or fork origin (a
candidate to ditch or redesign).

Two settings: **ALL** (today, the default) and **COMMON** (the rarely used pages hidden). The column **Shown in COMMON**
says: **yes**, or **HIDE** (and why). Hidden pages keep their values and still sound; only the editing is hidden. A hidden
page also leaves the quick layers and the section tabs. Dead pages are deleted, not hidden.

## First, the words (read this once)
| Word | What it means |
|---|---|
| **Track** | One instrument line. The FM-1 has 6. Each plays one sound (a drum kit, a bass, a piano...) with its own notes. |
| **Engine** | The kind of sound-maker a track uses (ANALOG = classic synth, FM6 = glassy electric-piano type, DRUM = drum machine, VOICE = a singing voice...). |
| **Preset** | A ready-made sound you load, like picking a patch. |
| **Pattern / Step** | A pattern is a short loop of notes (usually 1-4 bars). A step is one slot in it, like one box on a grid where a note can sit. |
| **Piano roll** | A picture of the pattern: pitch up the side, time across, notes as blocks. |
| **Envelope (ENV)** | How a note's loudness changes over time: **A**ttack (how fast it fades in), **D**ecay (the dip after), **S**ustain (the held level), **R**elease (the fade after you let go). A piano is fast attack, long release; a pad is slow attack. |
| **Filter** | Removes high sounds (or low ones). **Cutoff** = where it starts cutting (low cutoff = muffled, high = bright). **Resonance** = a whistly boost at that point. |
| **LFO** | A slow, automatic wobble that nudges something up and down: wobbling pitch is vibrato, wobbling volume is tremolo, wobbling the filter is the "wah-wah" bass. |
| **Effect** | Something added to the sound after it's made: **Delay** (echoes), **Reverb** (the sound of a room or hall), **Chorus** (thickens, like two players slightly apart), **Distortion** (crunch). |
| **Send** | A dial that says *how much* of one track goes into an effect. Think of a shared echo unit: each track decides how much of itself to send into it. Send 0 = dry, no effect; send high = a lot of echo. One effect shared by all tracks, instead of an effect on every track. |
| **Bus** | The shared effect itself: the "place" the sends feed into (the delay bus is the shared echo unit). |
| **Mixer** | Volume and left-right position (pan) for each track, to balance them. |
| **BPM** | Beats per minute: the tempo. **Swing** pushes every second note late for a bouncy feel. |
| **Quantize** | Snaps notes to the beat's grid so a sloppy rhythm sounds tight. |
| **Scale / Root** | The set of notes that sound "right" together (major, minor...) and the home note it is built on (the root). Locking to a scale means no wrong notes. |
| **Chord** | Several notes at once. **Arpeggiator (ARP)**: plays the notes of a held chord one at a time, in a pattern. |
| **Humanize** | A little random timing and loudness so a pattern sounds played, not mechanical. |
| **Parameter lock / automation** | Changing a knob during the pattern, step by step, so the sound moves (e.g. the filter opens on step 5). |
| **Section / Song** | A project is made of up to four sections (A-D, e.g. verse, chorus); the song says in what order they play. |
| **Project** | Your whole piece: all six tracks, patterns and settings, saved in one of 8 slots. |

## EDIT (shape the sound; the page names change with the engine)
| Page | Breadcrumb | What it does, in plain words | In COMMON | Why |
|---|---|---|---|---|
| EDIT 1 | EDIT . SOUND . EDIT 1 | The first four main dials of whatever engine the track uses (for ANALOG: wave shape, tuning...). The page every sound starts on. | yes | This *is* making the sound |
| EDIT 2 | EDIT . SOUND . EDIT 2 | The next four main dials. | yes | Same |
| FILTER | EDIT . TONE . FILTER | Brightness. Cutoff (bright vs muffled), resonance (whistle), drive (grit). | yes | The most-twiddled control on any synth, easy to hear |
| DRIFT J | EDIT . TONE . DRIFT | Makes the notes drift slightly out of tune, like an old analog synth. | yes | Taste, not essential |
| VOICE | EDIT . TONE . VOICE | How many notes can play at once (polyphony, or one at a time as in a bass line), and **glide** (the pitch slides between notes). | yes | Matters once you play bass / lead lines |
| VOICE 2 | EDIT . TONE . VOICE 2 | How voices are handed out, and **unison detune** (several copies of a note spread slightly apart for a fat sound). | yes | Fine detail of VOICE |
| WORDS J | EDIT . TONE . WORDS | VOICE engine only: choose the language and type the lyric the singer sings. | yes | One engine's own feature; fun, so not hidden for long |
| FOCUS J | EDIT . FOCUS . FOCUS | Shows one of the engine's dials large, with an LFO or envelope aimed at it. | **HIDE** | Does what EDIT 1/2 plus MOD already do; ditch candidate |
| OPERATOR | EDIT . OPERATOR . OPERATOR | FM6 only. FM sound is built from six "operators" (little tone makers that modulate each other). Pick one and set its pitch ratio and strength. | yes | Needed to design FM by hand; presets cover beginners |
| OP ENV | EDIT . OPERATOR . OP ENV | FM6 only: that operator's own envelope (how it fades in and out). | **HIDE** | Deep FM design |
| OPERATOR 2 | EDIT . OPERATOR . OPERATOR 2 | FM6 only: mode, tiny detune, how much it responds to how hard you press. | **HIDE** | Deep FM design |
| KIT, KIT 2 | EDIT . KIT . KIT | DRUM only: the kit's main dials (tuning, snap, tone of the whole kit). | yes | The drum sound |
| KIT 3 | EDIT . KIT . KIT 3 | DRUM only: kit-wide tone filter and feedback. | yes | Extra shaping |
| SOUND 1-4 | EDIT . SOUND . SOUND | DRUM only: a drum kit has 8 sounds (kick, snare, hats...). Edit one at a time here. | yes | Per-drum editing beyond the kit |
| LEVELS, LEVELS 2 | EDIT . LEVEL . LEVELS | DRUM only: the volume of each of the 8 drum sounds against each other. | yes | Mixing inside the kit |
| X-MOD J | EDIT . TONE . X-MOD | DRUM only: a wobble and randomness across the whole kit. | **HIDE** | Experimental |
| FILTER (drum) | EDIT . TONE . FILTER | DRUM only: filters the whole kit. | yes | Like FILTER |
| STEREO | EDIT . TONE . STEREO | DRUM only: where each drum sits left to right. | yes | Mixing detail |
| VOICE (drum) | EDIT . TONE . VOICE | DRUM only: how drum voices overlap. | **HIDE** | Rarely needed |
| OP1-4 ENV, OP LEVEL | none reaches them | Old DIGITAL FM pages. | dead | Delete |
| RETRIG, ROTOR | none reaches them | Old LOFI / WHEEL pages. | dead | Delete |

## ENV / LFO (sound that moves)
| Page | Breadcrumb | What it does, in plain words | In COMMON | Why |
|---|---|---|---|---|
| ENV | ENV . ENV . ENV | Draw the note's loudness shape: Attack, Decay, Sustain, Release. | yes | Easy to hear: soft pad vs plucky |
| ENV (all tracks) | ENV . TRACKS . ENV | The same for all six tracks on one screen. | yes | Convenience |
| ENV DEST | ENV . ENV . ENV DEST | Also make the envelope open the filter, bend the pitch, or change the tone, not only the loudness. Plus **LOOP** (envelope repeats). | yes | Needs the idea of "routing" |
| STAGE J | ENV . STAGE . STAGE | One envelope stage large (A, D, S or R). | **HIDE** | Repeats ENV; ditch candidate |
| MOD | LFO . MOD . MOD | A patch bay: pick a source (LFO, envelope...), a target (pitch, filter...) and an amount, in 4 slots. | **HIDE** | Open-ended routing |
| LFO | LFO . LFO . LFO | The wobble: speed, shape, start offset, fade-in. | yes | Familiar vibrato / wah |
| LFO DEST | LFO . LFO . LFO DEST | What the wobble moves: pitch, filter, tone, volume. | yes | Goes with LFO |
| MSEQ J, MSEQ 2 J | LFO . MSEQ . MSEQ | A step-by-step wobble you draw yourself, with its own speed. | **HIDE** | Yehnah extra, advanced |

## FX (effects)
| Page | Breadcrumb | What it does, in plain words | In COMMON | Why |
|---|---|---|---|---|
| FX (named SENDS) | FX . TRACK . SENDS | Four dials for this track: how much goes into **distortion**, **chorus**, **delay** and **reverb**. 0 = none. | yes | The easiest way to add echo and space |
| DIST J | FX . TRACK . DIST | Distortion: amount, character (type) and tone. | yes | Fine-tuning the dist send |
| AUTO PAN J | FX . TRACK . AUTO PAN | The track swings left-right automatically. | **HIDE** | Special effect |
| PHASER J | FX . TRACK . PHASER | A swooshing "jet" effect. | **HIDE** | Special effect |
| BUSES J | FX . BUSES . BUSES | The key dials of the shared delay, reverb, chorus and slicer on one page. | yes | Overview of the shared effects |
| DLY | FX . DELAY . DLY | Delay: echo time, how many repeats (feedback), tone, loudness. | yes | Effect tuning |
| DLY 2 | FX . DELAY . DLY 2 | Stereo width, pitch-shifted echoes, "spray", wobble. | **HIDE** | Detail |
| DLY 3 | FX . DELAY . DLY 3 | Analog or digital echo, ping-pong (echoes bounce left-right). | **HIDE** | Detail |
| REVERB | FX . REVERB . REVERB | Reverb: room or spring, size, damping, delay before it starts. | yes | Effect tuning |
| REVERB 2 | FX . REVERB . REVERB 2 | Slow modulation, tone, width. | **HIDE** | Detail |
| CHORUS | FX . CHORUS . CHORUS | Chorus: speed, depth, stereo. | yes | Effect tuning |
| SLICER J | FX . SLICER . SLICER | Chops the sound into a rhythm (like a trance gate): mode, pattern, speed, depth. | **HIDE** | Yehnah extra |
| SL BUS, SL BUS 2 J | FX . SLICER . SL BUS | Choose which tracks go into the slicer. | **HIDE** | With SLICER |
| SL PITCH J | FX . SLICER . SL PITCH | A step pattern that also changes pitch. | **HIDE** | With SLICER |
| MASTER J | FX . MASTER . MASTER | The whole mix: clipper (loudness), drum bus, and **ducking** (the music dips when the kick hits, the "pumping" sound). | yes | Loudness is wanted early |
| PUNCH J | FX . PUNCH . PUNCH | Record a short effects burst and trigger it live. | **HIDE** | Performance trick |

## KEY / ARP (notes and chords)
| Page | Breadcrumb | What it does, in plain words | In COMMON | Why |
|---|---|---|---|---|
| SCL | SCL . KEY . SCL | Pick the key (root) and scale; optionally quantize; transpose all tracks. Keeps every note "in key". | yes | Prevents wrong notes |
| CHORD J | SCL . CHORD . CHORD | One key plays a whole chord. | yes | The fork's main idea, but the next step |
| HARP J | SCL . HARP . HARP | A harp-style chord player: choose a style (genre) and a chord set; strum on the plate. | yes | The fork's signature |
| OMNI J | SCL . OMNI . OMNI | An Omnichord-style auto-player. | **HIDE** | Variation on HARP |
| PROG J | SCL . PROG . PROG | Play a ready-made chord progression and write it to the track. | **HIDE** | Needs HARP first |
| ARP | ARP . ARP . ARP | Arpeggiator: pattern, speed, octaves, note length. | yes | A known toy, but a new idea |
| ARP 2 | ARP . ARP . ARP 2 | Swing, random skips, hold, note order. | **HIDE** | Detail |

## SEQ (the pattern)
| Page | Breadcrumb | What it does, in plain words | In COMMON | Why |
|---|---|---|---|---|
| PATTERN | SEQ . PATTERN . PATTERN | How long the loop is, how finely it is divided, swing, note length. | yes | Basics of any loop |
| STEP | SEQ . BAR . STEP | The piano roll: put notes in. | yes | Where notes are entered |
| NOTE | SEQ . BAR . NOTE | Edit the selected note: pitch, move, fine timing. | yes | Editing notes |
| BAR | SEQ . BAR . BAR | Pick a bar of the pattern and set its length. | yes | Longer patterns |
| STRETCH | SEQ . PATTERN . STRETCH | Squash, repeat, nudge or rotate the whole pattern. | yes | Pattern tools |
| TRACK | SEQ . TRACK . TRACK | Transpose the track, chance each note plays, humanize, quantize the recording. | yes | Feel |
| PLAYHEADS J | SEQ . PATTERN . PLAYHEADS | Three extra "playheads" read the pattern at different speeds, for canon-like layering. | **HIDE** | Yehnah extra |
| MUTATE J | SEQ . TRACK . MUTATE | The pattern slowly changes itself. | **HIDE** | Yehnah extra |
| COMP, COMP 2 J | SEQ . STEP . COMP | Give one step special behaviour: e.g. only every 3rd loop. | **HIDE** | Step-level programming |
| AUTOMATION | SEQ . AUTO . AUTOMATION | Parameter locks: a knob moves on chosen steps. | **HIDE** | Advanced |
| AUTO LIST | SEQ . AUTO . AUTO LIST | The same locks as a list. | **HIDE** | Advanced |
| SONG, PHRASES, CHANCE | not browsed | Song view, pattern loader, a step's chance. | follows parent | Reached from code |

## SAVE / GLO (keep it, balance it)
| Page | Breadcrumb | What it does, in plain words | In COMMON | Why |
|---|---|---|---|---|
| PRESETS | SAVE . SOUND . PRESETS | Browse ready-made sounds and load one. | yes | How a beginner finds sounds |
| USER | SAVE . SOUND . USER PRESETS | Save a sound you made, or load one. | yes | Keep your work |
| MORPH J | SAVE . SOUND . MORPH | Blend two tracks into one sound. | **HIDE** | Fork experiment |
| PROJECT (SECTIONS) | SAVE . PROJECT . SECTIONS | Switch between sections A-D. | yes | Song structure |
| ARRANGE | SAVE . PROJECT . ARRANGE | Order the sections into a song. | yes | Songs |
| PROJECT (PROJECTS) | SAVE . LIBRARY . PROJECTS | The 8 project slots: save and load. | yes | Keep your work |
| TOOLS | SAVE . LIBRARY . TOOLS | Clear a sequence, reset a sound. | yes | Start over safely |
| MIXER | GLO . MIX . MIXER | Volume, left-right position and reverb amount for each track, plus a DJ-style filter. | yes | Balance |
| SENDS | GLO . MIX . SENDS | Every track's four effect sends side by side. | yes | Overview of FX > SENDS |
| GLOBAL | GLO . GLOBAL . GLOBAL | Tempo (BPM), swing, sync clock, master tuning. | yes | Tempo is basic |
| SYSTEM | GLO . SYSTEM . SYSTEM | How many tracks play, ECO / LITE / FULL (how hard the device works), routing, info. | yes | Set once and forget |

## Not in the page list
| Screen | What | In COMMON |
|---|---|---|
| HOME: TRACKS, MACRO | Track overview; macro dials that move several things at once | yes |
| HOME: ENGINE, ROUTES | Change a track's engine; routing | yes |
| HOME: IDEAS | Random "dice" that suggest a track, sound, wobble, effect or texture | yes |
| Quick layers (hold a button) | Shortcut maps | show only the level's pages |
| MENU rows | LARGE, RESTORE, INIT | yes; CLICK (metronome), COUNT-IN, USB, LOAD, CALIBR still shown |

## Notes on the cut
- Hidden in COMMON: about 30 pages (every **HIDE** above); about 50 stay.
- Where a page is hidden, its tab goes too if the group has no other page (e.g. SCL . OMNI, SCL . PROG, FX . SLICER,
  FX . PUNCH, LFO . MSEQ, ENV . STAGE, EDIT . FOCUS, SEQ . AUTO), so the section has fewer tabs.
- A group's first page stays so the tab makes sense (PATTERN, STEP).
- Hide-or-keep calls that are close: DRIFT, VOICE 2, WORDS, KIT 3, BUSES, STRETCH (shown now); ARP 2 (hidden now); tell me.
- Duplicates to decide on (ditch rather than hide): FOCUS vs EDIT 1/2, STAGE vs ENV, SENDS (GLO) vs FX, BUSES vs the
  effects' own pages.
- Delete now (dead): OP1-4 ENV, OP LEVEL, RETRIG, ROTOR and their tests.
