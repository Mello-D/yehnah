# Engines: the Clipper-only ones, and ambient (2026-11-02, investigation only)

Clipper 1.3 (fbd8434) engines compiled with our clang (-Os, FPU flags as ours) in one unit, nm per symbol, each symbol
given to the file naming it most (the method of CLIPPER-STEP1.md; its shared-engine numbers came out the same to a few
bytes). Generated headers stubbed; their real sizes added by hand. CPU: Clipper's own host counts
(build/meas/clip_cpu.txt, per mille of its idle mix; its ANALOG averages 3,418 for comparison).
Budget now: ~56.2 KB flash free, RAM tight (the pool at its floor).

## The Clipper-only engines

| engine | flash | of it tables | RAM a track | CPU avg / max | presets | ours already like it |
|---|---|---|---|---|---|---|
| TERRAIN (wave terrain, slow drift) | 18.0 KB | 16 KB | 0 | 3,322 / 3,665 | 10 | – |
| VECTOR (Prophet VS / Wavestation) | 24.2 KB | 21 KB | 0 | 3,157 / 4,559 | 18 | WSEQ (in part) |
| HYBRID (3 osc, 2 filters, sync / ring / supersaw) | 16.1 KB | 6.6 KB | 1.8 KB + 1.7 KB | 3,760 / 5,878 | 32 | ANALOG |
| GRAIN + SAMPLE (+ their 25 KB synthesized sources) | 5.2 + 25 KB | 25 KB | GRAIN 7.0 KB | 2,231 / 2,489 | 10 + 4 | – (our GRAIN delay is an effect) |
| FMX (opsix-style, 6 ops, waves) | 33.7 KB | 28.7 KB | 0.9 KB | 5,581 / 7,527 | 16 | FM6 |
| PHYS (physical models) | 10.6 KB | – | 8.6 KB | 2,869 / 3,656 | 32 | MODAL, GUITAR, KALIMBA |
| TRIO | 3.8 KB | – | 0 | 3,119 / 3,478 | 20 | ANALOG (folded in) |
| MACRO (after Plaits) | 3.8 KB | – | 0 | 4,501 / 6,612 | 24 | PLAITS |
| WAVE (wavetables) | ~3.2 KB + **128 KB spectra** | 128 KB | **64 KB** mipmaps | 4,233 / 5,190 | 28 | – : does not fit |

Not counted: each engine's UI / editor pages, its params.c entries, preset level trims (~a few hundred B each).

## Ambient: what we have

- **Engines**: DRIFT (ours: six wandering sine partials, BLOOM, BREATH; 14 presets: DRIFT PAD, GLASS DRONE, OCTAVE
  CLOUD, BELL HAZE, DEEP DRONE, SHIMMER SWELL, SLOW BLOOM ...), STRINGS, JUNO pads, VPS, WSEQ, PLAITS, MODAL / KALIMBA
  bells, NOISE (wind, rain), FM6. About 45 presets already named PAD / DRONE / GLASS / CHOIR / SWELL across engines.
- **FX**: reverb ROOM SPRING SHIMMER RESO CLOUD (SIZE to a 0.99 feedback, AIR pitch / tone on SHIMMER and CLOUD), the
  delay's GRAIN mode (PITCH: each repeat further away; SPRAY), ANALOG / DIGITAL delay, PING, chorus with STEREO,
  VINTAGE, the mod wheel's target. ADSR to 10 s. GEN has an AMBIENT style.
- **The limit**: the reverb and delay are one shared bus each (GLO > FX). A preset sets only its four sends, so an
  ambient preset can't pick SHIMMER or CLOUD itself.

## The gap

Other firmwares' ambient engines are mostly granular, evolving wavetable and terrain or vector motion. Of those:
TERRAIN and VECTOR fit (no RAM, 18 / 24 KB, CPU like ANALOG). GRAIN needs 30 KB and 7 KB of RAM a track, too much RAM
for six tracks as it is. WAVE does not fit.

**Done 2026-11-02:** TERRAIN (27) and VECTOR (28) ported (image 567,868 B: ~13.4 KB free). HYBRID no longer fits.
