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


# Full tail after C44 check — what does C9D path do?
print("=== handler tail 0x3066b4.. ===")
for insn in md.disasm(data[0x3066B4 - base : 0x3066B4 - base + 0x100], 0x3066B4):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.address > 0x306750:
        break

print()
# Who writes C9D? Search halfword/byte patterns - maybe offset in add
# Look at 0x2e48xx init that sets flags before enable C44
print("=== UI init 0x2e47e0 (sets flags then C44) ===")
for insn in md.disasm(data[0x2E47E0 - base : 0x2E47E0 - base + 0x80], 0x2E47E0):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.mnemonic == "pop" and "pc" in insn.op_str:
        break

print()
# Resolve what flag 0x2e4828 writes
# ldr r1,[pc,#0x314] at 0x2e4820 area - from earlier: ldr r1,[pc,#0x314] at 4820
for pc, boff in [(0x2E4820, 0x314), (0x2E4826, 0x324)]:
    # check encoding
    w = struct.unpack_from("<H", data, pc - base)[0]
    if (w & 0xF800) == 0x4800:
        imm = (w & 0xFF) * 4
        print("pc 0x%X lit -> ER_RW+0x%X" % (pc, u32(lit(pc, imm))))

# Capstone showed: ldr r1, [pc, #0x314] at 0x2e4820 - that's wide? 
# Actually: 0x2e4820: ldr r1, [pc, #0x314] - Thumb1 max imm is 0x3FC, 0x314 is OK
raw = struct.unpack_from("<H", data, 0x2E4820 - base)[0]
print("raw 2e4820=%04X" % raw)
if (raw & 0xF800) == 0x4800:
    print("flag before C44 = ER_RW+0x%X" % u32(lit(0x2E4820, (raw & 0xFF) * 4)))

# Search drawBitmap / DispUp via helper table calls - look for bl to known refresh
# In robotol, refresh often goes through function pointer. Search string "ref" or calls to 0x304550 with draw-ish
# Check 0x2e87ac early exit: cmp r2,#0x45 — what is r2?
print("\n=== 0x2e87ac state cmp 0x45 source ===")
# ldr r2,[pc,#0xac] at 0x2e87d0
raw = struct.unpack_from("<H", data, 0x2E87D0 - base)[0]
print("2e87d0=%04X" % raw)
# Might be wide LDR. Disasm already said ldr r2, [pc, #0xac]
la = lit(0x2E87D0, 0xAC)
# Wait - for thumb LDR literal imm is in bytes from capstone. Capstone #0xac means byte offset
print("state field ER_RW+0x%X (cmp to 0x45)" % u32(la))

# Also find family case that might do screen refresh - search bl 0x304550 near draw
# Look at 0x30ddda caller of enable
print("\n=== caller 0x30ddda ===")
for insn in md.disasm(data[0x30DD80 - base : 0x30DD80 - base + 0xA0], 0x30DD80):
    print("0x%08x:\t%s\t%s" % (insn.address, insn.mnemonic, insn.op_str))
    if insn.address > 0x30DE00:
        break
