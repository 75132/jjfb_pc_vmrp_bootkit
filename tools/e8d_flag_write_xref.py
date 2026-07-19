#!/usr/bin/env python3
"""Stage E8D: wider write xrefs for idle flags (STR/STRH/STRB/computed/memcpy cover)."""
from __future__ import annotations

import argparse
import json
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


def classify_near_bl(blob: bytes, code_base: int, store_off: int, window: int = 0x60) -> str:
    start = max(0, store_off - window)
    i = start
    best = None
    while i + 3 <= store_off:
        h0 = u16(blob, i)
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0 and i + 3 < len(blob):
            h1 = u16(blob, i + 2)
            pc = code_base + i
            tgt = bl_target(pc, h0, h1)
            if tgt is not None:
                best = (pc, tgt & ~1)
            i += 4
            continue
        i += 2
    if not best:
        return "unknown"
    _, tgt = best
    if tgt == 0x304559:
        return "plat_helper_0x304559"
    if tgt == 0x30D2F9 or tgt == 0x30D2F8:
        return "handler_10165_0x30D2F9"
    if 0x2E0000 <= tgt <= 0x320000:
        return "robotol_internal"
    return f"bl_0x{tgt:X}"


def covers_byte(store_off_base: int, size: int, target: int) -> bool:
    return store_off_base <= target < store_off_base + size


def find_literal_offsets(blob: bytes, code_base: int, offset_val: int) -> List[int]:
    lit_vas = []
    for off in range(0, len(blob) - 3, 4):
        if u32(blob, off) == offset_val:
            lit_vas.append(code_base + off)
    return lit_vas


def find_direct_literal_stores(blob: bytes, code_base: int, offset_val: int) -> List[dict]:
    """LDR lit offset; ADD rN,r9; STR/STRB/STRH nearby."""
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
        for _ in range(8):
            if j + 1 >= len(blob):
                break
            h2 = u16(blob, j)
            pc2 = code_base + j
            kind = None
            cover = 1
            if (h2 & 0xFE00) == 0x7000:  # STRB Rt,[Rn,Rm]
                kind = "strb_reg"
            elif (h2 & 0xF800) == 0x7000:  # STRB Rt,[Rn,#imm]
                kind = "strb_imm"
            elif (h2 & 0xF800) == 0x8000:  # STRH
                kind, cover = "strh", 2
            elif (h2 & 0xF800) == 0x6000:  # STR
                kind, cover = "str", 4
            elif (h2 & 0xFF00) == 0x4400 and ((h2 >> 3) & 0xF) == 9:
                j += 2
                continue
            if kind:
                hits.append(
                    {
                        "kind": kind,
                        "store_pc": f"0x{pc2:X}",
                        "ldr_pc": f"0x{pc:X}",
                        "cover_bytes": cover,
                        "near_call_class": classify_near_bl(blob, code_base, j),
                        "via": "literal_offset",
                    }
                )
                break
            j += 2
    return hits


def find_aligned_word_stores(blob: bytes, code_base: int, byte_off: int) -> List[dict]:
    """STR/STRH whose aligned base could cover byte_off (same literal word/halfword)."""
    hits: List[dict] = []
    # Word-aligned base that contains byte_off
    word_off = byte_off & ~3
    half_off = byte_off & ~1
    for base_off, cover, label in (
        (word_off, 4, "str_covers_byte"),
        (half_off, 2, "strh_covers_byte"),
    ):
        if base_off == byte_off and cover == 1:
            continue
        for h in find_direct_literal_stores(blob, code_base, base_off):
            if h["cover_bytes"] >= cover or h["kind"].startswith("str"):
                nh = dict(h)
                nh["via"] = label
                nh["aligned_off"] = f"0x{base_off:X}"
                hits.append(nh)
    return hits


def find_memcpy_like(blob: bytes, code_base: int, byte_off: int) -> List[dict]:
    """Heuristic: BL to common copy helpers near LDR of nearby offsets in 0xC00..0xD00."""
    hits: List[dict] = []
    # Scan for LDR lit in band around target, then BL within 0x20
    for i in range(0, len(blob) - 5, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + i
        imm = (h & 0xFF) << 2
        lit = ((pc + 4) & ~2) + imm
        loff = lit - code_base
        if loff < 0 or loff + 4 > len(blob):
            continue
        val = u32(blob, loff)
        if not (0xC00 <= val <= 0x1200):
            continue
        if abs(val - byte_off) > 0x40:
            continue
        # look ahead for BL
        for k in range(i + 2, min(i + 0x30, len(blob) - 3), 2):
            h0 = u16(blob, k)
            if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0:
                h1 = u16(blob, k + 2)
                tgt = bl_target(code_base + k, h0, h1)
                if tgt is None:
                    continue
                hits.append(
                    {
                        "kind": "memcpy_candidate_bl",
                        "store_pc": f"0x{code_base + k:X}",
                        "ldr_pc": f"0x{pc:X}",
                        "lit_off": f"0x{val:X}",
                        "bl_target": f"0x{tgt & ~1:X}",
                        "near_call_class": classify_near_bl(blob, code_base, k),
                        "via": "nearby_band_bl",
                    }
                )
                break
    return hits[:40]


def classify_writer(near: str) -> str:
    if "10165" in near:
        return "APP_EVENT_REQUIRED"
    if near == "plat_helper_0x304559":
        return "PLATFORM_SIDE_EFFECT_REQUIRED"
    if near == "robotol_internal":
        return "UNREACHED_INTERNAL_STATE"
    if near.startswith("bl_0x"):
        return "UNREACHED_INTERNAL_STATE"
    return "UNREACHED_INTERNAL_STATE"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--flag-map", default="out/e8c_tmp/flag_map.json")
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument("-o", "--out", default="out/e8d_tmp/flag_write_xref_v2.md")
    ap.add_argument("--class-out", default="out/e8d_tmp/writer_class.md")
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    fmap = json.loads(Path(args.flag_map).read_text(encoding="utf-8"))
    focus = {0xC44, 0xC9D, 0xCF5, 0xCD1, 0x11B0}

    lines = [
        "# E8D flag write xref v2",
        f"ext={args.ext}",
        f"code_base=0x{args.code_base:X}",
        "",
        "Detects direct STRB/STRH/STR via literal+R9, aligned wider stores covering the byte,",
        "and nearby-band BL candidates (memcpy-like heuristic).",
        "",
    ]
    class_lines = [
        "# E8D writer classification",
        "",
        "| offset | store_pc | via | near_call | class |",
        "|--------|----------|-----|-----------|-------|",
    ]

    c9d_notes: List[str] = []

    for fl in fmap.get("flags", []):
        off_u = fl.get("er_rw_offset_u")
        if off_u is None:
            continue
        off_u = int(off_u)
        if off_u not in focus:
            continue
        lines.append(f"## offset 0x{off_u:X} (site {fl.get('ldr_va')})")
        direct = find_direct_literal_stores(blob, args.code_base, off_u)
        wider = find_aligned_word_stores(blob, args.code_base, off_u)
        memcpy = find_memcpy_like(blob, args.code_base, off_u) if off_u == 0xC9D else []
        # de-dupe by store_pc
        seen: Set[str] = set()
        all_hits: List[dict] = []
        for h in direct + wider + memcpy:
            sp = h.get("store_pc") or ""
            key = sp + h.get("via", "")
            if key in seen:
                continue
            seen.add(key)
            all_hits.append(h)

        lines.append(f"- direct_literal_stores: {len(direct)}")
        lines.append(f"- wider_aligned_cover: {len(wider)}")
        lines.append(f"- memcpy_like: {len(memcpy)}")
        if not all_hits and off_u == 0xC9D:
            lines.append("- **C9D: no store sites found** — candidate LOADER_INIT / external / template")
            c9d_notes.append("no STR/STRH/STRB/memcpy-like in robotol.ext covering 0xC9D")
        for h in all_hits[:30]:
            lines.append(
                f"  - {h.get('kind')} store={h.get('store_pc')} via={h.get('via')} "
                f"class={h.get('near_call_class')}"
            )
            cls = classify_writer(h.get("near_call_class") or "unknown")
            class_lines.append(
                f"| 0x{off_u:X} | {h.get('store_pc')} | {h.get('via')} | "
                f"{h.get('near_call_class')} | {cls} |"
            )
        if off_u == 0xC9D:
            lines.append("")
            lines.append("### C9D special")
            if wider:
                lines.append("- covered by wider STR/STRH of aligned base — not pure STRB miss")
                c9d_notes.append("wider store cover present")
            elif memcpy:
                lines.append("- memcpy-like BL near band literals")
                c9d_notes.append("memcpy-like candidates present")
            else:
                lines.append("- no robotol writer — LOADER_INIT_REQUIRED or PLATFORM/external")
                c9d_notes.append("LOADER_INIT_REQUIRED or external")
        lines.append("")

    lines.append("## C9D provenance summary")
    for n in c9d_notes:
        lines.append(f"- {n}")
    lines.append("")

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text("\n".join(lines), encoding="utf-8")
    Path(args.class_out).write_text("\n".join(class_lines) + "\n", encoding="utf-8")
    print(f"wrote {args.out}")
    print(f"wrote {args.class_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
