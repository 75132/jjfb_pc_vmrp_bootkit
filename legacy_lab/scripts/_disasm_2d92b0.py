# -*- coding: utf-8 -*-
import os
import struct
import zlib

ROOT = r"c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit"
MRP = os.path.join(ROOT, r"game_files\mythroad\320x480\gwy\jjfb.mrp")


def find_robotol(data: bytes):
    # Prefer packed member robotol.ext inside MRP (Mythroad list).
    # Fallback: scan for known ARM patterns near name.
    name = b"robotol.ext\x00"
    i = data.find(name)
    print("name_idx", i)
    # Some packs store name then offset/size nearby; try common layouts.
    for off in range(max(0, i - 64), min(len(data), i + 64)):
        pass
    # Try gzip/zlib members via mr_unzip style: look for PK? unlikely.
    # Use embedded list: search all occurrences of b"robotol"
    idxs = []
    start = 0
    while True:
        j = data.find(b"robotol", start)
        if j < 0:
            break
        idxs.append(j)
        start = j + 1
    print("robotol refs", idxs[:20], "count", len(idxs))
    return idxs


def try_extract_by_scan(data: bytes):
    """Heuristic: after filename robotol.ext there is often uint32 size + data."""
    name = b"robotol.ext"
    i = data.find(name)
    if i < 0:
        return None
    # Walk forward looking for a zlib header or large blob
    for base in range(i, min(i + 256, len(data) - 8)):
        for endian in ("<I", ">I"):
            (sz,) = struct.unpack_from(endian, data, base)
            if 10000 < sz < 2_000_000 and base + 4 + sz <= len(data):
                blob = data[base + 4 : base + 4 + sz]
                # try raw and zlib
                for label, cand in (("raw", blob),):
                    if cand[:2] in (b"\x1f\x8b", b"\x78\x9c", b"\x78\x01", b"\x78\xda"):
                        try:
                            out = zlib.decompress(cand)
                            print("zlib ok", label, "at", base, "out", len(out))
                            return out
                        except Exception:
                            pass
                    # Mythroad often stores uncompressed after header
                    if cand[0:2] == b"\x00\x00" or True:
                        # check if looks like ARM code (high entropy + thumb)
                        if cand.count(b"\x00") < len(cand) * 0.2:
                            print("candidate", base, endian, sz)
    return None


def disasm_thumb(buf: bytes, addr: int, count: int = 24):
    # Minimal thumb16 dump without capstone
    i = 0
    n = 0
    while i + 2 <= len(buf) and n < count:
        hw = struct.unpack_from("<H", buf, i)[0]
        # 32-bit thumb?
        is32 = (hw & 0xE000) == 0xE000 and (hw & 0x1800) != 0
        if is32 and i + 4 <= len(buf):
            hw2 = struct.unpack_from("<H", buf, i + 2)[0]
            print(f"  0x{addr+i:08X}: {hw:04x} {hw2:04x}")
            i += 4
        else:
            print(f"  0x{addr+i:08X}: {hw:04x}")
            i += 2
        n += 1


def main():
    data = open(MRP, "rb").read()
    print("mrp", len(data))
    find_robotol(data)
    # From log: ext_base=0x2D8DF4, pc=0x2D92B0 => offset 0x4BC from ext base
    # ext code often starts after a small header at load address.
    # Search runtime dump if any
    for root, _, files in os.walk(os.path.join(ROOT, "runtime")):
        for f in files:
            if "robotol" in f.lower() or f.endswith(".bin"):
                p = os.path.join(root, f)
                if os.path.getsize(p) > 1000:
                    print("bin", p, os.path.getsize(p))

    # Use mr_get_method style: many MRP files have table at start
    # Dump bytes around each 'robotol.ext' + look for size field after name pad
    i = data.find(b"robotol.ext")
    print("context", data[i : i + 64])
    # Mythroad file list: each entry 128-byte name? try offset after 128-aligned name
    for ent_off in range(0, min(len(data), 0x20000), 4):
        if data[ent_off : ent_off + 11] == b"robotol.ext":
            print("entry@", hex(ent_off), data[ent_off : ent_off + 32])
            # common: name[128], offset, size, ...
            for name_len in (32, 64, 128):
                if ent_off + name_len + 8 <= len(data):
                    off, sz = struct.unpack_from("<II", data, ent_off + name_len)
                    print(f"  try name_len={name_len} off={off} sz={sz}")
                    if 0 < off < len(data) and 1000 < sz < len(data) and off + sz <= len(data):
                        blob = data[off : off + sz]
                        print("  blob head", blob[:16].hex(), "tail", blob[-8:].hex())
                        # decompress if needed
                        for wbits in (15, -15, 31):
                            try:
                                out = zlib.decompress(blob, wbits)
                                print("  decompressed", len(out), "wbits", wbits)
                                # ext load base 0x2D8DF4; code at +0x4BC relative if identity
                                # Often first bytes are header; try several bases
                                for base_off in (0, 0x14, 0x20, 0x40, 0x100):
                                    target = base_off + 0x4BC
                                    if target + 32 <= len(out):
                                        print(f"  --- around file+{base_off:#x}+0x4BC ---")
                                        disasm_thumb(out[target : target + 64], 0x2D8DF4 + 0x4BC)
                                return
                            except Exception:
                                pass
                        # uncompressed
                        for base_off in (0, 0x14, 0x20):
                            target = base_off + 0x4BC
                            if target + 32 <= len(blob):
                                print(f"  raw --- around +{base_off:#x}+0x4BC ---")
                                disasm_thumb(blob[target : target + 64], 0x2D8DF4 + 0x4BC)


if __name__ == "__main__":
    main()
