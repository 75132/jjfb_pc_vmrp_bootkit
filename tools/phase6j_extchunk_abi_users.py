#!/usr/bin/env python3
"""Phase 6J: static LDR [Rn,#0x28] users (extChunk ABI slot)."""
from __future__ import annotations

import argparse
from pathlib import Path

from phase6j_common import load_ext_targets, scan_thumb_ldr_imm, scan_thumb_str_imm


def nearby_p0c_load(blob: bytes, site_off: int, window: int = 0x30) -> list[dict]:
    """LDR …,[Rn,#0xC] within window before a #0x28 use (possible P+0xC then +0x28)."""
    lo = max(0, site_off - window)
    hits = []
    for h in scan_thumb_ldr_imm(blob[lo:site_off], {0xC}):
        h = dict(h)
        h["file_offset"] = lo + h["file_offset"]
        hits.append(h)
    return hits


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gwy_root")
    ap.add_argument("out_md")
    args = ap.parse_args()
    gwy = Path(args.gwy_root)
    lines = [
        "# Phase 6J — mrc_extChunk ABI users (`+0x28`)",
        "",
        "Live 6H fault: `expr=r0+0x28 r0=0` with `r0` expected as `P+0xC` (**TARGET_OBSERVED**).",
        "Static scan: Thumb `LDR Rt,[Rn,#0x28]`. Nearby `LDR …,#0xC` = candidate P→chunk→slot chain (**HYPOTHESIS**).",
        "",
    ]

    compare: dict[str, int] = {}
    for label, blob in load_ext_targets(gwy):
        if not any(x in label for x in ("gbrwcore.ext", "robotol.ext", "cfunction.ext", "gamelist.ext")):
            continue
        hits = scan_thumb_ldr_imm(blob, {0x28})
        compare[label] = len(hits)
        with_p = 0
        lines.append(f"## `{label}` size={len(blob)}")
        lines.append("")
        lines.append(f"- LDR #0x28 sites: `{len(hits)}`")
        lines.append("")
        for h in hits[:50]:
            near = nearby_p0c_load(blob, h["file_offset"])
            flag = " near_LDR_#0xC" if near else ""
            if near:
                with_p += 1
            lines.append(
                f"- `0x{h['file_offset']:X}` `{h['disasm']}`{flag}"
            )
            for n in near[:3]:
                lines.append(f"  - prior `0x{n['file_offset']:X}` `{n['disasm']}`")
        if len(hits) > 50:
            lines.append(f"- … truncated ({len(hits) - 50} more)")
        lines.append(f"- sites with nearby LDR #0xC: `{with_p}`")
        lines.append("")

    lines.append("## Cross-module count")
    lines.append("")
    for k, v in sorted(compare.items()):
        lines.append(f"- `{k}`: `{v}`")
    lines.append("")
    lines.append(
        "If gbrwcore and jjfb/robotol both have many `#0x28` users, the slot is a "
        "**CROSS_TARGET** platform ABI, not jjfb-only."
    )
    lines.append("")

    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
