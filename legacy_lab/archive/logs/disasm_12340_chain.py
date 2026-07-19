from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)


def lit(pc, boff):
    return ((pc + 4) & ~3) + boff


def u32(va):
    return struct.unpack_from("<I", data, va - base)[0]


def show(label, start, n=0x100):
    print("=== %s @0x%X ===" % (label, start))
    for insn in md.disasm(data[start - base : start - base + n], start):
        print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
        if insn.mnemonic == "pop" and "pc" in insn.op_str and insn.address > start + 8:
            break
    print()


show("305e70", 0x305E70, 0x50)
show("303c48", 0x303C48, 0x80)
show("30460c", 0x30460C, 0x80)

# Resolve 0x12340 literal and args at call site 0x305e8e
print("12340 lit @0x%X = 0x%X" % (lit(0x305E8E, 0x1C), u32(lit(0x305E8E, 0x1C))))

# Find all BL to 0x304550 that load 0x12340 nearby - already know 305e70
# Search who else references 0x12340 literal at 0x305eac
print("\n=== refs to lit 0x305EAC ===")
lit_addr = 0x305EAC
for pc in range(lit_addr - 0x80, lit_addr, 2):
    w = struct.unpack_from("<H", data, pc - base)[0]
    if (w & 0xF800) == 0x4800:
        imm = (w & 0xFF) * 4
        if lit(pc, imm) == lit_addr:
            print("ref @0x%X" % pc)

# Also search for other 0x12340 immediates via movw/movt patterns - skip
# Disasm wrapper path: after 12340 returns, what does 305e70 do with r0?
# From earlier: ignores r0 from 12340! Uses r5 and r4 instead.
print("\n=== After 12340 in 305e70: r0 from plat ignored; uses r5 (303c48 ret) ===")

# Who calls 305e70?
target = 0x305E70
callers = []
for i in range(0, len(data) - 3, 2):
    h1 = struct.unpack_from("<H", data, i)[0]
    h2 = struct.unpack_from("<H", data, i + 2)[0]
    if (h1 & 0xF800) != 0xF000 or (h2 & 0xF800) != 0xF800:
        continue
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
    if dest == target:
        callers.append(pc)
print("callers of 0x305e70:", [hex(c) for c in callers[:20]], "count", len(callers))

for c in callers[:6]:
    print("\n--- caller @0x%X ---" % c)
    for insn in md.disasm(data[c - 0x18 - base : c - 0x18 - base + 0x40], c - 0x18):
        m = " <<<" if insn.address == c else ""
        print("0x%08x:\t%s\t%s%s" % (insn.address, insn.mnemonic, insn.op_str, m))
        if insn.address > c + 8:
            break
