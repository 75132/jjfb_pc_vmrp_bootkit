#!/usr/bin/env python3
"""E9N: static Thumb decode of 0x305C3C text draw core + glyph/platform helpers."""
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
OUT = Path(__file__).resolve().parent / "e9n_disasm.txt"


def disasm_linear(blob: bytes, start: int, end: int, label: str) -> list[str]:
    lines = [f"\n===== {label} 0x{start:X}..0x{end:X} ====="]
    pc = start
    bls = []
    branches = []
    while pc < end:
        sz, text, meta = decode(blob, pc)
        mark = ""
        if meta.get("bl"):
            mark = f"  ; BL->{meta['bl']:X}"
            bls.append((pc, meta["bl"]))
        if meta.get("b_target"):
            branches.append((pc, text, meta["b_target"]))
            mark += f"  ; branch->{meta['b_target']:X}"
        if meta.get("ldr_pc") and meta.get("lit_val") is not None:
            v = meta["lit_val"]
            mark += f"  ; lit=0x{v:X}"
            if v & 0x80000000:
                mark += f" signed={v - 0x100000000}"
        lines.append(f"0x{pc:08X}: {text}{mark}")
        if meta.get("pop_pc") or text == "BX lr":
            lines.append(f"  ;; likely epilogue @0x{pc:X}")
        pc += sz
    if bls:
        lines.append("-- BLs --")
        for a, t in bls:
            lines.append(f"  0x{a:X} -> 0x{t:X}")
    if branches:
        lines.append("-- Branches --")
        for a, t, tgt in branches:
            lines.append(f"  0x{a:X}: {t} -> 0x{tgt:X}")
    return lines


def find_fn_end(blob: bytes, start: int, limit: int = 0x600) -> int:
    pc = start
    end = start + limit
    while pc < end:
        sz, text, meta = decode(blob, pc)
        if meta.get("pop_pc") or text == "BX lr":
            return pc + sz
        pc += sz
    return start + limit


def main() -> None:
    blob = EXT.read_bytes()
    lines: list[str] = [
        f"EXT={EXT}",
        f"size={len(blob)} CODE_BASE=0x{CODE_BASE:X}",
        "",
        "===== E9N 305C3C text draw core analysis =====",
    ]

    probes = {
        "305C3C_text_core": 0x305C3C,
        "305E08_helper": 0x305E08,
        "2F99A4_glyph_lookup": 0x2F99A4,
        "310754_char_advance": 0x310754,
        "2F2360_glyph_blit": 0x2F2360,
        "2D9648_str_advance": 0x2D9648,
        "305BFC_wrapper": 0x305BFC,
    }

    lines.append("\n===== function bounds =====")
    bounds = {}
    for name, va in probes.items():
        s = find_fn_start(blob, va)
        e = find_fn_end(blob, s)
        bounds[name] = (s, e, va)
        lines.append(f"  {name}: start=0x{s:X} end~=0x{e:X} (probe=0x{va:X})")

    # Full 305C3C core through epilogue
    s, e, _ = bounds["305C3C_text_core"]
    lines += disasm_linear(blob, s, min(e + 0x80, s + 0x400), "305C3C_full")

    for name in (
        "2F2360_glyph_blit",
        "2F99A4_glyph_lookup",
        "310754_char_advance",
        "305E08_helper",
        "2D9648_str_advance",
    ):
        s, e, _ = bounds[name]
        lines += disasm_linear(blob, s, min(e, s + 0x180), name)

    # Clip / branch summary inside 305C3C
    lines.append("\n===== 305C3C clip checks (static) =====")
    lines.append("  0x305C52: CMP *(R9+0x830), x  ; BLT 0x305CA0 inner_clip_skip")
    lines.append("  0x305C7A: CMP x, #0  ; BGE 0x305CE2 positive_x_path")
    lines.append("  0x305C98: CMP glyph_type, #1  ; BGT 0x305CA4 wide_glyph")
    lines.append("  0x305CDC: CMP x, #0  ; BLT 0x305C8E char_loop")
    lines.append("  0x305CEA: CMP *(R9+0x830), x+color  ; BGE clip_color")
    lines.append("  0x305CFA: CMP y+height, *(R9+0x824)  ; BLE clip_y")
    lines.append("  0x305D16: CMP draw_mode, #1  ; BNE 0x305D5C skip_glyph_blit")
    lines.append("  0x305CA0: epilogue (inner clip skip / early exit)")

    lines.append("\n===== R9 offsets used in 305C3C =====")
    for off, note in [
        (0x830, "screen width (clip)"),
        (0x828, "y baseline offset"),
        (0x824, "screen height (clip)"),
        (0x818, "text ctx dim (measure path)"),
        (0x81C, "text ctx dim (measure path)"),
    ]:
        lines.append(f"  R9+0x{off:X}: {note}")

    lines.append("\n===== BL callers =====")
    for label, tgt in [
        ("305C3C", 0x305C3C),
        ("2F2360", 0x2F2360),
        ("2F99A4", 0x2F99A4),
        ("310754", 0x310754),
        ("303C50", 0x303C50),
        ("305E08", 0x305E08),
        ("2D9648", 0x2D9648),
    ]:
        cs = find_bl_callers(blob, tgt)
        lines.append(f"  BL->{label}: count={len(cs)} sample={[hex(x) for x in cs[:12]]}")

    # 305D95 context — return address after BL 2F2360?
    lines += disasm_linear(blob, 0x305D58, 0x305E00, "305D58_blit_tail")

    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
