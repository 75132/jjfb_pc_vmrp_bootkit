#!/usr/bin/env python3
"""P13 helper: decode 0x2D9600 and find 0x1E205 producers."""
from __future__ import annotations

import struct
from pathlib import Path
from typing import List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[2]
BLOB = ROOT / "out/research_p9_extract/robotol.ext"
BASE = 0x2D8DF4


def u16(b: bytes, o: int) -> int:
    return struct.unpack_from("<H", b, o)[0]


def u32(b: bytes, o: int) -> int:
    return struct.unpack_from("<I", b, o)[0]


def sx(v: int, bits: int) -> int:
    s = 1 << (bits - 1)
    return (v & (s - 1)) - (v & s)


def bl_target(pc: int, h0: int, h1: int) -> Optional[int]:
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
    imm32 = sx(imm32, 25)
    return (pc + 4 + imm32) & ~1


def decode(blob: bytes, pc: int) -> Tuple[int, str]:
    o = pc - BASE
    h = u16(blob, o)
    if (h & 0xF800) == 0xF000 and o + 3 < len(blob):
        h1 = u16(blob, o + 2)
        t = bl_target(pc, h, h1)
        if t is not None:
            kind = "BL" if (h1 & 0x1000) else "BLX"
            return 4, f"{kind} 0x{t:X}"
        return 4, f"T32 0x{h:04X} 0x{h1:04X}"
    if (h & 0xFF00) == 0xB500:
        regs = [f"r{i}" for i in range(8) if h & (1 << i)]
        if h & 0x100:
            regs.append("lr")
        return 2, "PUSH {" + ",".join(regs) + "}"
    if (h & 0xFF00) == 0xBD00:
        regs = [f"r{i}" for i in range(8) if h & (1 << i)]
        if h & 0x100:
            regs.append("pc")
        return 2, "POP {" + ",".join(regs) + "}"
    if (h & 0xFF80) == 0xB080:
        op = "SUB" if (h & 0x80) else "ADD"
        return 2, f"{op} sp,#0x{(h & 0x7F) * 4:X}"
    if (h & 0xFE00) == 0x1C00:
        return 2, f"ADDS r{h & 7},r{(h >> 3) & 7},#{(h >> 6) & 7}"
    if (h & 0xF000) == 0xD000:
        conds = "EQ NE CS CC MI PL VS VC HI LS GE LT GT LE".split()
        cond = (h >> 8) & 0xF
        imm = sx(h & 0xFF, 8) << 1
        cname = conds[cond] if cond < len(conds) else "?"
        return 2, f"B{cname} 0x{pc + 4 + imm:X}"
    if (h & 0xF800) == 0x6800:
        return 2, f"LDR r{h & 7},[r{(h >> 3) & 7},#0x{((h >> 6) & 0x1F) * 4:X}]"
    if (h & 0xF800) == 0x6000:
        return 2, f"STR r{h & 7},[r{(h >> 3) & 7},#0x{((h >> 6) & 0x1F) * 4:X}]"
    if (h & 0xF800) == 0x4800:
        imm = (h & 0xFF) * 4
        lit = ((pc + 4) & ~2) + imm
        val = u32(blob, lit - BASE)
        return 2, f"LDR r{(h >> 8) & 7},[pc,#0x{imm:X}] ; =0x{val:X}"
    if (h & 0xFF87) == 0x4780:
        return 2, f"BLX r{(h >> 3) & 0xF}"
    if (h & 0xFF87) == 0x4700:
        return 2, f"BX r{(h >> 3) & 0xF}"
    if (h & 0xFF00) == 0x4600:
        rd = ((h & 0x80) >> 4) | (h & 7)
        rm = (h >> 3) & 0xF
        return 2, f"MOV r{rd},r{rm}"
    if (h & 0xF800) == 0x2000:
        return 2, f"MOVS r{(h >> 8) & 7},#0x{h & 0xFF:X}"
    if (h & 0xF800) == 0x2800:
        return 2, f"CMP r{(h >> 8) & 7},#0x{h & 0xFF:X}"
    if (h & 0xFFC0) == 0x4280:
        return 2, f"CMP r{h & 7},r{(h >> 3) & 7}"
    if (h & 0xF800) == 0xE000:
        imm = sx(h & 0x7FF, 11) << 1
        return 2, f"B 0x{pc + 4 + imm:X}"
    if (h & 0xF800) == 0x9800:
        return 2, f"LDR r{(h >> 8) & 7},[sp,#0x{(h & 0xFF) * 4:X}]"
    if (h & 0xF800) == 0x9000:
        return 2, f"STR r{(h >> 8) & 7},[sp,#0x{(h & 0xFF) * 4:X}]"
    if (h & 0xF800) == 0x3000:
        return 2, f"ADDS r{(h >> 8) & 7},#0x{h & 0xFF:X}"
    if (h & 0xFF00) == 0x4400:
        rd = ((h & 0x80) >> 4) | (h & 7)
        rm = (h >> 3) & 0xF
        return 2, f"ADD r{rd},r{rm}"
    return 2, f"??? 0x{h:04X}"


def main() -> None:
    blob = BLOB.read_bytes()
    print("=== fn 0x2D9600 (forced, no early stop) ===")
    pc = 0x2D9600
    for _ in range(40):
        n, text = decode(blob, pc)
        mark = "  <<<< FAULT" if pc == 0x2D960E else ""
        print(f"0x{pc:X}: {text}{mark}")
        pc += n

    target = 0x2D9600
    callers: List[int] = []
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(BASE + o, u16(blob, o), u16(blob, o + 2))
        if t == target:
            callers.append(BASE + o)
    print(f"\nBL callers of 0x2D9600: count={len(callers)}")
    for c in callers[:50]:
        # show 8 insns before call
        print(f"  caller 0x{c:X}:")
        p = c - 16
        while p < c + 4:
            if p < BASE:
                p += 2
                continue
            n, text = decode(blob, p)
            flag = "  *" if p == c else ""
            print(f"    0x{p:X}: {text}{flag}")
            p += n

    for lit in (0x1E205, 0x1E201, 0x1E209):
        sites = [BASE + o for o in range(0, len(blob) - 3, 4) if u32(blob, o) == lit]
        print(f"\nlitpool 0x{lit:X}: count={len(sites)} first={[hex(x) for x in sites[:15]]}")


if __name__ == "__main__":
    main()
