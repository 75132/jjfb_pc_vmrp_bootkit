from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
print("len", len(data))

bases = [0x2D8DEC, 0x2D8DE0, 0x2D0000, 0x300000]
handler = 0x306304  # clear thumb bit

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True

for base in bases:
    off = handler - base
    if off < 0 or off >= len(data):
        print(f"base=0x{base:X} off out of range")
        continue
    print(f"\n=== base=0x{base:X} file_off=0x{off:X} ===")
    chunk = data[off : off + 0x140]
    for insn in md.disasm(chunk, handler):
        print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.address >= handler + 0x120:
            break

for code in [0x10140, 0x10120, 0x10102, 0x10113, 0x10800, 0x1E209, 0x1E200]:
    n = struct.pack("<I", code)
    locs = []
    i = 0
    while True:
        j = data.find(n, i)
        if j < 0:
            break
        locs.append(j)
        i = j + 1
    print(f"imm 0x{code:X}: count={len(locs)} first={list(map(hex, locs[:8]))}")

# Also try base from helper: helper@0x304AE5 -> file?
# If helper is near start of code, base = helper - small_off
# mr_get_method and code load: code_buf often == load address
print("\n--- try deduce base from file size ---")
# If loaded at B, file maps [B, B+len)
# handler must be in range
for guess_end in [0x2D8DEC + len(data), 0x300000 + len(data)]:
    print("guess end", hex(guess_end))
