#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The UI palettes (run by tools/build.py generate()).

  gen_ui_palettes.py OUT.h [--report]

A palette is five colours: BG (background), SURF (surface: dialogs, menu rows, mixer strips),
TEXT, THEME (identity colour: values, curves, gauges, selection) and ACCENT (the one active
thing: hot knob, cursor, playhead, a sounding step). The firmware derives the rest with fixed
blends at palette_set() time (src/gfx.c, the same integer maths as mix() here):
  MID  = mix(BG, TEXT, 70 %)    labels, units, secondary text
  DIM  = mix(BG, TEXT, 42 %)    inactive, empty steps, disabled
  LINE = mix(BG, TEXT, 18 %)    1 px dividers, the faint gauge track
  SEL  = mix(BG, THEME, 80 %)   selection fill and gauge fill (the "mid" of the theme)
  TINT = mix(BG, THEME, 12 %)   a faint area (the selected drum lane)
  RAISE = mix(SURF, TEXT, 12 %) a raised area on a surface: empty step stubs, guides, the selected drum
                                lane, fader slots, button wells, chips
  KEY  = mix(BG, TEXT, 78 %)    a keycap's fill (a mid-light cushion; its label INK, unavailable: a DIM fill)
  INK  = BG: text on a selection or THEME fill
Outside the palette: REC, a fixed red; the QR code's fixed black and white; the crash screen's fixed red.
Saved settings hold the palette's index (no older ids are migrated: the chord-harp fork, 2026-11-02).
--report prints the WCAG contrast of every pairing. The tool fails (exit 1) on any miss.
"""
import argparse
import sys
from pathlib import Path

# name, bg, surf, text, theme, accent (8-bit RGB; stored as RGB565)
PALETTES = [
    # (chord-harp fork) vaporwave, dark and light: neon pink, electric cyan, purple, a sunset's violet night
    ("VAP DK", (20, 6, 40),     (40, 14, 72),    (248, 232, 255), (1, 205, 254),   (255, 113, 206)),
    ("VAP LT", (255, 236, 250), (248, 226, 248), (48, 16, 84), (132, 16, 152),  (0, 88, 160)),
    # (chord-harp fork) jet black, the Batman look: charcoal cards, grey text, gunmetal values, the bat-signal's yellow
    ("NOIRE",  (0, 0, 0),       (12, 12, 16),    (204, 206, 212), (150, 158, 172), (255, 214, 0)),
]
# (0.8.9) MENU > DISPLAY > SKIN (the approved mock-up opciones_skin.html, "0.6 COLORS"): each knob its colour, as the 0.6
# firmware had them, on its card's value and gauge (and the EDIT chips, the knob row of BOTTOM and HUD): KNOB 1..4
KNOBS = {
    "VAP DK": [(255, 113, 206), (1, 205, 254), (5, 255, 161), (255, 251, 150)],
    "VAP LT": [(168, 10, 118), (0, 80, 150), (100, 30, 170), (0, 92, 96)],
    "NOIRE":  [(255, 214, 0), (150, 158, 172), (228, 230, 236), (255, 160, 0)],
}
DEFAULT = "VAP DK"                 # the palette of new settings (src/panel.c)
REC_DARK, REC_LIGHT = (255, 72, 72), (190, 24, 40)
QR_LIGHT, QR_DARK, CRASH_BG, CRASH_INK = (255, 255, 255), (0, 0, 0), (160, 0, 0), (255, 255, 255)
PCT = {"MID": 70, "DIM": 42, "LINE": 18, "SEL": 80, "TINT": 12, "KEY": 78}
RAISE_PCT = 12                     # SURF -> TEXT


def to565(c):
    r, g, b = c
    if r == g == b:                     # a gray stays on the RGB565 gray axis (G = 2 R)
        return ((r >> 3) << 11) | ((r >> 3) << 6) | (r >> 3)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def from565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)


def cdiv(a, b):
    return a // b if a >= 0 else -((-a) // b)


def mix(a, b, pct):
    """src/gfx.c ux_mix: a + (b - a) * pct / 100 per channel, rounded"""
    out = 0
    for sh, mask in ((11, 31), (5, 63), (0, 31)):
        x, y = (a >> sh) & mask, (b >> sh) & mask
        d = (y - x) * pct
        out |= (x + cdiv(d + (50 if d >= 0 else -50), 100)) << sh
    return out


def luma(c):
    """src/gfx.c ux_luma: 0..255, integer"""
    r, g, b = c >> 11, (c >> 5) & 63, c & 31
    return (r * 255 // 31 * 54 + g * 255 // 63 * 183 + b * 255 // 31 * 19) >> 8


def lum(c565):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = from565(c565)
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def contrast(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


def derive(p):
    name, *cols = p
    bg, surf, text, theme, accent = (to565(c) for c in cols)
    d = dict(bg=bg, surf=surf, text=text, theme=theme, accent=accent,
             mid=mix(bg, text, PCT["MID"]), dim=mix(bg, text, PCT["DIM"]),
             line=mix(bg, text, PCT["LINE"]), sel=mix(bg, theme, PCT["SEL"]),
             tint=mix(bg, theme, PCT["TINT"]), raise_=mix(surf, text, RAISE_PCT), key=mix(bg, text, PCT["KEY"]))
    d["ink"] = bg
    d["light"] = luma(bg) > 128
    d["rec"] = to565(REC_LIGHT if d["light"] else REC_DARK)
    for i, k in enumerate(KNOBS[name]):
        d[f"k{i}"] = to565(k)
    return name, d


# (foreground, background, minimum contrast, why)
CHECKS = [("text", "bg", 12.0, "body text"), ("text", "surf", 9.0, "body text on a surface"),
          ("mid", "bg", 5.0, "labels"), ("mid", "surf", 4.0, "labels on a surface"),
          ("dim", "bg", 2.2, "inactive (not read)"), ("line", "bg", 1.25, "1 px dividers"),
          ("theme", "bg", 4.5, "values, curves"), ("theme", "surf", 4.5, "values on a surface"),
          ("accent", "bg", 4.5, "hot value, cursor, playhead"), ("accent", "surf", 4.5, "hot on a surface"),
          ("ink", "sel", 4.5, "text on a selection"), ("ink", "theme", 4.5, "text on a THEME fill"),
          ("sel", "bg", 1.8, "gauge fill vs track"), ("rec", "bg", 3.0, "REC (graphic)"),
          ("text", "raise_", 7.0, "text on a RAISE chip or button"), ("ink", "rec", 4.5, "text on the REC chip"),
          ("ink", "key", 4.5, "a keycap's label"), ("key", "bg", 3.0, "a keycap on the background"),
          ("key", "surf", 2.5, "a keycap on a surface"), ("ink", "dim", 1.8, "an unavailable keycap's label"),
          ("ink", "accent", 4.5, "text on the ARM badge")] + \
         [(f"k{i}", b, 4.5 if b != "raise_" else 4.0, f"KNOB {i + 1}'s value") for i in range(4) for b in ("bg", "surf", "raise_")]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    fails = []
    rows = [derive(p) for p in PALETTES]
    for name, d in rows:
        for f, b, lim, why in CHECKS:
            c = contrast(d[f], d[b])
            if c < lim:
                fails.append(f"{name}: {f}/{b} {c:.2f} < {lim} ({why})")
    if a.report:
        print(f"{'palette':8s} " + " ".join(f"{f}/{b}".ljust(12) for f, b, _, _ in CHECKS))
        for name, d in rows:
            print(f"{name:8s} " + " ".join(f"{contrast(d[f], d[b]):5.1f}".ljust(12) for f, b, _, _ in CHECKS))
        print("minimum  " + " ".join(f"{lim:<12.2f}" for _, _, lim, _ in CHECKS))
    if a.out:
        names = [p[0] for p in PALETTES]
        assert set(KNOBS) == {p[0] for p in PALETTES}
        out = ["/* generated by tools/gen_ui_palettes.py: the UI palettes (5 colours each, the rest derived) */",
               "#pragma once", "#include <stdint.h>", "",
               "/* ui_pal_t {name, bg, surf, text, theme, accent} (src/gfx.c) */",
               "static const ui_pal_t UI_PALETTES[] = {"]
        for p in PALETTES:
            v = [to565(c) for c in p[1:]]
            out.append(f'    {{"{p[0]}", ' + ", ".join(f"0x{x:04x}" for x in v) + "},")
        out += ["};", "/* (0.8.9) KNOB 1..4's colours (MENU > DISPLAY > SKIN, the 0.6 colours) */",
                "static const uint16_t UI_KNOBS[][4] = {"]
        out += [f'    {{' + ", ".join(f"0x{to565(c):04x}" for c in KNOBS[p[0]]) + f'}},   /* {p[0]} */' for p in PALETTES]
        out += ["};", f"#define UI_NPALETTES {len(PALETTES)}u",
                f"#define UI_DEFAULT_INDEX {names.index(DEFAULT)}u          /* {DEFAULT}: new settings' (src/panel.c) */"]
        out += [f"#define UI_{k}_PCT {v}" for k, v in PCT.items()] + [f"#define UI_RAISE_PCT {RAISE_PCT}   /* SURF -> TEXT */"]
        out += [f"#define UI_REC_DARK 0x{to565(REC_DARK):04x}u", f"#define UI_REC_LIGHT 0x{to565(REC_LIGHT):04x}u",
                f"#define UI_QR_LIGHT 0x{to565(QR_LIGHT):04x}u", f"#define UI_QR_DARK 0x{to565(QR_DARK):04x}u",
                f"#define UI_CRASH_BG 0x{to565(CRASH_BG):04x}u", f"#define UI_CRASH_INK 0x{to565(CRASH_INK):04x}u", ""]
        Path(a.out).write_text("\n".join(out))
    for f in fails:
        print("FAIL", f)
    if fails:
        sys.exit(1)


if __name__ == "__main__":
    main()
