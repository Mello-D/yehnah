#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mello-D
"""(chord-harp fork, 2026-10-28) The firmware's robots (ui_robot.c, as tests/pixart_gallery.c dumps them: OUTDIR/ref) against
their design page's (export_parts.mjs: tools/robots/parts/ref), pixel by pixel, each colour as RGB565 makes it.
    python3 tools/robots/check_port.py [GALLERYDIR] [PARTSDIR] [SHEET.png]
Prints each robot / state that differs (how many pixels, the first few); SHEET: page | firmware | differences, 4 x.
Exit 1 when any differ by more than a few pixels (the page's doubles against the firmware's floats: a pixel on an edge)."""
import os, sys
from PIL import Image

gdir = sys.argv[1] if len(sys.argv) > 1 else 'build/pg/out'
pdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'parts')
sheet = sys.argv[3] if len(sys.argv) > 3 else None
q = lambda c: ((c[0] >> 3) * 255 // 31, (c[1] >> 2) * 255 // 63, (c[2] >> 3) * 255 // 31)
bad, worst, rows = 0, 0, []
nr = len([f for f in os.listdir(os.path.join(pdir, 'ref')) if f.endswith('_0.png')])
for r in range(nr):
    for k in range(4):
        a = Image.open(os.path.join(pdir, 'ref', f'{r}_{k}.png')).convert('RGBA')
        b = Image.open(os.path.join(gdir, 'ref', f'{r}_{k}.ppm')).convert('RGB')
        diff = []
        for y in range(34):
            for x in range(30):
                pa = a.getpixel((x, y))
                ca = q(pa) if pa[3] else (0, 0, 0)
                if ca != b.getpixel((x, y)):
                    diff.append((x, y))
        if diff:
            print(f'robot {r} state {k}: {len(diff)} px differ, e.g. {diff[:6]}')
            bad += 1
            worst = max(worst, len(diff))
        rows.append((a, b, diff))
print(f'check_port: {nr * 4 - bad} of {nr * 4} identical, worst {worst} px')
if sheet:
    S = Image.new('RGB', (4 * 4 * (30 * 3 + 6), 20 * 4 * 36), (40, 40, 48))
    for i, (a, b, diff) in enumerate(rows):
        r, k = divmod(i, 4)
        x0, y0 = k * 4 * (30 * 3 + 6), r * 4 * 36
        S.paste(Image.alpha_composite(Image.new('RGBA', a.size, (0, 0, 0, 255)), a).convert('RGB').resize((120, 136), Image.NEAREST), (x0, y0))
        S.paste(b.resize((120, 136), Image.NEAREST), (x0 + 120, y0))
        d = Image.new('RGB', (30, 34), (0, 0, 0))
        for p in diff:
            d.putpixel(p, (255, 0, 0))
        S.paste(d.resize((120, 136), Image.NEAREST), (x0 + 240, y0))
    S.save(sheet)
sys.exit(1 if worst > 3 else 0)
