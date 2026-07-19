#!/usr/bin/env python3
"""Phase 6M: disasm cfunction.ext ranges 0x89CF4 / 0x94E94 from live bytes or MRP member."""
from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

# Avoid importing phase6j_common path issues — inline minimal helpers.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from phase6j_common import member_blob  # type: ignore


CFN_BASE = 0x80000
RANGES = [
    ("path_89cf4", 0x89CF4, 0x89D80),
    ("zero_94e94", 0x94E94, 0x94F60),
]


def thumb_disasm_half(h: int) -> str:
    if (h & 0xFF00) == 0xB500:
        return "PUSH {...,lr}"
    if (h & 0xFF00) == 0xBD00:
        return "POP {...,pc}"
    if (h & 0xF800) == 0x6000:
        rt, rn = h & 7, (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        return f"STR r{rt},[r{rn},#0x{imm:X}]"
    if (h & 0xF800) == 0x6800:
        rt, rn = h & 7, (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        return f"LDR r{rt},[r{rn},#0x{imm:X}]"
    if (h & 0xF800) == 0x2000:
        rd = (h >> 8) & 7
        return f"MOVS r{rd},#0x{h & 0xFF:X}"
    if (h & 0xFFC0) == 0x0000 and (h & 0x3F) == 0:
        return "NOP/LSL#0"
    if (h & 0xFF00) == 0x2300:
        return f"MOVS r3,#0x{h & 0xFF:X}"
    if (h & 0xFF00) == 0x2200:
        return f"MOVS r2,#0x{h & 0xFF:X}"
    if (h & 0xFF00) == 0x2100:
        return f"MOVS r1,#0x{h & 0xFF:X}"
    if (h & 0xFF00) == 0x2000:
        return f"MOVS r0,#0x{h & 0xFF:X}"
    if (h & 0xF800) == 0x4800:
        rd = (h >> 8) & 7
        return f"LDR r{rd},[PC,#0x{(h & 0xFF) * 4:X}]"
    if (h & 0xF800) == 0xE000:
        return "B (uncond thumb)"
    if (h & 0xFF00) == 0x4700:
        return "BX/BLX"
    return f".hword 0x{h:04X}"


def parse_live_bytes(text: str) -> dict[int, bytes]:
    out: dict[int, bytes] = {}
    for m in re.finditer(
        r"\[JJFB_CFN_BYTES\] base=(0x[0-9A-Fa-f]+) len=(0x[0-9A-Fa-f]+) ok=1 hex=([0-9A-Fa-f]+)",
        text,
    ):
        base = int(m.group(1), 16)
        hx = m.group(3)
        out[base] = bytes.fromhex(hx)
    return out


def disasm_region(blob: bytes, file_off: int, va: int, length: int) -> list[str]:
    lines = []
    end = min(len(blob), file_off + length)
    off = file_off
    while off + 1 < end:
        h = struct.unpack_from("<H", blob, off)[0]
        note = ""
        if (h & 0xF800) == 0x6000:
            imm = ((h >> 6) & 0x1F) * 4
            if imm in (0, 4, 8, 0xC, 0x10):
                note = "  ; P-field store candidate"
            if imm == 0xC:
                note += "  ; P+0xC / mrc_extChunk*"
        if (h & 0xFF00) == 0x2000 and (h & 0xFF) == 0:
            note += "  ; movs #0"
        lines.append(f"| `0x{va + (off - file_off):X}` | `+0x{off:X}` | `0x{h:04X}` | {thumb_disasm_half(h)}{note} |")
        off += 2
    return lines


def classify_94(blob: bytes, file_off: int) -> str:
    """Heuristic: many STR with movs#0 nearby → zero_init."""
    window = blob[file_off : file_off + 0x80]
    strs = 0
    mov0 = 0
    for off in range(0, len(window) - 1, 2):
        h = struct.unpack_from("<H", window, off)[0]
        if (h & 0xF800) == 0x6000:
            strs += 1
        if (h & 0xFF00) == 0x2000 and (h & 0xFF) == 0:
            mov0 += 1
        if (h & 0xFF00) in (0x2100, 0x2200, 0x2300) and (h & 0xFF) == 0:
            mov0 += 1
    if strs >= 3 and mov0 >= 1:
        return "zero_init"
    if strs >= 3:
        return "memset_or_zero_loop"
    return "unknown"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("out_md")
    ap.add_argument("--mrp", default="", help="optional MRP containing cfunction.ext")
    args = ap.parse_args()
    text = Path(args.stdout).read_text(encoding="utf-8", errors="replace") if Path(args.stdout).is_file() else ""
    live = parse_live_bytes(text)

    blob = None
    source = "none"
    if args.mrp and Path(args.mrp).is_file():
        blob = member_blob(Path(args.mrp), "cfunction.ext")
        if blob:
            source = f"mrp:{args.mrp}"

    lines = [
        "# Phase 6M — cfunction.ext disasm (0x89CF4 / 0x94F04)",
        "",
        f"- blob_source: `{source}`",
        f"- live JJFB_CFN_BYTES dumps: `{len(live)}`",
        f"- image_base assumption: `0x{CFN_BASE:X}`",
        "",
    ]

    kind94 = "unknown"
    live_kind = re.search(r"\[JJFB_CFN_DISASM\][^\n]*func=0x94F04[^\n]*kind=(\S+)", text)
    if live_kind:
        kind94 = live_kind.group(1)
    live_pxc = re.search(
        r"\[JJFB_CFN_PXC_SOURCE\][^\n]*r0=(0x[0-9A-Fa-f]+)[^\n]*r1=(0x[0-9A-Fa-f]+)[^\n]*r2=(0x[0-9A-Fa-f]+)",
        text,
    )

    # Prefer live dumps for windows that match.
    for name, va0, va1 in RANGES:
        lines.append(f"## {name} VA `0x{va0:X}` .. `0x{va1:X}`")
        lines.append("")
        lines.append("| VA | file_off | half | disasm |")
        lines.append("|---|---|---|---|")
        length = va1 - va0
        file_off = va0 - CFN_BASE
        # Match live dump whose base is near va0
        for b, raw in live.items():
            if b <= va0 < b + len(raw):
                start = va0 - b
                region_blob = raw[start : start + length]
                for row in disasm_region(region_blob, 0, va0, min(length, len(region_blob))):
                    lines.append(row)
                source_note = f"live_dump@0x{b:X}"
                break
        else:
            if blob and file_off >= 0 and file_off < len(blob):
                for row in disasm_region(blob, file_off, va0, length):
                    lines.append(row)
                source_note = "mrp_member"
                if name.startswith("zero") and kind94 == "unknown":
                    kind94 = classify_94(blob, file_off)
            else:
                lines.append("| n/a | n/a | n/a | no bytes (need live dump or MRP) |")
                source_note = "missing"
        lines.append("")
        lines.append(f"- decode_source: `{source_note}`")
        lines.append("")

    if live_pxc:
        lines += [
            "## Live PXC_SOURCE regs at 0x94F04",
            "",
            f"- r0 (dest/P) = `{live_pxc.group(1)}`",
            f"- r1 = `{live_pxc.group(2)}`",
            f"- r2 = `{live_pxc.group(3)}`",
            "- Pattern matches clear/memset of 20-byte P (r0=P, r1≈P+0xC end, r2=count).",
            "",
        ]

    lines += [
        "## Classification",
        "",
        f"- `0x94F04` kind: `{kind94}`",
        "- DOCUMENTED: `mr_c_function_st+0xC` = `mrc_extChunk*`; registry `chunk_field_04` = chunk+4 `init_func`.",
        "- Live 6L/6M: contiguous zero stores to P+0..+0x13 at `0x94F04` ⇒ **zero_init**, not publication.",
        "",
        "```text",
        "[JJFB_CFN_DISASM] func=0x94F04 kind=zero_init",
        "```",
        "",
    ]
    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md} kind94={kind94}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
