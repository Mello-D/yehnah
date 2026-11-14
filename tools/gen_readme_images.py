#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The README's pictures (docs/img/*.png): screens of the real firmware's drawing code, from tests/ui_render.c's PPMs.
   Run the UI renders first (tests/run_tests.sh, or build tests/ui_render.c and run it into build/ui_new), then:
       python3 tools/gen_readme_images.py [build/ui_new/ppm] [docs/img]
   Every screen is 240 x 240 on the FM-1 and is saved at 2x (nearest neighbour: the pixels stay square)."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image

PPM = Path(sys.argv[1] if len(sys.argv) > 1 else "build/ui_new/ppm")
OUT = Path(sys.argv[2] if len(sys.argv) > 2 else "docs/img")
PAL = "VAP DK"
OUT.mkdir(parents=True, exist_ok=True)

def shot(name, pal=PAL):
    return Image.open(PPM / f"{pal}_{name}.ppm").convert("RGB")

def save(im, fn, scale=2):
    im = Image.fromarray(np.repeat(np.repeat(np.asarray(im), scale, axis=0), scale, axis=1))   # (not Image.resize: exact pixel doubling)
    im.save(OUT / fn, optimize=True)

def grid(names, cols, fn, pals=None, gap=6, scale=2):
    ims = [shot(n, pals[i] if pals else PAL) for i, n in enumerate(names)]
    rows = (len(ims) + cols - 1) // cols
    sh = Image.new("RGB", (cols * 240 + (cols + 1) * gap, rows * 240 + (rows + 1) * gap), (12, 8, 20))
    for i, im in enumerate(ims):
        sh.paste(im, (gap + (i % cols) * (240 + gap), gap + (i // cols) * (240 + gap)))
    save(sh, fn, scale)

SINGLE = {   # file: render name
    "splash": "splash", "home": "home", "presets": "presets", "step": "roll_chords", "pattern": "lente_pattern",
    "drum": "drum", "drumx": "drumx", "chord": "key_chord", "harp": "key_harp", "omni": "key_omni",
    "arp": "key_arp", "fm6": "edit_fm6", "fmini": "edit_fmini", "guitar": "edit_guitar", "animal": "vx_bird",
    "voice": "vx_head_vowel", "mixer": "mixer", "fx": "fx", "delay": "fx_effect_delay", "punch": "layer_fx_grid",
    "arrange": "save_arrange", "macros": "home_being", "seqtools": "layer_seq_tools", "rec": "layer_rec",
    "perform": "perform_held",
}
for fn, n in SINGLE.items():
    save(shot(n), fn + ".png")
grid(["home", "presets", "roll_chords", "drum", "key_harp", "mixer"], 3, "hero.png")
grid(["home"] * 6, 3, "themes.png", pals=["VAP DK", "VAP LT", "NOIRE", "PASTEL DK", "GREEN", "PAPER"])
print(len(list(OUT.glob("*.png"))), "images in", OUT)
