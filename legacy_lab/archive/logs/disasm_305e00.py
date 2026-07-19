from capstone import *

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

print("=== 0x305e00 ===")
for insn in md.disasm(data[0x305E00 - BASE : 0x305E00 - BASE + 0xA0], 0x305E00):
    print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
    if insn.address >= 0x305E90:
        break
