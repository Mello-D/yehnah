# SWARM: the default X / Y parameter of each dot, per engine

Current (2026-11-15); the code's table is `SW_TAB` in `firmware/src/swarm.c`. Written X / Y for dots 1..4. Only smooth parameters that
keep what the sound is: no waveform, material, model, ratio, algorithm or chorus type (a stepped setting jumps, and SWARM cannot fade
it). Common parameters (any engine): FILTER (the part's cutoff, FCUT), FRES, DIST, DETUNE, VINT (each voice a little its own), DRIFT
(voices wander), PHASER (amount), REV. A common parameter that starts at its end of the range (FILTER open, PHASER / DIST / REV off) moves
one way only. A cutoff never goes below 15 % of its range (a muted track); a soft limit slows every parameter near its ends.

| Engine | Dot 1 | Dot 2 | Dot 3 | Dot 4 |
|---|---|---|---|---|
| ANALOG | DTN / NOIS | CUT / RES | DRV / MIX | VINT / PHASER |
| DIGITAL | IDX / FB | MDEC / FILTER | DETUNE / VINT | DIST / PHASER |
| PHASE | DCW / DCW2 | ENV / DTN | SUB / FILTER | DETUNE / DIST |
| LOFI | CRSH / TONE | DUTY / SWP | VIB / DETUNE | DIST / REV |
| VOICE | VOWEL / VOWEL2 | TALK / BUZZ | BREATH / Q | RAND / DRIFT |
| WHEEL | SUB / BODY | TOP / CLICK | DRV / VINT | PHASER / FILTER |
| DRUM | MORPH / TONE | DECAY / NOISE | FM ENV / FOLD | DRIVE / REV |
| NOISE | COLR / FREQ | RES / DENS | DRFT / CRSH | TRK / REV |
| FM6 | FB / MOD | MEG / VMOD | DETUNE / FILTER | VINT / DIST |
| ANIMAL | PRESS / CHAOS | SIZE / SWEEP | RATE / DRIFT | REV / FILTER |
| JUNO | PW / SUB | NOIS / DETUNE | CUT / RES | VINT / PHASER |
| OMNI | HARP1 / HARP2 | SHIM / BRIT | SUS / DAMP | RATE / FILTER |
| KALIMBA | HARD / TONE | WORN / GLID | DAMP / CLIK | DECY / REV |
| MINI | CUT / EMPH | CNTR / MOD | F.ATK / F.DEC | F.SUS / KBD |
| GUITAR | PICK / PLUCK | TONE / BODY | MUTE / DAMP | DECY / FILTER |
| ACID | CUT / RES | ENV / DEC | ACC / DIST | SWP / VINT |
| DRIFT | TILT / DRIFT | SPEED / FLICK | DETUNE / BLOOM | BREATH / REV |
| PLAITS | HARM / TIMB | MORPH / AUX | TONE / LPG | FILTER / DETUNE |
| STRINGS | 16' / 8' | 4' / ENS | CUT / RES | RATE / SHP |
| VPS | D / V | VENV / OSC2 | CUT / RES | DTN / SUB |
| MODAL | HARD / TONE | BARK / TREM | DAMP / CLIK | DECY / FILTER |
| WSEQ | RATE / XFADE | START / CUT | RES / DETUNE | DIST / VINT |
| TERRAIN | X / Y | RAD / ORB | DRIFT / CUT | RES / DETUNE |
| VECTOR | X / Y | CUT / DETUNE | VINT / DIST | PHASER / REV |
| HYBRID | CUT / RES | FENV / DETUNE | DIST / VINT | PHASER / REV |

Engines that have no list of their own (SAMPLE, TRIO, GRAIN, PHYS, SLICE) get the common ones.
Left out as stepped: every WAVE / MODEL / MAT / MODE / ALG / ratio / REG / PERC / ROTR / CHIP / TERR / SEQ / LOOP / HPF / CHOR, and the
engines' pitch-like parameters (SHIFT, PITCH, TUNE, KTR). Engines whose sound already blends between waves are in: VECTOR's X / Y (four
corners), TERRAIN's X / Y, WSEQ's XFADE, PHASE's DCW, ANALOG's MIX.
