from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

print("=== 0x3102e0 (state0 setup) ===")
for insn in md.disasm(data[0x3102E0 - base : 0x3102E0 - base + 0x120], 0x3102E0):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break

# Find stores to ER_RW+0x7E4 (7D8+0xC) via literal 0x7E4 or 0x7D8
print("\n=== literals 0x7E4 / 0x7D8 ===")
for val in (0x7E4, 0x7D8, 0xD14, 0x1510):
    sites = []
    for i in range(0, len(data) - 3, 2):
        if struct.unpack_from("<I", data, i)[0] == val:
            sites.append(base + i)
    print("0x%X: %d sites %s" % (val, len(sites), [hex(s) for s in sites[:6]]))
