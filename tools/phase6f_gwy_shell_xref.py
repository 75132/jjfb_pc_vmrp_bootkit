#!/usr/bin/env python3
"""Phase 6F: static string xref over GWY shell packages (read-only)."""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
import zlib
import gzip
from pathlib import Path

NEEDLES = [
    b"startGame",
    b"runapp",
    b"RunApp",
    b"gwyblink",
    b"napptype",
    b"nextid",
    b"ncode",
    b"nmrpname",
    b"mrc_extChunk",
    b"extChunk",
    b"cfunction",
    b"robotol",
    b"download",
    b"isFileOnServerNewer",
    b"getFileVersion",
    b"getClientInfo",
    b"simpleDownload",
    b"continueDownload",
    b"dl_confirm",
    b"_strCom",
    b"TestCom",
    b"mrc_init",
    b"gwy/jjfb.mrp",
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
        yield ("__file__", data)
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
        yield (name, try_decode(raw))


def find_strings(blob: bytes) -> list[dict]:
    hits = []
    for needle in NEEDLES:
        start = 0
        while True:
            i = blob.find(needle, start)
            if i < 0:
                break
            ctx = blob[max(0, i - 16) : i + len(needle) + 32]
            printable = "".join(chr(b) if 32 <= b < 127 else "." for b in ctx)
            hits.append(
                {
                    "needle": needle.decode("latin1", "replace"),
                    "offset": i,
                    "context": printable,
                }
            )
            start = i + 1
    return hits


def scan_path(path: Path) -> dict:
    out = {"path": str(path), "exists": path.exists(), "members": []}
    if not path.exists():
        return out
    if path.suffix.lower() == ".bin" or path.name == "cfg.bin":
        hits = find_strings(path.read_bytes())
        out["members"].append({"name": path.name, "hits": hits, "hit_count": len(hits)})
        return out
    for name, blob in iter_mrp_members(path):
        hits = find_strings(blob)
        if hits:
            out["members"].append({"name": name, "hits": hits, "hit_count": len(hits)})
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gwy_root", type=Path, help=".../mythroad/240x320/gwy")
    ap.add_argument("--json", type=Path)
    ap.add_argument("--md", type=Path)
    args = ap.parse_args()
    root = args.gwy_root
    targets = [
        root / "cfg.bin",
        root / "gamelist.mrp",
        root / "gbrwcore.mrp",
        root / "gbrwshell.mrp",
        root / "vdload.mrp",
        root / "jjfb.mrp",
    ]
    report = {"gwy_root": str(root), "packages": [scan_path(p) for p in targets]}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Phase 6F — gwy startGame/runapp static chain (string xref)",
        "",
        f"Resource gwy root: `{root}`",
        "",
        "Evidence: **TARGET_OBSERVED** string presence in packages; not proof of execution.",
        "",
    ]
    for pkg in report["packages"]:
        lines.append(f"## `{Path(pkg['path']).name}`")
        lines.append("")
        if not pkg["exists"]:
            lines.append("_missing_")
            lines.append("")
            continue
        if not pkg["members"]:
            lines.append("_no needle hits_")
            lines.append("")
            continue
        for m in pkg["members"]:
            lines.append(f"### member `{m['name']}` ({m['hit_count']} hits)")
            lines.append("")
            lines.append("| needle | offset | context |")
            lines.append("|---|---|---|")
            for h in m["hits"][:40]:
                lines.append(f"| `{h['needle']}` | `0x{h['offset']:X}` | `{h['context']}` |")
            lines.append("")
    if args.md:
        args.md.parent.mkdir(parents=True, exist_ok=True)
        args.md.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(json.dumps({"packages": len(report["packages"]), "ok": True}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
