from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

for label, start, n in [
    ("tail_cont", 0x30673C, 0x120),
    ("alt_2e88c4", 0x2E88C4, 0x100),
    ("enable_caller_30dd", 0x30DD80, 0x100),
]:
    print("=== %s @0x%X ===" % (label, start))
    for insn in md.disasm(data[start - base : start - base + n], start):
        print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
        if insn.mnemonic == "pop" and "pc" in insn.op_str and insn.address > start + 0x10:
            break
    print()

# Search for bl to drawBitmap-ish: helper table slot often via [sb,#off] then blx
# Or search mr_table drawBitmap usage - in EXT, refresh = drawBitmap helper
# Grep for literal that might be screen w/h used with refresh
# Look for bl 0x304550 with known refresh family apps

# Family cases that might refresh - check app that does screen
# From earlier switch, case that calls something with screen
# Search BL to functions that call drawBitmap by finding gui path
# In robotol, find "blx rN" after loading from helper table offset 0x68

# Simpler: find all sites that load helper+0x68 or similar
# Actually search for calls to addresses we know are draw wrappers in mythroad docs
# Look at 0x305e70 used from 2e87ac - might be text draw
print("=== 0x305e70 ===")
for insn in md.disasm(data[0x305E70 - base : 0x305E70 - base + 0x60], 0x305E70):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break
