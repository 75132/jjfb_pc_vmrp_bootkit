#!/usr/bin/env python3
"""Phase 6J: static Thumb STR [Rn,#imm] xref for P-like field writers."""
from __future__ import annotations

import argparse
from pathlib import Path

from phase6j_common import (
    FIELD_IMMS,
    cluster_str_sites,
    load_ext_targets,
    scan_thumb_str_imm,
)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gwy_root")
    ap.add_argument("out_md")
    args = ap.parse_args()
    gwy = Path(args.gwy_root)
    lines = [
        "# Phase 6J — P field writer xref (static)",
        "",
        "Evidence tags: static Thumb `STR Rt,[Rn,#imm]` for imm in `{0,4,8,0xC,0x10}`.",
        "Class: **HYPOTHESIS** until live PC proves execution; sites in shared modules = **CROSS_TARGET** candidates.",
        "",
        "## DOCUMENTED P layout (mr_helper.h)",
        "",
        "```",
        "+0x00 start_of_ER_RW",
        "+0x04 ER_RW_Length",
        "+0x08 ext_type",
        "+0x0C mrc_extChunk*",
        "+0x10 stack",
        "```",
        "",
    ]

    for label, blob in load_ext_targets(gwy):
        hits = scan_thumb_str_imm(blob, FIELD_IMMS)
        clusters = cluster_str_sites(hits, window=0x50)
        interesting = [c for c in clusters if c["has_0c"] or c["has_0_4_8"] or len(c["imms"]) >= 2]
        lines.append(f"## `{label}` size={len(blob)}")
        lines.append("")
        lines.append(f"- STR field sites: `{len(hits)}`")
        lines.append(f"- multi-field clusters (window 0x50): `{len(interesting)}`")
        lines.append("")
        # Prefer clusters that write +0xC or both +0/+4/+8
        ranked = sorted(
            interesting,
            key=lambda c: (1 if c["has_0c"] else 0, 1 if c["has_0_4_8"] else 0, len(c["imms"])),
            reverse=True,
        )
        for c in ranked[:24]:
            lines.append(
                f"- cluster `@0x{c['start']:X}..0x{c['end']:X}` imms=`{[hex(i) for i in c['imms']]}` "
                f"has_0c=`{c['has_0c']}` has_0_4_8=`{c['has_0_4_8']}`"
            )
            for s in c["sites"][:8]:
                lines.append(
                    f"  - `0x{s['file_offset']:X}` `{s['disasm']}` insn=`{s['insn']}`"
                )
        # Alone +0xC writers
        alone_0c = [h for h in hits if h["imm"] == 0xC]
        lines.append("")
        lines.append(f"### All `STR …,#0xC` sites ({len(alone_0c)})")
        lines.append("")
        for s in alone_0c[:40]:
            lines.append(f"- `0x{s['file_offset']:X}` `{s['disasm']}`")
        if len(alone_0c) > 40:
            lines.append(f"- … truncated ({len(alone_0c) - 40} more)")
        lines.append("")

    lines.append("## Notes")
    lines.append("")
    lines.append(
        "- Clusters with `has_0_4_8=yes` and `has_0c=no` are prime candidates for "
        "“wrote +0/+4/+8 but skipped +0xC” (**HYPOTHESIS** until live)."
    )
    lines.append(
        "- Live Phase 6H nested P writers were near gbrwcore file offset `~0x212C6` "
        "(TARGET_OBSERVED CONTEXT_FIELD_WRITE)."
    )
    lines.append("")

    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
