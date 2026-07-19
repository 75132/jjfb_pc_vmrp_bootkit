from capstone import *
import struct

binpath = r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\robotol_ext.bin"
data = open(binpath, "rb").read()
BASE = 0x2D8DEC

# At 0x30491A: ldr r2, [pc, #0xfc]
# pc = (0x30491A+4)&~3 = 0x30491C; lit = 0x30491C+0xFC = 0x304A18
lit = 0x304A18
off = lit - BASE
val = struct.unpack_from("<I", data, off)[0]
print(f"r2 lit @ {hex(lit)} = {hex(val)}")
# then add r2, pc at 0x304922: r2 = pc + val where pc=(0x304922+4)&~3=0x304924
# Thumb add rd, pc is actually ADR-like: encoding "add r2, pc" 
# In Capstone: add r2, pc means r2 = r2 + pc? or ADR?
# Looking at insn: "add r2, pc" after "ldr r2, [pc,#]" - typically this is:
#   ldr r2, [pc, #lit]  ; load offset
#   add r2, pc          ; r2 = pc + offset  (Thumb-2 or special)

# In ARM Thumb, `add rN, pc` as 16-bit is actually not standard.
# Capstone showed: add r2, pc  - might be `add r2, pc, r2` encoded as T2
# At 0x304922 - let's read raw bytes

raw = data[0x304922 - BASE : 0x304922 - BASE + 8]
print("bytes at 0x304922", raw.hex())

# Common pattern for position-independent: 
# ldr r2, =label - (pc+4); add r2, pc
pc_at_add = 0x304924  # typical for add r2,pc following
# If ldr loaded offset relative to next pc:
target = pc_at_add + val
# or if val is absolute already
print(f"if r2=pc+val: {hex(target)}")
print(f"if r2=val only: {hex(val)}")

# Check 0x30D2F9
print("expected handler", hex(0x30D2F9))

# Disasm case 9 body
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
print("\n=== case 9 body @ 0x30e196 ===")
for insn in md.disasm(data[0x30E196 - BASE : 0x30E196 - BASE + 0x50], 0x30E196):
    print(f"0x{insn.address:X}:\t{insn.mnemonic}\t{insn.op_str}")
