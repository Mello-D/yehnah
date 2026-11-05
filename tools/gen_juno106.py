#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(chord-harp fork) the Juno-106's factory bank (A11 .. B88) as JUNO presets.

  tools/gen_juno106.py FM106_PRESETS_H C_OUT WEB_OUT [TRIM_OUT]

Reads FM-106's firmware/src/fm106_presets.h (PatttF/fm-106, GPL-3.0-only: the 128 factory patches decoded from the
106's SysEx, p[] in its slider / switch order) and writes the JUNO_PRESETS lines (eng_juno.c) and the web editor's
P() lines (web/editor.html). The 106's panel onto JUNO's knobs:
  WAVE   S+P / SAW / PLS from its two buttons; PW: MAN its slider, LFO half the slider plus the LFO on SHAPE
  SUB NOIS HPF RES  as they are; CUT the slider; CHOR OFF / I / II / I+II
  A D S R  as they are, R at most 112 (GATE mode: 0 64 127 8); ENV the env amount halved, negative for INV
  extras (4 at most): the octave (16' -12, 4' +12), then VCF KYBD as KEY>F (P_KTRK: the 106's tracking, 127 ~1:1
  (FM-106's fm106_calc_vcf_freq: 96 of its ~1150 DAC steps an octave a semitone), less JUNO's own ~43 %), then the
  LFO's rate, then its deepest uses (PITCH at about 1/25 of the slider, ~19 cents a step; FILTER half; SHAPE the
  PWM), then its delay as FADE.
  VCA LEVEL as the level trim (TRIM_OUT, 1/2 dB, one a line): the 106's law, -10 .. +10 dB (FM106_VCA_LAW: 4096 at
  ~63); eng_juno.c's JUNO_TRIM starts from it, then each patch is levelled to Clipper's 17 (its comment).
Left out (SKIP): drums, effects (sweeps, noise, thunder ..: NOISE's) and names twice, so that 99 fit beside JUNO's 29
(128: favorites' row, and the editor protocol's 7-bit preset numbers).
"""
import math
import re
import sys

SKIP = {"A64", "A65", "A86", "B85",   # drums, a copy
        "A57", "A58", "A66", "A67", "A73", "A82", "A83", "A84", "A85", "A87", "A88", "B18", "B27", "B28", "B32",
        "B36", "B41", "B42", "B45", "B54", "B57", "B58", "B63", "B86", "B87",   # effects, a name twice: 99 fit
        "A77", "B74", "B82"}   # (2026-11-04, for flash) near copies of A68, B12 (an octave up), B31 (no chorus)
NAMES = {   # the 106's names cut at 15 letters: whole, or shorter (15 at most with the code; no & / ')
    "A15": "MOVING STR", "A16": "BRASS+STR", "A25": "DONALD PLK", "A38": "HIGH STR", "A41": "BASS CLAR",
    "A42": "ENG HORN", "A43": "BRASS ENS", "A48": "SYN BASS I", "A55": "SYN BASS II", "A62": "CLAV",
    "A63": "FRONTIER", "A76": "DARK SYNTH", "A82": "RESONANCE", "B22": "NYLON GTR", "B23": "ORCHESTRAL",
    "B24": "BRIGHT PLK", "B41": "RESO RISE", "B43": "DARK ENS", "B47": "PHASE ENS", "B48": "CHORUS BELL",
    "B54": "AUTO NOISE", "B55": "BRASS ENS 2", "B61": "EP TREMOLO", "B65": "FLUTE+HORN", "B67": "SURFS UP",
    "B74": "HIGH VIOLIN", "B78": "BRASSY ORG", "B81": "LOW DARK", "B82": "PICCOLO TRP", "B84": "HIGH STR 2",
    "B86": "FORBIDDEN",
}
TYPES = [   # the first match of the name: the type letter (ui.c PTYPE)
    ("BASS CLAR", "W"), ("BASS", "B"), ("LEAD", "L"), ("PAD", "P"), ("CHOIR", "V"), ("STRING", "S"), ("STR", "S"),
    ("VIOLIN", "S"), ("CELLO", "S"), ("PIZZ", "S"), ("ENSEMB", "S"), ("ORCH", "S"), ("TOMITA", "S"),
    ("BRASS", "R"), ("TRUMPET", "R"), ("FANFARE", "R"), ("HORN", "R"), ("ORGAN", "O"), ("OWGAN", "O"),
    ("CALLIOPE", "O"), ("ACCORDION", "O"), ("PIANO", "I"), ("RHODES", "I"), ("HARPSI", "K"), ("CLAV", "K"),
    ("FLUTE", "W"), ("PICCOLO", "W"), ("RECORDER", "W"), ("CLARINET", "W"), ("BASSOON", "W"), ("REED", "W"),
    ("GUITAR", "G"), ("GUITA", "G"), ("KOTO", "G"), ("LUTE", "G"), ("PLUCK", "U"), ("PLUC", "U"),
    ("BELL", "E"), ("CHIME", "E"), ("CELESTE", "E"), ("GONG", "E"), ("XYLO", "M"), ("STEEL DRUM", "M"),
    ("VIBE", "M"), ("TAP", "M"), ("TIMPANI", "D"), ("DRUM", "D"), ("SHAKER", "D"), ("SWEEP", "X"),
    ("RISE", "X"), ("ROCKET", "X"), ("STORM", "X"), ("HELICOPTER", "X"), ("THUNDER", "X"), ("BLIZZARD", "X"),
    ("NOISE", "X"), ("SURF", "X"), ("CAVERN", "X"), ("FORBIDDEN", "X"), ("FROGGY", "X"), ("MEOW", "B"),
    ("RESONAN", "X"), ("GOING UP", "X"), ("REPEATER", "X"), ("WAH", "K"), ("FUNK", "K"), ("SUSTAINER", "P"),
    ("ETHEREAL", "P"), ("DARK SYNTH", "P"), ("SYNTH", "P"), ("THUD", "B"),
]
P_TRANS, P_LRATE, P_LFADE, P_LD_PIT, P_LD_FLT, P_LD_SHP = "P_TRANS", "P_LRATE", "P_LFADE", "P_LD_PIT", "P_LD_FLT", "P_LD_SHP"
P_KTRK = "P_KTRK"
JUNO_KTRK = 43    # eng_juno.c's own tracking, % of 1:1: 128 of the cutoff's 256 a semitone, 14 steps an octave (300)


def ptype(name):
    for k, t in TYPES:
        if k in name:
            return t
    return "P"


def convert(code, name, p):
    saw, pulse = p[24], p[23]
    wave = "JW_SP" if saw and pulse else "JW_PLS" if pulse else "JW_SAW"
    pwm_lfo = pulse and not p[33] and p[6]
    pw = p[6] // 2 if pwm_lfo else p[6] if pulse else 0
    sub = p[7]
    chor = (1 if p[27] else 0) + (2 if p[28] else 0)
    env = [0, 64, 127, 8] if p[35] else [p[16], p[17], p[18], min(p[19], 112)]   # (a release past ~12 s capped)
    fenv = min(63, p[12] // 2) * (-1 if p[34] else 1)
    e = [wave, pw, sub, p[8], min(p[9], 3), p[10], p[11], chor]
    x = []
    if p[29] != 1:
        x.append((P_TRANS, -12 if p[29] == 0 else 12))
    kt = round(100 * p[14] * 254 / 256 / 127) - JUNO_KTRK
    if abs(kt) >= 4:
        x.append((P_KTRK, kt))
    uses = []
    pit = round(p[5] / 25)
    if pit:
        uses.append((p[5], P_LD_PIT, min(pit, 63)))
    if p[13] >= 2:
        uses.append((p[13], P_LD_FLT, min(p[13] // 2, 63)))
    if pwm_lfo:
        uses.append((p[6], P_LD_SHP, min(p[6] // 4, 63)))
    uses.sort(key=lambda u: -u[0])
    if uses and abs(p[3] - 60) > 4 and len(x) < 4:
        x.append((P_LRATE, p[3]))
    for _, pid, v in uses:
        if len(x) < 4:
            x.append((pid, v))
    if uses and p[4] and len(x) < 4:
        x.append((P_LFADE, p[4]))
    mono = 1 if ptype(name) == "B" else 0
    rev = 40 if ptype(name) in "PSVE" else 25
    trim = round(2 * (20 * math.log10(1295 / 4096) + 20 * p[15] / 127))   # FM106_VCA_LAW: 1295 * 10^(v/127)
    return dict(name=f"{code} {name}", e=e, env=env, fenv=fenv, mono=mono, rev=rev, x=x, t=ptype(name), trim=trim)


def main():
    src, c_out, web_out = sys.argv[1:4]
    trim_out = sys.argv[4] if len(sys.argv) > 4 else None
    text = open(src).read()
    out = []
    for m in re.finditer(r'\{"([AB]\d\d) ([^"]*)", \{([^}]*)\}\}', text):
        code, nm, vals = m.group(1), m.group(2).strip(), [int(v) for v in m.group(3).split(",")]
        if code in SKIP:
            continue
        nm = NAMES.get(code, nm.upper().rstrip(".").strip())
        assert len(code) + 1 + len(nm) <= 15 and not set(nm) & set("&/'"), nm
        out.append(convert(code, nm.upper(), vals))
    with open(c_out, "w") as f:
        f.write("    /* (chord-harp fork, tools/gen_juno106.py) the Juno-106's factory bank, A11 .. B88, from FM-106 (PatttF/fm-106,\n"
                "     * GPL-3.0-only: decoded from the 106's SysEx); drums, effects, names twice and near copies left out */\n")
        for r in out:
            xs = ", ".join(f"{pid} + 1, {v}" for pid, v in r["x"])
            f.write(f'    {{"{r["name"]}", {{{", ".join(str(v) for v in r["e"])}}}, {{{", ".join(map(str, r["env"]))}}}, '
                    f'{r["fenv"]}, {r["mono"]}, FX(0, 0, 0, {r["rev"]})' + (f", XP({xs})" if xs else "") + "},\n")
        f.write("    /* types: " + "".join(r["t"] for r in out) + " */\n")
    jw = {"JW_SAW": 0, "JW_PLS": 1, "JW_SP": 2}
    with open(web_out, "w") as f:
        for r in out:
            e = [jw.get(v, v) if isinstance(v, str) else v for v in r["e"]]
            f.write(f'        P("{r["name"]}", [{", ".join(map(str, e))}], [{", ".join(map(str, r["env"]))}]'
                    + (", 1)," if r["mono"] else "),") + "\n")
    if trim_out:
        with open(trim_out, "w") as f:
            f.write("".join(f"{r['trim']}\n" for r in out))
    print(len(out), "presets")


if __name__ == "__main__":
    main()
