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


sites = []
for i in range(0, len(data) - 3, 2):
    if struct.unpack_from("<I", data, i)[0] == 0x1510:
        sites.append(base + i)
print("0x1510 sites:", [hex(s) for s in sites])

for lit_addr in sites:
    refs = []
    for pc in range(max(base, lit_addr - 0xA0), lit_addr, 2):
        w = struct.unpack_from("<H", data, pc - base)[0]
        if (w & 0xF800) == 0x4800:
            imm = (w & 0xFF) * 4
            if lit(pc, imm) == lit_addr:
                refs.append(pc)
    print("lit@0x%X refs=%s" % (lit_addr, [hex(r) for r in refs]))
    for pc in refs:
        print("--- @0x%X ---" % pc)
        for insn in md.disasm(data[pc - 0x10 - base : pc - 0x10 - base + 0x50], pc - 0x10):
            print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
            if insn.address > pc + 0x28:
                break
