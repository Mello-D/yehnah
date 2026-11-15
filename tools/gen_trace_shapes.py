#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""TRACE's shapes (firmware/src/swarm.c): every shape is a CLOSED polyline of int8 points (-127..127, y down) the dot is
   walked along at a constant speed (the lengths are worked out at run time). Writes firmware/src/trace_data.h; run it
   again when a shape is changed:
       python3 tools/gen_trace_shapes.py [firmware/src/trace_data.h [preview.png]]
   Constellations are stick figures (approximate positions, drawn from memory), each one trail through its stars: after the
   last star the dot goes straight back to the first (nothing is walked twice). The Mobius strip is a lemniscate the dot
   walks twice: the firmware offsets the second lap to the other side of the band (flag 1)."""
import math
import sys

MOB, SKY = 1, 2


def poly(n, rot=-90):
    return [(math.cos(math.radians(rot + 360 * k / n)), math.sin(math.radians(rot + 360 * k / n))) for k in range(n)]


def star(n, k):
    """{n/k}: the closed one-stroke star"""
    out, i = [], 0
    for _ in range(n):
        a = math.radians(-90 + 360 * i / n)
        out.append((math.cos(a), math.sin(a)))
        i = (i + k) % n
    return out


def curve(f, n=36, t1=2 * math.pi):
    return [f(t1 * k / n) for k in range(n)]


def polar(r, n=40):
    return curve(lambda t: (r(t) * math.cos(t), r(t) * math.sin(t)), n)


def figure(stars, order):
    """a stick figure as ONE trail through its stars in the order given (no line is walked twice: after the last star
    the dot goes straight back to the first, like every other closed shape)"""
    return [stars[i] for i in order]


SHAPES = []


def add(name, pts, flags=0):
    SHAPES.append((name, pts, flags))


add("TRIANGLE", poly(3))
add("SQUARE", poly(4, -45))
add("TRAPEZOID", [(-0.55, -0.6), (0.55, -0.6), (1.0, 0.6), (-1.0, 0.6)])
add("DIAMOND", [(0, -1), (0.6, 0), (0, 1), (-0.6, 0)])
add("PENTAGON", poly(5))
add("HEXAGON", poly(6, 0))
add("PENTAGRAM", star(5, 2))
add("HEXAGRAM", [((1.0 if k % 2 == 0 else 0.577) * math.cos(math.radians(-90 + 30 * k)),
                  (1.0 if k % 2 == 0 else 0.577) * math.sin(math.radians(-90 + 30 * k))) for k in range(12)])
add("HEPT 7/2", star(7, 2))
add("HEPT 7/3", star(7, 3))
add("OCTAGRAM", star(8, 3))
add("CIRCLE", curve(lambda t: (math.cos(t), math.sin(t)), 32))
add("LEMNISCATE", curve(lambda t: (math.cos(t) / (1 + math.sin(t) ** 2), math.sin(t) * math.cos(t) / (1 + math.sin(t) ** 2)), 40))
add("CARDIOID", curve(lambda t: ((1 - math.cos(t)) * math.cos(t), (1 - math.cos(t)) * math.sin(t)), 36))
add("ROSE 3", polar(lambda t: math.cos(3 * t), 48))
add("ROSE 4", polar(lambda t: math.cos(2 * t), 48))
add("LISSAJOUS", curve(lambda t: (math.sin(3 * t), math.sin(2 * t + math.pi / 2)), 48))
add("SPIRO", curve(lambda t: (2 * math.cos(t) + math.cos(2 * t), 2 * math.sin(t) - math.sin(2 * t)), 48))
add("WAVE", [(-1 + 2 * k / 16, 0.5 * math.sin(2 * math.pi * 1.5 * k / 16)) for k in range(17)] +
    [(1 - 2 * k / 16, -0.5 * math.sin(2 * math.pi * 1.5 * (16 - k) / 16)) for k in range(1, 16)])
add("SPIRAL", [(r * math.cos(2.5 * 2 * math.pi * r), r * math.sin(2.5 * 2 * math.pi * r)) for r in [k / 30.0 for k in range(1, 31)]])
add("HEART", curve(lambda t: (16 * math.sin(t) ** 3,
                              -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))), 40))
add("TRIQUETRA", curve(lambda t: (math.sin(t) + 2 * math.sin(2 * t), math.cos(t) - 2 * math.cos(2 * t)), 48))
add("MOBIUS", curve(lambda t: (math.cos(t) / (1 + math.sin(t) ** 2), math.sin(t) * math.cos(t) / (1 + math.sin(t) ** 2)), 48), MOB)
add("CROSS", [(-0.33, -1), (0.33, -1), (0.33, -0.33), (1, -0.33), (1, 0.33), (0.33, 0.33), (0.33, 1), (-0.33, 1),
              (-0.33, 0.33), (-1, 0.33), (-1, -0.33), (-0.33, -0.33)])
add("BOLT", [(0.2, -1), (-0.55, 0.15), (-0.05, 0.15), (-0.3, 1), (0.6, -0.25), (0.05, -0.25)])

# the sky: (x, y) y down; each figure is one trail through its stars (the last star goes back to the first)
cas = [(-100, -30), (-50, 30), (0, -20), (50, 30), (100, -30)]
add("CASSIOPEIA", figure(cas, [0, 1, 2, 3, 4]), SKY)
dip = [(-100, -5), (-68, -22), (-35, -28), (-3, -18), (8, 25), (62, 28), (68, -12)]
add("BIG DIPPER", figure(dip, [0, 1, 2, 3, 6, 5, 4]), SKY)          # handle, then the bowl round
crux = [(0, -100), (70, -8), (10, 100), (-75, 5)]
add("CRUX", figure(crux, [0, 1, 2, 3]), SKY)
ori = [(-50, -90), (50, -85), (14, -8), (0, -4), (-12, 0), (-40, 95), (55, 90)]
add("ORION", figure(ori, [0, 1, 2, 3, 4, 5, 6]), SKY)               # shoulders, the belt, the feet
sco = [(-85, -70), (-78, -35), (-40, -8), (-18, 22), (10, 45), (45, 60), (75, 40), (92, 15), (88, -8)]
add("SCORPIUS", figure(sco, list(range(9))), SKY)
leo = [(-88, -75), (-60, -52), (-55, -22), (-78, 8), (-90, 40), (30, 30), (100, 10), (40, -20)]
add("LEO", figure(leo, list(range(8))), SKY)                         # the sickle, then the body round
cyg = [(0, -100), (85, -10), (0, 0), (-85, -15), (0, 100)]
add("CYGNUS", figure(cyg, [0, 1, 2, 3, 4]), SKY)
lyr = [(0, -95), (-25, -30), (35, -25), (55, 40), (-10, 45)]
add("LYRA", figure(lyr, [0, 1, 2, 3, 4]), SKY)


def norm(pts):
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    s = max(max(xs) - min(xs), max(ys) - min(ys)) / 2 or 1
    return [(round((x - cx) / s * 127), round((y - cy) / s * 127)) for x, y in pts]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "firmware/src/trace_data.h"
    prev = sys.argv[2] if len(sys.argv) > 2 else None
    data, offs = [], [0]
    for name, pts, fl in SHAPES:
        q = norm(pts)
        if len(q) > 1 and q[-1] == q[0]:
            q.pop()
        assert 3 <= len(q) <= 64, (name, len(q))
        data.extend(q)
        offs.append(len(data))
    maxp = max(offs[k + 1] - offs[k] for k in range(len(SHAPES)))
    with open(out, "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* GENERATED by tools/gen_trace_shapes.py: TRACE's shapes (see swarm.c). Do not edit. */\n")
        f.write("#define TR_N %d\n#define TR_MAXP %d\n" % (len(SHAPES), maxp))
        f.write("#define TRF_MOBIUS 1\n#define TRF_SKY 2\n")
        names = [s[0] for s in SHAPES]
        f.write("static const char *const TR_NAME[TR_N] = {%s};\n" % ", ".join('"%s"' % n for n in names))
        f.write("static const uint8_t TR_FLAG[TR_N] = {%s};\n" % ", ".join(str(s[2]) for s in SHAPES))
        f.write("static const uint16_t TR_OFF[TR_N + 1] = {%s};\n" % ", ".join(map(str, offs)))
        f.write("static const int8_t TR_PTS[%d] = {\n" % (2 * len(data)))
        for k in range(len(SHAPES)):
            f.write("    /* %s */ %s,\n" % (names[k], ", ".join("%d, %d" % p for p in data[offs[k]:offs[k + 1]])))
        f.write("};\n")
        f.write("static const uint8_t TR_DEF[4] = {%d, %d, %d, %d};   /* the first shapes: PENTAGRAM, MOBIUS, TRIQUETRA, ORION */\n" %
                tuple(names.index(n) for n in ("PENTAGRAM", "MOBIUS", "TRIQUETRA", "ORION")))
    print("%d shapes, %d points, longest %d, %d B of points" % (len(SHAPES), len(data), maxp, 2 * len(data)))
    if prev:
        from PIL import Image, ImageDraw
        cols = 7
        rows = (len(SHAPES) + cols - 1) // cols
        im = Image.new("RGB", (cols * 120, rows * 130), (20, 14, 30))
        d = ImageDraw.Draw(im)
        for k, (name, _, fl) in enumerate(SHAPES):
            ox, oy = (k % cols) * 120 + 60, (k // cols) * 130 + 62
            p = data[offs[k]:offs[k + 1]]
            xy = [(ox + x * 0.42, oy + y * 0.42) for x, y in p]
            d.line(xy + [xy[0]], fill=(200, 160, 255), width=1)
            if fl & SKY:
                for x, y in xy:
                    d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=(255, 240, 160))
            d.text((ox - 55, oy + 55), name, fill=(255, 255, 255))
        im.save(prev)


main()
