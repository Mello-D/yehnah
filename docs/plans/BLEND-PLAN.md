# BLEND: two tracks, two full sounds, one knob between (plan, 2026-10-29)

Replaces today's SAVE > MORPH (one track's values blended toward SOUND B: lists jump at 50%). The page looks as now.

## What you do
- Set each sound on its own track as usual (any engine, any preset, user sounds too).
- SAVE > MORPH, four knobs:
  1. **TRACK A**: OFF, T1..T6
  2. **TRACK B**: OFF, T1..T6 (A and B the same or either OFF: the page says "pick two tracks", nothing changes)
  3. **BLEND**: 0..100% (A only .. B only)
  4. a fourth knob: open, see below
- Play either track (its keys, its sequence, MIDI on its channel): both sound, mixed by BLEND. B plays A's notes;
  while blended, B's own steps stay silent (kept, back when the blend is off).

## How it sounds
- Both engines run on every note, each as its preset is: nothing in between, no jumps.
- Equal-power crossfade (50% = each at ~71%, no dip in loudness), on top of each track's LEVEL, slewed so a fast
  turn does not click. Each track keeps its own FX sends, pan and so on.
- CPU: the same as the two tracks playing one part together. Heavy pairs (e.g. PLAITS + JUNO, polyphonic) are the risk:
  check on the device; the voice budget already shared between tracks still applies.

## The page (as it looks now)
- Left side A: its track and preset name ("T1 SAW LEAD"); right side B the same; the divider where BLEND puts it,
  each side as wide as its share, its percentage in it. Knob colours as now.

## HOME
- When the selected track is in a blend, its robot and the other one stand together on the road, the same size as now,
  each dancing to the notes; the bigger share in front (or brighter). Look at it in the gallery before deciding.

## The fourth knob (pick one, or leave it empty)
- **MOVE**: what moves BLEND by itself: OFF, LFO (a slow sweep, the rate on this knob), VELOCITY (hard notes lean
  to B), MOD WHEEL. Recommended: makes the blend play itself.
- **B TUNE**: B a few cents or an octave apart, for thicker layers.
- **B DELAY**: B's notes a little late (0..50 ms), a slap / strum between the two sounds.

## Saving
- A, B, BLEND (and the fourth) are song settings (globals), saved with the song, not with a track's sound.
- Older songs with a SOUND B on a track: that part is dropped (it loads as A alone).

## The work, in order
1. The globals and the page (the current MORPH page's drawing kept, its knobs re-pointed).
2. Note following: wherever a note goes to track A (keys, chords, arp, sequence, MIDI, note off and panic), it goes
   to B too; B's own steps muted while blended.
3. The crossfade in the mix (per block, slewed).
4. HOME's two robots.
5. Remove the old SOUND B machinery (voice.c sb_*, ui.c sb_poll, P_XB / P_XAMT): ~1.6 KB RAM back, some flash.
6. The web editor and its tests, the host tests; one full suite; budgets (flash ~6 KB free).

## Not doing
- 8 tracks: ~6 KB more RAM with ~3.7 KB free; not without a big cut first.
