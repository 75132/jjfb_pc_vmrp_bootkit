#!/usr/bin/env python3
"""Stage E8C: static write xrefs for idle flag ER_RW offsets in robotol.ext."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import List, Optional, Tuple


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


def classify_near_bl(blob: bytes, code_base: int, store_off: int, window: int = 0x40) -> str:
    """Look backward for BL; classify by imm / known plat trampoline 0x304559."""
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
    pc, tgt = best
    if tgt == 0x304559:
        return "plat_helper_0x304559"
    # Heuristic buckets by address bands / common names in nearby pool strings — keep coarse.
    if 0x2E0000 <= tgt <= 0x320000:
        return "robotol_internal"
    return f"bl_0x{tgt:X}"


def find_literal_uses(blob: bytes, code_base: int, offset_val: int) -> List[dict]:
    """Find LDR lit that load this offset, plus nearby STRB/STR patterns after ADD r9."""
    hits: List[dict] = []
    n = len(blob) - 3
    # Find literal pool words equal to offset_val
    lit_vas = []
    for off in range(0, n - 3, 4):
        if u32(blob, off) == offset_val:
            lit_vas.append(code_base + off)

    for i in range(0, n - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + i
        imm = (h & 0xFF) << 2
        lit = ((pc + 4) & ~2) + imm
        if lit not in lit_vas:
            continue
        # Look ahead for ADD rN,r9 and STRB/STR
        j = i + 2
        kind = "load_only"
        store_pc = None
        for _ in range(6):
            if j + 1 >= len(blob):
                break
            h2 = u16(blob, j)
            pc2 = code_base + j
            if (h2 & 0xFF00) == 0x4400 and ((h2 >> 3) & 0xF) == 9:
                pass  # ADD *, r9
            elif (h2 & 0xFE00) == 0x7000:  # STRB Rt, [Rn, Rm]
                kind = "strb_indexed"
                store_pc = pc2
                break
            elif (h2 & 0xF800) == 0x6000:  # STR Rt, [Rn, #imm]
                kind = "str_imm"
                store_pc = pc2
                break
            elif (h2 & 0xF800) == 0x7000:  # STRB Rt, [Rn, #imm]
                kind = "strb_imm"
                store_pc = pc2
                break
            elif h2 == 0x56C0:
                kind = "ldrsb_use"
                break
            elif (h2 & 0xF800) == 0x6800:
                kind = "ldr_use"
                break
            j += 2
        cls = "unknown"
        if store_pc is not None:
            cls = classify_near_bl(blob, code_base, store_pc - code_base)
        hits.append(
            {
                "ldr_pc": f"0x{pc:X}",
                "kind": kind,
                "store_pc": f"0x{store_pc:X}" if store_pc else None,
                "near_call_class": cls,
            }
        )
    return hits


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--flag-map", default="out/e8c_tmp/flag_map.json")
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument("-o", "--out", default="out/e8c_tmp/flag_write_xref.md")
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    fmap = json.loads(Path(args.flag_map).read_text(encoding="utf-8"))
    lines = [
        "# E8C flag write xref",
        f"ext={args.ext}",
        f"code_base=0x{args.code_base:X}",
        "",
        "Classification is coarse (TARGET_OBSERVED heuristics). Writers set flag bytes via",
        "`LDR lit; ADD rN,r9; STRB/STR`. Load-only sites are the idle CMP readers.",
        "",
    ]
    for fl in fmap.get("flags", []):
        off_u = fl.get("er_rw_offset_u")
        if off_u is None:
            continue
        lines.append(f"## offset {fl.get('er_rw_offset')} (site {fl.get('ldr_va')})")
        lines.append(f"- load_width: {fl.get('load_width')} cmp_imm: {fl.get('cmp_imm')}")
        hits = find_literal_uses(blob, args.code_base, int(off_u))
        stores = [h for h in hits if h["kind"].startswith("str")]
        loads = [h for h in hits if not h["kind"].startswith("str")]
        lines.append(f"- load_sites: {len(loads)} store_sites: {len(stores)}")
        for h in stores[:20]:
            lines.append(
                f"  - WRITE {h['kind']} store_pc={h['store_pc']} ldr={h['ldr_pc']} "
                f"class={h['near_call_class']}"
            )
        if not stores:
            lines.append("  - no STR/STRB sites found for this literal (ROBOTOL_STATE_FLAG_NEVER_SET candidate)")
        for h in loads[:8]:
            lines.append(f"  - READ {h['kind']} ldr={h['ldr_pc']}")
        lines.append("")

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
