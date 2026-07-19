#!/usr/bin/env python3
"""v55: find robotol sites that can store ERW+0x8D0 (ui_mode) and map 0x306344 branch."""
from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

MAGIC = 0x4750524D


def parse_mrp(path: Path):
    data = path.read_bytes()
    magic, header_end, total_size, index_offset = struct.unpack_from("<4I", data, 0)
    if magic != MAGIC:
        raise ValueError("bad magic")
    pos = index_offset
    index_end = header_end + 8
    entries = {}
    while pos < index_end:
        nlen = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        raw = data[pos : pos + nlen]
        pos += nlen
        name = raw.rstrip(b"\0").decode("latin1")
        off, clen, flags = struct.unpack_from("<3I", data, pos)
        pos += 12
        entries[name] = {"offset": off, "compressed_len": clen, "flags": flags}
    return data, entries


def inflate(data: bytes, e: dict) -> bytes:
    return zlib.decompress(data[e["offset"] : e["offset"] + e["compressed_len"]], 31)


def is_thumb_str_reg_imm(hw: int) -> tuple[bool, int, int, int]:
    """Thumb STR Rt,[Rn,#imm5*4]  encoding 0b01100 Imm5 Rn Rt"""
    if (hw & 0xF800) == 0x6000:
        imm = ((hw >> 6) & 0x1F) * 4
        rn = (hw >> 3) & 7
        rt = hw & 7
        return True, rt, rn, imm
    return False, 0, 0, 0


def disasm_window(blob: bytes, off: int, before: int = 32, after: int = 48) -> list[str]:
    start = max(0, off - before) & ~1
    end = min(len(blob), off + after)
    lines = []
    i = start
    while i + 1 < end:
        hw = struct.unpack_from("<H", blob, i)[0]
        # skip obvious literal pool words if high entropy? keep simple
        kind = ""
        ok, rt, rn, imm = is_thumb_str_reg_imm(hw)
        if ok:
            kind = f" STR r{rt},[r{rn},#{imm}]"
        elif (hw & 0xF800) == 0x6800:
            imm = ((hw >> 6) & 0x1F) * 4
            rn = (hw >> 3) & 7
            rt = hw & 7
            kind = f" LDR r{rt},[r{rn},#{imm}]"
        elif (hw & 0xF800) == 0x4800:
            rt = (hw >> 8) & 7
            imm = (hw & 0xFF) * 4
            lit = (i + 4 + imm) & ~3
            val = struct.unpack_from("<I", blob, lit)[0] if lit + 4 <= len(blob) else None
            kind = f" LDR r{rt},[pc,#{imm}] lit@{lit:#x}={val:#x}" if val is not None else f" LDR r{rt},[pc,#{imm}]"
        elif (hw & 0xF800) == 0x2000:
            kind = f" MOVS r{hw>>8 & 7},#{hw & 0xFF}"
        elif (hw & 0xFF00) == 0x4400:
            kind = " ADD/MOV high regs"
        elif (hw & 0xF000) == 0xD000:
            cond = (hw >> 8) & 0xF
            rel = (hw & 0xFF)
            if rel >= 0x80:
                rel -= 0x100
            tgt = i + 4 + rel * 2
            kind = f" Bcond cond={cond} -> {tgt:#x}"
        elif (hw & 0xF800) == 0xE000:
            rel = hw & 0x7FF
            if rel >= 0x400:
                rel -= 0x800
            tgt = i + 4 + rel * 2
            kind = f" B -> {tgt:#x}"
        mark = " <<<" if i == off or abs(i - off) < 2 else ""
        lines.append(f"  {i:#010x}: {hw:04X}{kind}{mark}")
        i += 2
    return lines


def find_8d0_sites(blob: bytes, ext_base: int) -> list[dict]:
    sites = []
    for i in range(0, len(blob) - 3, 2):
        if struct.unpack_from("<I", blob, i)[0] != 0x8D0:
            continue
        # look back ~40 bytes for LDR pc and STR candidates around
        window = blob[max(0, i - 64) : min(len(blob), i + 32)]
        strs = []
        base = max(0, i - 64)
        for j in range(0, len(window) - 1, 2):
            hw = struct.unpack_from("<H", window, j)[0]
            ok, rt, rn, imm = is_thumb_str_reg_imm(hw)
            if ok:
                strs.append({"off": base + j, "rt": rt, "rn": rn, "imm": imm})
        sites.append(
            {
                "lit_off": i,
                "guest_va": ext_base + i,
                "nearby_str": strs,
            }
        )
    return sites


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mrp", type=Path)
    ap.add_argument("--ext-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument("--markdown", type=Path)
    args = ap.parse_args()

    data, entries = parse_mrp(args.mrp)
    blob = inflate(data, entries["robotol.ext"])
    sites = find_8d0_sites(blob, args.ext_base)

    # Key dispatch offsets
    targets = {
        "handler_306305": 0x306305,
        "dispatch_306344": 0x306344,
        "bl_2EF86C": 0x30662C,
        "splash_2EF86C": 0x2EF86C,
    }

    lines = []
    lines.append("# v55 robotol ui_mode (ERW+0x8D0) static map")
    lines.append("")
    lines.append(f"- MRP: `{args.mrp}`")
    lines.append(f"- robotol decompressed: {len(blob)} bytes")
    lines.append(f"- assumed ext_base: `{args.ext_base:#x}`")
    lines.append(f"- imm_8D0 literal sites: {len(sites)}")
    lines.append("")
    lines.append("## Dispatch / splash VA → file offset")
    lines.append("")
    for name, va in targets.items():
        off = va - args.ext_base
        ok = 0 <= off < len(blob)
        lines.append(f"- `{name}` `{va:#x}` → off `{off:#x}` in_range={ok}")
    lines.append("")
    lines.append("## 0x306344 neighborhood (ui_mode read / branch)")
    lines.append("")
    off344 = 0x306344 - args.ext_base
    if 0 <= off344 < len(blob):
        lines.append("```text")
        lines.extend(disasm_window(blob, off344, before=48, after=80))
        lines.append("```")
    lines.append("")
    lines.append("## 0x306305 neighborhood (10140 handler entry)")
    lines.append("")
    off305 = 0x306305 - args.ext_base
    if 0 <= off305 < len(blob):
        lines.append("```text")
        lines.extend(disasm_window(blob, off305, before=16, after=64))
        lines.append("```")
    lines.append("")
    lines.append("## imm_8D0 literal sites (candidate writers nearby)")
    lines.append("")
    for s in sites:
        lines.append(
            f"- lit@file `{s['lit_off']:#x}` guest `{s['guest_va']:#x}` "
            f"nearby_STR={len(s['nearby_str'])}"
        )
        for st in s["nearby_str"][:8]:
            lines.append(
                f"  - STR@{st['off']:#x} guest `{args.ext_base + st['off']:#x}` "
                f"r{st['rt']},[r{st['rn']},#{st['imm']}]"
            )
        # show disasm around literal-8
        lines.append("```text")
        lines.extend(disasm_window(blob, s["lit_off"], before=40, after=8))
        lines.append("```")
    lines.append("")
    lines.append("## Verdict seed")
    lines.append("")
    lines.append(
        "- Install execute hooks on STR sites near imm_8D0 and on code that "
        "ADDs 0x8D0 to ERW base, then re-run natural (NO FORCE) to see which "
        "writer PCs are reached vs gated."
    )
    lines.append(
        "- Focus: does ui_mode=0 branch at 0x306344 ever call a function that "
        "stores 0x45, or is that store only on a path never entered?"
    )

    out = "\n".join(lines) + "\n"
    if args.markdown:
        args.markdown.write_text(out, encoding="utf-8")
        print(f"wrote {args.markdown}")
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
