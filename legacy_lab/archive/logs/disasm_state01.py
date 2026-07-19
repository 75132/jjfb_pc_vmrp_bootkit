from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

# state0 @306638, state1 @30666e from branches
for label, start in [
    ("state0", 0x306638),
    ("state1", 0x30666E),
    ("state3", 0x306668),
    ("tail_6a4", 0x3066A4),
]:
    print("=== %s @0x%X ===" % (label, start))
    for insn in md.disasm(data[start - base : start - base + 0x60], start):
        print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
        if insn.mnemonic == "pop" and "pc" in insn.op_str:
            break
        if insn.address > start + 0x50:
            break
    print()
