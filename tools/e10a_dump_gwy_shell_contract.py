#!/usr/bin/env python3
"""E10A-Fix Lane A: parse original GWY shell contract from game_files.

Outputs:
  reports/e10a_gwy_shell_inventory.csv
  reports/e10a_gwy_cfg_records.csv
  reports/e10a_shell_strings.csv
  reports/e10a_shell_file_dependencies.csv
  out/e10a_shell/launch_transition_graph.md
"""
from __future__ import annotations

import csv
import hashlib
import re
import struct
import sys
import zlib
import gzip
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GWY = ROOT / "game_files" / "mythroad" / "320x480" / "gwy"
OUT = ROOT / "reports"
GRAPH = ROOT / "out" / "e10a_shell" / "launch_transition_graph.md"

RECORD_BASE = 1024
RECORD_SIZE = 272

SHELL_MRPS = [
    "gbrwcore.mrp",
    "gamelist.mrp",
    "gbrwshell.mrp",
    "roomlist.mrp",
    "vdload.mrp",
    "dload.mrp",
    "reglogin.mrp",
    "resmng.mrp",
    "font.mrp",
    "directpay.mrp",
    "smscharge.mrp",
    "jjfb.mrp",
]

STRING_NEEDLES = [
    "runapp",
    "startGame",
    "download",
    "update",
    "post_update",
    "no_update",
    "no update",
    "update success",
    "update fail",
    "simpleDownload",
    "continueDownload",
    "gwy/jjfb.mrp",
    "jjfb",
    "downimage",
    "roomlist",
    "gamelist",
    "gbrwcore",
    "gbrwshell",
    "module48",
    "cfg.bin",
    "gwyblink",
    "napptype",
    "nmrpname",
    "mrc_init",
    "mrc_extChunk",
    "isFileOnServerNewer",
    "getFileVersion",
    "checkmrpver",
]

ASCII_RE = re.compile(rb"[\x20-\x7e]{4,160}")


def rel(p: Path) -> str:
    try:
        return str(p.resolve().relative_to(ROOT.resolve())).replace("\\", "/")
    except Exception:
        return str(p).replace("\\", "/")


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


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


def parse_mrp(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 240 or data[:4] != b"MRPG":
        return {"members": [], "error": "not_mrpg"}
    first_data = struct.unpack_from("<I", data, 4)[0] - 4
    header_len = struct.unpack_from("<I", data, 12)[0]
    pos = header_len
    members = []
    while pos < first_data:
        name_len = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        name = data[pos : pos + name_len].rstrip(b"\0").decode("latin1", "replace")
        pos += name_len
        offset, stored_len, _ = struct.unpack_from("<III", data, pos)
        pos += 12
        members.append({"name": name, "offset": offset, "stored_length": stored_len})
    appid = struct.unpack_from("<I", data, 68)[0]
    appver = struct.unpack_from("<I", data, 72)[0]
    return {"members": members, "appid": appid, "appver": appver, "size": len(data)}


def be24(buf: bytes) -> int:
    return int.from_bytes(buf, "big")


def parse_cfg_record(data: bytes, index: int) -> dict:
    off = RECORD_BASE + index * RECORD_SIZE
    rec = data[off : off + RECORD_SIZE]
    if len(rec) != RECORD_SIZE:
        return {}
    path_match = re.search(rb"gwy/[A-Za-z0-9_./-]+\.mrp", rec)
    title = rec[0x5C:0x70].decode("utf-16be", "replace").rstrip("\0")
    return {
        "index": index,
        "file_offset": off,
        "icon": rec[0x40:0x58].split(b"\0")[0].decode("ascii", "replace"),
        "napptype": rec[0x57],
        "title_suffix": title,
        "nextid": be24(rec[0x72:0x75]),
        "ncode": be24(rec[0x78:0x7B]),
        "narg": be24(rec[0x7B:0x7E]),
        "narg1": rec[0x7E],
        "target_mrp": path_match.group().decode() if path_match else "",
    }


def scan_strings(blob: bytes, source: str, rows: list[dict]) -> None:
    low = blob.lower()
    for needle in STRING_NEEDLES:
        nb = needle.lower().encode("ascii")
        start = 0
        while True:
            i = low.find(nb, start)
            if i < 0:
                break
            ctx = blob[max(0, i - 8) : i + len(nb) + 48]
            s = "".join(chr(b) if 32 <= b < 127 else "." for b in ctx)
            rows.append(
                {
                    "source_file": source,
                    "needle": needle,
                    "offset": i,
                    "context": s[:120],
                }
            )
            start = i + 1
    for m in ASCII_RE.finditer(blob):
        s = m.group().decode("ascii", errors="ignore")
        if any(n in s.lower() for n in ("gwy/", ".mrp", "downimage", "update", "runapp")):
            rows.append(
                {
                    "source_file": source,
                    "needle": "(ascii)",
                    "offset": m.start(),
                    "context": s[:120],
                }
            )


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    GRAPH.parent.mkdir(parents=True, exist_ok=True)

    inv_rows: list[dict] = []
    cfg_rows: list[dict] = []
    str_rows: list[dict] = []
    dep_rows: list[dict] = []

    cfg_path = GWY / "cfg.bin"
    if cfg_path.is_file():
        cfg_data = cfg_path.read_bytes()
        max_idx = (len(cfg_data) - RECORD_BASE) // RECORD_SIZE
        for idx in range(max(0, max_idx)):
            rec = parse_cfg_record(cfg_data, idx)
            if not rec.get("target_mrp") and not rec.get("title_suffix"):
                continue
            rec["source_file"] = rel(cfg_path)
            rec["sha256"] = sha256_file(cfg_path)
            cfg_rows.append(rec)
        scan_strings(cfg_data, rel(cfg_path), str_rows)

    for name in SHELL_MRPS:
        p = GWY / name
        if not p.is_file():
            inv_rows.append(
                {
                    "pack_name": name,
                    "pack_path": rel(p),
                    "exists": 0,
                    "member_count": 0,
                    "sha256": "",
                    "appid": "",
                    "appver": "",
                    "role": "missing",
                }
            )
            continue
        try:
            rep = parse_mrp(p)
        except Exception as e:
            inv_rows.append(
                {
                    "pack_name": name,
                    "pack_path": rel(p),
                    "exists": 1,
                    "member_count": 0,
                    "sha256": sha256_file(p),
                    "appid": "",
                    "appver": "",
                    "role": f"parse_error:{e}",
                }
            )
            continue
        role = "shell_core" if "gbrwcore" in name else "shell"
        if "gamelist" in name:
            role = "gamelist"
        elif "jjfb" in name:
            role = "game_target"
        elif "downimage" in name or "vdload" in name or "dload" in name:
            role = "download"
        inv_rows.append(
            {
                "pack_name": name,
                "pack_path": rel(p),
                "exists": 1,
                "member_count": len(rep.get("members") or []),
                "sha256": sha256_file(p),
                "appid": f"0x{rep.get('appid', 0):X}",
                "appver": f"0x{rep.get('appver', 0):X}",
                "role": role,
            }
        )
        data = p.read_bytes()
        for mem in rep.get("members") or []:
            mname = mem["name"]
            inv_rows.append(
                {
                    "pack_name": name,
                    "pack_path": rel(p),
                    "exists": 1,
                    "member_count": 1,
                    "member_name": mname,
                    "sha256": "",
                    "appid": "",
                    "appver": "",
                    "role": "member",
                }
            )
            raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
            decoded = try_decode(raw)
            src = f"{rel(p)}::{mname}"
            scan_strings(decoded if decoded else raw, src, str_rows)
            if mname.endswith(".ext") or mname == "start.mr":
                dep_rows.append(
                    {
                        "source_pack": name,
                        "member": mname,
                        "depends_on": "ext_or_start",
                        "referenced": mname,
                    }
                )

    jjfbol = GWY / "jjfbol"
    if jjfbol.is_dir():
        for p in sorted(jjfbol.glob("downimage*.mrp")):
            if not p.is_file():
                continue
            rep = parse_mrp(p)
            inv_rows.append(
                {
                    "pack_name": p.name,
                    "pack_path": rel(p),
                    "exists": 1,
                    "member_count": len(rep.get("members") or []),
                    "sha256": sha256_file(p),
                    "appid": "",
                    "appver": "",
                    "role": "side_pack_downimage",
                }
            )
            dep_rows.append(
                {
                    "source_pack": "jjfb.mrp",
                    "member": "robotol.ext",
                    "depends_on": "side_pack",
                    "referenced": rel(p),
                }
            )

    cfg36 = next((r for r in cfg_rows if r.get("index") == 36), None)
    if not cfg36:
        cfg36 = parse_cfg_record(cfg_path.read_bytes(), 36) if cfg_path.is_file() else {}

    graph = f"""# E10A GWY Launch Transition Graph

Parsed from `{rel(GWY)}`.

## Observed chain (TARGET_OBSERVED + static inventory)

```
GWY entry (gwy.mrp / launcher)
  -> gbrwcore.mrp / gbrwcore.ext
  -> gamelist.mrp / gamelist.ext
  -> cfg.bin record select (index 36 observed for jjfb)
  -> update / no-update / post_update branch
  -> lib.startGame / lib.runapp
  -> target MRP (e.g. {cfg36.get('target_mrp', 'gwy/jjfb.mrp')})
  -> robotol.ext splash @ 0x2EF86C
  -> AC8 gate @ 0x2EF8AE (logo vs loading-only)
```

## cfg index 36 (if present)

| field | value |
|-------|-------|
| target | `{cfg36.get('target_mrp', '')}` |
| napptype | {cfg36.get('napptype', '')} |
| nextid | {cfg36.get('nextid', '')} |
| ncode | {cfg36.get('ncode', '')} |
| narg | {cfg36.get('narg', '')} |
| narg1 | {cfg36.get('narg1', '')} |
| title_suffix | {cfg36.get('title_suffix', '')} |

## Side-pack / downimage

- `showN!WxH@downimageN.bmp` members live in `gwy/jjfbol/downimageN.mrp`
- `@downimage` string referenced from robotol.ext (not main jjfb.mrp members)

## Inventory summary

- shell packs scanned: {len(SHELL_MRPS)}
- cfg records with target: {len(cfg_rows)}
- string hits: {len(str_rows)}

Evidence: static parse only; does not prove runtime order without shell_trace.
"""
    GRAPH.write_text(graph, encoding="utf-8")

    def write_csv(path: Path, fields: list[str], rows: list[dict]) -> None:
        with path.open("w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
            w.writeheader()
            w.writerows(rows)

    write_csv(
        OUT / "e10a_gwy_shell_inventory.csv",
        ["pack_name", "pack_path", "exists", "member_count", "member_name", "sha256", "appid", "appver", "role"],
        inv_rows,
    )
    write_csv(
        OUT / "e10a_gwy_cfg_records.csv",
        ["source_file", "index", "file_offset", "icon", "napptype", "title_suffix", "nextid", "ncode", "narg", "narg1", "target_mrp", "sha256"],
        cfg_rows,
    )
    write_csv(OUT / "e10a_shell_strings.csv", ["source_file", "needle", "offset", "context"], str_rows)
    write_csv(
        OUT / "e10a_shell_file_dependencies.csv",
        ["source_pack", "member", "depends_on", "referenced"],
        dep_rows,
    )

    print(f"Wrote {OUT / 'e10a_gwy_shell_inventory.csv'} rows={len(inv_rows)}")
    print(f"Wrote {OUT / 'e10a_gwy_cfg_records.csv'} rows={len(cfg_rows)}")
    print(f"Wrote {OUT / 'e10a_shell_strings.csv'} rows={len(str_rows)}")
    print(f"Wrote {GRAPH}")
    print("GWY_SHELL_CONTRACT_PARSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
