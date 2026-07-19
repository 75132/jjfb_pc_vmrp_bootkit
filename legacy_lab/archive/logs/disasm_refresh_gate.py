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


# Flag checks after app=24 call
for pc, boff, name in [
    (0x3066A4, 0x7C, "flagA"),
    (0x3066B4, 0x70, "flagB"),
    (0x3066C0, 0x68, "flagC"),
    (0x3066CA, 0x64, "flagD"),
    (0x3066D4, 0x5C, "ptrE"),
]:
    la = lit(pc, boff)
    print("%s: lit@0x%X = ER_RW+0x%X" % (name, la, u32(la)))

print()
print("=== 0x2e87ac (possible refresh) ===")
start = 0x2E87AC
for insn in md.disasm(data[start - base : start - base + 0x100], start):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break
    if insn.address > start + 0xC0:
        break

print()
print("=== 0x312abc ===")
start = 0x312ABC
for insn in md.disasm(data[start - base : start - base + 0x80], start):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break
