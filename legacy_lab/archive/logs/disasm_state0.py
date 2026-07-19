from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

def disasm_va(va, size=0x100, title=""):
    off = va - BASE
    print(f"\n=== {title} VA=0x{va:X} ===")
    chunk = data[off : off + size]
    for insn in md.disasm(chunk, va):
        print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.address >= va + size - 4:
            break

for va, title in [
    (0x306638, "state0_body"),
    (0x30666e, "state1_body"),
    (0x306668, "state3_body"),
    (0x30669c, "fallback_69c"),
    (0x306738, "epilogue_ish"),
]:
    disasm_va(va, 0x80, title)

# Also check: after 0x1E209 returns, does handler advance state?
# Dump ER_RW layout hint: offset 0x7D8+0xF8 = state
# Search who stores to state (str to [rN,#0xF8] or similar near 0x7D8)

# Find stores of small immediates near module load - search "module" string
for s in [b"module", b"Module", b"initNetwork", b"network", b"jjfbol", b".ext", b"gwy"]:
    i = 0
    hits = []
    while True:
        j = data.find(s, i)
        if j < 0:
            break
        hits.append(j)
        i = j + 1
    print(f"str {s}: count={len(hits)} first={[hex(BASE+h) for h in hits[:6]]}")
