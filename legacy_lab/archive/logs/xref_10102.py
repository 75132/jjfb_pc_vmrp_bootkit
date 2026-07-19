from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

# Find code that loads 0x10102 from pool at 0x304a1c
pool = 0x304A1C
# LDR [pc,#imm] targets align(pc+4)+imm == pool
# Scan 0x400 bytes before pool
start = pool - 0x400 - BASE
chunk_off = max(0, start)
print("Searching xrefs to 0x10102 pool...")
for off in range(chunk_off, pool - BASE, 2):
    insns = list(md.disasm(data[off : off + 4], BASE + off, count=1))
    if not insns:
        continue
    insn = insns[0]
    if insn.mnemonic != "ldr" or "pc" not in insn.op_str:
        continue
    import re
    m = re.search(r"#(0x[0-9a-fA-F]+|\d+)", insn.op_str)
    if not m:
        continue
    imm_s = m.group(1)
    imm = int(imm_s, 16) if imm_s.startswith("0x") else int(imm_s)
    pc = (insn.address + 4) & ~3
    if pc + imm == pool:
        print(f"xref {hex(insn.address)}: {insn.mnemonic} {insn.op_str}")
        # disasm surrounding function
        fa = insn.address - 0x20
        print("--- context ---")
        for i2 in md.disasm(data[fa - BASE : fa - BASE + 0x80], fa):
            print(f"0x{i2.address:X}:\t{i2.mnemonic}\t{i2.op_str}")
            if i2.address >= insn.address + 0x40:
                break

# Also decode switch case 9 target from table at 0x30D2F8
# After add pc, r3 the table is at adr #0xc from 0x30D30E
# adr r3, #0xc at 0x30D30E -> table at align? ADR is (pc&~3)+imm for thumb
# Thumb ADR: pc = (addr+4)&~3; + imm
adr_insn = 0x30D30E
table = ((adr_insn + 4) & ~3) + 0xC
print(f"\nswitch table at {hex(table)}")
# each entry halfword, index*2 for ldrh [r3, r0] wait: ldrh r3, [r3, r0] then lsl #1; but also adds r3, r3, r0 before
# adds r3, r3, r0; ldrh r3, [r3, r0]; lsls r3, r3, #1; add pc, r3
# So offset = table + index + index = table + 2*index, loads halfword, *2 added to pc
# PC for add pc is (addr+4) of the add pc insn at 0x30D316 -> pc=0x30D31A?
# For add pc, reg in thumb: PC = (instruction_address + 4) + reg

add_pc_addr = 0x30D316
pc_base = add_pc_addr + 4  # 0x30D31A
for idx in [0, 1, 3, 4, 5, 9, 0xB, 0xC, 0x17, 0x18]:
    off = table - BASE + 2 * idx  # ldrh [table+idx, idx] = table+2*idx
    # Wait: adds r3,r3,r0 then ldrh r3,[r3,r0] => address = table + r0 + r0 = table + 2*r0
    ent_off = table - BASE + 2 * idx
    if ent_off < 0 or ent_off + 2 > len(data):
        continue
    hw = struct.unpack_from("<H", data, ent_off)[0]
    target = pc_base + (hw << 1)
    print(f"  case {idx}: entry={hw} -> {hex(target)}")
