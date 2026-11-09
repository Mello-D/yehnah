#!/usr/bin/env python3
"""Put the CPU test song (gen_cpu_song's sec_A..D.bin + chain.bin) into SONG 6 of a Yehnah backup file.
Everything else in the backup (settings, songs 1..5, presets) stays byte for byte; only song 6's four sections
(archive ids 60..63) and song 6's row in the song index (id 10) change.
  inject_song.py GENDIR IN_BACKUP.json OUT_BACKUP.json
Make IN_BACKUP.json with the installer's BACKUP, restore OUT_BACKUP.json with RESTORE (it writes ALL objects,
so keep IN_BACKUP.json as the way back)."""
import base64, json, struct, sys, zlib

SONG, ROW_OFF, ROW_LEN = 5, 8, 36          # song 6 (index 5); song_index_t: magic u32, version, cur, rsv[2], rows[6] x 36
gen, src, dst = sys.argv[1:4]
f = json.load(open(src))
objs = {o["id"]: o for o in f["objects"]}

def put(oid, data):
    objs[oid].update(size=len(data), crc=zlib.crc32(data) & 0xFFFFFFFF, data=base64.b64encode(data).decode())

for k, c in enumerate("ABCD"):
    sec = open(f"{gen}/sec_{c}.bin", "rb").read()
    assert len(sec) == 3840 and sec[:4] == b"DNUF", "not a FUND section"
    put(40 + 4 * SONG + k, sec)
idx = bytearray(base64.b64decode(objs[10]["data"]))
assert len(idx) >= ROW_OFF + 6 * ROW_LEN and struct.unpack_from("<I", idx)[0] == 0x474E4F53, "no song index in the backup"
chain = open(f"{gen}/chain.bin", "rb").read()
assert len(chain) == ROW_LEN
idx[ROW_OFF + SONG * ROW_LEN: ROW_OFF + (SONG + 1) * ROW_LEN] = chain
put(10, bytes(idx))
json.dump(f, open(dst, "w"))
print("song 6 now holds the CPU test song ->", dst)
