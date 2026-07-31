import struct

data = open(r"out\tmp_gamelist_disasm\gamelist.ext", "rb").read()


def decode_bl(off):
    h1, h2 = struct.unpack_from("<HH", data, off)
    if (h1 & 0xF800) != 0xF000:
        return None
    if (h2 & 0xC000) != 0xC000:
        return None
    s = (h1 >> 10) & 1
    imm10 = h1 & 0x3FF
    j1 = (h2 >> 13) & 1
    j2 = (h2 >> 11) & 1
    imm11 = h2 & 0x7FF
    I1 = (~(j1 ^ s)) & 1
    I2 = (~(j2 ^ s)) & 1
    imm32 = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
    if s:
        imm32 -= 1 << 25
    target = off + 4 + imm32
    kind = "BLX" if (h2 & 0x1000) == 0 else "BL"
    return kind, target, h1, h2


for o in range(0xCE80, 0xCE98, 2):
    r = decode_bl(o)
    if r:
        print(f"0x{o:X}: {r[0]} -> 0x{r[1]:X} enc={r[2]:04X} {r[3]:04X}")

# find 0x6EE immediates near builder
print("scan ADD/LDR near 0x13A34 for 0x6EE:")
for i in range(0, 0x120, 2):
    hw = struct.unpack_from("<H", data, 0x13A34 + i)[0]
    # ADD Rd, #imm8 with high? look for literal 0x06EE nearby
print("literal 0x06EE occurrences:", [hex(i) for i in range(0, len(data) - 1) if data[i : i + 2] == b"\xee\x06"][:20])

# dump around builder for LDR r?, [r9, #imm]
print("\n--- builder first instructions ---")
for i in range(0, 96, 2):
    hw = struct.unpack_from("<H", data, 0x13A34 + i)[0]
    print(f"  +{i:02X}: {hw:04X}")
