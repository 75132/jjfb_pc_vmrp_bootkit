from pathlib import Path
import struct

blob = Path("out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext").read_bytes()
CB = 0x2D8DF4


def u16(o):
    return struct.unpack_from("<H", blob, o)[0]


def u32(o):
    return struct.unpack_from("<I", blob, o)[0]


def se(v, b):
    s = 1 << (b - 1)
    return (v & (s - 1)) - (v & s)


def bl_t(pc, h0, h1):
    if (h0 & 0xF800) != 0xF000 or (h1 & 0xC000) != 0xC000:
        return None
    s = (h0 >> 10) & 1
    imm10 = h0 & 0x3FF
    j1 = (h1 >> 13) & 1
    j2 = (h1 >> 11) & 1
    imm11 = h1 & 0x7FF
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    return (pc + 4 + se(imm32, 25)) & ~1


def dis(start, end):
    a = start
    while a < end:
        o = a - CB
        h = u16(o)
        pc = a
        if (h & 0xF800) == 0xF000:
            h1 = u16(o + 2)
            t = bl_t(pc, h, h1)
            ts = hex(t) if t is not None else "?"
            print(f"{pc:08X} BL {ts}")
            a += 4
            continue
        s = f"{pc:08X} {h:04X}"
        if (h & 0xF800) == 0x4800:
            lit = ((pc + 4) & ~2) + ((h & 0xFF) << 2)
            s += f" LDR r{(h>>8)&7}=0x{u32(lit-CB):X}"
        elif (h & 0xFF00) == 0x4400:
            rd = (((h >> 7) & 1) << 3) | (h & 7)
            rm = (h >> 3) & 0xF
            s += f" ADD r{rd},r{rm}"
        elif (h & 0xF800) == 0x2000:
            s += f" MOVS r{(h>>8)&7},#{h&0xFF}"
        elif (h & 0xFE00) == 0x1C00:
            s += f" ADDS r{h&7},r{(h>>3)&7},#{(h>>6)&7}"
        elif (h & 0xFF00) == 0xB500:
            s += " PUSH"
        elif (h & 0xF800) == 0x6800:
            s += f" LDR r{h&7},[r{(h>>3)&7},#{((h>>6)&0x1F)*4}]"
        print(s)
        a += 2


print("=== 2E2F40 ===")
dis(0x2E2F40, 0x2E2F80)
print("=== 2E2520 ===")
dis(0x2E2520, 0x2E25A0)
