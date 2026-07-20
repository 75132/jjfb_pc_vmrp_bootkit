from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

rob = Path("out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext").read_bytes()
# From CROSS_MODULE: robotol target=0x2D8DFC target_offset=0x8 => base=0x2D8DF4
base = 0x2D8DF4
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
md.detail = True

def disasm_range(va_start, size, label):
    off = va_start - base
    print(f"\n=== {label} VA=0x{va_start:X} file=0x{off:X} ===")
    code = rob[off : off + size]
    for insn in md.disasm(code, va_start):
        print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")

# 304BF0 function prologue through compare loop
disasm_range(0x304BF0, 0x120, "304BF0_start")
disasm_range(0x304F00, 0xC0, "compare_loop")

# Also try to find cfunction for 0x94E94
cfn_paths = list(Path(".").rglob("cfunction.ext"))
print("\ncfunction candidates:", cfn_paths[:8])
for p in cfn_paths[:5]:
    data = p.read_bytes()
    # DSM often loaded at 0x80000
    for b in (0x80000, 0x0):
        off = 0x94E94 - b
        if 0 <= off < len(data) - 64:
            print(f"\n=== strcmp? {p} base=0x{b:X} ===")
            for insn in md.disasm(data[off : off + 80], 0x94E94 if b else off):
                print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
            break
