#!/usr/bin/env python3
"""Decode 10140 timer path gates leading to sendAppEvent(0x1E209,9)."""
import struct
from pathlib import Path

blob = Path("out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext").read_bytes()
BASE = 0x2D8DF4


def u16(o):
    return struct.unpack_from("<H", blob, o)[0]


def u32(o):
    return struct.unpack_from("<I", blob, o)[0]


def bl_t(pc, h0, h1):
    s = (h0 >> 10) & 1
    imm10 = h0 & 0x3FF
    j1 = (h1 >> 13) & 1
    j2 = (h1 >> 11) & 1
    imm11 = h1 & 0x7FF
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    if imm32 & (1 << 24):
        imm32 -= 1 << 25
    return (pc + 4 + imm32) & ~1


def dis(pc, nmax=80):
    p = pc
    for _ in range(nmax):
        o = p - BASE
        h = u16(o)
        if (h & 0xF800) == 0xF000:
            h1 = u16(o + 2)
            if (h1 & 0xC000) == 0xC000:
                t = bl_t(p, h, h1)
                print(f"  0x{p:X}: BL 0x{t:X}")
                p += 4
                continue
        # ADD/MOV high regs
        if (h & 0xFF00) == 0x4400:
            rd = ((h >> 4) & 8) | (h & 7)
            rm = (h >> 3) & 0xF
            op = "ADD" if (h & 0x0300) == 0x0000 else "?"
            if (h & 0x0300) == 0x0000:
                print(f"  0x{p:X}: ADD r{rd},r{rm}")
            elif (h & 0x0300) == 0x0200:
                print(f"  0x{p:X}: MOV r{rd},r{rm}")
            else:
                print(f"  0x{p:X}: hi-op 0x{h:04X} rd={rd} rm={rm}")
            p += 2
            continue
        if (h & 0xFF00) == 0x4500:
            print(f"  0x{p:X}: CMP hi 0x{h:04X}")
            p += 2
            continue
        if (h & 0xFF00) in (0xB400, 0xB500):
            regs = [f"r{i}" for i in range(8) if h & (1 << i)]
            if h & 0x100:
                regs.append("lr")
            print(f"  0x{p:X}: PUSH {{{','.join(regs)}}}")
            p += 2
            continue
        if (h & 0xFF00) in (0xBC00, 0xBD00):
            regs = [f"r{i}" for i in range(8) if h & (1 << i)]
            if h & 0x100:
                regs.append("pc")
            print(f"  0x{p:X}: POP {{{','.join(regs)}}}")
            p += 2
            continue
        if (h & 0xFF80) == 0xB080:
            imm = (h & 0x7F) * 4
            print(f"  0x{p:X}: {'SUB' if h & 0x80 else 'ADD'} sp,#0x{imm:X}")
            p += 2
            continue
        if (h & 0xF800) == 0x4800:
            rt = (h >> 8) & 7
            imm = (h & 0xFF) * 4
            lit = ((p + 4) & ~2) + imm
            val = u32(lit - BASE)
            print(f"  0x{p:X}: LDR r{rt},[pc,#0x{imm:X}] ; =0x{val:X}")
            p += 2
            continue
        if (h & 0xF800) == 0x2000:
            print(f"  0x{p:X}: MOVS r{(h >> 8) & 7},#0x{h & 0xFF:X}")
            p += 2
            continue
        if (h & 0xF800) == 0x2800:
            print(f"  0x{p:X}: CMP r{(h >> 8) & 7},#0x{h & 0xFF:X}")
            p += 2
            continue
        if (h & 0xF000) == 0xD000:
            cond = (h >> 8) & 0xF
            imm = ((h & 0xFF) ^ 0x80) - 0x80
            names = {
                0: "EQ",
                1: "NE",
                2: "CS",
                3: "CC",
                4: "MI",
                5: "PL",
                6: "VS",
                7: "VC",
                8: "HI",
                9: "LS",
                10: "GE",
                11: "LT",
                12: "GT",
                13: "LE",
            }
            print(f"  0x{p:X}: B{names.get(cond,'??')} 0x{p + 4 + (imm << 1):X}")
            p += 2
            continue
        if (h & 0xF800) == 0xE000:
            imm = h & 0x7FF
            if imm & 0x400:
                imm -= 0x800
            print(f"  0x{p:X}: B 0x{p + 4 + (imm << 1):X}")
            p += 2
            continue
        if (h & 0xF800) == 0x9000:
            print(f"  0x{p:X}: STR r{(h >> 8) & 7},[sp,#0x{(h & 0xFF) * 4:X}]")
            p += 2
            continue
        if (h & 0xF800) == 0x9800:
            print(f"  0x{p:X}: LDR r{(h >> 8) & 7},[sp,#0x{(h & 0xFF) * 4:X}]")
            p += 2
            continue
        if (h & 0xFE00) == 0x1E00:
            print(f"  0x{p:X}: SUBS r{h & 7},r{(h >> 3) & 7},#{(h >> 6) & 7}")
            p += 2
            continue
        if (h & 0xFE00) == 0x1C00:
            print(f"  0x{p:X}: ADDS r{h & 7},r{(h >> 3) & 7},#{(h >> 6) & 7}")
            p += 2
            continue
        if (h & 0xF800) == 0x3000:
            print(f"  0x{p:X}: ADDS r{(h >> 8) & 7},#0x{h & 0xFF:X}")
            p += 2
            continue
        if (h & 0xF800) == 0x3800:
            print(f"  0x{p:X}: SUBS r{(h >> 8) & 7},#0x{h & 0xFF:X}")
            p += 2
            continue
        if (h & 0xF800) == 0x6800:
            print(f"  0x{p:X}: LDR r{h & 7},[r{(h >> 3) & 7},#0x{((h >> 6) & 0x1F) * 4:X}]")
            p += 2
            continue
        if (h & 0xF800) == 0x6000:
            print(f"  0x{p:X}: STR r{h & 7},[r{(h >> 3) & 7},#0x{((h >> 6) & 0x1F) * 4:X}]")
            p += 2
            continue
        if (h & 0xF800) == 0x7800:
            print(f"  0x{p:X}: LDRB r{h & 7},[r{(h >> 3) & 7},#0x{(h >> 6) & 0x1F:X}]")
            p += 2
            continue
        if (h & 0xF800) == 0x7000:
            print(f"  0x{p:X}: STRB r{h & 7},[r{(h >> 3) & 7},#0x{(h >> 6) & 0x1F:X}]")
            p += 2
            continue
        if (h & 0xFFC0) == 0x5600:
            print(f"  0x{p:X}: LDRSB r{h & 7},[r{(h >> 3) & 7},r{(h >> 6) & 7}]")
            p += 2
            continue
        if (h & 0xFFC0) == 0x4280:
            print(f"  0x{p:X}: CMP r{h & 7},r{(h >> 3) & 7}")
            p += 2
            continue
        print(f"  0x{p:X}: ??? 0x{h:04X}")
        p += 2


print("=== 0x30630C timer entry ===")
dis(0x30630C, 100)
print("\n=== path to 0x30666A ===")
dis(0x3065E0, 50)
