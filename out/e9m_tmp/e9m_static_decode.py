#!/usr/bin/env python3
"""E9M: static Thumb decode of splash text ABI — uses repo decoder (no Capstone)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from e8w_f6c_object_xref import (  # noqa: E402
    CODE_BASE,
    decode,
    find_bl_callers,
    find_fn_start,
    u16,
    u32,
)

EXT = ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext"
OUT = Path(__file__).resolve().parent / "e9m_disasm.txt"


def disasm_linear(blob: bytes, start: int, end: int, label: str) -> list[str]:
    lines = [f"\n===== {label} 0x{start:X}..0x{end:X} ====="]
    pc = start
    bls = []
    while pc < end:
        sz, text, meta = decode(blob, pc)
        mark = ""
        if meta.get("bl"):
            mark = f"  ; BL->{meta['bl']:X}"
            bls.append((pc, meta["bl"]))
        if meta.get("ldr_pc") and meta.get("lit_val") is not None:
            v = meta["lit_val"]
            mark += f"  ; lit=0x{v:X}"
            if v & 0x80000000:
                mark += f" signed={v - 0x100000000}"
        lines.append(f"0x{pc:08X}: {text}{mark}")
        if meta.get("pop_pc") or text == "BX lr":
            lines.append(f"  ;; likely epilogue @0x{pc:X}")
            # keep going a bit for trailing pools
        pc += sz
    if bls:
        lines.append("-- BLs --")
        for a, t in bls:
            lines.append(f"  0x{a:X} -> 0x{t:X}")
    return lines


def find_fn_end(blob: bytes, start: int, limit: int = 0x500) -> int:
    pc = start
    end = start + limit
    while pc < end:
        sz, text, meta = decode(blob, pc)
        if meta.get("pop_pc") or text == "BX lr":
            return pc + sz
        # also POP with pc in high regs thumb2
        pc += sz
    return start + limit


def main() -> None:
    blob = EXT.read_bytes()
    lines: list[str] = [
        f"EXT={EXT}",
        f"size={len(blob)} CODE_BASE=0x{CODE_BASE:X}",
    ]

    probes = {
        "305BFC_text_draw": 0x305BFC,
        "305E78_text_measure": 0x305E78,
        "2F2174_layout": 0x2F2174,
        "303C50_measure_helper": 0x303C50,
        "304558_plat_bridge": 0x304558,
        "2EF86C_splash_entry": 0x2EF86C,
        "2EFB0E_post_r4": 0x2EFB0E,
        "2EFBA2_caller": 0x2EFBA2,
    }

    lines.append("\n===== function bounds =====")
    bounds = {}
    for name, va in probes.items():
        s = find_fn_start(blob, va)
        e = find_fn_end(blob, s)
        bounds[name] = (s, e, va)
        lines.append(f"  {name}: start=0x{s:X} end~=0x{e:X} (probe=0x{va:X})")

    # Full function disasms for key funcs
    for name in (
        "305BFC_text_draw",
        "305E78_text_measure",
        "2F2174_layout",
        "303C50_measure_helper",
        "304558_plat_bridge",
    ):
        s, e, _ = bounds[name]
        # cap huge funcs
        e2 = min(e, s + 0x200)
        lines += disasm_linear(blob, s, e2, name)

    # Caller path: post-r4 through BL 305BFC
    lines += disasm_linear(blob, 0x2EFAF0, 0x2EFBD0, "post_r4_path_2EFAF0_2EFBD0")

    # Extra window on 305BFC if start equals probe (short prologue)
    lines += disasm_linear(blob, 0x305BFC, 0x305D80, "305BFC_wide")
    lines += disasm_linear(blob, 0x305E78, 0x305EB8, "305E78_wide")
    lines += disasm_linear(blob, 0x2F2174, 0x2F22B0, "2F2174_wide")
    lines += disasm_linear(blob, 0x303C50, 0x303D40, "303C50_wide")

    # Callers of each
    lines.append("\n===== BL callers =====")
    for label, tgt in [
        ("305BFC", 0x305BFC),
        ("305E78", 0x305E78),
        ("2F2174", 0x2F2174),
        ("303C50", 0x303C50),
        ("304558", 0x304558),
    ]:
        cs = find_bl_callers(blob, tgt)
        lines.append(f"  BL->{label}: count={len(cs)} sample={[hex(x) for x in cs[:16]]}")

    # FFE7917B analysis helpers near caller
    lines.append("\n===== FFE7917B / layout math context =====")
    lines.append(f"  0xFFE7917B as signed32 = {0xFFE7917B - 0x100000000}")
    lines.append(f"  0xFFE7917B & 0xFFFF = 0x{0xFFE7917B & 0xFFFF:X}")
    # dump halfwords around 2EFB60-2EFBA2 with focus on ADD/SUB/MUL/LDR that set r1
    lines += disasm_linear(blob, 0x2EFB40, 0x2EFBB0, "arg_setup_2EFB40_2EFBB0")

    # Search literal pool for values near FFE7917B pattern
    hits = []
    for i in range(0, len(blob) - 3, 4):
        if u32(blob, i) == 0xFFE7917B:
            hits.append(CODE_BASE + i)
    lines.append(f"  literal FFE7917B occurrences: {[hex(h) for h in hits]}")

    # Also check if r1 could be result of SUB screen_w/2 - text_w/2 style overflow
    # Common: (a - b) with unsigned wrap. Dump measure return path uses.
    lines.append("\n===== stack/arg STR/LDR near 2EFBA2 (raw halfwords) =====")
    for pc in range(0x2EFB40, 0x2EFBB0, 2):
        h = u16(blob, pc - CODE_BASE)
        sz, text, meta = decode(blob, pc)
        if any(
            k in text
            for k in (
                "STR",
                "LDR",
                "MOV",
                "ADD",
                "SUB",
                "MUL",
                "NEG",
                "BL ",
                "MOVS",
                "sp",
            )
        ):
            lines.append(f"0x{pc:08X}: {text}  raw=0x{h:04X}")

    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT}")
    print("\n".join(lines[:80]))


if __name__ == "__main__":
    main()
