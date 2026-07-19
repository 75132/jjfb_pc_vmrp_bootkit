from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

def disasm_va(va, size=0x100, title=""):
    off = va - BASE
    print(f"\n=== {title} VA=0x{va:X} ===")
    if off < 0 or off >= len(data):
        print("OOR", off)
        return
    for insn in md.disasm(data[off : off + size], va):
        print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.address >= va + size - 4:
            break

disasm_va(0x30D2F8, 0xC0, "handler_1E200_family")
disasm_va(0x30D240, 0x80, "handler_10162")
disasm_va(0x30D2F0, 0x40, "handler_10165")
disasm_va(0x2f2a18, 0x90, "state_store_2f2a18")
disasm_va(0x30cbba, 0x60, "state_store_30cbba")

off = data.find(struct.pack("<I", 0x10102))
print(f"\n0x10102 pool at VA {hex(BASE+off)}")
# walk backwards to find function that uses it - disasm from likely code
# Search for immediate load patterns near known init
disasm_va(0x30D000, 0x100, "near_30D000")
