# -*- coding: utf-8 -*-
import os, struct, zlib

MRP = r"c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\320x480\gwy\jjfb.mrp"
data = open(MRP, "rb").read()
i = data.find(b"robotol.ext\x00")
print("idx", i)
# After name: packed size, method/offset (from log mr_get_method(161178)=0x2759A)
psize, method = struct.unpack_from("<II", data, i + 12)
print(hex(psize), hex(method), psize, method)
blob = data[method : method + psize]
print("blob", len(blob), blob[:8].hex())
out = None
for wbits in (15, -15, 31, 47):
    try:
        out = zlib.decompress(blob, wbits)
        print("zlib ok", wbits, len(out))
        break
    except Exception as e:
        print("fail", wbits, type(e).__name__)
if out is None:
    # try skip 4-byte header
    for skip in (0, 4, 8, 16):
        for wbits in (15, -15, 31):
            try:
                out = zlib.decompress(blob[skip:], wbits)
                print("zlib ok skip", skip, wbits, len(out))
                break
            except Exception:
                pass
        if out is not None:
            break
if out is None:
    raise SystemExit("decompress failed")

open(r"c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\_robotol.bin", "wb").write(out)
print("wrote", len(out))

# ext_base from log 0x2D8DF4; crash 0x2D92B0 => file offset depends on
# whether load address maps to start of decompressed image.
# From log: mr_cacheSync(0x002D8DE0, 253504) — load at 0x2D8DE0
load = 0x2D8DE0
pc = 0x2D92B0
off = pc - load
print("off from load", hex(off))
lr = 0x2D91E5
print("lr off", hex(lr - load))

def dump(off, n=32):
    chunk = out[off : off + n]
    print(chunk.hex())
    # thumb dump
    i = 0
    while i + 2 <= len(chunk):
        hw = struct.unpack_from("<H", chunk, i)[0]
        is32 = (hw & 0xE000) == 0xE000 and (hw & 0x1800) != 0
        if is32 and i + 4 <= len(chunk):
            hw2 = struct.unpack_from("<H", chunk, i + 2)[0]
            print(f"  +{off+i:#x} / va {load+off+i:#x}: {hw:04x} {hw2:04x}")
            i += 4
        else:
            print(f"  +{off+i:#x} / va {load+off+i:#x}: {hw:04x}")
            i += 2

print("--- around pc ---")
dump(off - 16, 48)
print("--- around lr ---")
dump(lr - load - 8, 32)
