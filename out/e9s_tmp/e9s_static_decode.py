#!/usr/bin/env python3
"""E9S: static Thumb decode of BD0 writers 0x2FC418 / 0x2FC444 + callers."""
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
)

EXT = ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext"
OUT = Path(__file__).resolve().parent / "e9s_disasm.txt"


def find_fn_end(blob: bytes, start: int, limit: int = 0x800) -> int:
    pc = start
    end = start + limit
    while pc < end:
        sz, text, meta = decode(blob, pc)
        if meta.get("pop_pc") or text == "BX lr":
            return pc + sz
        pc += sz
    return start + limit


def disasm_linear(blob: bytes, start: int, end: int, label: str) -> list[str]:
    lines = [f"\n===== {label} 0x{start:X}..0x{end:X} ====="]
    pc = start
    while pc < end:
        sz, text, meta = decode(blob, pc)
        mark = ""
        bl = meta.get("bl")
        if bl is not None:
            mark += f"  ; BL->0x{bl:X}"
        bt = meta.get("b_target")
        if bt is not None:
            mark += f"  ; ->0x{bt:X}"
        if "STR" in text or meta.get("str"):
            mark += "  ; STORE"
        if meta.get("ldr_pc") and meta.get("lit_val") is not None:
            mark += f"  ; lit=0x{meta['lit_val']:X}"
        lines.append(f"0x{pc:08X}: {text}{mark}")
        pc += sz
    return lines


def main() -> None:
    blob = EXT.read_bytes()
    lines: list[str] = [
        f"EXT={EXT}",
        f"size={len(blob)} CODE_BASE=0x{CODE_BASE:X}",
        "",
        "===== E9S BD0 / 2FC418 analysis =====",
    ]

    probes = {
        "2FC418_uimode_or_bd0": 0x2FC418,
        "2FC444_str_site": 0x2FC444,
        "2FC03C_uimode_pre": 0x2FC03C,
        "2DADC4_upstream": 0x2DADC4,
        "2D9648_concat": 0x2D9648,
        "305EB8_sched": 0x305EB8,
        "2F5404_sched": 0x2F5404,
        "2EF86C_splash": 0x2EF86C,
    }

    lines.append("\n===== function bounds + callers =====")
    for name, va in probes.items():
        s = find_fn_start(blob, va)
        e = find_fn_end(blob, s)
        callers = find_bl_callers(blob, va)
        lines.append(
            f"  {name}: probe=0x{va:X} start=0x{s:X} end~=0x{e:X} "
            f"callers={len(callers)} {[hex(c) for c in callers[:12]]}"
        )

    s = find_fn_start(blob, 0x2FC418)
    e = find_fn_end(blob, s)
    lines += disasm_linear(blob, s, min(e + 0x20, s + 0x180), "2FC418_full_fn")
    lines += disasm_linear(blob, 0x2FC3F0, 0x2FC480, "2FC3F0_window")

    # STR sites that write [rn,#0x30] near BA0 family
    lines.append("\n===== STR [rn,#0x30] candidates (potential BD0=BA0+0x30) =====")
    pc = CODE_BASE
    end = CODE_BASE + len(blob)
    while pc + 1 < end:
        sz, text, meta = decode(blob, pc)
        if "STR" in text and "#0x30" in text:
            lines.append(f"  0x{pc:X}: {text}")
        pc += sz

    lines.append("\n===== BL callers of 0x2FC418 — context =====")
    for c in find_bl_callers(blob, 0x2FC418)[:20]:
        fs = find_fn_start(blob, c)
        lines.append(f"\n-- caller 0x{c:X} fn_start=0x{fs:X} --")
        lines += disasm_linear(blob, max(fs, c - 0x30), c + 0x14, f"near_{c:X}")

    lines.append("\n===== BL callers of 0x2FC444 (if any; may be mid-fn) =====")
    for c in find_bl_callers(blob, 0x2FC444)[:10]:
        lines.append(f"  BL->2FC444 from 0x{c:X}")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT} lines={len(lines)}")


if __name__ == "__main__":
    main()
