# Engine settings audit (2026-11-11, first pass)

Every engine has exactly 8 settings (`P_E0..E7`, EDIT 1 and EDIT 2, four knobs a page). This lists, per engine, what is
squeezed, fixed or left out because of that limit. Upstream checkouts used: clipper-fm1, fm1-fimba, fm1-fomni, fmini-1.
**Confidence:** HIGH = read in the source or an upstream list; MED = from the header comment only; LOW = not looked at yet.

## Clear cases (settings dropped or squeezed by the 8-knob limit)
| Engine | Exposed (8) | Squeezed / left out | Confidence |
|---|---|---|---|
| FMINI | CUT EMPH CNTR F.ATK F.DEC F.SUS MOD KBD | DONE: 19 more via the PATCH list | HIGH |
| WHEEL (drawbar) | REG SUB BODY TOP PERC CLICK DRV ROTR | **the 9 drawbars** (16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1') are one of 16 registrations plus 3 offsets; PERC's harmonic / soft / fast are packed in one knob; rotor SLOW/FAST speed | HIGH |
| STRINGS | 16' 8' 4' ENS / CUT RATE SHP RES | fine as is; ensemble depth and speed only, the two LFO rates fixed (~0.6 / ~6 Hz) | MED |
| HYBRID | CUT RES FENV PTCH M1..M4 | **the whole patch**: 3 oscillators (type, shape, level, coarse, fine, flags), 2 filters (type, routing, balance), 2 envelopes, 3 LFOs, a 16-slot matrix; patches are read-only flash, Clipper's web editor for them was left out | HIGH |
| KALIMBA | MAT HARD DECY TONE GLID WORN DAMP CLIK | FiMba's BODY, BUZZ, WAH, WAH RATE (left out on purpose: "body"), TUNING, RELEASE, STRUM, FEEDBACK | HIGH |
| OMNI | MODE HARP1 HARP2 SUS SHIM RATE BRIT DAMP | HARP1 / HARP2 are two knobs for one voice choice; FoMni's chord types (9) fixed by the keys; drums / rhythms deliberately out | MED |
| FM6 | ALG FB MOD RATIO MEG VMOD DETUNE SLOT | the 155-byte patch (6 operators x ratio, level, rates...) is web-editor only; OPERATOR / OP ENV / OPERATOR 2 pages cover part of it on the device | HIGH |
| DIGITAL | ALG R2 R3 R4 IDX MDEC FB "-" | one slot unused: per-operator LEVEL / DETUNE would fit; the operators' envelopes follow one MDEC | HIGH |
| PHASE | WAVE WAVE2 DCW ENV DTN LINE SUB "-" | one slot unused; CZ-1 tones (open item 11) would need per-line DCW / DCA envelopes | HIGH |
| FORMANT | VOWEL VOWEL2 TALK SHIFT BUZZ BREATH Q RAND | F4 fixed 3300 Hz; key tracking fixed 30 / 127 (comments in the source) | HIGH |
| PLAITS | MODEL HARM TIMB MORPH AUX TONE LPG VEL | the particles' diffuser is out; FM / level modulation inputs; decay | MED |

## No obvious loss (the engine is the upstream's, or small by design) — to confirm in the second pass
JUNO (106's four HPF positions, chorus I/II, PW, sub: all there; ENV / LFO come from the track pages), ACID (303: all
there), ANALOG (2 osc, noise, drive, KTR: osc 2's tune / PW / sync are not), LOFI, NOISE, DRIFT (6 partials; the per-partial
levels fixed), TERRAIN, VECTOR (16 waves), VPS, WSEQ, MODAL, ANIMAL (each MODE reads the 8 knobs its own way; the
hidden second set of per-mode settings is the likely gap), DRUM (kit pages exist, covered by the SOUND pages).

## Not an engine setting, but a squeezed one (candidates for the same row list)
Settings spread over a second page that only hold one or two values: ENV DEST / LFO DEST, MSEQ 2, DIST, AUTO PAN,
PHASER, ARP 3, DRIFT, VOICE / VOICE 2, DLY 3, REVERB 2, SL BUS 2, the GUITAR / TUNE / HARP / OMNI SCL tabs.

## Second pass needs
1. Read each DSP for tunable constants (rates, depths, fixed frequencies) that are not settings: the grep for "fixed"
   found only FORMANT; the rest needs a read per engine (LOW: PLAITS, TERRAIN, WSEQ, ANIMAL, DRUM, VPS, VECTOR).
2. For each "clear case", the user picks which dropped settings are worth bringing in.
3. Storage for them: the free packed slots (as FMINI's P_LN0..6), or a per-track extras block (RAM is full: 40 B free).

## Second pass (2026-11-11): the DSPs read for fixed values and for settings that were never knobs
Confirmed gaps, most useful first (each says what the DSP already has or could take):
- **ANALOG**: OSC 2 has only DTN (cents, +-): no OSC 2 range / semitone (a fifth, an octave up), no separate OSC 2 wave
  (both oscillators share WAVE), no sync; PWM width comes only from SHAPE (the matrix / LFO), no PW knob; the filter
  MODEL sits on a separate FILTER page. Slots are not the limit here: the DSP simply reads 2 waves, 1 pulse width.
- **DIGITAL**: OP 1's ratio is fixed 1; the operators' LEVELs and the carriers' mix are fixed per algorithm; one INDEX
  and one MDEC serve all three modulators (a classic 4-op has per-operator levels and envelopes); no per-op fine detune.
  Free: 1 slot ("-") plus the DETUNE the track already has.
- **PHASE**: WAVE2 alternates every other cycle (no balance, no DCW2 / separate DCA envelope); LINE MIX / RING only;
  one slot ("-") free. CZ-1's DCW / DCA / DCO envelopes (open item 11) = 3 more envelopes.
- **WSEQ**: the 16 SEQUENCES are fixed tables (up to 16 steps of {wave, semitones, level}); no step editing; no wave
  per step on the device. START / LOOP / RATE / XFADE are there.
- **VECTOR**: 16 fixed corner waves; X / Y / MOVE there; no MOVE shape or joystick path.
- **HYBRID / FM6**: see the first table; both are patch-driven and the device edits only offsets / macros.
- **FORMANT**: F4 3300 Hz, key tracking 30 / 127, one Q for all four formants (Q is one knob), no separate bandwidth.
- **JUNO**: complete for the 106's DCO / HPF / VCF / chorus; the 106's ENV (VCF depth, polarity, ADSR), LFO (rate, delay),
  VCA mode come from the track ENV / LFO pages, so they are spread over three families, not missing.
- **ACID**: complete for the 303 (the header's list: WAVE CUT RES ENV DEC ACC DIST SWP); the slide TIME is the track's GLIDE.
- **STRINGS**: ensemble LFO rates fixed (~0.6 / ~6 Hz): the RATE knob scales both together; no separate slow / fast depth.
- **LOFI**: the frame clock is fixed 240 Hz (STEP mode), the arpeggio step ~50 Hz (ARP); no knobs for either.
- **NOISE**: the DC blocker ~14 Hz fixed (META); LEN (LFSR / META) and CLK share the knobs by MODE (fine, mode-dependent).
- **TERRAIN**: 8 fixed surface tables; the DRIFT speed ~0.03..1 Hz is one knob; fine.
- **VPS**: D, V, VENV, OSC2 tune, SUB, filter: complete. **KALIMBA, MODAL, GUITAR, DRIFT, OMNI, PLAITS**: as the first table.
- **DRUM**: the kit pages hold the sound-level settings; per-lane tune / decay are on SOUND 1..4, covered.

Not found anywhere (so not a gap): hidden tunables in ACID, JUNO, VPS, TERRAIN, NOISE beyond the ones listed.
Still unread in detail (the header suggests little more): ANIMAL's per-mode settings (9 shared labels: each MODE reads
them its own way), PLAITS' per-model knobs, DRIFT's per-partial levels.
