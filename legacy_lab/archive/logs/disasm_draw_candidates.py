from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)


def show(label, start, nbytes=0xC0):
    print("=== %s @0x%X ===" % (label, start))
    for insn in md.disasm(data[start - base : start - base + nbytes], start):
        print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
        if insn.mnemonic == "pop" and "pc" in insn.op_str and insn.address > start + 8:
            break
    print()


show("2e9934", 0x2E9934, 0x100)
show("2f2a00", 0x2F2A00, 0x100)
show("305b20", 0x305B20, 0x80)
show("305b68", 0x305B68, 0x80)

# Trace: does 305b20 eventually blx to helper drawBitmap?
# Look for ldr from known dsm helper / mr_table
