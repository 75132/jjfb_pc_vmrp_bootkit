#!/usr/bin/env python3
"""Minimal Thumb disassembler for a code slice (no capstone)."""
from __future__ import annotations
import sys

REGS = ["r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"]


def reg(n: int) -> str:
    return REGS[n]


def disasm(data: bytes, va: int) -> None:
    i = 0
    while i + 1 < len(data):
        addr = va + i
        h = data[i] | (data[i + 1] << 8)
        b15_11 = (h >> 11) & 0x1F
        if b15_11 in (0x1D, 0x1E, 0x1F):
            if i + 3 >= len(data):
                break
            h2 = data[i + 2] | (data[i + 3] << 8)
            w = (h << 16) | h2
            mnem = f"32b 0x{w:08X}"
            if (h & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xD000:
                s = (h >> 10) & 1
                imm10 = h & 0x3FF
                j1 = (h2 >> 13) & 1
                j2 = (h2 >> 11) & 1
                imm11 = h2 & 0x7FF
                I1 = 1 - (j1 ^ s)
                I2 = 1 - (j2 ^ s)
                imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
                if s:
                    imm -= 1 << 25
                mnem = f"bl 0x{addr + 4 + imm:X}"
            elif (h & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xC000:
                s = (h >> 10) & 1
                imm10 = h & 0x3FF
                j1 = (h2 >> 13) & 1
                j2 = (h2 >> 11) & 1
                imm11 = h2 & 0x7FF
                I1 = 1 - (j1 ^ s)
                I2 = 1 - (j2 ^ s)
                imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
                if s:
                    imm -= 1 << 25
                mnem = f"blx 0x{(addr + 4 + imm) & ~1:X}"
            elif (h & 0xFFE0) == 0xF8C0:
                rt = (h2 >> 12) & 0xF
                rn = h & 0xF
                imm = h2 & 0xFFF
                mnem = f"str.w {reg(rt)}, [{reg(rn)}, #0x{imm:X}]"
            elif (h & 0xFFE0) == 0xF8D0:
                rt = (h2 >> 12) & 0xF
                rn = h & 0xF
                imm = h2 & 0xFFF
                mnem = f"ldr.w {reg(rt)}, [{reg(rn)}, #0x{imm:X}]"
            elif (h & 0xFFF0) == 0xF240:
                ii = (h >> 10) & 1
                imm4 = h & 0xF
                imm3 = (h2 >> 12) & 7
                imm8 = h2 & 0xFF
                rd = (h2 >> 8) & 0xF
                imm = (imm4 << 12) | (ii << 11) | (imm3 << 8) | imm8
                mnem = f"movw {reg(rd)}, #0x{imm:X}"
            elif (h & 0xFFF0) == 0xF2C0:
                ii = (h >> 10) & 1
                imm4 = h & 0xF
                imm3 = (h2 >> 12) & 7
                imm8 = h2 & 0xFF
                rd = (h2 >> 8) & 0xF
                imm = (imm4 << 12) | (ii << 11) | (imm3 << 8) | imm8
                mnem = f"movt {reg(rd)}, #0x{imm:X}"
            print(f"0x{addr:06X}: {data[i]:02x}{data[i+1]:02x}{data[i+2]:02x}{data[i+3]:02x}  {mnem}")
            i += 4
            continue

        mnem = f"??? 0x{h:04X}"
        if (h & 0xF800) == 0x6000:
            rt = h & 7
            rn = (h >> 3) & 7
            imm = ((h >> 6) & 0x1F) << 2
            mnem = f"str {reg(rt)}, [{reg(rn)}, #0x{imm:X}]"
        elif (h & 0xF800) == 0x6800:
            rt = h & 7
            rn = (h >> 3) & 7
            imm = ((h >> 6) & 0x1F) << 2
            mnem = f"ldr {reg(rt)}, [{reg(rn)}, #0x{imm:X}]"
        elif (h & 0xF800) == 0x2000:
            rd = (h >> 8) & 7
            imm = h & 0xFF
            mnem = f"movs {reg(rd)}, #0x{imm:X}"
        elif (h & 0xF800) == 0x3000:
            rd = (h >> 8) & 7
            imm = h & 0xFF
            mnem = f"adds {reg(rd)}, #0x{imm:X}"
        elif (h & 0xFF00) == 0x4700:
            rm = (h >> 3) & 0xF
            mnem = f"bx {reg(rm)}" if (h & 0x80) == 0 else f"blx {reg(rm)}"
        elif (h & 0xFF00) == 0x4600:
            rd = h & 7
            rm = (h >> 3) & 0xF
            if h & 0x80:
                rd += 8
            mnem = f"mov {reg(rd)}, {reg(rm)}"
        elif (h & 0xFFC0) == 0x4400:
            rd = h & 7
            rm = (h >> 3) & 0xF
            if h & 0x80:
                rd += 8
            mnem = f"add {reg(rd)}, {reg(rm)}"
        elif (h & 0xF800) == 0x4800:
            rt = (h >> 8) & 7
            imm = (h & 0xFF) << 2
            mnem = f"ldr {reg(rt)}, [pc, #0x{imm:X}]"
        elif (h & 0xF800) == 0xE000:
            imm = (h & 0x7FF) << 1
            if imm & 0x800:
                imm -= 0x1000
            mnem = f"b 0x{addr + 4 + imm:X}"
        elif (h & 0xF000) == 0xD000:
            cond = (h >> 8) & 0xF
            imm = (h & 0xFF) << 1
            if imm & 0x100:
                imm -= 0x200
            mnem = f"b.cond({cond}) 0x{addr + 4 + imm:X}"
        elif (h & 0xFE00) == 0x1C00:
            rd = h & 7
            rn = (h >> 3) & 7
            imm = (h >> 6) & 7
            mnem = f"adds {reg(rd)}, {reg(rn)}, #{imm}"
        elif (h & 0xFF80) == 0xB080:
            imm = (h & 0x7F) << 2
            mnem = f"sub sp, #0x{imm:X}" if (h & 0x80) else f"add sp, #0x{imm:X}"
        elif (h & 0xFE00) == 0xB400:
            mnem = f"push {{regs=0x{h & 0x1FF:X}}}"
        elif (h & 0xFE00) == 0xBC00:
            mnem = f"pop {{regs=0x{h & 0x1FF:X}}}"
        elif (h & 0xF800) == 0x2800:
            rn = (h >> 8) & 7
            imm = h & 0xFF
            mnem = f"cmp {reg(rn)}, #0x{imm:X}"
        elif (h & 0xFFC0) == 0x4280:
            rn = h & 7
            rm = (h >> 3) & 7
            mnem = f"cmp {reg(rn)}, {reg(rm)}"
        elif (h & 0xF800) == 0x7000:
            rt = h & 7
            rn = (h >> 3) & 7
            imm = (h >> 6) & 0x1F
            mnem = f"strb {reg(rt)}, [{reg(rn)}, #{imm}]"
        elif (h & 0xF800) == 0x7800:
            rt = h & 7
            rn = (h >> 3) & 7
            imm = (h >> 6) & 0x1F
            mnem = f"ldrb {reg(rt)}, [{reg(rn)}, #{imm}]"
        print(f"0x{addr:06X}: {data[i]:02x}{data[i+1]:02x}        {mnem}")
        i += 2


def main() -> None:
    path = sys.argv[1]
    code_base = int(sys.argv[2], 0)
    va = int(sys.argv[3], 0)
    n = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x80
    data = open(path, "rb").read()
    off = va - code_base
    disasm(data[off : off + n], va)


if __name__ == "__main__":
    main()
