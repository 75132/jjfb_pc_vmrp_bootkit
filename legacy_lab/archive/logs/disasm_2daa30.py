from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)


def lit(insn_pc, byte_off):
    return ((insn_pc + 4) & ~3) + byte_off


def u32(va):
    return struct.unpack_from("<I", data, va - base)[0]


# 0x3130ec literals
for pc, boff, name in [
    (0x3130EE, 0x2C, "buf_ptr_off"),
    (0x3130FE, 0x20, "cursor_off"),
]:
    la = lit(pc, boff)
    print("%s lit@0x%X = 0x%X (ER_RW+0x%X)" % (name, la, u32(la), u32(la)))

print()
print("=== 0x2daa30 ===")
start = 0x2DAA30
for insn in md.disasm(data[start - base : start - base + 0x100], start):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break
    if insn.mnemonic == "bx" and "lr" in insn.op_str:
        break
