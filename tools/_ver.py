from capstone import *
import pathlib, struct
data = pathlib.Path(r'out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext').read_bytes()
base = 0x2D8DF4

def thumb_lit(pc, imm):
    return ((pc + 4) & ~3) + imm

# 0x2F6C46: ldr r1, [pc, #0x58]
lit = thumb_lit(0x2F6C46, 0x58)
off = struct.unpack_from('<i', data, lit - base)[0]
# after add r1, pc at 0x2F6C4A: r1 = pc + off, pc for add is 0x2F6C4A+4=0x2F6C4E
str_va = (0x2F6C4A + 4) + off
print(f'lit@0x{lit:X} off={off} str_va=0x{str_va:X}')
soff = str_va - base
s = data[soff:soff+64]
print('suffix bytes', s[:40])
print('suffix str', s.split(b'\x00')[0])

# Also 0x2F6C54 ldr r0,[pc,#0x4c] then add r0,sb — ER_RW list offset
lit2 = thumb_lit(0x2F6C54, 0x4c)
off2 = struct.unpack_from('<i', data, lit2 - base)[0]
print(f'list lit@0x{lit2:X} erw_off=0x{off2:X}')

# disasm 0x308d98 and 0x2f6ba0
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
def dis(va, n=64):
    print(f'=== 0x{va:X} ===')
    for i in md.disasm(data[va-base:va-base+n], va):
        print(f'0x{i.address:08X}: {i.mnemonic} {i.op_str}')
dis(0x308d98, 80)
dis(0x2f6ba0, 64)
