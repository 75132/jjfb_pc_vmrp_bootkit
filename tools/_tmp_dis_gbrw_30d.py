#!/usr/bin/env python3
from pathlib import Path

data = Path(r"out/tmp_gbrwcore_disasm/gbrwcore.ext").read_bytes()
base = 0x300000


def half(va: int) -> int:
    off = va - base
    return int.from_bytes(data[off : off + 2], "little")


def word(va: int) -> int:
    off = va - base
    return int.from_bytes(data[off : off + 4], "little")


def sx(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & ((1 << bits) - 1) ^ sign) - sign


RN = [
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "sp",
    "lr",
    "pc",
]
COND = {
    0: "eq",
    1: "ne",
    2: "cs",
    3: "cc",
    4: "mi",
    5: "pl",
    6: "vs",
    7: "vc",
    8: "hi",
    9: "ls",
    10: "ge",
    11: "lt",
    12: "gt",
    13: "le",
}


def dis_range(start: int, end: int) -> None:
    va = start
    while va < end:
        h = half(va)
        h2 = half(va + 2) if va + 2 < end + 4 else 0
        if (h & 0xF800) == 0xF000 and (h2 & 0xE800) == 0xE800:
            s = (h >> 10) & 1
            imm10 = h & 0x3FF
            j1 = (h2 >> 13) & 1
            j2 = (h2 >> 11) & 1
            imm11 = h2 & 0x7FF
            i1 = (~(j1 ^ s)) & 1
            i2 = (~(j2 ^ s)) & 1
            imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm32 |= 0xFE000000
            if imm32 & 0x80000000:
                imm32 -= 0x100000000
            target = (va + 4 + imm32) & 0xFFFFFFFF
            kind = "BLX" if (h2 & 0x1000) == 0 else "BL"
            print(f"0x{va:X}: {kind} 0x{target:X}  ; {h:04X} {h2:04X}")
            va += 4
            continue

        op = f".hword 0x{h:04X}"
        if (h & 0xFE00) == 0xB400:
            regs = [RN[i] for i in range(8) if h & (1 << i)]
            if h & 0x100:
                regs.append("lr" if (h & 0xFE00) == 0xB400 else "pc")
            # push B4xx / B5xx
            if (h & 0xFF00) == 0xB500 or (h & 0xFF00) == 0xB400:
                regs = [RN[i] for i in range(8) if h & (1 << i)]
                if h & 0x0100:
                    regs.append("lr")
                op = "push {" + ",".join(regs) + "}"
        if (h & 0xFF00) == 0xBD00 or (h & 0xFF00) == 0xBC00:
            regs = [RN[i] for i in range(8) if h & (1 << i)]
            if h & 0x0100:
                regs.append("pc")
            op = "pop {" + ",".join(regs) + "}"
        elif (h & 0xF800) == 0x2000:
            op = f"movs {RN[(h >> 8) & 7]}, #{h & 0xFF}"
        elif (h & 0xFE00) == 0x1C00:
            rd, rn, imm = h & 7, (h >> 3) & 7, (h >> 6) & 7
            op = f"adds {RN[rd]}, {RN[rn]}, #{imm}"
        elif (h & 0xF800) == 0x4800:
            rt, imm = (h >> 8) & 7, h & 0xFF
            lit = ((va + 4) & ~3) + imm * 4
            op = f"ldr {RN[rt]}, [pc, #{imm * 4}] ; ->0x{lit:X}=0x{word(lit):X}"
        elif (h & 0xFF00) == 0x4400:
            rd = (((h >> 7) & 1) << 3) | (h & 7)
            rm = (h >> 3) & 15
            op = f"add {RN[rd]}, {RN[rm]}"
        elif (h & 0xF800) == 0x6800:
            rt, rn, imm = h & 7, (h >> 3) & 7, ((h >> 6) & 0x1F) * 4
            op = f"ldr {RN[rt]}, [{RN[rn]}, #{imm}]"
        elif (h & 0xF800) == 0x6000:
            rt, rn, imm = h & 7, (h >> 3) & 7, ((h >> 6) & 0x1F) * 4
            op = f"str {RN[rt]}, [{RN[rn]}, #{imm}]"
        elif (h & 0xF800) == 0x2800:
            op = f"cmp {RN[(h >> 8) & 7]}, #{h & 0xFF}"
        elif (h & 0xF000) == 0xD000 and ((h >> 8) & 0xF) < 14:
            imm = sx(h & 0xFF, 8) * 2
            op = f"b{COND[(h >> 8) & 0xF]} 0x{(va + 4 + imm) & 0xFFFFFFFF:X}"
        elif (h & 0xFF87) == 0x4700:
            op = f"bx {RN[(h >> 3) & 15]}"
        elif (h & 0xFF87) == 0x4780:
            op = f"blx {RN[(h >> 3) & 15]}"
        elif (h & 0xF800) == 0x3000:
            op = f"adds {RN[(h >> 8) & 7]}, #{h & 0xFF}"
        elif (h & 0xFF00) == 0x4600:
            rd = (((h >> 7) & 1) << 3) | (h & 7)
            rm = (h >> 3) & 15
            op = f"mov {RN[rd]}, {RN[rm]}"
        elif (h & 0xF800) == 0xE000:
            imm = sx(h & 0x7FF, 11) * 2
            op = f"b 0x{(va + 4 + imm) & 0xFFFFFFFF:X}"
        elif (h & 0xF800) == 0x0000:
            rd, rm, imm = h & 7, (h >> 3) & 7, (h >> 6) & 0x1F
            op = f"lsls {RN[rd]}, {RN[rm]}, #{imm}"
        print(f"0x{va:X}: {op}")
        va += 2


if __name__ == "__main__":
    dis_range(0x30CFF0, 0x30D05C)
    print("--- pool ---")
    print(f"0x30D054: 0x{word(0x30D054):X}")
    print(f"0x30D058: 0x{word(0x30D058):X}")
    # Also decode BL at 0x30D03C precisely
    print("--- nearby BLs ---")
    for va in (0x30D016, 0x30D024, 0x30D03C, 0x30D04A, 0x30D078):
        h, h2 = half(va), half(va + 2)
        print(f"raw @0x{va:X}: {h:04X} {h2:04X}")
