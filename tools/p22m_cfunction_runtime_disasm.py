#!/usr/bin/env python3
"""Disassemble exported cfunction runtime image for P22M (+0x17000..+0x1D300)."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

ANCHORS = {
    0x174C8: "+0x174C8 container/unlink",
    0x17970: "+0x17970 index→node",
    0x1D098: "+0x1D098 derived check",
    0x1D0DC: "+0x1D0DC BL +0x174C8",
    0x1D0E0: "+0x1D0E0 post-remove cont",
}


def u32(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def branch_target(pc: int, insn: int) -> int | None:
    if (insn & 0x0E000000) != 0x0A000000:
        return None
    imm = insn & 0x00FFFFFF
    if imm & 0x00800000:
        imm |= ~0xFFFFFF
    return (pc + 8 + (imm << 2)) & 0xFFFFFFFF


def describe(pc: int, w: int, base: int) -> str:
    if (w & 0x0FFFFFF0) == 0x012FFF30:
        return f"BLX r{w & 0xF}"
    if (w & 0x0FFFFFF0) == 0x012FFF10:
        return f"BX r{w & 0xF}"
    if (w & 0x0F000000) == 0x0B000000:
        t = branch_target(pc, w)
        off = (t - base) if t is not None else 0
        return f"BL 0x{t:X} (+0x{off:X})" if t is not None else "BL ?"
    if (w & 0x0F000000) == 0x0A000000:
        t = branch_target(pc, w)
        cond = (w >> 28) & 0xF
        names = {
            0: "EQ",
            1: "NE",
            2: "CS",
            3: "CC",
            0xA: "GE",
            0xB: "LT",
            0xE: "",
        }
        c = names.get(cond, f"c{cond}")
        return f"B{c} 0x{t:X}" if t is not None else f"B{c} ?"
    if (w & 0xFFFF0000) == 0xE92D0000:
        return f"STMFD sp!,{{list=0x{w & 0xFFFF:X}}}"
    if (w & 0x0FFF8000) == 0x08BD8000:
        return f"LDMFD sp!,{{..pc list=0x{w & 0xFFFF:X}}}"
    if w == 0xE0410180:
        return "SUB r0,r1,r0,LSL#3"
    if (w & 0xFFF00000) == 0xE5940000:
        return f"LDR r{(w >> 12) & 0xF},[r4,#0x{w & 0xFFF:X}]"
    if (w & 0xFFF00000) == 0xE5840000:
        return f"STR r{(w >> 12) & 0xF},[r4,#0x{w & 0xFFF:X}]"
    if (w & 0xFFF00000) == 0xE3500000:
        return f"CMP r0,#0x{w & 0xFF:X}"
    if (w & 0xFFF00000) == 0xE5D00000:
        return f"LDRB r{(w >> 12) & 0xF},[r0,#0x{w & 0xFFF:X}]"
    return f"w=0x{w:08X}"


def find_entry(data: bytes, tip_off: int) -> int:
    o = tip_off & ~3
    # Tiny leaf (no prologue): tip itself if a return appears within 0x20 bytes.
    for fwd in range(o, min(len(data) - 3, o + 0x20), 4):
        w = u32(data, fwd)
        if (w & 0x0FFF8000) == 0x08BD8000:
            return o
    while o >= 4 and tip_off - o < 0x400:
        w = u32(data, o)
        # Stop if we crossed a prior function's LDMFD-pc.
        if o < tip_off and (w & 0x0FFF8000) == 0x08BD8000:
            return tip_off & ~3
        if (w & 0xFFFF0000) == 0xE92D0000:
            return o
        if (w & 0xFFFF0000) == 0xE52D0000:
            return o
        o -= 4
    return tip_off & ~3


def find_end(data: bytes, entry: int) -> int:
    end = min(len(data), entry + 0x800)
    for o in range(entry, end, 4):
        w = u32(data, o)
        if (w & 0x0FFF8000) == 0x08BD8000:
            return o + 4
        if (w & 0x0FFFFFF0) == 0x012FFF10 and (w & 0xF) == 14:
            return o + 4
    return 0


def scan_xrefs(data: bytes, base: int, targets: list[int]) -> list[tuple[int, int, str]]:
    out: list[tuple[int, int, str]] = []
    tset = set(targets)
    for o in range(0, len(data) - 3, 4):
        w = u32(data, o)
        if (w & 0x0F000000) not in (0x0A000000, 0x0B000000):
            continue
        t = branch_target(base + o, w)
        if t is None:
            continue
        toff = t - base
        if toff in tset:
            kind = "BL" if (w & 0x0F000000) == 0x0B000000 else "B"
            out.append((o, toff, kind))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--base", type=lambda x: int(x, 0), default=0x80000)
    ap.add_argument("--out", required=True)
    ap.add_argument("--xref-out", default="")
    ap.add_argument("--start", type=lambda x: int(x, 0), default=0x17000)
    ap.add_argument("--end", type=lambda x: int(x, 0), default=0x1D300)
    args = ap.parse_args()

    data = Path(args.bin).read_bytes()
    base = args.base
    start = max(0, args.start)
    end = min(len(data), args.end)

    lines: list[str] = []
    lines.append(f"# cfunction runtime disasm base=0x{base:X} size=0x{len(data):X}")
    lines.append(f"# range +0x{start:X} .. +0x{end:X}")
    lines.append("")

    for tip, label in ANCHORS.items():
        if tip >= len(data):
            continue
        ent = find_entry(data, tip)
        en = find_end(data, ent)
        lines.append(
            f"## {label}\n"
            f"- tip=+0x{tip:X} abs=0x{base + tip:X}\n"
            f"- fn_entry=+0x{ent:X} abs=0x{base + ent:X}\n"
            f"- fn_end=+0x{en:X} abs=0x{base + en:X}\n"
        )

    lines.append("## Linear disasm\n")
    for o in range(start & ~3, end & ~3, 4):
        w = u32(data, o)
        mark = ANCHORS.get(o, "")
        tag = f"  ; <<< {mark}" if mark else ""
        lines.append(f"+0x{o:05X}  0x{base + o:X}  {w:08X}  {describe(base + o, w, base)}{tag}")

    Path(args.out).write_text("\n".join(lines) + "\n", encoding="utf-8")

    if args.xref_out:
        xrefs = scan_xrefs(data, base, [0x174C8, 0x17970, 0x1D098])
        rows = ["caller_off,target_off,kind"]
        for co, to, k in xrefs:
            rows.append(f"0x{co:X},0x{to:X},{k}")
        # append function bounds
        for tip in (0x174C8, 0x17970, 0x1D098):
            ent = find_entry(data, tip)
            en = find_end(data, ent)
            rows.append(f"# target=0x{tip:X} entry=0x{ent:X} end=0x{en:X}")
        Path(args.xref_out).write_text("\n".join(rows) + "\n", encoding="utf-8")

    print(f"wrote {args.out} bytes={len(data)} range=+0x{start:X}..+0x{end:X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
