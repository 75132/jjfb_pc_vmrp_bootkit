#!/usr/bin/env python3
from __future__ import annotations

import struct
import zlib
from pathlib import Path

MRP = Path(r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320\gwy\jjfb.mrp")
EXT = 0x2D8DF4


def load_robotol() -> bytes:
    data = MRP.read_bytes()
    magic, header_end, total, index_offset = struct.unpack_from("<4I", data, 0)
    pos = index_offset
    index_end = header_end + 8
    entries = {}
    while pos < index_end:
        nlen = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        name = data[pos : pos + nlen].rstrip(b"\0").decode("latin1")
        pos += nlen
        off, clen, flags = struct.unpack_from("<3I", data, pos)
        pos += 12
        entries[name] = (off, clen)
    off, clen = entries["robotol.ext"]
    return zlib.decompress(data[off : off + clen], 31)


def dis(blob: bytes, off: int, n: int = 80) -> None:
    i = off
    end = min(len(blob), off + n)
    while i + 1 < end:
        hw = struct.unpack_from("<H", blob, i)[0]
        note = ""
        if (hw & 0xF800) == 0x4800:
            rt = (hw >> 8) & 7
            imm = (hw & 0xFF) * 4
            lit = (i + 4 + imm) & ~3
            val = struct.unpack_from("<I", blob, lit)[0] if lit + 4 <= len(blob) else None
            note = f" LDR r{rt},[pc,#{imm}] lit@{lit:#x}={val:#x}" if val is not None else ""
        elif (hw & 0xF800) == 0x6000:
            imm = ((hw >> 6) & 0x1F) * 4
            rn = (hw >> 3) & 7
            rt = hw & 7
            note = f" STR r{rt},[r{rn},#{imm}]"
        elif (hw & 0xF800) == 0x6800:
            imm = ((hw >> 6) & 0x1F) * 4
            rn = (hw >> 3) & 7
            rt = hw & 7
            note = f" LDR r{rt},[r{rn},#{imm}]"
        elif (hw & 0xF800) == 0x2000:
            note = f" MOVS r{(hw >> 8) & 7},#{hw & 0xFF}"
        elif (hw & 0xFF00) == 0x4400:
            note = " ADD hi"
        elif (hw & 0xF800) == 0x3000:
            note = f" ADDS r{(hw >> 8) & 7},#{hw & 0xFF}"
        elif (hw & 0xF800) == 0x2800:
            note = f" CMP r{(hw >> 8) & 7},#{hw & 0xFF}"
        elif (hw & 0xF000) == 0xD000:
            cond = (hw >> 8) & 0xF
            rel = hw & 0xFF
            if rel >= 0x80:
                rel -= 0x100
            note = f" Bcond{cond} -> {i + 4 + rel * 2:#x}"
        elif (hw & 0xF800) == 0xE000:
            rel = hw & 0x7FF
            if rel >= 0x400:
                rel -= 0x800
            note = f" B -> {i + 4 + rel * 2:#x}"
        elif (hw & 0xF800) == 0xF000 and i + 3 < end:
            hw2 = struct.unpack_from("<H", blob, i + 2)[0]
            if (hw2 & 0xF800) == 0xF800:
                s = (hw >> 10) & 1
                j1 = (hw2 >> 13) & 1
                j2 = (hw2 >> 11) & 1
                imm10 = hw & 0x3FF
                imm11 = hw2 & 0x7FF
                I1 = 1 - (j1 ^ s)
                I2 = 1 - (j2 ^ s)
                imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
                if s:
                    imm |= ~((1 << 25) - 1)
                tgt = i + 4 + imm
                print(f"{i:#08x}/{EXT + i:#08x}: {hw:04X} {hw2:04X} BL -> {tgt:#x} guest={EXT + tgt:#x}")
                i += 4
                continue
        print(f"{i:#08x}/{EXT + i:#08x}: {hw:04X}{note}")
        i += 2


def main() -> None:
    blob = load_robotol()
    print("=== mode0 branch target file 0x2d63c / guest 0x306430 ===")
    dis(blob, 0x2D63C, 96)
    print("\n=== mode0x45 branch file 0x2d666 / guest 0x30645A ===")
    dis(blob, 0x2D666, 64)
    print("\n=== fallthrough 0x2d634 ===")
    dis(blob, 0x2D634, 40)

    print("\n=== STR after LDR lit 0x8D0 ===")
    count = 0
    for i in range(0, len(blob) - 3, 2):
        if struct.unpack_from("<I", blob, i)[0] != 0x8D0:
            continue
        for back in range(2, 64, 2):
            j = i - back
            if j < 0:
                continue
            hw = struct.unpack_from("<H", blob, j)[0]
            if (hw & 0xF800) != 0x4800:
                continue
            imm = (hw & 0xFF) * 4
            lit = (j + 4 + imm) & ~3
            if lit not in (i, i & ~3, (i + 2) & ~3):
                continue
            for fwd in range(2, 48, 2):
                k = j + fwd
                if k + 1 >= len(blob):
                    break
                h2 = struct.unpack_from("<H", blob, k)[0]
                if (h2 & 0xF800) == 0x6000:
                    imm5 = ((h2 >> 6) & 0x1F) * 4
                    rn = (h2 >> 3) & 7
                    rt = h2 & 7
                    if imm5 == 0:
                        print(
                            f"cand lit@{i:#x}/{EXT + i:#x} ldr@{j:#x}/{EXT + j:#x} "
                            f"STR@{k:#x}/{EXT + k:#x} r{rt},[r{rn},#0]"
                        )
                        count += 1
                        break
    print("writer cands", count)

    # Also: MOVS rX,#0x45 then store to [reg] within 20 bytes of any 8D0 load
    print("\n=== MOVS #0x45 near 8D0 load ===")
    for i in range(0, len(blob) - 3, 2):
        if struct.unpack_from("<I", blob, i)[0] != 0x8D0:
            continue
        region = range(max(0, i - 80), min(len(blob) - 1, i + 80), 2)
        for j in region:
            hw = struct.unpack_from("<H", blob, j)[0]
            if (hw & 0xF800) == 0x2000 and (hw & 0xFF) == 0x45:
                print(f"MOVS #0x45 at {j:#x}/{EXT + j:#x} near lit {i:#x}/{EXT + i:#x}")


if __name__ == "__main__":
    main()
