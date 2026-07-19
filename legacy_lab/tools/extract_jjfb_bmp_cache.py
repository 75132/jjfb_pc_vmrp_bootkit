#!/usr/bin/env python3
import os, struct, zlib

root = os.path.join(os.path.dirname(__file__), "..", "runtime", "vmrp_win32", "vmrp_win32_20220102")
packs = [
    os.path.join(root, "mythroad", "gwy", "jjfb.mrp"),
    os.path.join(root, "mythroad", "dsm_gm.mrp"),
]
names = [
    "bar!16!18.bmp",
    "loadingbar!201!29.bmp",
    "textbar!120!30.bmp",
    "slogo!157!58.bmp",
]
outdir = os.path.join(root, "jjfb_bmp_cache")
os.makedirs(outdir, exist_ok=True)

for pack in packs:
    if not os.path.exists(pack):
        print("missing", pack)
        continue
    data = open(pack, "rb").read()
    for name in names:
        needle = name.encode() + b"\x00"
        i = data.find(needle)
        if i < 0:
            continue
        off, ln = struct.unpack_from("<II", data, i + len(needle))
        blob = data[off : off + ln]
        parts = name.split("!")
        expect = int(parts[1]) * int(parts[2].split(".")[0]) * 2
        out = None
        for wbits in (zlib.MAX_WBITS, 16 + zlib.MAX_WBITS):
            try:
                out = zlib.decompress(blob, wbits)
                break
            except Exception:
                out = None
        if out is None or len(out) != expect:
            print("fail", name, "len", None if out is None else len(out), "expect", expect)
            continue
        path = os.path.join(outdir, name + ".rgb565")
        open(path, "wb").write(out)
        print("ok", path, len(out), "from", os.path.basename(pack))
