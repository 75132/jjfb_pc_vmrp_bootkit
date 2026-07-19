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


# Find LDR of 0x7D8 then nearby str to +0xc
sites = []
for i in range(0, len(data) - 3, 2):
    if struct.unpack_from("<I", data, i)[0] == 0x7D8:
        sites.append(base + i)
print("0x7D8 sites:", len(sites), [hex(s) for s in sites[:15]])

for lit_addr in sites[:15]:
    refs = []
    for pc in range(max(base, lit_addr - 0xA0), lit_addr, 2):
        w = struct.unpack_from("<H", data, pc - base)[0]
        if (w & 0xF800) == 0x4800:
            imm = (w & 0xFF) * 4
            if lit(pc, imm) == lit_addr:
                refs.append(pc)
    for pc in refs:
        # disasm window looking for str.* #0xc
        chunk = []
        for insn in md.disasm(data[pc - base : pc - base + 0x40], pc):
            chunk.append(insn)
            if insn.address > pc + 0x38:
                break
        text = " | ".join("%s %s" % (i.mnemonic, i.op_str) for i in chunk)
        if "#0xc" in text or ", #0xc" in text or "0xc]" in text:
            print("\n*** interesting @0x%X ***" % pc)
            for insn in chunk:
                print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
