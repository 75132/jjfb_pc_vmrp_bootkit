from capstone import *
import struct
import re

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

# Find all "str rn, [rm, #0x78]" 
print("All str *, [*, #0x78]:")
for off in range(0, len(data) - 4, 2):
    insns = list(md.disasm(data[off : off + 4], BASE + off, count=1))
    if not insns:
        continue
    insn = insns[0]
    if insn.mnemonic == "str" and "#0x78" in insn.op_str and "sp" not in insn.op_str:
        # show a bit of context before
        ctx = []
        for i2 in md.disasm(data[off - 16 : off + 4], BASE + off - 16):
            ctx.append(f"{i2.mnemonic} {i2.op_str}")
        prev = " // ".join(ctx[-4:])
        print(f"  {hex(insn.address)}: {insn.mnemonic} {insn.op_str}  | prev: {prev}")

# Also strb to state-ish
print("\nSearch movs + str to #0x78 region with 0x7D8 base nearby (manual candidates)")
# Disasm around places that load 0x7D8 and then store small constants
n = struct.pack("<I", 0x7D8)
i = 0
while True:
    j = data.find(n, i)
    if j < 0:
        break
    lit_va = BASE + j
    # find ldr to this lit in previous 0x300
    for off in range(max(0, j - 0x300), j, 2):
        insns = list(md.disasm(data[off : off + 4], BASE + off, count=1))
        if not insns:
            continue
        insn = insns[0]
        if insn.mnemonic != "ldr" or "pc" not in insn.op_str:
            continue
        m = re.search(r"#(0x[0-9a-fA-F]+|\d+)", insn.op_str)
        if not m:
            continue
        imm_s = m.group(1)
        imm = int(imm_s, 16) if imm_s.startswith("0x") else int(imm_s)
        pc = (insn.address + 4) & ~3
        if pc + imm != lit_va:
            continue
        # disasm 0x40 and look for str of small imm
        for i2 in md.disasm(data[off : off + 0x60], BASE + off):
            if i2.mnemonic in ("str", "strb", "strh") and "sp" not in i2.op_str:
                # check prev for movs small
                print(f"ldr@{hex(insn.address)} lit7d8 -> {hex(i2.address)}: {i2.mnemonic} {i2.op_str}")
            if i2.address >= insn.address + 0x50:
                break
    i = j + 1
