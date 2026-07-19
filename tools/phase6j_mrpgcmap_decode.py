#!/usr/bin/env python3
"""Phase 6J: MRPGCMAP header / entry decode vs live first_pc / fault."""
from __future__ import annotations

import argparse
import re
from pathlib import Path

from phase6j_common import load_ext_targets, mrpgcmap_header, member_blob


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gwy_root")
    ap.add_argument("out_md")
    ap.add_argument("--stdout", default="", help="optional live stdout for first_pc/fault")
    args = ap.parse_args()
    gwy = Path(args.gwy_root)
    live = ""
    if args.stdout and Path(args.stdout).is_file():
        live = Path(args.stdout).read_text(encoding="utf-8", errors="replace")

    first_pc = None
    header_live = None
    fault_fn = None
    fault_pc = None
    m = re.search(r"observed_first_pc(?:_raw)?=(0x[0-9A-Fa-f]+)", live)
    if m:
        first_pc = int(m.group(1), 16)
    m = re.search(r"header_entry_candidate=(0x[0-9A-Fa-f]+)", live)
    if m:
        header_live = int(m.group(1), 16)
    m = re.search(r"\[JJFB_EXTCHUNK_FAULT\][^\n]*function_start=(0x[0-9A-Fa-f]+)", live)
    if m:
        fault_fn = int(m.group(1), 16)
    m = re.search(r"\[JJFB_EXTCHUNK_FAULT\][^\n]*memory_access_pc=(0x[0-9A-Fa-f]+)", live)
    if m:
        fault_pc = int(m.group(1), 16)
    base_m = re.search(r"\[JJFB_SHELL_EXT\][^\n]*base=(0x[0-9A-Fa-f]+)", live)
    code_base = int(base_m.group(1), 16) if base_m else None

    lines = [
        "# Phase 6J — MRPGCMAP entry decode",
        "",
        "Header entry candidate policy in core: `guest_code_base + 8` "
        "(**DOCUMENTED** image+8 / 8-byte `MRPGCMAP` prefix).",
        "",
        "## Live anchors (if stdout provided)",
        "",
        f"- shell EXT base: `{hex(code_base) if code_base else 'n/a'}`",
        f"- header_entry_candidate (live): `{hex(header_live) if header_live else 'n/a'}`",
        f"- observed_first_pc: `{hex(first_pc) if first_pc else 'n/a'}`",
        f"- fault function_start: `{hex(fault_fn) if fault_fn else 'n/a'}`",
        f"- fault memory_access_pc: `{hex(fault_pc) if fault_pc else 'n/a'}`",
        "",
    ]

    for label, blob in load_ext_targets(gwy):
        if not label.endswith((".ext",)):
            continue
        if "reg.ext" in label:
            continue
        hdr = mrpgcmap_header(blob)
        lines.append(f"## `{label}`")
        lines.append("")
        lines.append(f"- magic: `{hdr['magic']}`")
        lines.append(f"- prefix_hex: `{hdr['prefix_hex']}`")
        lines.append(f"- code_size: `{hdr['code_size']}`")
        lines.append(
            f"- entry_offset_candidate: `{hdr['entry_offset_candidate']}` "
            f"(evidence={hdr['evidence']})"
        )
        if code_base and "gbrwcore.ext" in label:
            cand_va = code_base + 8
            lines.append(f"- entry_va_if_base={hex(code_base)}: `{hex(cand_va)}`")
            if header_live is not None:
                lines.append(
                    f"- matches live header_entry_candidate: "
                    f"`{'yes' if cand_va == header_live else 'no'}`"
                )
            if first_pc is not None:
                off = first_pc - code_base
                lines.append(f"- first_pc file_offset: `0x{off:X}`")
                lines.append(
                    f"- first_pc == header+8: `{'yes' if first_pc == cand_va else 'no'}` "
                    "(TARGET_OBSERVED mismatch → WRONG_ENTRY_SELECTION)"
                )
            if fault_fn is not None:
                lines.append(f"- fault function file_offset: `0x{fault_fn - code_base:X}`")
        lines.append("")

    # Also decode standalone if members missing naming
    for pkg in ("gbrwcore.mrp", "jjfb.mrp", "wxjwq.mrp"):
        p = gwy / pkg
        if not p.is_file():
            continue
        for mem in ("gbrwcore.ext", "robotol.ext", "mrc_loader.ext"):
            blob = member_blob(p, mem)
            if not blob:
                continue
            # already covered via load_ext_targets; skip duplicate narrative
            _ = blob

    lines.append("## Interpretation")
    lines.append("")
    lines.append(
        "- If `first_pc` is a callback continuation (e.g. after `_mr_c_function_new`) "
        "and not `image+8`, publication/init at header entry may never run "
        "(**HYPOTHESIS** → conclusion B)."
    )
    lines.append(
        "- `mrc_loader.ext` size is tiny; compare jjfb vs wxjwq hashes in cross-target report."
    )
    lines.append("")

    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
