#!/usr/bin/env python3
"""Disassemble 3054A4 / 2F5390 registration path."""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

EXT = 0x2D8DF4


def main() -> int:
    p = Path(
        r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit"
        r"\runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320\gwy\jjfb.mrp"
    )
    d = p.read_bytes()
    magic, hend, total, idx = struct.unpack_from("<4I", d, 0)
    pos, end = idx, hend + 8
    e: dict[str, tuple[int, int, int]] = {}
    while pos < end:
        n = struct.unpack_from("<I", d, pos)[0]
        pos += 4
        name = d[pos : pos + n].rstrip(b"\0").decode("latin1")
        pos += n
        off, clen, flags = struct.unpack_from("<3I", d, pos)
        pos += 12
        e[name] = (off, clen, flags)
    rob = zlib.decompress(d[e["robotol.ext"][0] : e["robotol.ext"][0] + e["robotol.ext"][1]], 31)

    def dump(va: int, nbytes: int = 160) -> None:
        off = va - EXT
        i = off
        end_i = min(len(rob), off + nbytes)
        while i < end_i:
            hw = rob[i] | (rob[i + 1] << 8)
            va2 = EXT + i
            extra = ""
            if i + 4 <= len(rob) and (hw & 0xF800) == 0xF000:
                hw2 = rob[i + 2] | (rob[i + 3] << 8)
                if (hw2 & 0xF800) == 0xF800:
                    s = (hw >> 10) & 1
                    imm10 = hw & 0x3FF
                    j1 = (hw2 >> 13) & 1
                    j2 = (hw2 >> 11) & 1
                    imm11 = hw2 & 0x7FF
                    I1 = j1 ^ s ^ 1
                    I2 = j2 ^ s ^ 1
                    imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
                    if s:
                        imm |= ~((1 << 25) - 1)
                    to = (va2 + 4 + imm) & 0xFFFFFFFF
                    print(f"0x{va2:08X}: {hw:04X} {hw2:04X}  BL 0x{to:X}")
                    i += 4
                    continue
            if (hw & 0xFF00) == 0xB500:
                extra = " PUSH"
            elif (hw & 0xFF00) == 0xBD00:
                extra = " POP"
            elif (hw & 0xF800) == 0x2800:
                extra = f" cmp r{(hw >> 8) & 7}, #{hw & 0xFF}"
            elif (hw & 0xF800) == 0x2000:
                extra = f" movs r{(hw >> 8) & 7}, #{hw & 0xFF}"
            elif (hw & 0xF800) == 0x4800:
                imm = (hw & 0xFF) * 4
                base = (va2 + 4) & ~2
                lit = base + imm
                if 0 <= lit - EXT + 3 < len(rob):
                    val = struct.unpack_from("<I", rob, lit - EXT)[0]
                    extra = f" ldr r{(hw >> 8) & 7}, =0x{val:X}"
            elif hw == 0x4770:
                extra = " bx lr"
            elif (hw & 0xFF87) == 0x4780:
                extra = f" blx r{(hw >> 3) & 0xF}"
            print(f"0x{va2:08X}: {hw:04X}{extra}")
            if (hw & 0xFF00) == 0xBD00 and va2 > va:
                break
            i += 2

    print("=== 2F5390 ===")
    dump(0x2F5390, 140)
    print("=== 3054A4 ===")
    dump(0x3054A4, 100)
    idx = 0
    while True:
        j = rob.find(b"timer err", idx)
        if j < 0:
            break
        print("str", hex(EXT + j), rob[j : j + 24])
        idx = j + 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
