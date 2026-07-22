#!/usr/bin/env python3
"""E10A-3.1o: annotate cfunction helpers 0xAC374=strlen and 0xAC4A4=strstr."""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out" / "e10a31o"
BASE = 0x80000


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def dec(w: int, pc: int) -> str:
    cond = (w >> 28) & 0xF
    cs = {
        0xE: "",
        0: "EQ",
        1: "NE",
        2: "CS",
        3: "CC",
        8: "HI",
        9: "LS",
        0xA: "GE",
        0xB: "LT",
        0xC: "GT",
        0xD: "LE",
    }.get(cond, hex(cond))
    if (w & 0x0FFFFFF0) == 0x012FFF10:
        return f"BX{cs} r{w & 0xF}"
    if (w & 0xFFFF0000) == 0xE8BD0000:
        return f"LDMFD sp!, {{0x{w & 0xFFFF:X}}}"
    if (w & 0xFFFF0000) == 0xE92D0000:
        return f"STMFD sp!, {{0x{w & 0xFFFF:X}}}"
    if (w & 0x0FE00FF0) == 0x01A00000:
        return f"MOV{cs} r{(w >> 12) & 0xF}, r{w & 0xF}"
    if (w & 0x0FF00000) == 0x03A00000:
        return f"MOV{cs} r{(w >> 12) & 0xF}, #0x{w & 0xFF:X}"
    if (w & 0x0FF00000) == 0x03500000:
        return f"CMP{cs} r{(w >> 16) & 0xF}, #0x{w & 0xFF:X}"
    if (w & 0x0F700000) == 0x04700000:
        return f"LDRB{cs} r{(w >> 12) & 0xF}, [r{(w >> 16) & 0xF}], #1"
    if (w & 0x0F700000) == 0x04500000:
        return f"LDRB{cs} r{(w >> 12) & 0xF}, [r{(w >> 16) & 0xF}]"
    if (w & 0x0E000000) == 0x0A000000:
        imm = w & 0xFFFFFF
        if imm & 0x800000:
            imm -= 0x1000000
        tgt = pc + 8 + imm * 4
        op = "BL" if w & (1 << 24) else "B"
        return f"{op}{cs} 0x{tgt:X}"
    if (w & 0x0FF00000) == 0x02800000:
        return f"ADD{cs} r{(w >> 12) & 0xF}, r{(w >> 16) & 0xF}, #0x{w & 0xFF:X}"
    if (w & 0x0FF00000) == 0x02400000:
        return f"SUB{cs} r{(w >> 12) & 0xF}, r{(w >> 16) & 0xF}, #0x{w & 0xFF:X}"
    if (w & 0x0FF000F0) == 0x00400000:
        return f"SUB{cs} r{(w >> 12) & 0xF}, r{(w >> 16) & 0xF}, r{w & 0xF}"
    if (w & 0x0FF000F0) == 0x01500000:
        return f"CMP{cs} r{(w >> 16) & 0xF}, r{w & 0xF}"
    return f".word 0x{w:08X}"


def dump(cfn: bytes, lo: int, hi: int) -> list[str]:
    lines = []
    for pc in range(lo, hi, 4):
        w = u32(cfn, pc - BASE)
        lines.append(f"  0x{pc:08X}: {w:08X}  {dec(w, pc)}")
    return lines


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    cfn = (ROOT / "out/vmrp_run/cfunction.ext").read_bytes()
    lines = [
        "# E10A-3.1o cfunction helpers: 0xAC374=strlen, 0xAC4A4=strstr",
        "",
        "## Live call (method0 failfn 0x2E3F85)",
        "",
        "| PC | target | r0 | r1 | role |",
        "|----|--------|----|----|------|",
        "| 0x2E3F94 | 0xAC374 | haystack buf | (fn) | strlen(haystack) |",
        "| 0x2E3FB4 | 0xAC4A4 | haystack buf | needle=\"napptype\" | strstr; NULL→MVNS -1 |",
        "",
        "## 0xAC374 — strlen",
        "",
        "```",
        *dump(cfn, 0xAC374, 0xAC390),
        "```",
        "",
        "Returns end-start (C string length).",
        "",
        "## 0xAC4A4 — strstr(haystack=r0, needle=r1)",
        "",
        "```",
        *dump(cfn, 0xAC4A4, 0xAC53C),
        "```",
        "",
        "Returns pointer to first match, or 0 if needle longer than haystack / not found.",
        "",
        "## Failfn contract",
        "",
        "1. strlen(buf)",
        "2. strstr(buf, key) — key from caller r3 (first call: \"napptype\")",
        "3. NULL → MVNS → return -1",
        "4. else skip key + one char (`=`), take value until `_`, return 0",
        "",
        "## SMSCFG pairing",
        "",
        "- `@0x355` int16_le = copy length for field at `@0x377`",
        "- `@0x377` = C-string haystack (must contain `napptype=<val>`)",
        "- Compat int16=1 only copies 1 byte → empty/NUL haystack → strstr fails",
        "",
    ]
    path = OUT / "helpers_ac374_ac4a4_annotated.txt"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
