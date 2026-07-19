from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

def disasm_va(va, size=0x100, title=""):
    off = va - BASE
    print(f"\n=== {title} VA=0x{va:X} ===")
    if off < 0 or off >= len(data):
        print("OOR")
        return
    for insn in md.disasm(data[off : off + size], va):
        print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.address >= va + size - 4:
            break

disasm_va(0x3102e0, 0xA0, "fn_3102e0")
disasm_va(0x2f995c, 0x40, "fn_2f995c")
disasm_va(0x2f9968, 0x40, "fn_2f9968")

# xref-ish: who writes state at erw+0x7D8+0xF8 = +0x8D0
# Search for literal 0x8D0 or 0x7D8 in file
for imm in [0x8D0, 0x7D8, 0xCA7, 0xCA8, 0xED8]:
    n = struct.pack("<I", imm)
    locs = []
    i = 0
    while True:
        j = data.find(n, i)
        if j < 0:
            break
        locs.append(BASE + j)
        i = j + 1
    print(f"lit 0x{imm:X}: {[hex(x) for x in locs[:12]]} count={len(locs)}")

# strings around jjfbol/gwy
for va in [0x313af0, 0x313ae0]:
    off = va - BASE
    chunk = data[off : off + 64]
    print(hex(va), repr(chunk))

# Find code refs to jjfbol string VA 0x313af8 - search for offset patterns is hard;
# disassemble nearby functions that might load module
# Search ASCII paths
for s in [b"jjfbol", b"gwy/", b".EXT", b".ext", b"moudle", b"modul", b"load", b"http", b"socket", b"20000", b"21002", b"6009"]:
    i = 0
    hits = []
    while True:
        j = data.find(s, i)
        if j < 0:
            break
        hits.append(j)
        i = j + 1
        if len(hits) > 8:
            break
    if hits:
        print(f"{s}: {[hex(BASE+h) for h in hits]}")
