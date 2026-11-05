#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(chord-harp fork, 2026-10-30) The web editor's mock device (web/editor.html makeMockDevice ENG) mirrors the firmware's
engines: this rewrites the entries of the engines named, from build/host/desc.json (tests/descdump.c), in the mock's
own notation (D / E / P). web/test_web.mjs checks the mock against desc.json.
  python3 tools/sync_editor_engines.py ENGINE...        (refresh build/host/desc.json first)"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
F = ["INT", "PCT", "BIPCT", "TIME", "LFOHZ", "CUTOFF", "DB", "SEMI", "ENUM", "BPM", "NOTE", "ONOFF", "OCT", "STEPS"]


def js(v):
    return json.dumps(v, ensure_ascii=False).replace('","', '", "').replace(",", ", ").replace(",  ", ", ")


def desc(d):
    if d["fmt"] == 8 and d["names"] and d["min"] == 0 and d["max"] == len(d["names"]) - 1 and not d["unit"]:
        return 'E(%s, %s, %d)' % (js(d["label"]), js(d["names"]), d["def"])
    s = 'D(%s, F.%s, %d, %d, %d' % (js(d["label"]), F[d["fmt"]], d["min"], d["max"], d["def"])
    if d["names"] or d["unit"]:
        s += ', %s' % (js(d["names"]) if d["names"] else "null")
    if d["unit"]:
        s += ', %s' % js(d["unit"])
    return s + ')'


def preset(p):
    s = 'P(%s, %s, %s' % (js(p["name"]), js(p["e"]), js(p["env"]))
    tail = [p["mono"], p["pat"]]
    while tail and not tail[-1]:
        tail.pop()
    return s + ''.join(', %d' % x for x in tail) + ')'


def entry(e):
    edit = [desc(d) for d in e["edit"]]
    L = ['    { name: %s, titles: %s, edit: [%s,' % (js(e["name"]), js(e["titles"]), ', '.join(edit[:4])),
         '        %s],' % ', '.join(edit[4:])]
    ps = [preset(p) for p in e["presets"]]
    rows = []
    for i in range(0, len(ps), 2):
        rows.append('        ' + ', '.join(ps[i:i + 2]))
    L.append('      presets: [' + (',\n'.join(rows)).lstrip() + '] },')
    return '\n'.join(L)


def main(names):
    d = json.loads((ROOT / "build/host/desc.json").read_text())
    path = ROOT / "web/editor.html"
    html = path.read_text()
    a = html.index("  const ENG = [\n")
    b = html.index("\n  ];\n", a)
    body = html[a:b]
    for name in names:
        fe = [e for e in d["ENG"] if e["name"] == name]
        assert len(fe) == 1, name
        m = re.search(r'\n    \{ name: "%s", .*?\] \},?(?=\n    \{ name: |\n    /\*|\n  \]|$)' % re.escape(name), body, re.S)
        assert m, name
        new = '\n' + entry(fe[0])
        body = body[:m.start()] + new + body[m.end():]
        print(name, len(fe[0]["presets"]), "presets")
    path.write_text(html[:a] + body + html[b:])


if __name__ == "__main__":
    main(sys.argv[1:])
