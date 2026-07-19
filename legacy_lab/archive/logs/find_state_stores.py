from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True

# Find functions that load lit 0x7D8 then store to +0xF8 (0x80+0x78)
# Simpler: scan all code for pattern: after add rN, sb involving 0x7D8, look for str near +0xF8

# Brute: disassemble whole binary looking for "str.*#0xf8" or "str.*, #0x78" after add 0x80
hits = []
# scan every 2 bytes as potential thumb start - too slow/noisy
# Instead find all PC-relative loads of 0x7D8 and disasm 0x40 bytes after

n = struct.pack("<I", 0x7D8)
i = 0
sites = []
while True:
    j = data.find(n, i)
    if j < 0:
        break
    sites.append(j)
    i = j + 1

print(f"0x7D8 sites: {len(sites)}")

# For each literal pool entry, find LDR instructions that reference it
# Thumb LDR [pc,#imm]: target = align(pc+4)+imm == litaddr
# We can search backwards from lit for candidate LDR encodings

def find_ldr_to_lit(lit_off):
    lit_va = BASE + lit_off
    found = []
    # search previous 0x400 bytes for ldr rn, [pc, #imm] that resolve to lit
    start = max(0, lit_off - 0x400)
    chunk = data[start:lit_off]
    # naive: try disasm from each halfword
    for off in range(start, lit_off, 2):
        try:
            insns = list(md.disasm(data[off:off+4], BASE+off, count=1))
        except Exception:
            continue
        if not insns:
            continue
        insn = insns[0]
        if insn.mnemonic == "ldr" and "pc" in insn.op_str and "[" in insn.op_str:
            # parse imm
            import re
            m = re.search(r"#(0x[0-9a-fA-F]+|\d+)", insn.op_str)
            if not m:
                continue
            imm_s = m.group(1)
            imm = int(imm_s, 16) if imm_s.startswith("0x") else int(imm_s)
            pc = (insn.address + 4) & ~3
            if pc + imm == lit_va:
                found.append(insn.address)
    return found

# Focus on stores of state: search for "str rN, [rM, #0x78]" where rM was +0x80 from 0x7D8 base
# Broader: disasm regions that reference 0x7D8 and print str instructions

store_sites = []
for lit_off in sites[:28]:
    xrefs = find_ldr_to_lit(lit_off)
    for xa in xrefs:
        # disasm 0x60 from xref
        off = xa - BASE
        for insn in md.disasm(data[off:off+0x80], xa):
            if insn.mnemonic.startswith("str"):
                if "0xf8" in insn.op_str or "0x78" in insn.op_str or "0x80" in insn.op_str:
                    store_sites.append((xa, insn.address, insn.mnemonic, insn.op_str))
            if insn.address >= xa + 0x70:
                break

print("\nInteresting stores near 0x7D8 loads:")
for s in store_sites[:40]:
    print(f"  ldr@{hex(s[0])} -> {hex(s[1])}: {s[2]} {s[3]}")

# Also search globally for str to #0xf8
print("\nGlobal str * #0xf8 (sample):")
count = 0
for off in range(0, len(data) - 4, 2):
    insns = list(md.disasm(data[off:off+4], BASE+off, count=1))
    if not insns:
        continue
    insn = insns[0]
    if insn.mnemonic == "str" and "#0xf8" in insn.op_str:
        print(f"  {hex(insn.address)}: {insn.mnemonic} {insn.op_str}")
        count += 1
        if count >= 30:
            break
print("shown", count)
