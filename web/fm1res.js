// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Copyright (C) 2026 Mello-D
// (chord-harp fork, 2026-11-04, after Melodee's fm1res.js) the resource region (firmware/src/res.c): the update loader
// writes it from the package; when it could not (a cut, a transfer error), editor command 82 does, from the build's
// <package>-res.bin (tools/build.py: a 32-byte header (magic MRS1, length, CRC-32, ..., header CRC), then the payload).
// 82: [0] status, [1] begin, [2, offset in 3 x 7 bits, packed bytes] write, [3] commit; reply rc, installed, this
// build's length and CRC-32 (5 x 7 bits each).
import { bkPack, bkR32 } from "./fm1backup.js";

export const RES_COMMAND = 82;
const RES_CHUNK = 256;
const RES_MAGIC = 0x3153524d;

// the reply's fields, or null when it is not one
export function resStatus(a) {
  if (!a || a.length < 12) return null;
  return { rc: a[0], installed: a[1] === 1, length: bkR32(a, 2), crc: bkR32(a, 7) };
}
// a -res.bin's header: { length, crc } or null when it is not one
export function resHeader(region) {
  if (!region || region.length < 32) return null;
  const v = new DataView(region.buffer, region.byteOffset, 32);
  if (v.getUint32(0, true) !== RES_MAGIC) return null;
  const length = v.getUint32(4, true);
  return length && region.length >= 32 + length ? { length, crc: v.getUint32(8, true) } : null;
}
const resFail = (what, rc) => { const e = new Error(`resources: ${what} (rc ${rc})`); e.code = "res"; e.rc = rc; return e; };

// request: (args, options) -> reply bytes (BackupConnection.request). Returns "kept" (the device already has this
// build's), "written", or null (no answer to 82: firmware without a resource region); throws on a failed write
export async function installResources(request, region, onProgress = () => {}) {
  const head = resHeader(region);
  if (!head) throw resFail("not a resource file", -1);
  const ask = async (args, timeout = 3000) => resStatus(await request([RES_COMMAND, args], { timeout, retries: 0 }));
  let st = null;
  for (let i = 0; i < 4 && !st; i++) {               // (just restarted: the splash may hold the answer back)
    try { st = await ask([0], 1500); } catch (_) { await new Promise((r) => setTimeout(r, 1000)); }
  }
  if (!st) return null;
  if (st.length !== head.length || st.crc !== head.crc) throw resFail("the file is for another build", -2);
  if (st.installed) return "kept";
  st = await ask([1], 20000);                        // (erases the region: up to ~1 s)
  if (st.rc) throw resFail("begin", st.rc);
  const body = region.subarray(32, 32 + head.length);
  for (let off = 0; off < body.length; off += RES_CHUNK) {
    const chunk = body.subarray(off, off + RES_CHUNK);
    st = await ask([2, off & 127, (off >> 7) & 127, (off >> 14) & 127, ...bkPack(chunk)]);
    if (st.rc) throw resFail(`write at ${off}`, st.rc);
    onProgress(off + chunk.length, body.length);
  }
  st = await ask([3], 5000);
  if (st.rc || !st.installed) throw resFail("commit", st.rc);
  return "written";
}
