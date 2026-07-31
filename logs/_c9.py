raw=open(r"out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext","rb").read()
base=0x2D8DF4
dest=0x30E1A0
fo=dest-base
b=raw[fo:fo+64]
print(b.hex())
# decode a few
i=0
while i<48:
    h=b[i]|(b[i+1]<<8)
    if (h&0xF800) in (0xF000,0xF800,0xE800):
        h2=b[i+2]|(b[i+3]<<8)
        print(f"0x{dest+i:08X}: {h:04x}{h2:04x}")
        i+=4; continue
    print(f"0x{dest+i:08X}: {h:04x}")
    i+=2
