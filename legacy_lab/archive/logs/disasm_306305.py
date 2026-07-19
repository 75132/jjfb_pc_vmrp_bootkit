from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

# Timer/state handler 0x306305 — how it builds 0x1E209(app, ...)
print("=== handler 0x306305 ===")
start = 0x306304  # thumb clear
for insn in md.disasm(data[start - base : start - base + 0x200], start):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.address > start + 0x180:
        break
    if insn.mnemonic == "pop" and "pc" in insn.op_str and insn.address > start + 0x40:
        break
