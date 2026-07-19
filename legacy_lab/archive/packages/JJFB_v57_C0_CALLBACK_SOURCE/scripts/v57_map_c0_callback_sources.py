#!/usr/bin/env python3
"""v57: static producers of family app=0xC0 and callback 2F5405 registration."""
from __future__ import annotations

import argparse
import hashlib
import struct
import zlib
from pathlib import Path

EXT = 0x2D8DF4


def parse_mrp(p: Path):
    d = p.read_bytes()
    magic, hend, total, idx = struct.unpack_from("<4I", d, 0)
    if magic != 0x4750524D:
        raise SystemExit("bad MRP")
    pos, end = idx, hend + 8
    e = {}
    while pos < end:
        n = struct.unpack_from("<I", d, pos)[0]
        pos += 4
        name = d[pos : pos + n].rstrip(b"\0").decode("latin1")
        pos += n
        off, clen, flags = struct.unpack_from("<3I", d, pos)
        pos += 12
        e[name] = (off, clen, flags)
    return d, e


def find_bl_to(rob: bytes, target_va: int, limit: int = 64):
    hits = []
    i = 0
    tgt = target_va & ~1
    while i + 4 <= len(rob):
        hw1 = rob[i] | (rob[i + 1] << 8)
        hw2 = rob[i + 2] | (rob[i + 3] << 8)
        if (hw1 & 0xF800) == 0xF000 and (hw2 & 0xF800) == 0xF800:
            s = (hw1 >> 10) & 1
            imm10 = hw1 & 0x3FF
            j1 = (hw2 >> 13) & 1
            j2 = (hw2 >> 11) & 1
            imm11 = hw2 & 0x7FF
            I1 = j1 ^ s ^ 1
            I2 = j2 ^ s ^ 1
            imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm |= ~((1 << 25) - 1)
            from_va = EXT + i
            to_va = (from_va + 4 + imm) & 0xFFFFFFFF
            if (to_va & ~1) == tgt:
                hits.append(from_va)
                if len(hits) >= limit:
                    break
            i += 4
            continue
        i += 2
    return hits


def dump_window(rob: bytes, va: int, before: int = 24, after: int = 48) -> str:
    off = va - EXT
    start = max(0, off - before)
    end = min(len(rob), off + after)
    lines = []
    for i in range(start, end, 2):
        hw = rob[i] | (rob[i + 1] << 8)
        mark = " <<<" if i == off else ""
        lines.append(f"  0x{EXT+i:08X}: {hw:04X}{mark}")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mrp", type=Path)
    ap.add_argument("--markdown", type=Path, default="")
    args = ap.parse_args()
    d, e = parse_mrp(args.mrp)
    off, clen, _ = e["robotol.ext"]
    rob = zlib.decompress(d[off : off + clen], 31)

    movs = []
    for i in range(0, len(rob) - 1, 2):
        hw = rob[i] | (rob[i + 1] << 8)
        if (hw & 0xF800) == 0x2000 and (hw & 0xFF) == 0xC0:
            movs.append((EXT + i, (hw >> 8) & 7))
    cmps = []
    for i in range(0, len(rob) - 1, 2):
        hw = rob[i] | (rob[i + 1] << 8)
        if (hw & 0xF800) == 0x2800 and (hw & 0xFF) == 0xC0:
            cmps.append((EXT + i, (hw >> 8) & 7))

    bl_targets = {
        "30D300_family": 0x30D300,
        "30D2F9_handler": 0x30D2F9,
        "30DC44_C0_case": 0x30DC44,
        "2FEBBC_reset": 0x2FEBBC,
        "3054A4_register": 0x3054A4,
        "2F53AC_reg_site": 0x2F53AC,
        "30D128_reg_site": 0x30D128,
        "2F5390_reg_prep": 0x2F5390,
    }
    bls = {k: find_bl_to(rob, va) for k, va in bl_targets.items()}

    needle = struct.pack("<I", 0x2F5405)
    lits = []
    idx = 0
    while True:
        j = rob.find(needle, idx)
        if j < 0:
            break
        # align-ish word sites
        if j % 2 == 0:
            lits.append(EXT + j)
        idx = j + 2

    # Who calls the registration sites?
    callers_2f53ac = find_bl_to(rob, 0x2F53AC)
    callers_30d128 = find_bl_to(rob, 0x30D128)
    callers_2f5390 = find_bl_to(rob, 0x2F5390)

    lines = []
    lines.append("# v57 Family 0xC0 / Callback 2F5405 Source Map")
    lines.append("")
    lines.append(f"- MRP SHA256: `{hashlib.sha256(d).hexdigest()}`")
    lines.append(f"- robotol.ext decompressed: {len(rob)}")
    lines.append(f"- ext_base assumed: `0x{EXT:X}`")
    lines.append("")
    lines.append("## MOVS/CMP imm 0xC0")
    lines.append("")
    lines.append(f"- MOVS #0xC0 sites: **{len(movs)}**")
    for va, rd in movs:
        lines.append(f"  - `0x{va:X}` movs r{rd}, #0xC0")
        lines.append("```")
        lines.append(dump_window(rob, va))
        lines.append("```")
    lines.append(f"- CMP #0xC0 sites: **{len(cmps)}**")
    for va, rn in cmps[:30]:
        lines.append(f"  - `0x{va:X}` cmp r{rn}, #0xC0")
    if len(cmps) > 30:
        lines.append(f"  - ... +{len(cmps)-30} more")
    lines.append("")
    lines.append("## BL callers")
    lines.append("")
    for k, hits in bls.items():
        lines.append(f"- BL → `{k}`: **{len(hits)}** — " + (", ".join(f'`0x{x:X}`' for x in hits) or "(none)"))
    lines.append("")
    lines.append("## Callback registration producers")
    lines.append("")
    lines.append(f"- literal `0x2F5405` pool sites: " + (", ".join(f'`0x{x:X}`' for x in lits) or "(none)"))
    lines.append(f"- BL → `0x2F5390`: " + (", ".join(f'`0x{x:X}`' for x in callers_2f5390) or "(none)"))
    lines.append(f"- BL → `0x2F53AC`: " + (", ".join(f'`0x{x:X}`' for x in callers_2f53ac) or "(none)"))
    lines.append(f"- BL → `0x30D128`: " + (", ".join(f'`0x{x:X}`' for x in callers_30d128) or "(none)"))
    lines.append("")
    lines.append("## Windows around key VAs")
    lines.append("")
    for label, va in [
        ("family_dispatch_30D300", 0x30D300),
        ("C0_case_30DC44", 0x30DC44),
        ("reg_site_2F53AC", 0x2F53AC),
        ("reg_site_30D128", 0x30D128),
        ("register_fn_3054A4", 0x3054A4),
    ]:
        lines.append(f"### {label}")
        lines.append("```")
        lines.append(dump_window(rob, va, 32, 64))
        lines.append("```")
        lines.append("")

    lines.append("## Focus for v57 dynamics (NO inject)")
    lines.append("")
    lines.append("1. Hook every MOVS/CMP #0xC0 site and every BL that reaches `30DC44` / `2FEBBC`.")
    lines.append("2. Hook callers of `2F5390` / `2F53AC` / `30D128` — if never hit, registration never starts.")
    lines.append("3. Trace host `sendAppEvent` / `0x1E209` app values beyond logging app=9.")
    lines.append("4. Still forbid FORCE ui_mode / inject C0 / inject event 5·12 / host UI blit.")
    lines.append("")

    out = "\n".join(lines) + "\n"
    if args.markdown:
        args.markdown.write_text(out, encoding="utf-8")
        print(f"wrote {args.markdown}")
    print(out[:4000])
    print("...\n(truncated)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
