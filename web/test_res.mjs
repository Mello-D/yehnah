// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mello-D
// (chord-harp fork, 2026-11-04) fm1res.js against a simulated device (firmware/src/editor_res.c, res.c): the region
// written when missing, kept when it is this build's, refused for another build's file; firmware without 82 skipped.
import { installResources, resHeader } from "./fm1res.js";
import { bkU32, bkCrc } from "./fm1backup.js";
import { readFileSync } from "node:fs";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const athrows = async (f) => { try { await f(); return false; } catch { return true; } };

function unpackGroups(a) {                          // editor.c ed_unpack7: msb byte, then up to 7 bytes
  const out = [];
  for (let i = 0; i < a.length;) { const m = a[i++]; for (let j = 0; j < 7 && i < a.length; j++) out.push(a[i++] | ((m >> j) & 1) << 7); }
  return out;
}
function device(region, installed) {
  const h = resHeader(region), d = { flash: new Uint8Array(h.length).fill(0xff), installed, open: false, log: [] };
  const reply = (rc) => [rc, d.installed ? 1 : 0, ...bkU32(h.length), ...bkU32(h.crc)];
  d.request = async ([cmd, a]) => {
    if (cmd !== 82) throw new Error("unexpected");
    d.log.push(a[0]);
    if (a[0] === 0) return reply(0);
    if (a[0] === 1) { d.installed = false; d.open = true; d.flash.fill(0xff); return reply(0); }
    if (a[0] === 2) {
      const off = a[1] | a[2] << 7 | a[3] << 14, b = unpackGroups(a.slice(4));
      if (!d.open || off + b.length > h.length) return reply(1);
      d.flash.set(b, off); return reply(0);
    }
    if (a[0] === 3) { d.open = false; d.installed = bkCrc(d.flash) === h.crc; return reply(d.installed ? 0 : 7); }
    return reply(1);
  };
  return d;
}
const path = process.argv[2] || "build/yehnah-res.bin";
const region = new Uint8Array(readFileSync(path)), h = resHeader(region);
ok(h && h.length + 32 === region.length && bkCrc(region.subarray(32)) === h.crc, "the build's -res.bin: header (MRS1, length, CRC-32) over its payload");
{
  const d = device(region, false);
  const how = await installResources(d.request, region);
  ok(how === "written" && d.installed && d.flash.every((v, i) => v === region[32 + i]), "missing: begin, the payload in chunks, commit; the device holds it");
}
{
  const d = device(region, true);
  ok(await installResources(d.request, region) === "kept" && d.log.join() === "0", "already this build's: only the status asked, nothing erased");
}
{
  const d = device(region, false), other = region.slice(); other[40] ^= 1;
  const v = new DataView(other.buffer); v.setUint32(8, bkCrc(other.subarray(32)), true);
  ok(await athrows(() => installResources(d.request, other)) && d.log.join() === "0", "another build's file refused before anything is erased");
}
ok(await installResources(async () => { throw new Error("timeout"); }, region) === null, "firmware without command 82 (no answer): skipped");
console.log(fails ? "resource web tests FAILED" : "resource web tests passed");
process.exit(fails ? 1 : 0);
