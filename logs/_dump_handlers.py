import struct, zlib, os, sys
path = r"game_files\mythroad\320x480\gwy\jjfb.mrp"
data = open(path,"rb").read()
# MRP: find robotol.ext member - use simple scan for name
name = b"robotol.ext"
idx = data.find(name)
print("name_at", idx)
# Try inflate known offset from logs: offset=231624 stored=161178
off, stored, unpacked = 231624, 161178, 253420
blob = data[off:off+stored]
try:
    raw = zlib.decompress(blob)
except Exception:
    raw = zlib.decompress(blob, -15)
print("raw_len", len(raw))
# image_base from log 0x2D8DF4, handler thumb 0x30D301 -> file offset = pc - base
base = 0x2D8DF4
pc = 0x30D300
fo = pc - base
print("file_off", hex(fo))
chunk = raw[fo:fo+64]
print("bytes", chunk.hex())
# also dump 0x30D2F9 (10165)
pc2 = 0x30D2F8
fo2 = pc2 - base
print("10165_bytes", raw[fo2:fo2+64].hex())
# dump caller 0x304589 area
pc3 = 0x304588
fo3 = pc3 - base
print("caller_bytes", raw[fo3:fo3+48].hex())
open(r"logs\robotol_handler_30D300.bin","wb").write(raw[fo:fo+256])
open(r"logs\robotol_handler_30D2F8.bin","wb").write(raw[fo2:fo2+256])
