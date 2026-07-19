from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

targets = [
    ("app9_fn", 0x305E00),
    ("app24_fn", 0x3130EC),
    ("epilogue", 0x30DA70),
]
for label, start in targets:
    print("=== %s @0x%X ===" % (label, start))
    end = start + 0x120
    n = 0
    for insn in md.disasm(data[start - base : end - base], start):
        print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
        n += 1
        if insn.mnemonic == "pop" and "pc" in insn.op_str:
            break
        if insn.mnemonic == "bx" and "lr" in insn.op_str:
            break
        if n > 80:
            break
    print()
