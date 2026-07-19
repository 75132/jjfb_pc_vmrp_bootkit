from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
import struct

path = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
base = 0x2D8DEC
data = open(path, "rb").read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

# Who writes ER_RW+0xC44? Search for literal 0xC44 in robotol
hits = []
for i in range(0, len(data) - 3, 2):
    if struct.unpack_from("<I", data, i)[0] == 0xC44:
        hits.append(base + i)
print("literal 0xC44 sites:", [hex(h) for h in hits[:20]], "count", len(hits))

# Also 0xC9D as byte offset - often as halfword or word
for val, name in [(0xC44, "C44"), (0xC9D, "C9D"), (0xCD1, "CD1"), (0xCF5, "CF5"), (0x11B0, "11B0")]:
    hs = []
    for i in range(0, len(data) - 3, 2):
        if struct.unpack_from("<I", data, i)[0] == val:
            hs.append(base + i)
    print("%s: %d sites, first=%s" % (name, len(hs), [hex(x) for x in hs[:8]]))

# Disasm more of 0x2e87ac to see draw/refresh calls
print("\n=== 0x2e87ac fuller ===")
start = 0x2E87AC
for insn in md.disasm(data[start - base : start - base + 0x200], start):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str and insn.address > start + 0x20:
        # may be early return; continue a bit
        if insn.address > start + 0x100:
            break
    if insn.address > start + 0x1C0:
        break
