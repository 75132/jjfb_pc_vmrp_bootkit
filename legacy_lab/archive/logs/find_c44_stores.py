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


# For each 0xC44 literal, disassemble ~32 bytes before it looking for strb that uses it
sites = []
for i in range(0, len(data) - 3, 2):
    if struct.unpack_from("<I", data, i)[0] == 0xC44:
        sites.append(base + i)

for lit_addr in sites:
    # scan backwards for ldr rx,[pc,#imm] that points to this literal
    found = []
    for pc in range(lit_addr - 0x80, lit_addr, 2):
        if pc < base:
            continue
        w = struct.unpack_from("<H", data, pc - base)[0]
        # LDR Rt, [PC, #imm] T1: 01001ttt iiiiiiii
        if (w & 0xF800) == 0x4800:
            imm = (w & 0xFF) * 4
            target = lit(pc, imm)
            if target == lit_addr:
                found.append(pc)
    print("--- lit 0xC44 @0x%X refs=%s ---" % (lit_addr, [hex(x) for x in found]))
    for pc in found[:3]:
        start = pc - 8
        for insn in md.disasm(data[start - base : start - base + 0x40], start):
            print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
            if insn.address > pc + 0x28:
                break
        print()
