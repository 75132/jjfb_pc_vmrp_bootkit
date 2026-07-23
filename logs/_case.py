raw=open(r"out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext","rb").read()
base=0x2D8DF4
# case_uhi table at 0x30D324, offsets as u16 LE
tbl=0x30D324
for case in range(12):
    fo=tbl-base+case*2
    off=raw[fo]| (raw[fo+1]<<8)
    dest=tbl+off*2
    print(f"case {case}: off=0x{off:x} dest=0x{dest:08X}")
# dump case 9 first 32 bytes
fo9=0x30D324+0x73e*2 - base  # from earlier halfword at case9
# re-read case9 halfword
fo=tbl-base+9*2
off=raw[fo]|(raw[fo+1]<<8)
dest=tbl+off*2
print("case9 dest", hex(dest), "bytes", raw[dest-base:dest-base+32].hex())
# also case for app=0x1E209&0xff = 9 same
# What if switch is on (event&0xff)? same
# Early exit path at bcs +12 from 0x30D314 -> 0x30D314+4+12=0x30D324? 
# bcs imm8: dest = pc+4+imm*2, pc at insn = 0x30D314
# imm=4 -> 0x30D314+4+8=0x30D320 ? that's the bl, weird
print("bcs target", hex(0x30D314+4+4*2))
