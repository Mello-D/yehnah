# Clipper hybrid, step 1: measure and list (2026-10-30)

Clipper = isod89/clipper-fm1 at fbd8434 (1.3, 2026-10-29), cloned into the session scratchpad (wiped daily) and
built with our toolchain (its ./build.sh, all 23 engines: CLIPPER_SYNTH 1). Its image: 562,020 B (303 KB of it
samples); RAM .data+.bss 74,328 B; pool 313,528 of 344,064 B. Measurements in build/meas/ (gitignored).

## 1. The 12 shared engines (+ DIGITAL): ours vs Clipper's

Flash / RAM: `nm` on each build's yehnah.elf, each symbol given to the source file that names it most (inlined code
counts with its caller: about ±1-2 KB each). CPU: both repos' tests/regress.c on this PC, counting instructions with
the CPU's counter (scratch copies with a Linux counter; Clipper's also with its job table enlarged, below): one part,
8 notes held, the engine's heaviest preset, instructions per sample **minus the idle mix** (ours 726, Clipper's 653).
The heaviest presets differ between the two (different preset sets), so CPU compares engines, not sounds.

| engine (Clipper's name) | flash ours | flash Clipper | RAM ours | RAM Clipper | CPU ours | CPU Clipper | presets ours / Clipper |
|---|---|---|---|---|---|---|---|
| ACID | 2,814 | 1,600 | 0 | 0 | 341 | 416 | 23 / 16 |
| ANALOG | 9,442 | 3,352 | 0 | 0 | 1,645 | 2,456 | 59 / 32 |
| FM6 (+ core, bank) | 18,318 | 13,671 | 14,072 | 1,692 + pool | 1,030 | 1,432 | 44 / 8 |
| WHEEL (DRAWBAR) | 2,886 | 2,500 | 5,112 | 2,496 | 1,715 | 1,871 | 17 / 14 |
| FORMANT (VOICE) | 3,065 + sing.c | 2,921 | 4 | 4 | 974 | 971 | 23 / 12 |
| JUNO | 6,017 | 2,100 | 0 | 0 | 1,323 (4 voices) | 2,479 (8 voices) | 128 / 17 |
| LOFI | 9,948 | 2,238 | 0 | 0 | 1,856 | 756 | 53 / 12 |
| NOISE | 2,670 | 2,838 | 0 | 0 | 1,367 | 1,629 | 9 / 14 |
| PHASE | 2,229 | 1,780 | 0 | 0 | 1,480 | 1,357 | 24 / 16 |
| STRINGS | 2,603 | 2,226 | 48 | 24 | 3,340 | 3,167 | 24 / 16 |
| VPS | 1,872 | 1,644 | 0 | 0 | 1,843 | 1,914 | 11 / 10 |
| WSEQ | 6,996 | 3,258 | 0 | 0 | 1,685 | 1,694 | 17 / 16 |
| DIGITAL (ours retired) | 1,036 | 2,882 | 0 | 0 | – | 1,209 | – / 30 |
| level trims (preset_trim_clipper.h) | – | 736 | – | – | | | |
| **total** | **~69.9 KB** | **~43.7 KB** | **~18.8 KB** | **~4.2 KB + FM6 in the pool** | | | **432 / 213** |

Notes:
- Clipper keeps FM6's (and HYBRID's, PHYS's, FMX's) voice state in a per-part arena in the pool (eng_arena: 25,776 B
  for its 3 parts, sized by its biggest engines). For FM6 alone that's about what ours keeps in .bss (fm6_note
  6,528 B), so count FM6's RAM as roughly even, not as 12 KB freed. Ours also holds the B1..B32 bank (3.8 KB RAM),
  which Clipper doesn't have.
- Clipper's ANALOG and JUNO cost more CPU: its ANALOG presets use the track filter models (PROPH LEAD), and its JUNO
  plays 8 voices where ours is capped at 4.
- Ours costs more on LOFI (the bytebeat / floatbeat FLOAT PAD is the heaviest) and on ANALOG flash (SYNC / RING /
  SAW3 / MORPH, analog_ext and analog_morph alone ~5 KB).

**The hybrid's total:** swapping the 12 (+ DIGITAL) frees about **26 KB flash**. Part of that is our extra presets.
Then about 3-5 KB comes back for what Clipper's presets need from the voice layer (section 3: the track filter models,
hsvf_run alone 1.6 KB; VINTAGE; SPICE; the mod wheel target; velocity to filter; the arp and a 4th extra in
preset_t). Net: **~20 KB more flash free** (we have ~10 KB now), RAM about even to a few KB freed (WHEEL 2.6 KB,
the bank 3.8 KB if it goes). CPU: about even overall. JUNO at 8 voices costs ~1.9x ours, so keep our 4-voice cap
(the presets don't depend on voice count). The six tracks' shared budget (voice.c) stays ours.

## 2. Per engine: what we'd lose, and where our presets came from (needs the user's OK)

Features: from our files' "(Yehnah x.y)" / "(chord-harp fork)" notes, checked against Clipper 1.3's file (its knobs,
its lists of waves and modes). Presets: each one traced by name to the commit that first added it.

Where the 432 presets came from:
- **Felucca (F, 43)**: Felucca's own factory sounds, by its author Leo Kuroshita (human-made, upstream). 14 of them
  have a Clipper preset of the same name (Clipper descends from Felucca too; same name, values not compared).
- **Yehnah (J, 63)**: Yehnah's (the project we forked, before 2026-10-21), mostly its 0.7.0 "46 factory presets"
  pack. Its commits are authored by Claude in Yehnah's own sessions: AI-made, though not in this fork.
- **Clipper (C, 73)**: ported from Clipper earlier, with small changes (below). A swap replaces them with Clipper
  1.3's own versions, so they aren't lost; only our changes go.
- **Other sources (S, 115)**: **99 Juno-106 factory patches** (Roland's A11..B88, from FM-106's decoded SysEx,
  mapped onto JUNO's knobs by our script tools/gen_juno106.py: the sounds are Roland's, the mapping is ours), and
  **Melodee's 16 FM6 voices** (Kerem Kilic's, GPL-3.0, the patches copied from Melodee's ROM; only each one's
  preset wrapper is ours).
- **Made in this fork (M, 138)**: by me (Claude), not checked by anyone else: the 109-preset pack (10-25), the
  chip-tune pack, the LCD-4 sounds (LCD-4 has sound models but no presets, so these were made here after its
  models), the world sounds and Mega Drive set (10-27), the INIT presets.

| engine | ours | F | J | C | S | M | Clipper's |
|---|---|---|---|---|---|---|---|
| ACID | 23 | – | – | 15 | – | 8 | 16 |
| ANALOG | 59 | 12 | 13 | – | – | 34 | 32 |
| FM6 | 44 | 8 | 7 | – | 16 Melodee | 13 | 8 |
| WHEEL | 17 | 5 | 5 | – | – | 7 | 14 |
| VOICE | 23 | 4 | 11 | – | – | 8 | 12 |
| JUNO | 128 | – | – | 16 | 99 Juno-106 | 13 | 17 |
| LOFI | 53 | 4 | 13 | – | – | 36 | 12 |
| NOISE | 9 | 4 | 4 | – | – | 1 | 14 |
| PHASE | 24 | 6 | 10 | – | – | 8 | 16 |
| STRINGS | 24 | – | – | 16 | – | 8 | 16 |
| VPS | 11 | – | – | 10 | – | 1 | 10 |
| WSEQ | 17 | – | – | 16 | – | 1 | 16 |
| **total** | **432** | **43** | **63** | **73** | **115** | **138** | **183** (+30 DIGITAL) |

Two of the sourced sets could survive a swap: JUNO's DSP is already Clipper's (same knobs), so the 99 Juno-106
patches would still play the same on Clipper's JUNO; and Melodee's 16 FM6 voices are standard 6-operator patches
that Clipper's FM6 loads as they are. Keeping them breaks only the "Clipper's presets and nothing else" rule;
they weren't made by me.

### ANALOG (ours: Felucca's, much extended in Yehnah)
- **Three more waves, SYNC, RING and SAW3**: hard sync (osc 2 restarted by osc 1, DETUNE sweeps the ratio 1..8.9:
  the classic sync lead), ring modulation (bells, metal), and three detuned saws (the Minimoog stack). Yehnah folded
  its TRIO engine into ANALOG on them; LCD-4's FAT and SCREAM sounds use them.
- **MORPH**: the plain waves blend into each other continuously (SAW to SQR to TRI to SIN and round), moved by a knob
  and by the LFO / ENV on SHAPE. Clipper's ANALOG switches waves in steps only.
- **INT**: osc 2 a set interval (±24 semitones) from osc 1: fifths, octaves. Clipper's has KTR (filter key
  tracking) on that knob instead.
- **The filter's TYPE inside the engine**: HP and BP from its own state-variable filter, and COMB (the part through a
  feedback comb tuned to the newest note). After a swap, ANALOG would use the part's filter type like every other
  engine does, and Clipper's track FILTER models (MOOG, MS20, DIODE, SEM ...) would come with the port.
- Clipper's gains: a band-limited triangle; the filter models on its presets.
- Presets lost: 34 made here (world winds, CS-80 / Synthex brass, LCD-4's), 13 Yehnah, 12 Felucca (SUB BASS kept by
  name).

### LOFI (ours: Felucca's chip voice, much extended in Yehnah)
- **BYTE (bytebeat)**: tiny formulas of a time counter make the sound (the 8-bit "music from one line of code"
  idea), with its own second page: formula, GRIT, MOTN (motion), VAR (mutates the time). **FLOAT (floatbeat)**: the
  same in floating point, as step patterns / a generator. These are the bulk of LOFI's code (lofi_byte, lofi_float:
  ~4.8 KB) and its heaviest presets.
- **Filter and deformations on every wave**: CUT, BEND, MASK, plus the part's filter types (COMB too); MORPH.
- **What we'd gain**: Clipper's LOFI keeps Felucca's original knobs that Yehnah replaced: SWP (a pitch sweep down
  after the note), a built-in ARP (OCT / MAJ / MIN chip arpeggio) and TONE.
- Presets lost: 36 made here (chip-tune pack, LCD-4's ARCADE / ARP), 13 Yehnah (the BYTE / FLOAT ones), 4 Felucca
  (WAVE LEAD kept by name).

### PHASE (ours: Felucca's CZ-style engine)
- **FB**: phase feedback, the oscillator bending its own phase (as a DX operator's feedback): from a harder edge
  through growl to noise, moved by the ENV / LFO. Clipper's 8th knob is empty.
- **MORPH** on the waves.
- Presets lost: 8 made here, 10 Yehnah, 6 Felucca.

### VOICE / FORMANT (ours: Felucca's, with Yehnah's singing)
- **WORDS**: the track sings syllables, words and whole lyrics, in English and Spanish (LANG, LYRIC: typed, sent
  from the web editor or diced), with the 0.9.8 singer's glides, eased sibilants and vibrato. That's sing.c (1,317
  lines) plus the word renderer, and the song generator uses it: GEN gives a VOICE track words and a lyric 70 % of
  the time. Clipper's VOICE sings vowels only (VOWEL, VOWEL2, TALK between them).
- The biggest loss. Swapping VOICE would leave sing.c unused, or need it bolted onto Clipper's engine, which breaks
  the "taken whole" rule. **Recommendation: keep our VOICE.**
- Presets lost: 8 made here, 11 Yehnah (the singers), 4 Felucca (CHOIR AAH, WOW BASS, WHISPER kept by name).

### FM6 (both Felucca's, msfa / Dexed core)
- **Our extra voices**: Melodee's 16 (M1..M16, sourced: could be kept, above), the six Mega Drive-styled ones
  (G1..G6, made here), our INIT voice. Both keep a user bank in flash (ours B1..B32 with a 3.8 KB RAM copy; Clipper's
  B1..B27, read in place, no RAM).
- The engine itself is about the same in both (Clipper's has UNISON's phase spread, ours more presets).
- Presets lost: 13 made here, 7 Yehnah, 8 Felucca (TINE EP kept by name), Melodee's 16 unless kept.

### WHEEL / DRAWBAR (both Felucca's organ)
- **DUST**: an old organ's crackle and hiss (one per part). **ROTOR DEPTH**: how deep the rotary speaker swirls
  (Clipper's is fixed at full). Everything else is the same knobs: registrations, PERC, CLICK, DRIVE, ROTOR.
- Presets lost: 7 made here (SHO, flute and clarinet stops), 5 Yehnah, 5 Felucca (4 kept by name: FULL ORGAN,
  JAZZ PERC, GOSPEL, SOFT FLUTE).

### NOISE (both Felucca's)
- No features lost: the same modes and knobs. Clipper's adds UNISON's spread. We'd swap 9 presets (1 made here) for
  14.

### ACID, JUNO, STRINGS, VPS, WSEQ (ours are already Clipper's DSP)
- Only our changes go, and Clipper's originals come back: JUNO's 4-voice cap (keep the cap: it's CPU, not sound),
  its chorus without Clipper's INSERT types, VINTAGE off; WSEQ's RATE as 8 note lengths (Clipper's is 0..127 in
  bands); the arp on ACID / JUNO presets; ACID SQR LEAD's vibrato on the LFO.
- Presets: ACID 8, JUNO 13, STRINGS 8, VPS 1, WSEQ 1 made here would go; JUNO's 99 Juno-106 patches can stay.

### All engines
- BLEND keeps working (it blends two presets of one engine, whatever they are). The preset TYPE letters and banks
  would need setting for Clipper's presets (data we'd add, not sound).

## 2b. Recommendations (2026-10-30; the user's aim: whichever sounds more authentic for each engine)

"Authentic" here means sounds made and checked by people who know the instrument (Clipper's team, Felucca's author,
Roland's own Juno-106 patches), and DSP modelled on the real circuits, over presets I made and nobody checked.

| engine | recommend | why |
|---|---|---|
| ACID, STRINGS, VPS, WSEQ | **swap** | Already Clipper's DSP: its original presets come back, untouched. Only my presets go. |
| JUNO | **swap, keep the 99 Juno-106 patches** | Clipper's 17 checked presets + Roland's factory patches, the most authentic set there is. Keep our 4-voice cap (CPU only). Caveat: the knob mapping is my script's, not checked against a real Juno. |
| NOISE | **swap** | Nothing lost; 14 checked presets for 9. |
| WHEEL | **swap** | Only DUST and ROTOR DEPTH go; Clipper's 14 include Felucca's organ registrations. |
| FM6 | **swap, keep Melodee's 16 voices** | Same engine; Clipper's checked presets + Melodee's human-made patches. My Mega Drive set goes. |
| PHASE | **swap** | FB goes, but FB is a Yehnah addition, not a CZ feature: Clipper's PHASE is closer to the CZ. |
| ANALOG | **swap** | Clipper's filter models (MOOG ladder, MS20, DIODE, SEM) are the more authentic analog sound, and its presets use them. Sync / ring / supersaw come back later with Clipper's HYBRID (has all three, with its own checked presets) once space allows. |
| LOFI | **swap** | Clipper's keeps Felucca's chip features (pitch sweep, chip arpeggio, TONE), the real-chip behaviour; bytebeat isn't a chip sound. Lost: BYTE / FLOAT and their 13 Yehnah presets. |
| VOICE | **keep ours** | WORDS (sung lyrics) exists nowhere in Clipper and the song generator relies on it. Clipper's VOICE is the same vowel core without it. |

Net: 11 engines (+ DIGITAL) swapped, VOICE kept. The flash freed (~20 KB) makes room for HYBRID next.

## 2c. Keeping our presets to check later; which ones Clipper replaces (the user asked, 2026-10-30)

**Replaced by a Clipper preset of the same name** (build/meas/presets_cmp.json; knobs, envelope, filter env, mono
compared):
- 73 are our earlier ports of Clipper's (ACID 15, JUNO 16, STRINGS 16, VPS 10, WSEQ 16): Clipper's versions are the
  same sound, apart from our documented tweaks (the arp on 2 JUNO presets and ACID ARP, SQR LEAD's vibrato, TAPE
  STR's LFO, and WSEQ's RATE written in our 8-step list).
- 23 are genuinely different presets under the same name: ANALOG 10 (SUB BASS, JUNO PAD, REESE, SUPERSAW, WARM PAD,
  WOBBLE, MINI BASS, FAT LEAD, TAURUS, PAN FLUTE), LOFI 4 (WAVE LEAD, GB BASS, ZAP, CHIP ARP), WHEEL 4 (FULL ORGAN,
  JAZZ PERC, SOFT FLUTE; GOSPEL same values), FM6 3 (BRASS SECT, DRAWBARS; TINE EP same values), NOISE 2 (WIND,
  RAIN: same values). (VOICE's 4 aren't affected: VOICE stays ours.)
- The other 313 have no Clipper counterpart.

**Keeping all of ours alongside Clipper's**: possible as data (a separate bank, say OLD, per engine), but not all
would still sound as they do now, because some knobs change meaning:
- Play the same (same knobs): ACID, JUNO, STRINGS, VPS, WSEQ (RATE translated back), NOISE, FM6; WHEEL without DUST
  and ROTOR DEPTH (5 presets use them, to check); PHASE 19 of 24 (GROWL, FB BASS, PD LEAD, SYNC LEAD, CZ CLAV use FB).
- **ANALOG: 35 of 59 can't**: they use SYNC / RING / SAW3 or INT. Clipper's 8th knob is the filter's key tracking,
  so even the other 24 would need a value picked for it.
- **LOFI: none as they are**: our knobs 5-8 are CUT VIB BEND MASK, Clipper's SWP VIB ARP TONE (our CUT 127 would be
  a full pitch sweep). A translation (CUT to TONE, BEND / MASK dropped) is an approximation; the 8 BYTE / FLOAT
  presets can't play at all.
- Flash: about 42 B a preset (the row + its name); the ~336 non-Clipper, non-VOICE presets come to ~14 KB of the
  ~20 KB the swap frees.

**Done (2026-10-30):** preset-archive/ (432 WAVs), tag presets-before-clipper. **Recommended instead, for checking later**: before step 2 removes anything, render every one of our 432 presets
with today's engines (Clipper's preset_level.c harness adapted to ours: the same phrases, 4.75 s each) into a
listening archive (WAV files named by engine and preset), and tag the commit in git so any of them can be brought
back. Then the ones the user approves get re-added after the swap: on the same engine where the knobs match, or
(ANALOG's sync / ring / supersaw ones) onto HYBRID later. This keeps the flash for HYBRID and doesn't keep
presets that sound different from what the user would be judging.

## 3. What Clipper's presets use beyond the engine (the voice layer the port must bring)

Our preset_t has no arp and room for 3 extra (param, value) pairs. Clipper's has arp[4] and 4 pairs.

| engine | sends | ARP | extras used (count) | extras we lack |
|---|---|---|---|---|
| ACID | 16/16 | 1 | GLMODE 16, TRANS 13, GLIDE 3, MWDST 1 | MWDST |
| ANALOG | 32/32 | 0 | GLIDE 16, TRANS 14, LRATE 10, GLMODE 6, LD_PIT 5, FMODEL 5, VOICE 3, LFADE 3, LD_FLT 3, ED_PIT 3, DETUNE 3, LD_SHP 2, VELF 1, MWDST 1 | FMODEL, VELF, MWDST |
| JUNO | 17/17 | 2 | LRATE 6, LD_SHP 4, GLIDE 4, VINT 3, SPICE 1, SLOOP 1, LD_PIT 1, LD_FLT 1 | VINT, SPICE, SLOOP |
| FORMANT | 12/12 | 0 | GLIDE 3, TRANS 2, LRATE 2, MWDST 1, LFADE 1, LD_PIT 1, LD_AMP 1 | MWDST |
| PHASE | 16/16 | 0 | GLIDE 3, TRANS 2, MWDST 1, LRATE 1, LD_SHP 1 | MWDST |
| DIGITAL | 30/30 | 0 | TRANS 6, LRATE 5, LD_AMP 4, GLIDE 2, VELF 1, MWDST 1, LD_PIT 1, ED_SHP 1 | VELF, MWDST |
| LOFI | 12/12 | 1 | TRANS 2, GLIDE 2 | – |
| NOISE, STRINGS, VPS | all | 0 | a few LFO ones | – |
| FM6, DRAWBAR, WSEQ | all | 0 | none | – |

What we lack: **FMODEL** = the track FILTER's 8 models (SVF MOOG MS20 DIODE SEM NOTCH BP HP, Clipper fx.c, via
fm_eff); **VELF** velocity → filter; **MWDST** the mod wheel's target; **VINT** VINTAGE (each voice slightly its own,
voice.c); **SPICE / SLOOP** the arp's / sequencer's variation. Clipper also trims every preset's level
(preset_trim_clipper.h, generated by its tools/level_presets.py from its whole mix), and its FM6 presets are
generated (tools/gen_fm6_patches.py → yehnah_fm6.h), not in eng_fm6.c.

## 4. Clipper's reference renders: done

tests/preset_level.c (built on the host from Clipper's build/gen) renders **all 407 presets** through the whole mix
(sends, master), 4.75 s each with a phrase that suits it, in 11 s: build/meas/clip_presets.raw (mono int32, 209,440
samples a preset, 341 MB) and clip_presets.txt (engine, preset, role, name). Only WAVE's USER WT is silent (no user
wavetable on the host). That's the reference for step 2: build the same harness on our port, compare per preset.

Clipper's own regress.c on Linux: 436 renders, 152 of its stored hashes differ (they were made on a Mac with its
clang, so exact hashes don't carry across compilers; compare samples on one machine instead). 2 health failures,
both expected on a host (USER WT; the GM kit's samples). Its 5 voice-stealing checks fail here (0 voices taken):
not looked into, they don't touch the presets.

Found in Clipper (worth reporting upstream): tests/regress.c's job table (MAXJ 512) is too small for 1.3's jobs. It
writes past the array: glibc aborts ("buffer overflow detected"); without that check every render comes out silent.

## Corrections to HANDOFF

- Clipper does have drums: drums.c + drum_synth.c (32 synthesised kits). The hybrid keeps ours anyway.
- Clipper runs 3 synth parts (NPART 3), 8 voices each budget-shared, on both cores.

## Step 1 is done. For the user

1. Section 2: confirm, engine by engine, which of our features may go (WORDS, bytebeat, ANALOG's extra waves and
   filter types, PHASE FB, WHEEL DUST / ROTOR DEPTH, our FM6 slots and bank, our extra presets), or keep that
   engine as ours.
2. Then step 2 ports the confirmed engines, Clipper's code and presets verbatim, plus the voice-layer pieces from
   section 3, and checks each preset against build/meas/clip_presets.raw.
