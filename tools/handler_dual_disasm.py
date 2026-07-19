#!/usr/bin/env python3
"""Stage E8A: dual-mode Thumb/ARM disassembly of robotol handler window."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def disasm_thumb(data: bytes, base_va: int) -> list[str]:
    lines: list[str] = []
    i = 0
    n = len(data)
    while i + 1 < n:
        pc = base_va + i
        h0 = u16(data, i)
        size = 2
        note = f"h0=0x{h0:04X}"
        tgt = None
        # 32-bit Thumb-2 if high matches
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0 and i + 3 < n:
            h1 = u16(data, i + 2)
            size = 4
            note = f"h0=0x{h0:04X} h1=0x{h1:04X}"
            if (h0 & 0xF800) == 0xF000 and (h1 & 0xC000) == 0xC000:
                s = (h0 >> 10) & 1
                imm10 = h0 & 0x3FF
                j1 = (h1 >> 13) & 1
                j2 = (h1 >> 11) & 1
                imm11 = h1 & 0x7FF
                i1 = (~(j1 ^ s)) & 1
                i2 = (~(j2 ^ s)) & 1
                imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
                imm32 = sign_extend(imm32, 25)
                tgt = (pc + 4 + imm32) | (1 if (h1 & 0x1000) else 0)
                kind = "BL" if (h1 & 0x1000) else "BLX"
                note = f"{kind} imm -> 0x{tgt:X}"
        elif (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
            imm = sign_extend(h0 & 0xFF, 8) << 1
            tgt = (pc + 4 + imm) | 1
            note = f"Bcond -> 0x{tgt:X}"
        elif (h0 & 0xF800) == 0xE000:
            imm = sign_extend(h0 & 0x7FF, 11) << 1
            tgt = (pc + 4 + imm) | 1
            note = f"B -> 0x{tgt:X}"
        elif (h0 & 0xFF80) == 0x4700:
            rm = (h0 >> 3) & 0xF
            note = f"{'BLX' if (h0 & 0x80) else 'BX'} r{rm}"
        elif h0 == 0xB5F0:
            note = "PUSH {r4-r7,lr}"
        elif h0 == 0xBDF0:
            note = "POP {r4-r7,pc}"
        elif (h0 & 0xFF00) == 0xB000:
            note = f"ADD/SUB SP #imm"
        raw = data[i : i + size]
        hexb = raw.hex().upper()
        mark = ""
        if pc == 0x30630C:
            mark = "  ; << HANDLER ENTRY"
        elif pc == 0x306338:
            mark = "  ; << FAULT PC"
        lines.append(f"  0x{pc:08X}: {hexb:<10}  {note}{mark}")
        i += size
    return lines


def disasm_arm(data: bytes, base_va: int) -> list[str]:
    lines: list[str] = []
    # Align to 4 from base
    i = 0
    n = len(data) - (len(data) % 4)
    while i + 3 < n:
        pc = base_va + i
        w = u32(data, i)
        note = f"word=0x{w:08X}"
        cond = (w >> 28) & 0xF
        # Branch
        if (w & 0x0E000000) == 0x0A000000:
            imm = sign_extend(w & 0xFFFFFF, 24) << 2
            tgt = pc + 8 + imm
            note = f"B/BL cond={cond} -> 0x{tgt:X}"
        elif (w & 0x0FFFFFF0) == 0x012FFF10:
            note = f"BX r{w & 0xF} cond={cond}"
        mark = ""
        if pc == 0x30630C:
            mark = "  ; << HANDLER ENTRY (if T=0)"
        elif pc == 0x306338:
            mark = "  ; << FAULT PC (if T=0)"
        lines.append(f"  0x{pc:08X}: {data[i:i+4].hex().upper():<10}  {note}{mark}")
        i += 4
    return lines


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), required=True)
    ap.add_argument("--handler", type=lambda x: int(x, 0), default=0x30630C)
    ap.add_argument("--before", type=lambda x: int(x, 0), default=0x80)
    ap.add_argument("--after", type=lambda x: int(x, 0), default=0x140)
    ap.add_argument("-o", "--out", required=True)
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    start_va = args.handler - args.before
    end_va = args.handler + args.after
    start_off = start_va - args.code_base
    end_off = end_va - args.code_base
    if start_off < 0 or end_off > len(blob):
        raise SystemExit(f"window out of range off=0x{start_off:X}..0x{end_off:X} size={len(blob)}")
    window = blob[start_off:end_off]
    fault_off = 0x306338 - args.code_base
    fault_bytes = blob[fault_off : fault_off + 8].hex().upper()

    lines = [
        f"# Handler dual disasm",
        f"ext={args.ext}",
        f"code_base=0x{args.code_base:X}",
        f"handler_va=0x{args.handler:X} file_off=0x{args.handler - args.code_base:X}",
        f"fault_va=0x306338 file_off=0x{fault_off:X} bytes={fault_bytes}",
        f"window_va=0x{start_va:X}..0x{end_va:X}",
        "",
        "## Thumb mode",
        *disasm_thumb(window, start_va),
        "",
        "## ARM mode (what Unicorn sees if T=0)",
        *disasm_arm(window, start_va),
        "",
    ]
    Path(args.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
