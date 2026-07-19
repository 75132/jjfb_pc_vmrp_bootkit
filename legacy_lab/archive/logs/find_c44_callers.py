from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

# Find BL to 0x2fc8b8 / 0x2fc8ba
target = 0x2FC8B8
# Also function might be entered at 0x2fc8b8
print("=== enable_C44 fn ===")
for insn in md.disasm(data[0x2FC8B8 - base : 0x2FC8B8 - base + 0x30], 0x2FC8B8):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))

# Scan for BL encodings that land on 0x2fc8b8
# Thumb BL: two halfwords F000 | ... and F800 | ...
callers = []
for i in range(0, len(data) - 3, 2):
    h1 = struct.unpack_from("<H", data, i)[0]
    h2 = struct.unpack_from("<H", data, i + 2)[0]
    if (h1 & 0xF800) != 0xF000 or (h2 & 0xD000) != 0xD000:
        # also try classic F000 F800 BL
        if (h1 & 0xF800) != 0xF000 or (h2 & 0xF800) != 0xF800:
            continue
    # decode BL
    s = (h1 >> 10) & 1
    imm10 = h1 & 0x3FF
    j1 = (h2 >> 13) & 1
    j2 = (h2 >> 11) & 1
    imm11 = h2 & 0x7FF
    I1 = 1 - (j1 ^ s)
    I2 = 1 - (j2 ^ s)
    imm32 = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
    if imm32 & (1 << 24):
        imm32 -= 1 << 25
    pc = base + i
    dest = (pc + 4 + imm32) & 0xFFFFFFFF
    if dest == target or dest == target + 2:
        callers.append(pc)

print("callers of 0x2FC8B8:", [hex(c) for c in callers])

for c in callers[:8]:
    print("\n--- caller context @0x%X ---" % c)
    start = c - 0x20
    for insn in md.disasm(data[start - base : start - base + 0x50], start):
        mark = " <<<" if insn.address == c else ""
        print("0x%08x:\t%s\t%s%s" % (insn.address, insn.mnemonic, insn.op_str, mark))
        if insn.address > c + 0x10:
            break

# Also check 0x2f4e76 path - what function, who calls, what is r4
print("\n=== 0x2f4e50 (C44 strb r4) ===")
for insn in md.disasm(data[0x2F4E40 - base : 0x2F4E40 - base + 0x80], 0x2F4E40):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break
