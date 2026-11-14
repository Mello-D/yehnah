# Yehnah

Firmware for the M-VAVE FM-1, by Mello-D. Free software (GPL-3.0-only), built on
[JIANT FM](https://github.com/juanjiant-bit/JIANT) by Juan Jiant, which grew from
[Felucca](https://github.com/hugelton/Felucca) by Hügelton Instruments.

## What it is

Yehnah turns the FM-1 into a six-track synth, drum machine and song sequencer, all synthesised (no samples), with a
web installer, a patch editor and a browser emulator.

### Highlights

- **Chord modes** (SCL > CHORD): one key plays a whole chord. TRIAD, 7TH, ADD9, SUS2 and SUS4 are always in key (in the
  seven-note modes the chord is the mode's own; in pentatonic, blues and other scales every tone is the nearest the
  scale offers); MAJ, MIN, 7, MAJ7, MIN7 and POWER work on any key. VOICING: CLOSE, LEAD (voice leading), OPEN, +BASS.
  Chords record into steps, arpeggiate, and light up the keys so you can learn them.
- **Handheld-style chord playing** (after the Omnichord and the Orchid): STRUM spreads the notes low to high like a
  harp, and the HARP and OMNI tabs of the scale page draw the chord keys as a strip of keyboard to play with
  one finger; PROG holds chord progressions.
- **Sound morphing**: MORPH slides ANALOG, PHASE and LOFI continuously between waves (with DRIFT, and LFOs or
  envelopes that sweep the morph); DRUM-X morphs between drum voices; ANIMAL and VOICE morph creatures and vowels;
  macros M1-M4 and dice (EDIT + SELECT) move many parameters at once.
- **25 engines**: ACID, ANALOG, ANIMAL, DIGITAL, DRIFT, DRUM-X, FM6, FMINI, GUITAR, HYBRID, JUNO, KALIMBA, LOFI, MODAL, NOISE,
  OMNI, PHASE, PLAITS, STRINGS, TERRAIN, VECTOR, VOICE, VPS, WHEEL and WSEQ, many with their own on-screen drawing.
- **Songs**: step sequencer with parameter locks and an arpeggiator, four sections (SAVE > ARRANGE) with scenes and
  SONG REC, and SAVE > ERASE to clear a section or the whole project.
- **Effects**: per-track and bus FX, delay, five reverbs, chorus, slicer, master and DJ filter.

## Building and testing

See [BUILDING.md](BUILDING.md). In short:

```
./build.sh                                              # firmware: build/yehnah.fwsc
python3 web/make_site.py build/yehnah.fwsc dev build/site   # the installer site (served for the web installer)
web/emu/build.sh                                        # browser emulator: build/emu
tests/run_tests.sh                                      # host, web editor and emulator tests
```

## Installing (at your own risk)

1. **Back up first**: the installer saves a complete backup before it writes.
2. **Coming from another custom firmware? Return to official V15 first**, then install Yehnah: writing it straight
   over another custom firmware has bricked an FM-1. The installer checks the FM-1 first and writes only over
   official V15 or Yehnah. Offline: `python3 tools/fm1_install.py build/yehnah.fwsc`.
3. If the FM-1 crashes, freezes or clicks, hold **REC** while switching it on (one core) and note what
   MENU > ABOUT says.
4. To go back to the official firmware, use M-VAVE's updater or the installer's **Return to official V15**.

If the FM-1 stays black after an interrupted update, check whether the computer sees it as **WL80UBOOT** (or a USB
device 4C4A:8057): that's the chip's boot mode, and running the installer again with another cable recovers it. If
that's not enough, [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) reads and writes the flash
with a Seeed XIAO RP2040.

## Documents

| Document | What's in it |
| --- | --- |
| [docs/MANUAL.md](docs/MANUAL.md) | The manual |
| [BUILDING.md](BUILDING.md) | Toolchain, build, emulator and tests |
| [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md) | The editor's SysEx protocol |
| [LICENSING.md](LICENSING.md) | Licenses of each part |

## Credits

- **[JIANT FM](https://github.com/juanjiant-bit/JIANT)** by Juan Jiant (Argentina): the firmware Yehnah is built on.
- **[Felucca](https://github.com/hugelton/Felucca)** by [Hügelton Instruments](https://hugelton.com)
  (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)): JIANT's base; PHASE's waveforms
  (a port of [CrispyZebra](https://github.com/hugelton/CrispyZebra), GPL-3.0); the DRUM voices and kits; the
  [Fukiai](https://github.com/hugelton/Fukiai) icon font ([MIT](LICENSES/MIT-Fukiai.txt)). And Felucca's
  contributors: keremimo, ChanceTheMaker, andreahaku, spinkham, zednaked, jasonpersinger.
- **[Hortator](https://github.com/deadactive/hortator)** by DEADACTIVE (GPL-3.0, itself built on Felucca): the drum models of the
  HORT kits (`firmware/src/hm_*.c`, `drum_hort.c`). Its ports of Plaits, stmlib and DaisySP keep their MIT license.
- **[SLOOP](https://github.com/isod89/sloop-fm1)** by isod89 (GPL-3.0): the sections, quick chain and SONG REC system
  the song mode is based on.
- FMINI is the engine of [FMini-1](https://github.com/ruppert/fmini-1) by Guilherme Ruppert (GPL-3.0), ported: its Minimoog ladder
  filter, oscillators and mixer, and its 200 factory patches (`firmware/src/eng_fmini.c`, `eng_fmini_data.h`, `eng_fmini_filter.h`;
  `tools/gen_fmini_data.py` makes the data files).
- SCL > GUITAR's chords: the 50 chord styles of [guitchordi](https://github.com/stoogs/guitchordi) by stoogs (GPL-3.0)
  (`tools/gtr/`, `firmware/src/harp_gtr.h`).
- Fonts: [Chakra Petch](https://github.com/m4rc1e/Chakra-Petch) ([SIL OFL 1.1](LICENSES/OFL-ChakraPetch.txt)),
  [IBM Plex Mono](https://github.com/IBM/plex) ([SIL OFL 1.1](LICENSES/OFL-IBMPlexMono.txt)),
  [Michroma](https://github.com/googlefonts/Michroma-font) ([SIL OFL 1.1](LICENSES/OFL-Michroma.txt)),
  [Inter Tight](https://github.com/rsms/inter-tight) ([SIL OFL 1.1](LICENSES/OFL-InterTight.txt)); in the emulator,
  [DotGothic16](https://github.com/fontworks-fonts/DotGothic16) ([SIL OFL 1.1](LICENSES/OFL-DotGothic16.txt)).
- VOICE: after [klattsch](https://github.com/tgies/klattsch) by Tony Gies (MIT); formant data from Klatt (1980) and
  Hillenbrand et al. (1995).
- FM6: msfa from [Dexed](https://github.com/asb2m10/dexed), Google Inc. and Pascal Gauthier
  ([Apache-2.0](LICENSES/Apache-2.0-msfa.txt)).
- Both cores: the way to start the AC79's second core and the guard against its misreads after
  [Melodee](https://github.com/keremimo/melodee) by keremimo and [fm1-x0x](https://github.com/charlesvestal/fm1-x0x)
  by Charles Vestal (both GPL-3.0).
- Emulator: after [X0X](https://github.com/charlesvestal/fm1-x0x) by charlesvestal (GPL-3.0).
- Testing and CPU work on a real FM-1: **praztalfractal** ([GitHub: gospodindilly-ops](https://github.com/gospodindilly-ops)).
- Package format: [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK)
  ([Apache-2.0](LICENSES/Apache-2.0.txt); three of its files go into every package, none in this tree).

## AI-assisted development

Yehnah is developed with the help of AI agents for the code, the tests and the documentation. The instrument's
design and the decisions are made by its author.

## License

Free software: [GPL-3.0-only](LICENSE), like JIANT FM and Felucca. The fonts and the ported DSP keep their own
licenses ([LICENSES/](LICENSES/)); details in [LICENSING.md](LICENSING.md).

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; Yehnah is not affiliated with them, nor with
JIANT FM's author. M-VAVE and FM-1 are trademarks of their owners; Yehnah is not affiliated with or endorsed by them.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments — Felucca.
JIANT FM's and Yehnah's modifications are distributed under the same license.
