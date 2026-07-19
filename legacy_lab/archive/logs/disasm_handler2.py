from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
handler = 0x306304
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

def disasm_va(va, size=0x200, title=""):
    off = va - BASE
    print(f"\n=== {title} VA=0x{va:X} file=0x{off:X} ===")
    if off < 0 or off >= len(data):
        print("OOR")
        return
    chunk = data[off : off + size]
    for insn in md.disasm(chunk, va):
        print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
        if insn.address >= va + size - 4:
            break

# literal pools near handler entry
def read_lit(pc_insn_addr, imm):
    # Thumb LDR [pc,#imm]: align PC to 4 then +imm
    pc = (pc_insn_addr + 4) & ~3
    return pc + imm

# From entry:
# 0x306304: ldr r0, [pc, #0x3e0] -> lit
# 0x30631A: ldr r6, [pc, #0x3d0]
# 0x306326: ldr r0, [pc, #0x3c8]
# 0x306336/338: ldr r0/r4
# 0x30633E: ldr r0, [pc, #0x3ac]

lits = [
    (0x306304, 0x3E0, "flag_ptr"),
    (0x30631A, 0x3D0, "r6_base"),
    (0x306326, 0x3C8, "clear_byte"),
    (0x306336, 0x3BC, "after_bl"),
    (0x306338, 0x3BC, "r4_extcode"),
    (0x30633E, 0x3AC, "state_obj"),
]
print("Literal pool values (ER_RW-relative offsets):")
for addr, imm, name in lits:
    la = read_lit(addr, imm)
    off = la - BASE
    if 0 <= off + 4 <= len(data):
        val = struct.unpack_from("<I", data, off)[0]
        print(f"  {name}: ldr@{hex(addr)} lit@{hex(la)} = 0x{val:X} (signed {val if val < 0x80000000 else val-0x100000000})")

# State 0 path and neighbors from branches
for va, title in [
    (0x306428, "state0"),
    (0x30642a, "state1"),
    (0x30642c, "state3"),
    (0x306368, "state4_body"),
    (0x3066a4, "after_plat_call"),
    (0x30644e, "default_unk"),
    (0x3066b4, "r1_nonzero_path"),
    (0x304550, "wrapper_304550"),
    (0x2feb8c, "tick_prep_2feb8c"),
]:
    disasm_va(va, 0x80 if va != 0x304550 else 0x60, title)

# Find what loads 0x1E209 near state0 - search xrefs in handler region
# Disassemble more of state0
disasm_va(0x306428, 0x120, "state0_long")
