#!/usr/bin/env python3
"""Phase 6H: gamelist post-update / cfg36 branch static map + live merge."""
from __future__ import annotations

import argparse
import re
import struct
import zlib
import gzip
from pathlib import Path

CFG36 = b"napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink"
NEEDLES = [
    CFG36,
    b"lib.runapp",
    b"lib.startGame",
    b"gwyblink",
    b"cfunction.ext",
    b"checkmrpver",
    b"isFileOnServerNewer",
]


def try_decode(raw: bytes) -> bytes:
    for fn in (
        gzip.decompress,
        lambda b: zlib.decompress(b, 16 + zlib.MAX_WBITS),
        zlib.decompress,
    ):
        try:
            return fn(raw)
        except Exception:
            pass
    return raw


def member_blob(path: Path, want: str):
    data = path.read_bytes()
    first = struct.unpack_from("<I", data, 4)[0] - 4
    hlen = struct.unpack_from("<I", data, 12)[0]
    pos = hlen
    while pos < first:
        nl = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        name = data[pos : pos + nl].rstrip(b"\0").decode("latin1", "replace")
        pos += nl
        off, slen, _ = struct.unpack_from("<III", data, pos)
        pos += 12
        if want in name:
            return try_decode(data[off : off + slen])
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gwy_root")
    ap.add_argument("stdout_log")
    ap.add_argument("out_md")
    args = ap.parse_args()
    root = Path(args.gwy_root)
    text = Path(args.stdout_log).read_text(encoding="utf-8", errors="replace") if Path(args.stdout_log).is_file() else ""
    blob = member_blob(root / "gamelist.mrp", "gamelist.ext")
    lines = [
        "# Phase 6H — gamelist post-update success branch",
        "",
        "Evidence: **TARGET_OBSERVED** static strings + live tags.",
        "",
        "## Static map (`gamelist.ext`)",
        "",
    ]
    if blob is None:
        lines.append("- gamelist.ext: **MISSING**")
    else:
        lines.append(f"- size: `{len(blob)}`")
        lines.append("")
        lines.append("| needle | file_offset | next_step_hypothesis |")
        lines.append("|---|---|---|")
        for n in NEEDLES:
            i = blob.find(n)
            if i < 0:
                continue
            nxt = "unknown"
            if n == CFG36:
                nxt = "build cfg36 then call gbrwcore lib.startGame/lib.runapp or dispatcher (HYPOTHESIS)"
            elif b"runapp" in n or b"startGame" in n:
                nxt = "string only if present; gamelist may resolve via gbrwcore export table"
            elif b"checkmrpver" in n or b"isFileOnServerNewer" in n:
                nxt = "update-check path; no_update stub should take success/skip-download branch"
            lines.append(f"| `{n.decode('latin1','replace')}` | `0x{i:X}` | {nxt} |")

    live_cfg = "yes" if "[JJFB_GAMELIST_CFG36_BUILD]" in text else "no"
    live_post = "yes" if "[JJFB_GAMELIST_POST_UPDATE]" in text else "no"
    live_call = "yes" if "[JJFB_GAMELIST_CALL]" in text or re.search(r"via=guest_native", text) else "no"
    live_host = "yes" if "host_runapp_equivalent_after_no_update" in text else "no"

    lines.extend(
        [
            "",
            "## Live observation",
            "",
            f"- CFG36 build tag: `{live_cfg}`",
            f"- post-update branch tag: `{live_post}`",
            f"- guest-native call evidence: `{live_call}`",
            f"- host_runapp_equivalent still present: `{live_host}` (must be **no** for 6H mid success)",
            "",
            "## Required branch (target)",
            "",
            "```text",
            "gamelist.ext",
            "  -> sprintf cfg36 (napptype/nextid/ncode/narg/narg1/nmrpname/gwyblink)",
            "  -> update check => no_update / update_ok",
            "  -> gbrwcore lib.startGame / lib.runapp (guest-native)",
            "  -> start gwy/jjfb.mrp",
            "```",
            "",
        ]
    )
    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
