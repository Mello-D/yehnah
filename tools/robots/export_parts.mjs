// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mello-D
// (chord-harp fork, 2026-10-28) The engine robots' parts out of their design page (engine_robots.html's code, web/road/road.js, the source of truth:
// its drawing code is run here as it is), for robots_convert.py -> firmware/src/ui_robots.h.
//   node tools/robots/export_parts.mjs [OUTDIR] [--refs]   (default tools/robots/parts; --refs: robots.json and ref/ only,
//                                                           the part PNGs left as they are: hand edits kept)
// OUTDIR/<part>.png  each still part (the heads, the body, the arms in every state, the legs, the foot), RGBA, transparent
//                    round it; a head's or the body's own colours (the robot's ramp) are drawn in the marker ramp MARK
//                    (five grays), every other colour as the page draws it. Edit them by hand (Aseprite ..) in those
//                    colours only: robots_convert.py refuses any other.
// OUTDIR/parts.tsv   each part's name and where its top left sits on the robot's grid (x y: the page's robot coordinates)
// OUTDIR/robots.json the palette (the page's ramps and every other colour its code draws), MARK, the twenty robots
// OUTDIR/ref/        each robot in a few states as the page draws it (30 x 34, outlined): the C port is checked on them
import fs from 'node:fs';
import zlib from 'node:zlib';
import path from 'node:path';

const here = path.dirname(new URL(import.meta.url).pathname);
const args = process.argv.slice(2), refsOnly = args.includes('--refs');
const out = args.find(a => !a.startsWith('--')) || path.join(here, 'parts');
const html = fs.readFileSync(path.join(here, '../../web/road/road.js'), 'utf8');   // (the page's code)
const src = html.slice(html.indexOf('\nconst BAYER') + 1, html.indexOf('\n// ---- pixels'));
const MARK = ['#202020', '#484848', '#707070', '#a0a0a0', '#d8d8d8'];
const P = new Function(src + '\nreturn { R, INK, ROBOTS, POSES, HEAD_TOP, Layer, box, ell, rect, arm, head, robot, outline };')();

function png(file, w, h, px) {   // px: '#rrggbb' or null, row-major
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const c = px[y * w + x], i = y * (w * 4 + 1) + 1 + x * 4;
    if (c) { raw[i] = parseInt(c.slice(1, 3), 16); raw[i + 1] = parseInt(c.slice(3, 5), 16); raw[i + 2] = parseInt(c.slice(5, 7), 16); raw[i + 3] = 255; }
  }
  const chunk = (t, d) => { const b = Buffer.alloc(12 + d.length); b.writeUInt32BE(d.length, 0); b.write(t, 4); d.copy(b, 8); b.writeUInt32BE(zlib.crc32(b.subarray(4, 8 + d.length)), 8 + d.length); return b; };
  const ih = Buffer.alloc(13); ih.writeUInt32BE(w, 0); ih.writeUInt32BE(h, 4); ih[8] = 8; ih[9] = 6;
  fs.writeFileSync(file, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
}
// a part: drawn by f into a layer like the robot's (its 0 0 at 10 2), cut to what it drew
const parts = [];
function part(name, f) {
  const L = new P.Layer(40, 34, 10, 2);                 // (wider than the robot's grid: a part shifted right is not cut)
  f(L);
  let x0 = 99, y0 = 99, x1 = -1, y1 = -1;
  L.p.forEach((c, i) => { if (c) { const x = i % 40, y = (i / 40) | 0; x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); } });
  const w = x1 - x0 + 1, h = y1 - y0 + 1, px = [];
  for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) px.push(L.p[y * 40 + x]);
  if (!refsOnly) png(path.join(out, name + '.png'), w, h, px);
  parts.push([name, x0 - 10, y0 - 2]);
}
fs.mkdirSync(path.join(out, 'ref'), { recursive: true });
const S = P.R.steel;
for (const k of ['box', 'round', 'dome', 'wide', 'tall']) part('head_' + k, L => P.head(L, { head: k, hr: MARK }));
part('body', L => { P.box(L, 3, 14, 14, 11, MARK); P.rect(L, 8, 13, 4, 1, S[1]); for (const [x, y] of [[4, 15], [15, 15], [4, 23], [15, 23]]) L.set(x, y, MARK[4]); });
for (const r of [0, 1]) for (const st of [0, 1, 2, 3, 'up', 'out', 'wave', 'hips', 'diag'])
  part(`arm_${r ? 'r' : 'l'}_${typeof st === 'number' ? 'down' + st : st}`, L => P.arm(L, !!r, st, S));
part('leg', L => P.box(L, 6, 25, 3, 2, S, false));
part('leg_bent', L => P.box(L, 6, 26, 3, 1, S, false));
part('leg_wide', L => P.box(L, 3, 25, 4, 2, S, false));
part('foot', L => P.box(L, 4, 27, 5, 3, P.R.steelD));
if (!refsOnly) fs.writeFileSync(path.join(out, 'parts.tsv'), parts.map(p => p.join('\t')).join('\n') + '\n');

// the palette: the ramps in order, then every other colour the drawing code names
const ramps = Object.entries(P.R).map(([k, v]) => [k, v]);
const seen = new Set(ramps.flatMap(r => r[1])), misc = [];
for (const m of src.matchAll(/'#([0-9a-f]{6})'/g)) { const c = '#' + m[1]; if (!seen.has(c)) { seen.add(c); misc.push(c); } }
for (const c of MARK) if (seen.has(c)) throw new Error('MARK colour ' + c + ' is drawn by the page');
const rampOf = r => ramps.find(([, v]) => v === r)[0];
fs.writeFileSync(path.join(out, 'robots.json'), JSON.stringify({
  mark: MARK, ramps, misc, ink: P.INK, head_top: P.HEAD_TOP, poses: P.POSES,
  robots: P.ROBOTS.map(r => ({ name: r.name, head: r.head, hr: rampOf(r.hr), br: rampOf(r.br), ac: rampOf(r.ac), em: r.em, ant: r.ant, eyes: r.eyes, talk: !!r.talk, shape: r.shape })),
}, null, 1));

// the references: each robot in four states (its pose, its sound), outlined as the page draws it
const STATES = [
  { t: 0, hit: 0, level: 0, note: 0, ang: 0, blink: false, pose: 0, dip: 0 },
  { t: 1234, hit: 1, level: 1, note: 7, ang: 2.1, blink: false, pose: 3, dip: 0 },
  { t: 2600, hit: .45, level: .6, note: 4, ang: 4.4, blink: true, pose: 7, dip: 0 },
  { t: 5021, hit: .2, level: .3, note: 9, ang: 5.5, blink: false, pose: 12, dip: 1 },
];
const refs = [];
P.ROBOTS.forEach((sp, i) => STATES.forEach((st, k) => {
  const L = P.robot(sp, { ...st, pose: P.POSES[st.pose] });
  png(path.join(out, 'ref', `${i}_${k}.png`), 30, 34, L.p);
  refs.push(L.p);
}));
fs.writeFileSync(path.join(out, 'ref', 'states.json'), JSON.stringify(STATES));
console.log(`export_parts: ${parts.length} parts, ${misc.length} other colours, ${refs.length} references in ${out}`);
