#!/usr/bin/env python3
"""Stage E9W Lane A: static xrefs for writes/reads of R9+0xAC8 (splash logo gate).

Also scans nearby splash offsets R9+0xA80..0xAE0 and known candidates
0x2E4062 / lr 0x2F68FF / splash compare 0x2EF8A0.
"""
from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def bl_target(pc: int, h0: int, h1: int) -> Optional[int]:
    if (h0 & 0xF800) != 0xF000 or (h1 & 0xC000) != 0xC000:
        return None
    s = (h0 >> 10) & 1
    imm10 = h0 & 0x3FF
    j1 = (h1 >> 13) & 1
    j2 = (h1 >> 11) & 1
    imm11 = h1 & 0x7FF
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    imm32 = sign_extend(imm32, 25)
    return (pc + 4 + imm32) | (1 if (h1 & 0x1000) else 0)


def find_literal_offsets(blob: bytes, code_base: int, offset_val: int) -> List[int]:
    lit_vas = []
    for off in range(0, len(blob) - 3, 4):
        if u32(blob, off) == offset_val:
            lit_vas.append(code_base + off)
    return lit_vas


def enclosing_hint(pc: int) -> str:
    if 0x2EF86C <= pc < 0x2EFD00:
        return "splash_0x2EF86C"
    if 0x2E4000 <= pc < 0x2E4200:
        return "fn_near_0x2E4062"
    if 0x2F6800 <= pc < 0x2F6A00:
        return "fn_near_0x2F68E4"
    if 0x2FC400 <= pc < 0x2FC500:
        return "fn_0x2FC418_bd0"
    if 0x306300 <= pc < 0x306700:
        return "ui_dispatch_0x306344"
    return "robotol_other"


def relation_2ef86c(pc: int) -> str:
    if pc == 0x2EF8A0:
        return "AC8_READ_COMPARE"
    if 0x2EF86C <= pc < 0x2EF9F4:
        return "logo_branch_region"
    if 0x2EF9F4 <= pc < 0x2EFB00:
        return "loading_branch_region"
    if 0x2EFC40 <= pc < 0x2EFD00:
        return "splash_tail_writer_region"
    return "outside_splash"


def find_literal_memops(
    blob: bytes, code_base: int, offset_val: int, want_store: bool
) -> List[dict]:
    """LDR lit offset; ADD rN,r9; then STR*/LDR* nearby."""
    hits: List[dict] = []
    lit_vas = set(find_literal_offsets(blob, code_base, offset_val))
    n = len(blob) - 1
    for i in range(0, n - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + i
        imm = (h & 0xFF) << 2
        lit = ((pc + 4) & ~2) + imm
        if lit not in lit_vas:
            continue
        j = i + 2
        for _ in range(10):
            if j + 1 >= len(blob):
                break
            h2 = u16(blob, j)
            pc2 = code_base + j
            kind = None
            is_store = False
            if (h2 & 0xF800) == 0x6000:  # STR Rt,[Rn,#imm]
                kind, is_store = "str_imm", True
            elif (h2 & 0xF800) == 0x6800:  # LDR Rt,[Rn,#imm]
                kind, is_store = "ldr_imm", False
            elif (h2 & 0xF800) == 0x8000:  # STRH
                kind, is_store = "strh_imm", True
            elif (h2 & 0xF800) == 0x8800:  # LDRH
                kind, is_store = "ldrh_imm", False
            elif (h2 & 0xF800) == 0x7000:  # STRB
                kind, is_store = "strb_imm", True
            elif (h2 & 0xF800) == 0x7800:  # LDRB
                kind, is_store = "ldrb_imm", False
            elif (h2 & 0xFF00) == 0x4400 and ((h2 >> 3) & 0xF) == 9:
                j += 2
                continue
            if kind and is_store == want_store:
                insn = f"0x{h2:04X}"
                hits.append(
                    {
                        "writer_pc": f"0x{pc2:X}",
                        "exact_insn": f"{kind}:{insn}",
                        "source_reg_or_value": "Rt_from_str_or_Rn",
                        "enclosing_function": enclosing_hint(pc2),
                        "caller": "static_unknown",
                        "branch_predicates": relation_2ef86c(pc2),
                        "hit_naturally": "static_only",
                        "hit_displayfirst": "static_only",
                        "relation_to_0x2EF86C": relation_2ef86c(pc2),
                        "offset": f"0x{offset_val:X}",
                        "ldr_lit_pc": f"0x{pc:X}",
                        "role": "store" if want_store else "load",
                    }
                )
                break
            j += 2
    return hits


def disasm_window(blob: bytes, code_base: int, va: int, before: int = 8, after: int = 12) -> str:
    off = va - code_base
    if off < 0 or off >= len(blob):
        return ""
    start = max(0, off - before * 2)
    end = min(len(blob) - 1, off + after * 2)
    parts = []
    i = start
    while i <= end:
        pc = code_base + i
        h = u16(blob, i)
        mark = "*" if pc == va else " "
        parts.append(f"{mark}0x{pc:X}:0x{h:04X}")
        i += 2
    return " ".join(parts)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument(
        "--csv",
        default="reports/e9w_ac8_writer_trace.csv",
        help="CSV path (static candidates; dynamic hits appended by runtime)",
    )
    ap.add_argument("--md", default="reports/e9w_ac8_static_xref.md")
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    focus_offs = [0xAC8]
    band_offs = list(range(0xA80, 0xAE4, 4))

    rows: List[dict] = []
    lines = [
        "# E9W AC8 static writer/load xref",
        f"ext={args.ext}",
        f"code_base=0x{args.code_base:X}",
        "",
        "Focus: R9+0xAC8 splash logo gate (read @ ~0x2EF8A0).",
        "Historical candidates: writer ~0x2E4062, lr ~0x2F68FF.",
        "",
    ]

    # Known sites
    for known in (0x2E4062, 0x2F68FF, 0x2EF8A0, 0x2EF86C, 0x2EFC6C):
        lines.append(f"## known site 0x{known:X}")
        lines.append(f"- enclosing={enclosing_hint(known)}")
        lines.append(f"- relation={relation_2ef86c(known)}")
        win = disasm_window(blob, args.code_base, known)
        if win:
            lines.append(f"- window: `{win}`")
        lines.append("")

    for off in focus_offs:
        stores = find_literal_memops(blob, args.code_base, off, want_store=True)
        loads = find_literal_memops(blob, args.code_base, off, want_store=False)
        lines.append(f"## offset 0x{off:X}")
        lines.append(f"- literal_pool_count={len(find_literal_offsets(blob, args.code_base, off))}")
        lines.append(f"- store_candidates={len(stores)}")
        lines.append(f"- load_candidates={len(loads)}")
        for h in stores + loads:
            rows.append(h)
            lines.append(
                f"  - {h['role']} pc={h['writer_pc']} insn={h['exact_insn']} "
                f"fn={h['enclosing_function']} rel={h['relation_to_0x2EF86C']}"
            )
        lines.append("")

    lines.append("## band R9+0xA80..0xAE0 (literal store scan)")
    for off in band_offs:
        stores = find_literal_memops(blob, args.code_base, off, want_store=True)
        if not stores:
            continue
        lines.append(f"- 0x{off:X}: {len(stores)} store(s)")
        for h in stores[:8]:
            rows.append(h)
            lines.append(f"  - {h['writer_pc']} {h['exact_insn']} {h['enclosing_function']}")
    lines.append("")

    # Deduplicate CSV rows
    seen: Set[Tuple[str, str, str]] = set()
    uniq: List[dict] = []
    for r in rows:
        key = (r["writer_pc"], r["exact_insn"], r["offset"])
        if key in seen:
            continue
        seen.add(key)
        uniq.append(r)

    csv_path = Path(args.csv)
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "writer_pc",
        "exact_insn",
        "source_reg_or_value",
        "enclosing_function",
        "caller",
        "branch_predicates",
        "hit_naturally",
        "hit_displayfirst",
        "relation_to_0x2EF86C",
        "offset",
        "role",
        "note",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as fp:
        w = csv.DictWriter(fp, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in uniq:
            r = dict(r)
            r.setdefault("note", "static_xref")
            w.writerow(r)

    md_path = Path(args.md)
    md_path.parent.mkdir(parents=True, exist_ok=True)
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {csv_path} rows={len(uniq)}")
    print(f"wrote {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
