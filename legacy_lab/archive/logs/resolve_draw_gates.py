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


# 2f2a00 base struct offset
raw = struct.unpack_from("<H", data, 0x2F2A02 - base)[0]
print("2f2a02 raw=%04X" % raw)
# ldr r4, [pc, #0x138]
la = lit(0x2F2A02, 0x138)
print("2f2a00 r4 base = ER_RW+0x%X" % u32(la))

# 30675c halfword gate
raw = struct.unpack_from("<H", data, 0x30675C - base)[0]
print("30675c raw=%04X" % raw)
# from disasm: ldr r4, [pc, #0x18c]
la = lit(0x30675C, 0x18C)
print("30675c halfword @ ER_RW+0x%X" % u32(la))

print("\n=== 305b68 (draw candidate) ===")
for insn in md.disasm(data[0x305B68 - base : 0x305B68 - base + 0xA0], 0x305B68):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break

print("\n=== 305b20 ===")
for insn in md.disasm(data[0x305B20 - base : 0x305B20 - base + 0x80], 0x305B20):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break

# Follow bl from 305b68 if any to see drawBitmap
print("\n=== follow calls from 305b68 depth1 ===")
