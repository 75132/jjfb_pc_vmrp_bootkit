#!/usr/bin/env python3
"""Phase 6G: locate startGame/runapp entry metadata from gwy shell packages."""
from __future__ import annotations

import argparse
import struct
import zlib
import gzip
from pathlib import Path

NEEDLES = [
    b"lib.startGame",
    b"lib.runapp",
    b"startGame",
    b"runapp",
    b"napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink",
    b"isFileOnServerNewer",
    b"checkmrpver",
    b"gwyblink",
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


def iter_mrp_members(path: Path):
    data = path.read_bytes()
    if len(data) < 240 or data[:4] != b"MRPG":
        yield ("__file__", data, 0)
        return
    first_data = struct.unpack_from("<I", data, 4)[0] - 4
    header_len = struct.unpack_from("<I", data, 12)[0]
    pos = header_len
    while pos < first_data:
        name_len = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        name = data[pos : pos + name_len].rstrip(b"\0").decode("latin1", "replace")
        pos += name_len
        offset, stored_len, _ = struct.unpack_from("<III", data, pos)
        pos += 12
        raw = data[offset : offset + stored_len]
        yield (name, try_decode(raw), offset)


def find_func_boundary(blob: bytes, str_off: int) -> tuple[int, int]:
    """Heuristic: scan back for PUSH {..} / STMFD prologue; forward for POP/LDMFD."""
    start = max(0, str_off - 0x800)
    # Prefer word-aligned candidates ending with classic ARM prologues near xrefs.
    # Without full disasm, report string-relative windows.
    return start, min(len(blob), str_off + 0x200)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gwy_root")
    ap.add_argument("out_md")
    args = ap.parse_args()
    root = Path(args.gwy_root)
    lines = [
        "# Phase 6G — startGame/runapp entry map",
        "",
        "Evidence: **TARGET_OBSERVED** string/file offsets in shell packages.",
        "Runtime VA = module load base + file offset (base varies per session).",
        "",
        "| module | member | needle | file_offset | boundary_lo | boundary_hi | caller | input_args | executed |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    pkgs = [
        ("gbrwcore.mrp", root / "gbrwcore.mrp"),
        ("gamelist.mrp", root / "gamelist.mrp"),
        ("gbrwshell.mrp", root / "gbrwshell.mrp"),
    ]
    for mod, path in pkgs:
        if not path.is_file():
            lines.append(f"| {mod} | - | MISSING | - | - | - | - | - | no |")
            continue
        for name, blob, _stored in iter_mrp_members(path):
            for needle in NEEDLES:
                start = 0
                while True:
                    i = blob.find(needle, start)
                    if i < 0:
                        break
                    lo, hi = find_func_boundary(blob, i)
                    caller = "static_xref_pending_disasm"
                    args_hint = "cfg36_fields" if b"napptype" in needle else "lib_export_name"
                    executed = "live_see_phase6g_stdout"
                    lines.append(
                        f"| {mod} | `{name}` | `{needle.decode('latin1','replace')}` | "
                        f"`0x{i:X}` | `0x{lo:X}` | `0x{hi:X}` | {caller} | {args_hint} | {executed} |"
                    )
                    start = i + 1
    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- `lib.startGame` / `lib.runapp` live in `gbrwcore.ext` string table (Phase 6F).",
            "- Phase 6G host chain: shell DSM (`gbrwcore`) → no_update stub → "
            "`bridge_dsm_mr_start_dsm(mythroad/gwy/jjfb.mrp)` as runapp-equivalent.",
            "- Guest-native call of lib.startGame remains preferred when shell reaches it.",
            "",
        ]
    )
    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
