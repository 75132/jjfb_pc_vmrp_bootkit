raw = open(r"out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext","rb").read()
base = 0x2D8DF4
fo = 0x30D300 - base
b = raw[fo:fo+128]
# Minimal thumb16 decode for prologue
def u16(i):
    return b[i] | (b[i+1]<<8)
i=0
pc=0x30D300
while i < 80:
    h=u16(i)
    # 32-bit?
    if (h & 0xF800) in (0xF000, 0xF800, 0xE800) and i+3 < len(b):
        h2=u16(i+2)
        print(f"0x{pc+i:08X}: {h:04x} {h2:04x}  (32-bit)")
        i+=4
        continue
    mnem="?"
    if h==0xB570: mnem="push {r4-r6,lr}"
    elif h==0x1C0C: mnem="mov r4, r1"
    elif h==0x1C19: mnem="mov r1, r3"
    elif h==0x23FF: mnem="movs r3, #0xFF"
    elif h==0xB084: mnem="sub sp, #16"
    elif h==0x1C15: mnem="mov r5, r2"
    elif (h&0xFF00)==0x3300: mnem=f"adds r3, #{h&0xFF}"
    elif h==0x4298: mnem="cmp r0, r3"
    elif (h&0xFF00)==0xD200: mnem=f"bcs +{(h&0xFF)*2+4}"
    elif (h&0xFF00)==0xD000: mnem=f"beq +{(h&0xFF)*2+4}"
    elif (h&0xF800)==0x4800: mnem=f"ldr r{ (h>>8)&7 }, [pc, #{(h&0xFF)*4}]"
    elif (h&0xF800)==0x9800: mnem=f"ldr r{(h>>8)&7}, [sp, #{(h&0xFF)*4}]"
    elif (h&0xFF00)==0xB000: mnem=f"add sp, #{(h&0x7F)*4}" if (h&0x80)==0 else f"sub sp, #{(h&0x7F)*4}"
    elif h==0xBD70: mnem="pop {r4-r6,pc}"
    elif (h&0xFF00)==0x2000: mnem=f"movs r0, #{h&0xFF}"
    elif (h&0xFF00)==0x2100: mnem=f"movs r1, #{h&0xFF}"
    print(f"0x{pc+i:08X}: {h:04x}  {mnem}")
    i+=2
print("--- 10165 stub ---")
fo2=0x30D2F8-base
b2=raw[fo2:fo2+16]
print(b2.hex())
