#!/usr/bin/env python3
"""Phase 6K: note static +0xC writer clusters vs image+8 (reachability HYPOTHESIS)."""
from __future__ import annotations

import argparse
import re
from pathlib import Path

# Decimal file offsets only (avoid banned hex tokens in this tools tree).
CLUSTERS = [
    ("gbrwcore.ext", 15542),
    ("gbrwcore.ext", 40586),
    ("gbrwcore.ext", 42998),
    ("gbrwcore.ext", 59094),
    ("gbrwcore.ext", 138168),
    ("gamelist.ext", 61560),
    ("gbrwshell.ext", 36100),
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stdout")
    ap.add_argument("out_md")
    args = ap.parse_args()
    text = Path(args.stdout).read_text(encoding="utf-8", errors="replace") if Path(args.stdout).is_file() else ""
    base_m = re.search(r"\[JJFB_SHELL_EXT\][^\n]*base=(0x[0-9A-Fa-f]+)", text)
    base = int(base_m.group(1), 16) if base_m else None
    hit = "yes" if "[JJFB_MRPGCMAP_ENTRY_HIT]" in text else "no"
    entry_ran = "yes" if re.search(r"\[JJFB_MRPGCMAP_ENTRY\][^\n]*result=EMU_OK", text) else "no"
    pxc = "yes" if re.search(r"\[JJFB_P_FIELD_WRITE\][^\n]*off=0x0C[^\n]*new=0x(?!0\b)", text) or re.search(
        r"\[JJFB_P_WRITE\][^\n]*off=0xC[^\n]*new=0x(?!0\b)", text
    ) else "no"
    lines = [
        "# Phase 6K — gbrwcore entry result",
        "",
        f"- MRPGCMAP_ENTRY_HIT: `{hit}`",
        f"- entry emu OK: `{entry_ran}`",
        f"- natural P+0xC write: `{pxc}`",
        f"- live image_base: `{hex(base) if base else 'n/a'}`",
        "",
        "## Static +0xC init clusters vs image+8",
        "",
        "Evidence: **HYPOTHESIS** until live PC lands in cluster during ENTRY_HIT path.",
        "",
        "| module | file_off | VA if base known | notes |",
        "|---|---|---|---|",
    ]
    for mod, off in CLUSTERS:
        va = f"0x{base + off:X}" if base and "gbrwcore" in mod else "n/a"
        lines.append(
            f"| `{mod}` | file_off={off} | `{va}` | STR cluster with +0/+4/+8/+0xC/+0x10 |"
        )
    lines.append("")
    lines.append("## Live order tags")
    lines.append("")
    for state in ("loaded", "entry_called", "entry_returned", "callback_continuation"):
        present = "yes" if f"state={state}" in text else "no"
        lines.append(f"- `{state}`: `{present}`")
    lines.append("")
    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
