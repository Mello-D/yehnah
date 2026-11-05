// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mello-D
// (chord-harp fork) HOME's road and the engine robots in JS, shared: tools/robots/engine_robots.html (the robots'
// design page, their source of truth: export_parts.mjs runs the code between `const BAYER` and `// ---- pixels` as it
// is) and android/big's page (web/emu/index.html, the big FM-1: the scene drawn on the tablet from the FM-1's ROAD
// message, docs/plans/BIG-FM1-PLAN.md step 4). The rule: change firmware/src/ui_road.c (and ui_robot.c's dance) and
// this file together; tests/tablet_road.mjs checks the two draw the same scene.
//   Road.decode(bytes, at, n)  a ROAD message's payload (firmware/src/tab_screen.c ts_road) -> a frame's inputs
//   Road.frame(put, f)         the scene of those inputs: put(x, y, 0xRRGGBB) for each pixel, x 0..239, y 0..f.sh-1
(function (G) {
'use strict';
const BAYER = [[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]].map(r => r.map(v => v / 16));
const R = {   // (2026-11-03) the user's palette: twenty colours, each ramp built from them (darks toward #0d0221, the warm
               // ones' through magenta and purple, lights toward white); the greys a silver white; nothing brown (wood a
               // magenta, sand a pale yellow, green / mint cyan)
  steel:  ['#3a3466','#8a88b0','#c4c6dc','#e8eaf6','#ffffff'],
  steelD: ['#241734','#2e2157','#6a6894','#a8aac8','#d8daec'],
  copper: ['#261447','#650d89','#d40078','#ff6c11','#ffc49a'],
  teal:   ['#132f48','#1b677a','#25aab5','#2de2e6','#a1f2f4'],
  red:    ['#3d072b','#790e38','#c11647','#fd1d53','#fe99b2'],
  yellow: ['#261447','#920075','#ff6c11','#f9c80e','#fdf0b0'],
  violet: ['#380d4f','#591672','#791e94','#a162b4','#d0b0da'],
  green:  ['#132f48','#1b677a','#25aab5','#2de2e6','#a1f2f4'],
  black:  ['#0d0221','#241734','#2e2157','#540d6e','#791e94'],
  cream:  ['#2e2157','#8a88b0','#c4c6dc','#e8eaf6','#ffffff'],
  wood:   ['#350232','#670148','#a20162','#d40078','#ec8cc2'],
  navy:   ['#09174a','#052769','#023788','#4e73ac','#a6b9d5'],
  mint:   ['#1a5c70','#239fab','#2de2e6','#6cebee','#b6f5f6'],
  pink:   ['#3c023a','#760259','#bc017e','#f6019d','#fb8dd3'],
  sand:   ['#650d89','#ff6c11','#f9c80e','#fce693','#fffbe0'],
  moon:   ['#2e2157','#8a88b0','#c4c6dc','#e8eaf6','#ffffff'],
};
const INK = '#0d0221';
const LX = .55, LY = -.5, LZ = .67;
const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
const fr = v => v - Math.floor(v);
const hsh = n => { n = Math.imul(n ^ (n >>> 16), 0x45d9f3b); n = Math.imul(n ^ (n >>> 16), 0x45d9f3b); return (n ^ (n >>> 16)) >>> 0; };

// ---- drawing helpers: a robot is drawn into a Layer (its own grid, origin ox oy), then outlined
class Layer {
  constructor(w, h, ox, oy) { this.w = w; this.h = h; this.ox = ox; this.oy = oy; this.p = new Array(w * h).fill(null); }
  set(x, y, c) { x = Math.floor(x) + this.ox; y = Math.floor(y) + this.oy; if (x >= 0 && y >= 0 && x < this.w && y < this.h) this.p[y * this.w + x] = c; }
}
function rect(L, x0, y0, w, h, c) { for (let y = y0; y < y0 + h; y++) for (let x = x0; x < x0 + w; x++) L.set(x, y, c); }
function box(L, x0, y0, w, h, r, round = true) {
  for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) {
    if (round && w > 2 && h > 2 && (i === 0 || i === w - 1) && (j === 0 || j === h - 1)) continue;
    let c;
    if (j === 0) c = i > w * .4 ? r[4] : r[3];
    else if (j === h - 1) c = r[0];
    else if (i === w - 1) c = r[3];
    else if (i === 0 || j === h - 2) c = r[1];
    else c = j < h * .4 ? r[3] : r[2];
    L.set(x0 + i, y0 + j, c);
  }
}
function ell(L, cx, cy, rx, ry, r, clipY = 99) {
  for (let y = Math.floor(cy - ry); y <= Math.ceil(cy + ry); y++) for (let x = Math.floor(cx - rx); x <= Math.ceil(cx + rx); x++) {
    if (y > clipY) continue;
    const dx = (x + .5 - cx) / rx, dy = (y + .5 - cy) / ry, d2 = dx * dx + dy * dy;
    if (d2 > 1) continue;
    const li = dx * LX + dy * LY + Math.sqrt(1 - d2) * LZ;
    let c = r[clamp(Math.round((li + 1) / 2 * 4 - .3), 1, 4)];
    if (li > .93) c = r[4];
    if (d2 > .7 && li < .15) c = r[0];
    L.set(x, y, c);
  }
}
function outline(L) {   // dark all round
  const src = L.p.slice(), g = (x, y) => x >= 0 && y >= 0 && x < L.w && y < L.h ? src[y * L.w + x] : null;
  for (let y = 0; y < L.h; y++) for (let x = 0; x < L.w; x++) {
    if (src[y * L.w + x]) continue;
    if (!(g(x + 1, y) || g(x - 1, y) || g(x, y - 1) || g(x, y + 1))) continue;
    L.p[y * L.w + x] = INK;
  }
}

// the colour shown at x y: the robot's edge lit, cyan on the left, pink on the right, a little white on top (the street's
// neon; the outline is INK, so an edge is a pixel beside one of it that is not inside the robot's own INK)
const MIXC = (a, b, t) => '#' + [1, 3, 5].map(i => Math.round(parseInt(a.slice(i, i + 2), 16) * (1 - t) + parseInt(b.slice(i, i + 2), 16) * t).toString(16).padStart(2, '0')).join('');
function lit(L, x, y) {
  const c = L.p[y * L.w + x], e = L.edge;
  if (!c || c === INK || !e) return c;
  const k = e[y * L.w + x];
  return k === 1 ? MIXC(c, '#2de2e6', .5) : k === 2 ? MIXC(c, '#f706cf', .5) : k === 3 ? MIXC(c, '#ffffff', .25) : c;
}

// ---- the twenty
const ROBOTS = [
  { name: 'ANALOG',  about: 'a record on its chest, spinning faster as it plays', shape: 'std', head: 'box', hr: R.steel, br: R.copper, ac: R.red, em: 'record', ant: 'ball', eyes: 'dots' },
  { name: 'FM6',     about: 'a radio: the dial needle finds each note', shape: 'tall', head: 'round', hr: R.teal, br: R.steel, ac: R.yellow, em: 'grille', ant: 'twin', eyes: 'visor' },
  { name: 'PHASE',   about: 'a sleepy moon-bot; the moon on its chest waxes and wanes', shape: 'round', head: 'dome', hr: R.steel, br: R.navy, ac: R.navy, em: 'moon', ant: 'none', eyes: 'sleepy' },
  { name: 'LOFI',    about: '8-bit eyes and a cassette with turning reels', shape: 'squat', head: 'box', hr: R.sand, br: R.wood, ac: R.sand, em: 'tape', ant: 'none', eyes: 'pixel' },
  { name: 'VOICE',   about: 'a singer: its mouth opens with the level, the speaker pulses', shape: 'round', head: 'dome', hr: R.teal, br: R.violet, ac: R.teal, em: 'speaker', ant: 'twin', eyes: 'dots', talk: true },
  { name: 'WHEEL',   about: 'an organ: drawbars on its chest, a spinning Leslie for a hat', shape: 'squat', head: 'wide', hr: R.steel, br: R.wood, ac: R.copper, em: 'drawbars', ant: 'leslie', eyes: 'dots' },
  { name: 'DRUM',    about: 'a one-eyed drummer; the drum head flashes on each hit', shape: 'squat', head: 'dome', hr: R.red, br: R.yellow, ac: R.copper, em: 'drum', ant: 'dish', eyes: 'single' },
  { name: 'NOISE',   about: 'an old TV: rabbit ears and static that fizzes louder', shape: 'squat', head: 'wide', hr: R.steel, br: R.black, ac: R.cream, em: 'static', ant: 'rabbit', eyes: 'pixel' },
  { name: 'ANIMAL',  about: 'cat ears and a paw print; the toes twitch on a note', shape: 'round', head: 'round', hr: R.sand, br: R.pink, ac: R.pink, em: 'paw', ant: 'ears', eyes: 'dots' },
  { name: 'JUNO',    about: 'a keyboard chest: a key goes down with each note', shape: 'std', head: 'box', hr: R.violet, br: R.steel, ac: R.violet, em: 'keys', ant: 'ball', eyes: 'dots' },
  { name: 'OMNI',    about: 'a harp in its chest, a halo above; the plucked string shakes', shape: 'hover', head: 'round', hr: R.mint, br: R.cream, ac: R.mint, em: 'harp', ant: 'halo', eyes: 'sleepy' },
  { name: 'KALIMBA', about: 'metal tines on wood; the struck tine rings', shape: 'tall', head: 'box', hr: R.mint, br: R.wood, ac: R.wood, em: 'tines', ant: 'none', eyes: 'single' },
  { name: 'GUITAR',  about: 'a soundhole and strings, a lightning bolt on top', shape: 'std', head: 'round', hr: R.copper, br: R.wood, ac: R.red, em: 'guitar', ant: 'bolt', eyes: 'dots' },
  { name: 'ACID',    about: 'a scope drawing the acid line in green', shape: 'tracks', head: 'round', hr: R.yellow, br: R.black, ac: R.green, em: 'scope', ant: 'dish', eyes: 'single' },
  { name: 'DRIFT',   about: 'bands of light drifting across its chest', shape: 'hover', head: 'dome', hr: R.navy, br: R.black, ac: R.mint, em: 'bands', ant: 'ball', eyes: 'visor' },
  { name: 'PLAITS',  about: 'three strands plaiting round each other', shape: 'round', head: 'round', hr: R.pink, br: R.violet, ac: R.pink, em: 'braid', ant: 'twin', eyes: 'dots' },
  { name: 'STRINGS', about: 'a tall, sleepy cellist; the bow moves with the level', shape: 'tall', head: 'tall', hr: R.wood, br: R.copper, ac: R.red, em: 'bow', ant: 'none', eyes: 'sleepy' },
  { name: 'VPS',     about: 'a screen with its wave, bent at the knee', shape: 'std', head: 'box', hr: R.navy, br: R.steel, ac: R.pink, em: 'vps', ant: 'dish', eyes: 'visor' },
  { name: 'MODAL',   about: 'tone bars and a mallet that strikes them', shape: 'tracks', head: 'dome', hr: R.yellow, br: R.navy, ac: R.yellow, em: 'bars', ant: 'ball', eyes: 'single' },
  { name: 'WSEQ',    about: 'a beach-bot in shades; a wave breaks across its chest each step', shape: 'hover', head: 'round', hr: R.sand, br: R.mint, ac: R.navy, em: 'wave', ant: 'none', eyes: 'shades' },
  { name: 'TERRAIN', about: 'a surveyor: contour lines across its chest, a dish listening to the ground', shape: 'tracks', head: 'wide', hr: R.mint, br: R.wood, ac: R.sand, em: 'bands', ant: 'dish', eyes: 'visor' },
  { name: 'DIGITAL', about: 'a pocket calculator: its note in seven segments, lit on each hit', shape: 'std', head: 'box', hr: R.teal, br: R.pink, ac: R.yellow, em: 'segs', ant: 'twin', eyes: 'visor' },
  { name: 'VECTOR',  about: 'a joystick-bot: a scope on its chest where the point moves between four waves', shape: 'std', head: 'box', hr: R.violet, br: R.navy, ac: R.yellow, em: 'scope', ant: 'twin', eyes: 'pixel' },
  { name: 'HYBRID',  about: 'a tall patch-bot in shades: three waves on its chest, a bolt for an aerial', shape: 'std', head: 'tall', hr: R.copper, br: R.navy, ac: R.mint, em: 'wave', ant: 'bolt', eyes: 'shades' },
];
const HEAD_TOP = { box: 4, round: 4, dome: 4, wide: 5, tall: 2 };

// the moves: each arm down / out / up / wave / hips / diag; lean, squat (2: knees wide), a foot up, a nod, a side step
const POSES = [
  { la: 'down', ra: 'down' },
  { la: 'up', ra: 'up' },
  { la: 'up', ra: 'out', lean: -1 },
  { la: 'out', ra: 'up', lean: 1 },
  { la: 'out', ra: 'out', squat: 1 },
  { la: 'up', ra: 'down', lean: 1, lift: 2 },
  { la: 'down', ra: 'up', lean: -1, lift: 1 },
  { la: 'up', ra: 'up', squat: 2 },
  { la: 'wave', ra: 'down', nod: 1 },
  { la: 'down', ra: 'wave', nod: 1 },
  { la: 'hips', ra: 'hips', squat: 1, shift: -1 },
  { la: 'hips', ra: 'hips', squat: 1, shift: 1 },
  { la: 'diag', ra: 'hips', lean: -1 },
  { la: 'hips', ra: 'diag', lean: 1 },
  { la: 'wave', ra: 'wave', squat: 2 },
  { la: 'out', ra: 'out', lift: 1, shift: 1 },
];
function poseAt(beat, seed) {
  let i = hsh(beat * 31 + seed * 977) % POSES.length;
  if (i === hsh((beat - 1) * 31 + seed * 977) % POSES.length) i = (i + 5) % POSES.length;
  return POSES[i];
}

function arm(L, right, state, S) {
  const bx = (x, y, w, h) => box(L, right ? 20 - x - w : x, y, w, h, S);
  const el = (cx, cy, rx, ry) => ell(L, right ? 20 - cx : cx, cy, rx, ry, S);
  const px = (x, y, c) => L.set(right ? 19 - x : x, y, c);
  if (state === 'up') { bx(0, 8, 3, 7); el(1.5, 7.5, 1.6, 1.4); }
  else if (state === 'out') { bx(-4, 15, 7, 3); el(-4, 16.5, 1.4, 1.6); }
  else if (state === 'wave') { bx(-3, 15, 6, 3); bx(-3, 10, 3, 6); el(-1.5, 9.5, 1.6, 1.4); }
  else if (state === 'hips') { bx(-1, 15, 3, 5); bx(-1, 18, 4, 3); el(2.5, 19.5, 1.3, 1.3); }
  else if (state === 'diag') {
    for (let k = 0; k < 6; k++) { const x = 1 - k, y = 14 - k; px(x, y, S[3]); px(x + 1, y, S[3]); px(x + 1, y + 1, S[2]); px(x + 2, y + 1, S[2]); px(x + 2, y + 2, S[1]); }
    el(-4.5, 8.5, 1.5, 1.5);
  } else { const r = typeof state === 'number' ? state : 0; bx(0, 15 - r, 3, 7); el(1.5, 22.5 - r, 1.6, 1.4); }
}

function antenna(L, sp, s, ty) {
  const S = R.steel, ac = sp.ac, lit = s.hit > .3;
  switch (sp.ant) {
    case 'ball': rect(L, 10, ty - 2, 1, 2, S[2]); ell(L, 10.5, ty - 2.5, 1.5, 1.5, lit ? R.yellow : ac); break;
    case 'twin': for (const x of [6, 13]) { rect(L, x, ty - 3, 1, 3, S[2]); L.set(x, ty - 4, lit ? ac[4] : ac[2]); } break;
    case 'dish': rect(L, 8, ty - 3, 5, 1, S[3]); rect(L, 9, ty - 2, 3, 1, S[2]); L.set(10, ty - 1, S[1]); if (lit) L.set(10, ty - 4, ac[4]); break;
    case 'ears': {
      const hr = sp.hr;
      rect(L, 5, ty - 1, 3, 1, hr[3]); rect(L, 5, ty - 2, 2, 1, hr[3]); L.set(5, ty - 3, hr[3]); L.set(6, ty - 1, R.pink[3]);
      rect(L, 12, ty - 1, 3, 1, hr[3]); rect(L, 13, ty - 2, 2, 1, hr[3]); L.set(14, ty - 3, hr[3]); L.set(13, ty - 1, R.pink[3]);
      if (lit) { L.set(5, ty - 3, hr[4]); L.set(14, ty - 3, hr[4]); }
      break;
    }
    case 'leslie': {
      L.set(10, ty - 1, S[1]);
      box(L, 7, ty - 5, 7, 4, R.wood);
      rect(L, 8 + Math.floor(s.t / (140 - s.level * 90)) % 5, ty - 4, 1, 2, INK);
      break;
    }
    case 'halo': {
      const b = Math.round(Math.sin(s.t * .004));
      rect(L, 7, ty - 4 + b, 6, 1, lit ? '#fdf0b0' : '#fce693'); L.set(6, ty - 3 + b, '#fce693'); L.set(13, ty - 3 + b, '#fce693'); rect(L, 7, ty - 2 + b, 6, 1, '#ff6c11');
      break;
    }
    case 'bolt': for (const [x, y] of [[10, 1], [11, 2], [10, 3], [11, 3], [12, 4], [11, 5]]) L.set(x, ty - 6 + y, lit ? '#ffffff' : (y < 3 ? '#fdf0b0' : '#f9c80e')); break;
    case 'rabbit': for (let k = 0; k < 4; k++) { L.set(8 - k, ty - 1 - k, S[3]); L.set(11 + k, ty - 1 - k, S[3]); } L.set(4, ty - 5, lit ? ac[4] : ac[2]); L.set(15, ty - 5, lit ? ac[4] : ac[2]); break;
  }
}

function head(L, sp) {
  const hr = sp.hr;
  if (sp.head === 'box') box(L, 4, 4, 12, 9, hr);
  else if (sp.head === 'round') ell(L, 10, 8.5, 6.6, 4.8, hr);
  else if (sp.head === 'wide') box(L, 2, 5, 16, 8, hr);
  else if (sp.head === 'tall') box(L, 5, 2, 10, 11, hr);
  else { box(L, 4, 8, 12, 5, hr, false); ell(L, 10, 9, 6, 5, hr, 8); L.set(4, 12, null); L.set(15, 12, null); }
}

function eyes(L, sp, s) {
  const e = sp.eyes;
  if (e === 'dots' || e === 'sleepy') {
    for (const x of [6, 12]) {
      if (s.blink) { rect(L, x, 8, 2, 1, INK); continue; }
      L.set(x, 7, '#ffffff'); L.set(x + 1, 7, '#6cebee'); L.set(x, 8, '#6cebee'); L.set(x + 1, 8, '#239fab');
      if (e === 'sleepy') rect(L, x, 7, 2, 1, sp.hr[1]);
      else { L.set(x - 1, 8, R.teal[2]); L.set(x + 2, 7, R.teal[2]); }
    }
  } else if (e === 'pixel') {
    for (const x of [6, 12]) { if (s.blink) { rect(L, x, 8, 2, 1, INK); continue; } rect(L, x, 7, 2, 2, '#2de2e6'); L.set(x, 7, '#c0f6f7'); }
  } else if (e === 'shades') {
    rect(L, 5, 7, 4, 2, INK); rect(L, 11, 7, 4, 2, INK); rect(L, 9, 7, 2, 1, INK); L.set(6, 7, '#6a6894'); L.set(12, 7, '#6a6894');
  } else if (e === 'visor') {
    rect(L, 6, 7, 8, 2, INK);
    const g = Math.floor(s.t / 110) % 14, p = clamp(g < 8 ? 6 + g : 6 + (13 - g), 6, 13);
    L.set(p, 7, '#ff4365'); L.set(p, 8, '#c11647'); L.set(p - 1, 7, '#790e38'); L.set(p + 1, 7, '#790e38');
  } else {
    ell(L, 10, 8.5, 2.6, 2.6, R.cream);
    const pdx = Math.round(Math.cos(s.note) * .9), pdy = Math.round(Math.sin(s.note) * .6);
    rect(L, 9 + pdx, 8 + pdy, 2, 2, s.blink ? R.cream[1] : INK);
    if (!s.blink) L.set(10 + pdx, 8 + pdy, '#6cebee');
  }
}

function mouth(L, sp, s) {
  const open = sp.talk ? Math.round(s.level * 2.2) : (s.hit > .5 ? 1 : 0), y0 = open > 1 ? 10 : 11;
  for (let x = 8; x < 12; x++) for (let k = 0; k <= open; k++) L.set(x, y0 + k, INK);
  if (open > 1) rect(L, 9, y0 + open, 2, 1, R.red[2]);
}

const SEG7 = [0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f];   // a seven-segment digit's segments a..g
// the chest, x 5..14, y 16..22: each engine's own, moving with the sound
function emblem(L, sp, s) {
  const B = R.black, C = R.cream, S = R.steel, t = s.t, lv = s.level, hit = s.hit, ac = sp.ac;
  const screen = () => box(L, 5, 16, 10, 7, [B[0], B[0], B[1], B[2], B[3]]);
  const pick = n => (Math.floor(s.note) % n + n) % n;
  switch (sp.em) {
    case 'record': {
      for (let y = 15; y < 24; y++) for (let x = 5; x < 15; x++) {
        const dx = x + .5 - 10, dy = y + .5 - 19.5, d = Math.hypot(dx, dy), an = Math.atan2(dy, dx);
        if (d > 3.9) continue;
        let c = d > 2.1 && d < 2.9 ? B[2] : B[1];
        if (d > 1.6 && an > -1.3 && an < -.5) c = d > 2.9 ? B[4] : B[3];
        if (d < 1.4) c = dx > 0 && dy < 0 ? ac[4] : ac[2];
        L.set(x, y, c);
      }
      L.set(10 + Math.cos(s.ang) * 2.8, 19.5 + Math.sin(s.ang) * 2.8, B[3]);
      L.set(10 + Math.cos(s.ang), 19.5 + Math.sin(s.ang), '#ffffff');
      break;
    }
    case 'grille': {
      for (let y = 16; y < 23; y++) for (let x = 5; x < 12; x++) L.set(x, y, (y & 1) ? S[2] : S[0]);
      rect(L, 5 + pick(7), 16, 1, 7, ac[4]);
      ell(L, 13.5, 19.5, 1.7, 1.7, ac);
      break;
    }
    case 'moon': {
      for (let y = 16; y < 23; y++) for (let x = 6; x < 14; x++) {
        const d = Math.hypot(x + .5 - 10, y + .5 - 19.5);
        if (d > 3.4) continue;
        const sh = Math.hypot(x + .5 - 10 - 6 * Math.sin(t * .0006), y + .5 - 19.5) < 3.4;
        L.set(x, y, sh ? sp.br[0] : d > 2.6 ? R.moon[2] : (x + y) % 5 === 0 ? R.moon[1] : R.moon[3]);
      }
      if (hit > .4) L.set(11, 18, '#ffffff');
      break;
    }
    case 'tape': {
      const T = ['#0d0221', '#241734', '#3a3466', '#8a88b0', '#ffc49a'];
      box(L, 5, 17, 10, 6, T);
      rect(L, 7, 18, 6, 3, INK);
      const a = Math.floor(t / (180 - lv * 120)) & 1;
      for (const cx of [8, 11]) { rect(L, cx, 19, 2, 1, T[3]); L.set(cx + a, 19, '#ffffff'); }
      rect(L, 7, 22, 6, 1, T[1]);
      break;
    }
    case 'speaker': {
      const r = 3 + lv * .7;
      ell(L, 10, 19.5, r, r, C);
      ell(L, 10, 19.5, 1.4, 1.4, B);
      for (let i = 0; i < 12; i++) { const an = i / 12 * 6.283; if (Math.cos(an - 2.3) > .2) L.set(10 + Math.cos(an) * (r - 1), 19.5 + Math.sin(an) * (r - 1), C[1]); }
      break;
    }
    case 'drawbars': {
      const cols = ['#a20162', '#a20162', '#e8eaf6', '#e8eaf6', '#2e2157'];
      for (let i = 0; i < 5; i++) {
        const h = 1 + Math.round((Math.sin(i * 1.7 + t * .003) * .5 + .5) * (1 + lv * 4));
        rect(L, 6 + i * 2, 22 - h, 1, h, cols[i]); L.set(6 + i * 2, 22 - h, '#ffffff');
        L.set(6 + i * 2, 22, S[1]);
      }
      break;
    }
    case 'drum': {
      box(L, 5, 18, 10, 5, R.red, false);
      for (let x = 6; x < 14; x++) L.set(x, 19 + (x & 1) * 2, C[2]);
      ell(L, 10, 17.5, 5, 1.6, hit > .4 ? [C[3], C[3], C[4], C[4], '#ffffff'] : C);
      break;
    }
    case 'static': {
      screen();
      const f = Math.floor(t / 70);
      for (let y = 17; y < 22; y++) for (let x = 6; x < 14; x++) { const v = hsh(f * 1000 + y * 20 + x) % 100; if (v < 25 + lv * 50) L.set(x, y, v < 8 ? '#ffffff' : v < 30 ? '#8a88b0' : '#3a3466'); }
      break;
    }
    case 'paw': {
      ell(L, 10, 20.5, 2.4, 1.8, R.pink);
      const w = hit > .4 ? 1 : 0;
      for (const [x, y] of [[6.5, 17.5], [9.5, 16.5 - w], [12.5, 17.5]]) ell(L, x + .5, y + .5, 1.1, 1.1, R.pink);
      break;
    }
    case 'keys': {
      const k = pick(5);
      for (let i = 0; i < 5; i++) for (let y = 17; y < 22; y++) {
        const down = i === k && hit > .3;
        L.set(5 + 2 * i, y, down ? C[1] : y === 17 ? C[4] : C[3]); L.set(6 + 2 * i, y, C[2]);
      }
      for (const i of [0, 1, 3]) rect(L, 6 + 2 * i, 17, 1, 3, B[1]);
      break;
    }
    case 'harp': {
      rect(L, 6, 16, 1, 7, R.wood[3]);
      for (let x = 6; x < 14; x++) L.set(x, 16 + Math.round((x - 6) * .3), '#f9c80e');
      const k = pick(4);
      for (let i = 0; i < 4; i++) {
        const x = 8 + i * 2, top = 17 + Math.round((x - 6) * .3);
        for (let y = top; y < 22; y++) L.set(x + (i === k && hit > .2 && y > top && ((Math.floor(t / 40) + y) & 1) ? 1 : 0), y, C[3]);
      }
      rect(L, 6, 22, 9, 1, R.wood[2]);
      break;
    }
    case 'tines': {
      const k = pick(5), H = [3, 4, 5, 4, 3];
      rect(L, 5, 17, 10, 1, S[1]);
      for (let i = 0; i < 5; i++) { const ring = i === k && hit > .2; rect(L, 6 + i * 2, 18, 1, H[i], ring ? '#ffffff' : S[3]); if (ring && (Math.floor(t / 50) & 1)) L.set(7 + i * 2, 17 + H[i], S[4]); }
      break;
    }
    case 'guitar': {
      ell(L, 10, 19.5, 2.6, 2.6, B);
      for (let i = 0; i < 12; i++) { const an = i / 12 * 6.283; L.set(10 + Math.cos(an) * 3.2, 19.5 + Math.sin(an) * 3.2, C[2]); }
      for (let i = 0; i < 4; i++) rect(L, 8 + i, 16, 1, 7, S[4]);
      if (hit > .3) for (let i = 0; i < 4; i++) L.set(8 + i, 18 + (Math.floor(t / 40) + i) % 3, '#ffffff');
      break;
    }
    case 'scope': {
      screen();
      for (let x = 6; x < 14; x += 2) L.set(x, 19, '#073a4a');
      for (let x = 6; x < 14; x++) { const y = 19.5 + Math.sin(x * .9 + t * .012) * (.4 + lv * 2.4); L.set(x - 1, y, '#1a5c70'); L.set(x, y, '#2de2e6'); }
      break;
    }
    case 'bands': {
      screen();
      const cols = ['#2de2e6', '#6cebee', '#a162b4'];
      for (let k = 0; k < 3; k++) for (let x = 6; x < 14; x++) L.set(x, 17.5 + k * 1.6 + Math.sin(x * .6 + t * .004 + k * 1.3) * (.6 + lv * .6), cols[k]);
      break;
    }
    case 'braid': {
      const cols = [R.pink[3], R.yellow[3], R.teal[3]];
      for (let k = 0; k < 3; k++) for (let x = 5; x < 15; x++) {   // the back halves first, then the front
        const p = x * .9 - t * .006 + k * 2.09;
        if (Math.cos(p) <= 0) L.set(x, 19.5 + Math.sin(p) * 2.4, R.violet[1]);
      }
      for (let k = 0; k < 3; k++) for (let x = 5; x < 15; x++) {
        const p = x * .9 - t * .006 + k * 2.09;
        if (Math.cos(p) > 0) L.set(x, 19.5 + Math.sin(p) * 2.4, cols[k]);
      }
      break;
    }
    case 'bow': {
      for (let x = 8; x < 12; x++) rect(L, x, 16, 1, 7, S[3]);
      const bx = 4 + Math.round((Math.sin(t * .004) * .5 + .5) * 5 * (.3 + lv));
      rect(L, bx, 19, 7, 1, C[3]); rect(L, bx, 18, 7, 1, R.wood[1]); L.set(bx, 18, R.wood[3]);
      break;
    }
    case 'vps': {
      screen();
      const d = .25 + .5 * (Math.sin(t * .002) * .5 + .5);
      for (let x = 6; x < 14; x++) { const p = (x - 6) / 7, y = p < d ? p / d * .6 : .6 + (p - d) / (1 - d) * .4; L.set(x, 21 - y * 4, '#fb8dd3'); }
      break;
    }
    case 'bars': {
      const k = pick(4);
      for (let i = 0; i < 4; i++) rect(L, 5 + i, 16 + i * 2, 10 - i * 2, 1, i === k && hit > .3 ? '#ffffff' : R.yellow[3 - (i & 1)]);
      const my = 15 + k * 2 - (hit > .3 ? 0 : 1);
      L.set(13 - k, my, R.red[3]); L.set(14 - k, my - 1, R.wood[3]);
      break;
    }
    case 'segs': {
      box(L, 5, 16, 10, 7, [B[0], B[0], B[1], B[1], B[2]]);
      const n = pick(100), on = hit > .3 ? ac[4] : ac[3];
      for (const [k, d] of [[0, Math.floor(n / 10)], [1, n % 10]]) {
        const x = 6 + k * 4, m = SEG7[d];
        if (m & 1) rect(L, x + 1, 17, 2, 1, on); if (m & 2) L.set(x + 3, 18, on); if (m & 4) L.set(x + 3, 20, on);
        if (m & 8) rect(L, x + 1, 21, 2, 1, on); if (m & 16) L.set(x, 20, on); if (m & 32) L.set(x, 18, on);
        if (m & 64) rect(L, x + 1, 19, 2, 1, on);
      }
      break;
    }
    case 'wave': {
      rect(L, 5, 16, 10, 3, '#25aab5'); rect(L, 5, 19, 10, 3, '#023788'); rect(L, 5, 22, 10, 1, '#ffc49a');
      const c = 5 + Math.floor(t / 125) % 10;
      L.set(c, 19, '#e8eaf6'); L.set(c + 1, 18, '#e8eaf6'); L.set(c + 2, 18, '#e8eaf6'); L.set(c + 3, 19, '#c4c6dc'); L.set(c - 1, 20, '#c4c6dc');
      L.set(13, 17, '#fdf0b0');
      break;
    }
  }
}

// a robot in its pose: s = { t, hit, level, note, ang, blink, pose, dip }. Its shape (2026-11-03): std as before; tall
// (raised 2, a narrower body, stilts), squat (lowered 2, a wide body, stumps, wide feet), round (an oval body), tracks (a
// tread on four wheels, its lugs moving), hover (no legs: a thruster, its flame with the level, bobbing). up: the upper
// body's shift, spread: the shoulders'
function robot(sp, s) {
  const L = new Layer(30, 34, 5, 2), S = R.steel, D = R.steelD, P = s.pose || POSES[0], ty = HEAD_TOP[sp.head], sh = sp.shape;
  const lean = P.lean || 0, nod = P.nod || 0, legs = sh !== 'tracks' && sh !== 'hover';
  const squat = legs ? Math.max(P.squat || 0, s.dip || 0) : Math.min(1, Math.max(P.squat || 0, s.dip || 0));
  const up = sh === 'tall' ? -2 : sh === 'squat' ? 2 : sh === 'hover' ? Math.round(Math.sin(s.t * .005)) - 1 : 0;
  const spread = sh === 'squat' ? 2 : sh === 'tall' ? -1 : sh === 'round' ? 1 : 0;
  L.ox += lean; L.oy += up + squat + nod;
  antenna(L, sp, s, ty);
  L.ox -= lean + spread; L.oy -= nod;
  arm(L, false, P.la, S);
  L.ox += 2 * spread;
  arm(L, true, P.la === 'down' && P.ra === 'down' ? Math.round(s.hit * 3) : P.ra, S);
  L.ox -= spread; L.oy -= squat + up;
  const lu = (s.walk ? s.walk === 2 : legs && P.lift === 1) ? 2 : 0, ru = (s.walk ? s.walk === 4 : legs && P.lift === 2) ? 2 : 0;
  if (sh === 'tall')                                     // stilts, small feet
    for (const [x, u] of [[7, lu], [11, ru]]) { rect(L, x, 23 + squat - u, 1, 6 - squat, S[3]); rect(L, x + 1, 23 + squat - u, 1, 6 - squat, S[1]); box(L, x - 2 + (x > 10 ? 2 : 0), 29 - u, 4, 2, D, false); }
  else if (sh === 'squat') {                             // stumps, wide flat feet
    box(L, 4, 27 + Math.min(squat, 1) - lu, 4, 1, S, false); box(L, 12, 27 + Math.min(squat, 1) - ru, 4, 1, S, false);
    box(L, 2, 28 - lu, 7, 2, D, false); box(L, 11, 28 - ru, 7, 2, D, false);
  } else if (sh === 'tracks') {                          // a tread round four wheels, its lugs moving
    box(L, 7, 24, 6, 2, S, false);
    box(L, 1, 26, 18, 5, D);
    const ph = Math.floor(s.t / 90) % 3;
    for (let x = 2; x < 18; x++) if ((x + ph) % 3 === 0) { L.set(x, 26, D[4]); L.set(19 - x, 30, D[0]); }
    for (const x of [4, 8, 12, 16]) ell(L, x, 28.5, 1.5, 1.5, S);
  } else if (sh === 'hover') {                           // a thruster, its flame
    box(L, 6, 24 + up, 8, 2, D, false); rect(L, 7, 26 + up, 6, 1, S[1]);
    const f = 3 + Math.round(s.level * 2 + Math.sin(s.t * .03));
    for (let k = 0; k < f; k++) { const w = Math.max(1, 3 - (k >> 1)); rect(L, 10 - w, 27 + up + k, w * 2, 1, k === 0 ? '#ffffff' : k < 2 ? R.teal[4] : k < 4 ? R.teal[3] : R.pink[3]); }
  } else {
    if (squat < 2) { box(L, 6, 25 + squat - lu, 3, 2 - squat, S, false); box(L, 11, 25 + squat - ru, 3, 2 - squat, S, false); }
    else { box(L, 3, 25, 4, 2, S, false); box(L, 13, 25, 4, 2, S, false); }
    box(L, 4, 27 - lu, 5, 3, D); box(L, 11, 27 - ru, 5, 3, D);
  }
  L.oy += squat + up; L.ox += lean;
  if (sh === 'round') { ell(L, 10, 19.5, 8, 6.3, sp.br); rect(L, 8, 12, 4, 1, S[1]); }
  else if (sh === 'squat') { box(L, 0, 14, 20, 10, sp.br); rect(L, 8, 13, 4, 1, S[1]); for (const [x, y] of [[1, 15], [18, 15], [1, 22], [18, 22]]) L.set(x, y, sp.br[4]); }
  else if (sh === 'tall') { box(L, 4, 14, 12, 11, sp.br); rect(L, 9, 12, 2, 2, S[2]); }
  else if (sh === 'hover') { box(L, 3, 14, 14, 10, sp.br); rect(L, 8, 13, 4, 1, S[1]); }
  else { box(L, 3, 14, 14, 11, sp.br); rect(L, 8, 13, 4, 1, S[1]); for (const [x, y] of [[4, 15], [15, 15], [4, 23], [15, 23]]) L.set(x, y, sp.br[4]); }
  emblem(L, sp, s);
  L.oy += nod;
  head(L, sp);
  eyes(L, sp, s);
  mouth(L, sp, s);
  if (sp.head === 'box' || sp.head === 'wide' || sp.head === 'tall') L.set(sp.head === 'wide' ? 16 : sp.head === 'tall' ? 13 : 14, ty + 1, '#ffffff');
  L.ox = 5; L.oy = 2;
  L.edge = edges(L);
  outline(L);
  return L;
}
// each pixel's edge before the outline: 1 nothing on its left, 2 on its right, 3 above, 0 none
function edges(L) {
  const g = (x, y) => x >= 0 && y >= 0 && x < L.w && y < L.h && L.p[y * L.w + x];
  return L.p.map((c, i) => { const x = i % L.w, y = (i / L.w) | 0; return !c ? 0 : !g(x - 1, y) ? 1 : !g(x + 1, y) ? 2 : !g(x, y - 1) ? 3 : 0; });
}

// ---- pixels
const rgbC = {};
const rgb = h => rgbC[h] || (rgbC[h] = [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)]);
function put(img, W, x, y, h) { if (x < 0 || y < 0 || x >= W || y >= img.height) return; const c = rgb(h), i = (y * W + x) * 4; img.data[i] = c[0]; img.data[i + 1] = c[1]; img.data[i + 2] = c[2]; img.data[i + 3] = 255; }
function tint(img, W, x, y, h, a) { if (x < 0 || y < 0 || x >= W || y >= img.height) return; const c = rgb(h), i = (y * W + x) * 4; for (let k = 0; k < 3; k++) img.data[i + k] = Math.round(img.data[i + k] * (1 - a) + c[k] * a); }

// ---- HOME: the street (firmware/src/ui_road.c, ported: keep the two alike). Art pixels at 2 x, 120 x 71; the road moves a
// pixel a 1/16, round a 360 px street; a day is dusk and night (720 px); the track's 16 steps are the road's centre-line
// dashes; the robot at 3 x. Road.frame draws a frame from the firmware's own inputs (the ROAD message: tablet.c)
const RD_LOOP = 360, RD_DAY = 720, RD_WX = 2520, RD_MR = 11, RD_MK = 57;
const N = h => parseInt(h.slice(1), 16);
const RD_SKY = [[0x04030c, 0x07061a, 0x0c0a26, 0x140e34, 0x1e1240, 0x2c164c, 0x3e1c58, 0x582466],
  [0x0c0824, 0x1a0c38, 0x34104c, 0x5c1660, 0x8e2268, 0xc23a66, 0xf2645a, 0xff9a5a]];
const RD_COLS = { FAR: [0x0e0a1e, 0x22143c], FAR_E: [0x141028, 0x2c1a4a], MID: [0x08060f, 0x170d2a], HAZE: [0x6a2a7a, 0xff5aa0],
  BLD0: [0x1a1430, 0x241840], BLD1: [0x161226, 0x1e1636], BLD2: [0x1e1636, 0x2a1c48] };
const NE_CY = 0x30f0ff, NE_PK = 0xff3caa, NE_VI = 0x9a5cff, NE_GR = 0x40ff9a;
const RD_NEON = [NE_CY, NE_PK, 0xffd23c, NE_VI, NE_GR], RD_VW = [0xff71ce, 0xb967ff, 0x01cdfe, 0x05ffa1, 0xfffb96];
const RD_WIN = [0xffb050, 0x6ad8ff, 0xff8ccc];
const RD_FILL = [[32, 18, 16], [88, 17, 20], [116, 26, 12], [212, 18, 22], [264, 20, 18], [286, 16, 26]];
const RD_CAR = [[-30, 30, 1], [-8, 22, -1], [8, 40, 1], [20, 18, -1]];
const RD_SLIDE = [-48, -24, 0, 24, 48];
const rd = { way: 0, now: 0, d: 0, hz: 0, vt: 0, sl: [0, 0], sky: [], pal: {}, car: [0, 0, 0, 0], sp: [[0, 0, 0, 0], [0, 0, 0, 0]],
  pulse: 0, vm: 0, vl: new Array(10).fill(0), stc: [], ext: 0, face: 1 };
const rdc = { ft: [], mt: [], fk: [], ml: [], mw: [], mir: Array.from({ length: RD_MR }, () => new Array(120).fill(0)) };
const plain = new Uint8Array(120 * 71);
const STARS = Array.from({ length: 70 }, (_, i) => ({ x: Math.floor(fr(Math.sin(i * 127.1) * 43758.5) * 120), y: Math.floor(fr(Math.sin(i * 311.7) * 24634.6) * 40), z: 1 + i % 3, ph: i * 1.37 }));
const fl = Math.floor, rnd = Math.round, ab = Math.abs;
const rdb = (x, y) => BAYER[y & 3][x & 3];
function tnt(c, l, a) {                                 // c towards l by a
  const ai = fl(a * 256); let o = 0;
  for (let k = 0; k < 24; k += 8) o |= ((((c >> k) & 255) * (256 - ai) + ((l >> k) & 255) * ai + 128) >> 8) << k;
  return o >>> 0;
}
const inBox = (x, y, x0, y0, w, h) => x >= x0 && x < x0 + w && y >= y0 && y < y0 + h;
const to565 = c => ((c >> 19) & 31) << 11 | ((c >> 10) & 63) << 5 | ((c >> 3) & 31);
const to888 = c => fl((c >> 11) * 255 / 31) << 16 | fl((c >> 5 & 63) * 255 / 63) << 8 | fl((c & 31) * 255 / 31);
const wxAt = x => (rd.way + x) % RD_WX, fxAt = x => (fl(rd.way / 4) + x) % 28000, mxAt = x => (fl(rd.way / 2) + x) % 27600;
function rdText(x, y, s, x0, y0, gap, down) {
  const G = 'EATSLPRY', F = [[7, 4, 6, 4, 7], [7, 5, 7, 5, 5], [7, 2, 2, 2, 2], [7, 4, 7, 1, 7], [4, 4, 4, 4, 7], [7, 5, 7, 4, 4], [7, 5, 6, 5, 5], [5, 5, 2, 2, 2]];
  for (const ch of s) {
    if (inBox(x, y, x0, y0, 3, 5)) return F[G.indexOf(ch) & 7][y - y0] >> (2 - (x - x0)) & 1;
    if (down) y0 += 6; else x0 += gap;
  }
  return 0;
}
function rdSky(x, y) {
  const F = Math.fround, k = fl(F(F(F(F((y + rd.ext) / (40 + rd.ext)) * 7) + F(F(rdb(x, y) - .5) * F(.9))) + .5));   // (as the firmware's floats)
  let c = rd.sky[clamp(k, 0, 7)];
  if (rd.d > 0) {                                       // dusk's sun, big, low, striped
    const dx = x + .5 - 56, dy = y + .5 - 24;
    if (dx * dx + dy * dy < 210 && !(y > 18 && (y - 18) % 4 >= 3 - fl((y - 18) / 6))) c = tnt(c, tnt(0xfff0a0, 0xff3c8c, (y - 10) / 29), rd.d);
  }
  if (y > 26) c = tnt(c, rd.pal.HAZE, (y - 26) / 18 * rd.hz);
  for (let i = 0; rd.d < .5 && i < 2; i++) {            // by night the searchlights
    const ddx = x + .5 - (i ? 96 : 22), ddy = 44 - y, e = ddx - ddy * rd.sl[i];
    if (ddy > 0 && ab(e) < ddy * (.035 + .0006 * ddy)) c = tnt(c, 0x8ae0ff, .18 * (1 - ddy / 100) * (1 - 2 * rd.d));
  }
  return c;
}
function rdFar(x, y, c) {                               // the megacity: far towers, near ones, the elevated rail and its train
  const ft = rdc.ft[x], fx = fxAt(x), mt = rdc.mt[x], ml = rdc.ml[x], mw = rdc.mw[x], mx = mxAt(x), mb = fl(mx / 23), fk = rdc.fk[x];
  if (y >= ft) {
    c = fk & 1 ? rd.pal.FAR_E : rd.pal.FAR;
    if ((fk & 2) && !(y & 1) && hsh(fx * 31 + y * 7) % 5 === 0) c = tnt(rd.pal.FAR, rd.d > .5 ? 0xffb070 : RD_NEON[hsh(fx + y) % 3], .55);
  } else if ((fk & 4) && y >= ft - 5)
    c = y === ft - 5 && (fl(rd.now / 600) + fl(fx / 7)) % 3 === 0 ? 0xff3040 : rd.pal.FAR;
  if (ml < mw && y >= mt) {
    const k = rd.pal.MID;
    c = k;
    if (ml === 0 || ml === mw - 1) c = tnt(k, RD_NEON[mb % 5], .65 - .25 * rd.d);
    else if (y % 3 === 0 && (ml & 1) && hsh(mx * 13 + y * 5) % 4 === 0) c = tnt(k, hsh(mb) & 1 ? 0x8ac8ff : 0xffb4d8, .35);
    if (mb % 4 === 1 && inBox(ml, y, 2, mt + 6, mw - 4, 12)) {   // a hologram: a diamond pulsing, scanlines
      const e = ab(ml - fl(mw / 2)) + ab(y - mt - 12), r = 2 + (fl(rd.now / 300) + mb) % 3;
      c = tnt(k, RD_NEON[(fl(mb / 4) + fl(rd.now / 2000)) % 5], (e <= r && e >= r - 1 ? .85 : .25) * (y & 1 ? .6 : 1));
    }
  } else if (ml === fl(mw / 2) && y < mt && y >= mt - (mt < 0 ? 9 : 3))
    c = y === mt - 9 && fl(rd.now / 500) & 1 ? 0xff3040 : rd.pal.MID;
  if (y >= 34 && y <= 36) {
    c = y === 34 ? 0x2a2440 : 0x120e20;
    if (y === 35 && mx % 6 === 0) c = rd.d > .5 ? 0x6aa0c0 : NE_CY;
  }
  if (y > 36 && mx % 40 < 2) c = 0x0e0a18;
  const tr = fl(rd.now / 20) % 400 - 140;
  if (y >= 31 && y <= 33 && x >= tr && x < tr + 34) c = y === 31 ? 0x3a3458 : (x - tr) % 4 < 2 && y === 32 ? NE_CY : 0x221c36;
  return c;
}
function rdAir(x, y, c) {                               // the flying cars, the dogfights
  const n = rd.now;
  for (let i = 0; i < 4; i++) {
    const dy = y - RD_CAR[i][0], dx = x - rd.car[i];
    if (dy === 0 && dx >= -3 && dx <= 3) c = dx === 3 * RD_CAR[i][2] ? NE_CY : dx === 3 || dx === -3 ? 0xff3040 : 0x1a1628;
    else if (dy === 1 && dx >= -1 && dx <= 1) c = 0x0c0a14;
    else if (dy === 2 && dx >= -2 && dx <= 2) c = tnt(c, NE_VI, .4);
  }
  for (let i = 0; i < 2; i++) {
    const [tx, ty, fx, fy] = rd.sp[i];
    if ((y < ty - 2 && y < fy - 2) || (y > ty + 2 && y > fy + 2)) continue;
    let dx = x - tx, dy = y - ty;
    if ((dy === 0 && dx >= -2 && dx <= 2) || ((dy === 1 || dy === -1) && (dx === -2 || dx === -1))) c = dx === 2 ? 0xff3040 : 0x5a1a2a;
    if (dy === 0 && dx === -3) c = 0xffb050;
    dx = x - fx; dy = y - fy;
    if (dy === 0 && dx >= -2 && dx <= 2) c = dx === 1 ? 0xc8fcff : 0x3a4a6a;
    if ((dy === 1 || dy === -1) && dx >= -2 && dx <= 0) c = NE_CY;
    if (dy === 0 && dx === -3) c = NE_VI;
    for (let k = 0; k < 3; k++) {
      const bx = fx + 4 + (fl(n / 8) + k * 7) % 20;
      if (y === fy && x >= bx && x < bx + 3 && bx + 3 < tx - 2) c = 0x80ffff;
    }
    const rx = tx - 4 - (fl(n / 10) + 5) % 18;
    if (y === ty + 1 && x > rx - 2 && x <= rx && rx > fx + 3) c = 0xff5050;
    if (fl(n / 8) % 20 > 16 && ab(x - tx + 2) + ab(y - ty) <= 2) c = tnt(c, 0xfff0a0, .8);
  }
  return c;
}
function rdViz(lx, ly) {                                // the club's screen: bars, a sunset over a grid, two waves
  const t = rd.now & 0xFFFFF;
  if (rd.vm === 0) {
    const e = ly < 11 ? 10 - ly : ly - 11, h = rd.vl[fl(lx / 4)];
    if (lx % 4 === 3) return 0x0c0616;
    return e < h ? RD_VW[e < 4 ? 0 : e < 8 ? 1 : 2] : ly === 10 || ly === 11 ? 0x2a1040 : 0x120820;
  }
  if (rd.vm === 1) {
    if (ly < 11) {
      const dx = lx - 19.5, dy = ly - 10.5, r = 6 + rd.vt * 3;
      if (dx * dx + dy * dy < r * r && !(ly > 5 && (ly - 5) % 3 === 2)) return tnt(RD_VW[4], RD_VW[0], ly / 10);
      return tnt(0x1a0830, 0x4a1060, ly / 10);
    }
    const r = ly - 10, u = (lx - 19.5) * 3 / r, z = 6 / r + t * .0012 * (1 + rd.vt), fu = u - rnd(u), fz = z - fl(z);
    if (ab(fu) < .06 * r || fz < .16) return tnt(RD_VW[2], RD_VW[0], fz < .16 ? .6 : 0);
    return 0x14062a;
  }
  const a = 1.5 + rd.vt * 8, p = lx * .35;
  const ya = ab(10.5 + a * Math.sin(p + t * .006) * Math.sin(lx * .08 + t * .0013) - ly - .5), yb = ab(10.5 + a * .7 * Math.sin(p * 1.3 - t * .005 + 1) - ly - .5);
  if (ya < .8) return RD_VW[0];
  if (yb < .8) return RD_VW[2];
  if (ya < 1.8) return tnt(0x100820, RD_VW[0], .35);
  if (yb < 1.8) return tnt(0x100820, RD_VW[2], .35);
  return ly === 10 ? 0x1c0c30 : 0x100820;
}
function rdFill(sx, y, c, x0, w, top, k) {              // a building in between
  const lx = sx - x0;
  if (!inBox(sx, y, x0, top, w, 45 - top)) return c;
  c = k % 3 === 0 ? rd.pal.BLD0 : k % 3 === 1 ? rd.pal.BLD1 : rd.pal.BLD2;
  if (y === top) c = 0x3a2e5a;
  else if (lx % 3 === 1 && (y - top) % 3 === 2 && y < 38 && hsh(sx * 17 + y * 3 + k) % 4 === 0) c = RD_WIN[hsh(sx + y) % 3];
  if (inBox(lx, y, w - 5, top + 4 + k % 5, 3, 2)) c = y & 1 ? 0x4a4a64 : 0x5a5c78;
  if (lx === 1 + k % 3 && y > top) c = 0x2a2440;
  if (y >= 39 && lx >= 1 && lx < w - 1) c = k & 1 ? (y & 1 ? 0x3a3450 : 0x2a2640) : y < 42 ? 0xffd080 : 0xc88a40;
  if ((k & 1) && inBox(lx, y, 2, top + 2, 3, 4)) c = hsh(lx + y * 3 + k) % 3 ? RD_NEON[k % 5] : tnt(RD_NEON[k % 5], 0, .6);
  return c;
}
function rdStreet(sx, y, c) {                           // EAT, SLEEP, PARTY, REPEAT and the buildings between
  for (let k = 0; k < 6; k++) c = rdFill(sx, y, c, RD_FILL[k][0], RD_FILL[k][1], RD_FILL[k][2], k);
  if (inBox(sx, y, 0, 22, 28, 23)) {                    // the noodle bar
    c = (sx + fl(y / 3)) % 9 ? 0x1a1230 : 0x221840;
    if (inBox(sx, y, 2, 24, 24, 6) && (sx - 2) % 6 < 4 && (y - 24) % 5 < 3) c = hsh(fl(sx / 6) * 7 + fl(y / 5)) % 3 ? 0x6ad8ff : 0x281e44;
    if (y === 31) c = (sx + fl(rd.now / 120)) % 4 ? NE_CY : 0x186878;
    if (y === 32 || y === 33) c = fl(sx / 2) % 2 ? NE_CY : 0x1a4a5a;
    if (inBox(sx, y, 3, 35, 22, 10)) c = y < 40 ? 0xffb050 : 0x8a4a28;
    if (inBox(sx, y, 6, 36, 3, 3) || inBox(sx, y, 18, 36, 3, 3)) c = 0xff3040;
  }
  if (sx === 14 && y === 21) c = 0x2a2c3a;
  if (inBox(sx, y, 8, 14, 13, 7)) c = rdText(sx, y, 'EAT', 9, 15, 4, 0) ? rd.pulse || rd.d > .5 ? NE_PK : 0xa02060 : 0x140c22;
  if (inBox(sx, y, 53, 30, 7, 15)) {                    // the vending machines
    c = inBox(sx, y, 53, 32, 7, 7) && sx !== 56 ? sx < 56 ? 0x2a6aff : NE_PK : 0x101828;
    if (y === 41 && sx !== 56) c = NE_CY;
  }
  if (inBox(sx, y, 62, 22, 23, 23)) {                   // the capsule hotel
    c = 0x181024;
    if (inBox(sx, y, 64, 24, 19, 20) && (y - 24) % 4 < 3 && (sx - 64) % 6 < 5) c = hsh(fl((sx - 64) / 6) + fl(y / 4) * 5) % 4 === 0 ? NE_VI : 0x2a1e40;
  }
  if (inBox(sx, y, 62, 13, 23, 9)) c = rdText(sx, y, 'SLEEP', 64, 15, 4, 0) ? NE_VI : y === 13 || y === 21 ? 0x5a3aa0 : 0x100a1a;
  if (inBox(sx, y, 110, 34, 2, 11)) c = 0x2a2c3a;      // the traffic light
  if (inBox(sx, y, 109, 26, 4, 9)) {
    const ON = [0xff4040, 0xffc040, 0x40ff80], j = fl((y - 27) / 3);
    c = (sx === 110 || sx === 111) && y >= 27 && (y - 27) % 3 === 0 && j < 3 ? fl(rd.now / 3000) % 3 === 2 - j ? ON[j] : tnt(0x14141c, ON[j], .12) : 0x14141c;
  }
  if (inBox(sx, y, 146, 8, 62, 37)) {                   // the club
    c = (sx - 146) % 8 ? 0x120c20 : 0x1c1430;
    if (inBox(sx, y, 150, 10, 44, 26)) c = inBox(sx, y, 152, 12, 40, 22) ? rdViz(sx - 152, y - 12) : rd.pulse ? NE_PK : 0x9a2a7a;
    if (inBox(sx, y, 154, 37, 36, 8)) c = (sx - 154) % 9 === 0 || (sx - 154) % 9 === 8 ? 0x2a1a30 : y > 39 ? 0xff70c8 : 0xb04a8a;
  }
  if (inBox(sx, y, 197, 0, 9, 34))                      // PARTY down its sign
    c = rdText(sx, y, 'PARTY', 200, 2, 0, 1) ? !rd.pulse && rd.d < .5 ? 0xffa0d8 : NE_PK : sx === 197 || sx === 205 || y === 0 || y === 33 ? NE_PK : 0x1a0820;
  if (inBox(sx, y, 234, 20, 26, 25)) {                  // the arcade, REPEAT
    const SC = [NE_CY, NE_PK, NE_VI];
    c = 0x160e26;
    if (inBox(sx, y, 236, 30, 22, 9)) c = (sx - 236) % 4 < 3 && y < 37 ? SC[(fl(sx / 4) + fl(rd.now / 300)) % 3] : 0x0a0814;
    if (inBox(sx, y, 240, 39, 14, 6)) c = y < 42 ? 0x6ad8ff : 0x3a6a8a;
    if (y < 28) c = rdText(sx, y, 'REPEAT', 236, 21, 4, 0) ? NE_GR : y === 20 || y === 27 ? 0x1a6a44 : 0x0e1a14;
  }
  if (inBox(sx, y, 304, 38, 50, 7) && (y === 38 || ((sx + y) % 3 === 0 && (sx - y) % 3 === 0))) c = 0x3a3458;
  return c;
}
function rdRoad(x, y) {                                 // wet, mirroring the street, its puddles catching the neon near the front
  const b = rdb(x, y), wx = wxAt(x);                    // (2026-11-04: no kerb, no pavement; the markings: rdMarks)
  if (y === 45) return 0x6a5aa8;                        // (2026-11-05: the kerb's lit edge, the gutter, the asphalt cooler)
  if (y === 46) return 0x07050c;
  let c = b > .6 ? 0x1a1a28 : 0x141420;
  if (y <= 56 && (x + y * 3) % 4) {
    const mx = x + (y % 3 === 0) - (y % 5 === 0), m = rdc.mir[56 - y][clamp(mx, 0, 119)];
    if ((m >> 11) * 77 * 8 + (m >> 5 & 63) * 151 * 4 + (m & 31) * 28 * 8 > 89 * 256) c = tnt(c, to888(m), .42 - (y - 46) * .018);
  }
  if (y > 63 && hsh(fl(wx / 5) * 13 + y) % 11 === 0) c = tnt(c, wx % 40 < 20 ? NE_PK : NE_CY, .18);
  return c;
}
function rdPx(x, y) {
  if (y >= 45) return rdRoad(x, y);
  const c0 = rdSky(x, y), c = rdStreet(wxAt(x) % RD_LOOP, y, rdAir(x, y, rdFar(x, y, c0)));
  plain[y * 120 + x] = c === c0;
  return c;
}
function rdTime(u) {                                    // dusk held, a turn to night, night held, a turn back
  let d = u < .4 ? 1 : u < .5 ? (.5 - u) * 10 : u < .9 ? 0 : (u - .9) * 10;
  d = d * d * (3 - 2 * d);
  rd.d = d; rd.hz = .4 - .15 * d;
  for (let k = 0; k < 8; k++) rd.sky[k] = tnt(RD_SKY[0][k], RD_SKY[1][k], d);
  for (const n in RD_COLS) rd.pal[n] = tnt(RD_COLS[n][0], RD_COLS[n][1], d);
}
function rdMovers(now, lv) {                            // lv: each track's level 0..256 (null: the splash, on its own)
  const t = now & 0xFFFFF, kick = 1 - now % 500 / 300;
  rd.sl = [.55 * Math.sin(t * .0007), .55 * Math.sin(t * .0007 + 2.1)];
  for (let i = 0; i < 4; i++) { const px = (fl(now / RD_CAR[i][1]) + i * 53) % 170 - 25; rd.car[i] = RD_CAR[i][2] > 0 ? px : 120 - px; }
  for (let i = 0; i < 2; i++) {
    const ph = i * 440, cy = i ? -34 : 5, tx = (fl(now / 25) + ph) % 880 - 40, a = t * .002 + ph;
    rd.sp[i] = [tx, cy + rnd(2 * Math.sin(a)), tx - 24, cy + rnd(2 * Math.sin(a - .8))];
  }
  const top = lv ? Math.max(0, ...lv.map(v => v / 256)) : 0;
  for (let i = 0; i < 10; i++) {
    const w = Math.sin(t * .006 * (1 + i * .37) + i * 1.7) * .5 + .5;
    const a = lv ? (.55 * lv[fl(i * lv.length / 10)] / 256 + .45 * top) * (.7 + .3 * w) + .06 * w : w * (.55 + .45 * Math.max(kick, 0) * (i < 3 ? 1 : .4));
    rd.vl[i] = clamp(rnd(a * 11), 0, 11);
  }
  rd.vt = lv ? top : .6;
}
function rdCols(way) {                                  // each column's towers (a quarter and a half of the way)
  rd.way = way;
  for (let x = 0; x < 120; x++) {
    const fx = fxAt(x), mx = mxAt(x), fb = fl(fx / 7), fl7 = fx % 7, fw = 4 + hsh(fb) % 4, mb = fl(mx / 23);
    rdc.ft[x] = fl7 < fw ? 20 + hsh(fb * 3 + 1) % 14 : 99;
    rdc.fk[x] = (fl7 === 0) | (fl7 & 1) << 1 | (fl7 === fl(fw / 2) && hsh(fb) % 3 === 0) << 2;
    rdc.mw[x] = 8 + hsh(mb * 5) % 6; rdc.ml[x] = mx % 23;
    rdc.mt[x] = mb % 5 === 2 ? -22 : 8 + hsh(mb * 7 + 2) % 14;
  }
}
function rdMarks(f, fill) {                             // the markings under the robot's feet (ui_road.c rd_marks): art rows
  const way = f.way, dy = f.dy;                         // RD_MK, RD_MK + 1, straight dashes (a ring of 120 art px)
  const mark = (a, w, c) => { fill(2 * a, dy + 2 * RD_MK, 2 * w, 4, c); if (a + w > 120) fill(2 * a - 240, dy + 2 * RD_MK, 2 * w, 4, c); };
  if (!rd.face) { for (let a = (14 - way % 14) % 14 - 14; a < 120; a += 14) mark(a, 7, 0x1aa0b0); return; }
  for (let s = 0; s < f.sk.length; s++)                 // TRACKS: the steps, sk each one's colour + 1 in rd.stc
    mark((1 + fl(s * 120 / f.n) + 120 - way % 120) % 120, f.n === 16 ? 5 : 2, rd.stc[f.sk[s] - 1]);
}

// lit's colour as the firmware makes it (ui_robot.c rb_lit: mixed in RGB565), as 0xRRGGBB; -1 nothing
function lit565(L, x, y) {
  const c = L.p[y * L.w + x], k = c && c !== INK && L.edge ? L.edge[y * L.w + x] : 0;
  if (!c) return -1;
  let v = to565(N(c));
  if (k) {
    const K = [0x2F1C, 0xF039, 0xFFFF][k - 1], a = k === 3 ? 1 : 2, m = (sh, w) => (((v >> sh & w) * (4 - a) + (K >> sh & w) * a + 2) >> 2) << sh;
    v = m(11, 31) | m(5, 63) | m(0, 31);
  }
  return to888(v);
}
// ---- a frame (ui_road.c rd_draw, rd_robot): f = { way, now, bp, dy, sh, pulse, vm, lv, face, n, sk, stc, robots }
function frame(put, f) {
  const { way, now, bp, dy, sh } = f;
  const fill = (x, y, w, h, c) => {
    for (let j = Math.max(0, y); j < Math.min(sh, y + h); j++) for (let i = Math.max(0, x); i < Math.min(240, x + w); i++) put(i, j, c);
  };
  rd.now = now;
  rdTime(way % RD_DAY / RD_DAY);
  rd.pulse = f.pulse;
  rdMovers(now, f.lv);
  rd.vm = f.vm;
  rdCols(way);
  rd.ext = dy > 0 ? (dy + 1) >> 1 : 0;
  rd.face = f.face;
  if (rd.face) rd.stc = f.stc;
  for (let y = dy > 0 ? -((dy + 1) >> 1) : 0; y < 71; y++) {
    const sy = dy + 2 * y, off = sy + 2 <= 0 || sy >= sh;
    if (off && (y < 33 || y >= 33 + RD_MR)) continue;   // (the rows the road mirrors: always)
    const row = [];
    for (let x = 0; x < 120; x++) row.push(rdPx(x, y));
    if (y >= 33 && y < 33 + RD_MR) rdc.mir[y - 33] = row.map(to565);
    if (off) continue;
    for (let x = 0; x < 120; x++) fill(2 * x, sy < 0 ? 0 : sy, 2, sy < 0 ? 1 : sy + 2 > sh ? sh - sy : 2, row[x]);
  }
  rdMarks(f, fill);
  if (rd.face) { fill(0, sh - 2, 240, 1, 0x07050c); fill(0, sh - 1, 240, 1, 0x6a5aa8); }   // the scene's foot over the knobs
  const isPlain = (x, y) => (rdPx(x, y), y < 45 && plain[y * 120 + x]);
  for (let i = 0; rd.d < .34 && i < STARS.length; i++) {   // the night's stars, brighter on the beat
    const s = STARS[i];
    if (dy + 2 * s.y < 0 || !isPlain(s.x, s.y) || !isPlain(s.x, s.y + 1)) continue;
    const tw = ((Math.sin((now & 0xFFFFF) * .0022 * s.z + s.ph) * .5 + .5) * .85 + bp * .25) * (1 - rd.d * 3);
    if (tw < .35) continue;
    fill(s.x * 2, dy + s.y * 2, 2, 2, tw > .85 ? 0xffffff : tw > .6 ? 0xb4b0e4 : 0x56528c);
    if (s.z === 3 && tw > .9) for (const [ddx, ddy] of [[-1, 0], [2, 0], [0, -1], [0, 2]]) fill(s.x * 2 + ddx, dy + s.y * 2 + ddy, 1, 1, 0x6a66a8);
  }
  for (const r of f.robots) {                           // each at q / 4 x (r.sc: quarters), its shadow; tipped over a planted foot
    const q = r.sc, rx = fl(28 * q / 12), cx = r.cx, fy = r.fy, s = r.s, P = POSES[s.pose] || POSES[0], at = v => fl(v * q / 4);
    for (let y = fy - 3; y < fy + 3; y++) for (let x = cx - rx - 1; x < cx + rx + 1; x++) {
      const ex = (x + .5 - cx) / rx, ey = (y + .5 - fy) / 2.6, d = Math.sqrt(ex * ex + ey * ey);
      if (d < 1 && (d < .7 || (x + y) & 1)) fill(x, y, 1, 1, 0x08060f);
    }
    const L = robot(ROBOTS[r.ri], { t: s.t, hit: s.hit, level: s.level, note: s.note, ang: s.ang, blink: s.blink, pose: P, dip: s.dip, walk: s.walk });
    const lift = s.walk ? 0 : P.lift || 0, shear = lift === 1 ? .17 : lift === 2 ? -.17 : 0;
    const y0 = fy - at(32) - rnd(s.hit * .375 * q), piv = y0 + at(33), x0 = cx - at(15);
    for (let ly = 0; ly < L.h; ly++) for (let lx = 0; lx < L.w; lx++) {
      const c = lit565(L, lx, ly);
      if (c < 0) continue;
      for (let sy = y0 + at(ly); sy < y0 + at(ly + 1); sy++) {
        const ox = shear ? rnd((piv - sy) * shear) : 0;
        fill(x0 + ox + at(lx), sy, at(lx + 1) - at(lx), 1, c);
      }
    }
  }
}

// ---- the ROAD message (little-endian): way u32, now u32, bp f32, dy i8, sh u8, flags u8 (1 pulse, 2 face), vm u8,
// nl u8 + nl levels u16 (nl 0: none), n u8 (16 / 32), ns u8 + ns steps' colours + 1 (a nibble each, high first),
// stc 6 x RGB (n, ns, stc only with face), nr u8 + nr robots: ri u8, cx i16, fy i16, sc u8, t hit level ang f32,
// note i16, blink pose dip walk u8 (ui_road.c rdm)
function decode(b, at, n) {
  const v = new DataView(b.buffer, b.byteOffset + at, n);
  let p = 0;
  const u8 = () => v.getUint8(p++), i8 = () => v.getInt8(p++);
  const u16 = () => (p += 2, v.getUint16(p - 2, true)), i16 = () => (p += 2, v.getInt16(p - 2, true));
  const u32 = () => (p += 4, v.getUint32(p - 4, true)), f32 = () => (p += 4, v.getFloat32(p - 4, true));
  try {
    const f = { way: u32(), now: u32(), bp: f32(), dy: i8(), sh: u8() }, fl8 = u8();
    f.pulse = fl8 & 1; f.face = fl8 >> 1 & 1; f.vm = u8();
    const nl = u8();
    f.lv = nl ? Array.from({ length: nl }, u16) : null;
    if (f.face) {
      f.n = u8();
      const ns = u8(), q = [];
      for (let i = 0; i < ns; i += 2) { const x = u8(); q.push(x >> 4); if (i + 1 < ns) q.push(x & 15); }
      f.sk = q;
      f.stc = Array.from({ length: 6 }, () => u8() << 16 | u8() << 8 | u8());
    }
    f.robots = [];
    for (let k = u8(); k > 0; k--) {
      const r = { ri: u8(), cx: i16(), fy: i16(), sc: u8() };
      r.s = { t: f32(), hit: f32(), level: f32(), ang: f32(), note: i16(), blink: u8(), pose: u8(), dip: u8(), walk: u8() };
      if (r.ri >= ROBOTS.length) return null;
      f.robots.push(r);
    }
    return p === n ? f : null;
  } catch (e) {
    return null;                                        // (too short)
  }
}

G.Road = { R, INK, ROBOTS, POSES, HEAD_TOP, Layer, box, ell, rect, arm, head, robot, outline, lit, poseAt, hsh, fr, put,
           rgb, N, tnt, RD_SLIDE, RD_DAY, frame, decode };
})(typeof window !== 'undefined' ? window : globalThis);
