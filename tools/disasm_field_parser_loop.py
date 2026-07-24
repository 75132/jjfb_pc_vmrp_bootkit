#!/usr/bin/env python3
"""Static Thumb disasm for field parser inner loop 0x30A0E0..0x30A130."""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROB = ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext"
OUT = ROOT / "reports/field_parser_loop_disasm.md"
CODE_BASE = 0x2D8DF4
# Include BE u16 length decode (0x30A0CC..0x30A0F6) + copy loop.
START = 0x30A0CC
END = 0x30A134


def reg(n: int) -> str:
    return f"r{n}" if n < 13 else ("sp" if n == 13 else "lr")


def decode_half(h: int, pc: int) -> tuple[str, str, list[str], int]:
    """Return mnemonic, operands, annotations, size."""
    ann: list[str] = []
    # PUSH/POP
    if (h & 0xFE00) == 0xB400:
        lo, hi = h & 0xFF, (h >> 8) & 0xFF
        regs = [f"r{i}" for i in range(8) if lo & (1 << i)]
        if hi:
            regs.append("lr")
        return "PUSH", "{" + ",".join(regs) + "}", ann, 2
    if (h & 0xFE00) == 0xBC00:
        lo, hi = h & 0xFF, (h >> 8) & 0xFF
        regs = [f"r{i}" for i in range(8) if lo & (1 << i)]
        if hi:
            regs.append("pc")
        return "POP", "{" + ",".join(regs) + "}", ann, 2
    # B<cond>
    if (h & 0xF000) == 0xD000 and (h & 0x0F00) != 0x0F00:
        conds = ["EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE"]
        c = (h >> 8) & 0xF
        imm = struct.unpack("<b", bytes([h & 0xFF]))[0]
        tgt = (pc + 4 + imm * 2) & 0xFFFFFFFF
        tag = "loop_back" if tgt <= pc else "exit_edge"
        if c == 11:
            tag = "loop_back" if tgt < pc else tag
        ann.append(tag)
        ann.append("cond_branch")
        return f"B{conds[c]}", f"0x{tgt:X}", ann, 2
    # B unconditional short
    if (h & 0xF800) == 0xE000:
        imm = h & 0x7FF
        if imm & 0x400:
            imm -= 0x800
        tgt = (pc + 4 + imm * 2) & 0xFFFFFFFF
        ann.append("branch")
        return "B", f"0x{tgt:X}", ann, 2
    # MOVS
    if (h & 0xF800) == 0x2000:
        rd = (h >> 8) & 7
        imm = h & 0xFF
        return "MOVS", f"{reg(rd)}, #0x{imm:X}", ann, 2
    # ADDS/SUBS imm3
    if (h & 0xFE00) == 0x1C00:
        rd, rn, imm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        return "ADDS", f"{reg(rd)}, {reg(rn)}, #{imm}", ann, 2
    # ADDS imm8
    if (h & 0xF800) == 0x3000:
        rd, imm = (h >> 8) & 7, h & 0xFF
        return "ADDS", f"{reg(rd)}, #{imm}", ann, 2
    # CMP imm8
    if (h & 0xF800) == 0x2800:
        rn, imm = (h >> 8) & 7, h & 0xFF
        ann.append("cmp")
        return "CMP", f"{reg(rn)}, #0x{imm:X}", ann, 2
    # CMP register (low)
    if (h & 0xFFC0) == 0x4280:
        rn, rm = (h >> 3) & 7, h & 7
        ann.append("cmp")
        return "CMP", f"{reg(rn)}, {reg(rm)}", ann, 2
    # CMP high reg
    if (h & 0xFF00) == 0x4500:
        rn, rm = (h >> 3) & 0xF, h & 0xF
        ann.append("cmp")
        return "CMP", f"{reg(rn)}, {reg(rm)}", ann, 2
    # LDR/STR word imm5*4 (bit11: 0=STR 1=LDR)
    if (h & 0xF800) == 0x6800:
        rt, rn = h & 7, (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        ann.append("load")
        return "LDR", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    if (h & 0xF800) == 0x6000:
        rt, rn = h & 7, (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        ann.append("store")
        return "STR", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    # LDR/STR reg offset
    if (h & 0xFE00) == 0x5800:
        rt, rn, rm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        if (h >> 9) & 1:
            ann.append("load")
            return "LDR", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
        ann.append("store")
        return "STR", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
    # LDRB/STRB reg offset (Thumb1): Rt=bits[2:0] Rn=bits[5:3] Rm=bits[8:6]
    if (h & 0xFE00) == 0x5C00:
        rt, rn, rm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        ann.append("load")
        return "LDRB", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
    if (h & 0xFE00) == 0x5400:
        rt, rn, rm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        ann.append("store")
        return "STRB", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
    # LDRB/STRB imm5
    if (h & 0xF000) == 0x7000:
        rt, rn = h & 7, (h >> 3) & 7
        imm = (h >> 6) & 0x1F
        if (h >> 11) & 1:
            ann.append("load")
            return "LDRB", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
        ann.append("store")
        return "STRB", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    # LDRH/STRH
    if (h & 0xF500) == 0x5000:
        rt, rn, rm = (h >> 6) & 7, (h >> 3) & 7, h & 7
        if h & 0x0800:
            ann.append("load")
            return "LDRH", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
        ann.append("store")
        return "STRH", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
    # LDR PC-relative
    if (h & 0xF800) == 0x4800:
        rd, imm = (h >> 8) & 7, (h & 0xFF) * 4
        ann.append("load")
        return "LDR", f"{reg(rd)}, [PC, #0x{imm:X}]", ann, 2
    # BX/BLX reg
    if (h & 0xFF87) == 0x4700:
        rm = (h >> 3) & 0xF
        return "BX", reg(rm), ann, 2
    return ".hword", f"0x{h:04X}", ann, 2


def disasm_range(blob: bytes, base: int, start: int, end: int) -> list[str]:
    lines: list[str] = []
    pc = start
    while pc < end:
        off = pc - base
        if off < 0 or off + 2 > len(blob):
            break
        h0 = struct.unpack_from("<H", blob, off)[0]
        # 32-bit BL
        if (h0 & 0xF800) == 0xF000 and off + 4 <= len(blob):
            h1 = struct.unpack_from("<H", blob, off + 2)[0]
            if (h1 & 0xD000) in (0xD000, 0xC000):
                raw = blob[off : off + 4]
                s = (h0 >> 10) & 1
                imm10 = h0 & 0x3FF
                j1 = (h1 >> 13) & 1
                j2 = (h1 >> 11) & 1
                imm11 = h1 & 0x7FF
                i1 = 1 - (j1 ^ s)
                i2 = 1 - (j2 ^ s)
                imm = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
                if s:
                    imm -= 1 << 25
                tgt = (pc + 4 + imm) & 0xFFFFFFFF
                raw_s = " ".join(f"{b:02X}" for b in raw)
                lines.append(
                    f"| 0x{pc:08X} | `{raw_s}` | Thumb | **BL** | 0x{tgt:X} | call |"
                )
                pc += 4
                continue
        mn, ops, ann, sz = decode_half(h0, pc)
        raw = " ".join(f"{b:02X}" for b in blob[off : off + sz])
        marks = ",".join(ann) if ann else ""
        bold = "**" if ann else ""
        lines.append(
            f"| 0x{pc:08X} | `{raw}` | Thumb | {bold}{mn}{bold} | {ops} | {marks} |"
        )
        pc += sz
    return lines


def main() -> None:
    if not ROB.exists():
        raise SystemExit(f"missing {ROB}")
    blob = ROB.read_bytes()
    body = disasm_range(blob, CODE_BASE, START, END)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    md = [
        "# Field parser disasm (0x30A0CC..0x30A130)",
        "",
        f"- **module:** robotol.ext @ `0x{CODE_BASE:X}`",
        f"- **range:** `0x{START:X}` .. `0x{END:X}`",
        "",
        "## Summary",
        "",
        "| role | PC |",
        "|---|---|",
        "| entry / args | **0x30A0CC** `r0=stream_base` `r1=state` |",
        "| BE u16 length write | **0x30A0E8** `ADDS r5, r1, #0` |",
        "| CMP r5,#0 | **0x30A0EA** (not the length write) |",
        "| malloc field | **0x30A0F6** `BL 0x2D99AC` |",
        "| loop head | **0x30A100** |",
        "| loop back-edge | **0x30A110** `BLT → 0x30A100` |",
        "| normal exit | **0x30A112** |",
        "",
        "## Length provenance (0x30A0D8..0x30A0EA)",
        "",
        "```text",
        "LDR  r0, [r4,#0]      ; cursor index",
        "LDRB r1, [r6, r0]    ; lo = stream[cursor]",
        "cursor++ ; STR [r4]",
        "LDRB r2, [r6, r0]    ; hi = stream[cursor]",
        "r5 = (lo << 8) | hi  ; BE u16 field length",
        "CMP  r5, #0          ; @0x30A0EA",
        "```",
        "",
        "## Instructions",
        "",
        "| PC | raw | mode | mnemonic | operands | marks |",
        "|---|---|---|---|---|---|",
        *body,
        "",
        "## Loop body (0x30A100..0x30A110)",
        "",
        "Carrying variables per iteration:",
        "- **`r1`**: incremented (`ADDS r1, #1`) — field byte index",
        "- **`r4`**: parser state (cursor index at `[r4,#0]`)",
        "- **`r5`**: BE u16 length — loop while **`r1 < r5`**",
        "- **`r6`**: stream base",
        "",
    ]
    OUT.write_text("\n".join(md), encoding="utf-8")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
