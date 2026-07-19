from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

raw = struct.unpack_from("<H", data, 0x30D30E - base)[0]
imm = raw & 0xFF
adr_base = ((0x30D30E + 4) & ~3) + imm * 4
print("ADR table=0x%X" % adr_base)


def tgt(i):
    off = struct.unpack_from("<H", data, adr_base + 2 * i - base)[0]
    return (0x30D316 + 4) + (off << 1)


for i in (9, 24):
    print("case %d -> 0x%X" % (i, tgt(i)))

for label, start in [("app9", tgt(9)), ("app24", tgt(24))]:
    print("=== %s @0x%X ===" % (label, start))
    end = start + 0xA0
    for insn in md.disasm(data[start - base : end - base], start):
        print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
        if insn.mnemonic == "pop" and "pc" in insn.op_str:
            break
        if insn.mnemonic == "bx" and "lr" in insn.op_str:
            break
