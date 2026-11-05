// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mello-D
// (the big FM-1, docs/plans/BIG-FM1-PLAN.md step 4) HOME's scene on the tablet: web/road/road.js (Road.decode,
// Road.frame) against the firmware's own (ui_road.c), on the ROAD messages tests/tablet_screen_test.c recorded with
// the scene the emulator drew for each (u16 n, n bytes, 240 x 138 RGB565 as web_fb holds it: byte-swapped).
//   node tests/tablet_road.mjs RECORD [SHEET.ppm]
// Prints the pixels that differ per frame; exit 1 when a frame is undecodable or differs in more than LIMIT pixels
// (the page's doubles and Math.sin against the firmware's floats and fm_sinf: a threshold's pixel here and there).
// SHEET: the worst frame, the firmware's | road.js's | the differences (white), 2 x.
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';

const here = path.dirname(new URL(import.meta.url).pathname);
createRequire(import.meta.url)(path.join(here, '../web/road/road.js'));
const Road = globalThis.Road;
const LIMIT = 40;                                        // (of 33,120; 2026-11-03: the worst 16, 1.3 a frame)
const rec = fs.readFileSync(process.argv[2]), sheet = process.argv[3];
const W = 240, H = 138, to565 = c => ((c >> 19) & 31) << 11 | ((c >> 10) & 63) << 5 | ((c >> 3) & 31);
let p = 0, frames = 0, bad = 0, total = 0, worst = { d: -1 };
while (p + 2 <= rec.length) {
  const n = rec[p] | rec[p + 1] << 8, f = Road.decode(rec, p + 2, n);
  const fw = new Uint16Array(W * H);
  for (let i = 0; i < W * H; i++) { const q = p + 2 + n + 2 * i, v = rec[q] | rec[q + 1] << 8; fw[i] = (v >> 8 | v << 8) & 0xFFFF; }
  p += 2 + n + 2 * W * H;
  frames++;
  if (!f || f.sh !== H) { console.log(`frame ${frames}: undecodable`); bad++; continue; }
  const js = new Uint16Array(W * H);
  Road.frame((x, y, c) => { js[y * W + x] = to565(c); }, f);
  let d = 0;
  for (let i = 0; i < W * H; i++) d += js[i] !== fw[i];
  total += d;
  if (d > LIMIT) bad++;
  if (d > worst.d) worst = { d, frames, fw, js, f };
}
console.log(`tablet_road: ${frames} frames, ${total} px differ in all (${(total / Math.max(1, frames)).toFixed(1)} a frame), ` +
            `the worst ${worst.d} (frame ${worst.frames}: way ${worst.f?.way}, robot ${worst.f?.robots[0]?.ri}), ${bad} over ${LIMIT}`);
if (sheet && worst.fw) {
  const S = 2, o = Buffer.alloc(3 * W * 3 * H * S * S), put = (x, y, v) => {
    for (let j = 0; j < S; j++) for (let i = 0; i < S; i++) {
      const k = 3 * ((y * S + j) * 3 * W * S + x * S + i);
      o[k] = (v >> 11) * 255 / 31; o[k + 1] = (v >> 5 & 63) * 255 / 63; o[k + 2] = (v & 31) * 255 / 31;
    }
  };
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const i = y * W + x;
    put(x, y, worst.fw[i]); put(W + x, y, worst.js[i]); put(2 * W + x, y, worst.fw[i] === worst.js[i] ? 0 : 0xFFFF);
  }
  fs.writeFileSync(sheet, Buffer.concat([Buffer.from(`P6 ${3 * W * S} ${H * S} 255\n`), o]));
}
process.exit(frames > 0 && !bad ? 0 : 1);
