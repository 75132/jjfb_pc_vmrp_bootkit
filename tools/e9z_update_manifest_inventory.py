#!/usr/bin/env python3
"""E9Z Task 2: update/downimage manifest inventory from original files.

Outputs:
  reports/e9z_update_manifest_inventory.csv
"""
from __future__ import annotations

import csv
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mrp_inspect import parse, try_decode  # noqa: E402

OUT_CSV = ROOT / "reports" / "e9z_update_manifest_inventory.csv"
GWY = ROOT / "game_files" / "mythroad" / "320x480" / "gwy"
MAIN = GWY / "jjfb.mrp"
JJFBOL = GWY / "jjfbol"
EXT_ROBOTOL = ROOT / "out" / "JJFB_E8A_delivery" / "02_mrp_extracted" / "jjfb" / "robotol.ext"
EXT_MRC = ROOT / "out" / "JJFB_E8A_delivery" / "02_mrp_extracted" / "jjfb" / "mrc_loader.ext"

# Generic patterns — names come from files, not hardcode success criteria
PAT_FOCUS = re.compile(
    r"(downimage|@pack|update|version|gamelist|module48|splash|resource|pack|"
    r"download|ready|check|ol/|\.mrp)",
    re.I,
)
ASCII_RE = re.compile(rb"[\x20-\x7e]{4,120}")


def rel(p: Path) -> str:
    try:
        return str(p.relative_to(ROOT)).replace("\\", "/")
    except ValueError:
        return str(p).replace("\\", "/")


def classify(s: str) -> str:
    low = s.lower()
    if "downimage" in low:
        return "splash_pack"
    if "update" in low or "version" in low or "check" in low:
        return "update_pack"
    if low.endswith(".mrp") or "@" in low or "pack" in low:
        return "side_pack"
    return "unknown"


def extract_strings(data: bytes, source: str, rows: list[dict]) -> None:
    for m in ASCII_RE.finditer(data):
        s = m.group().decode("ascii", errors="ignore")
        if not PAT_FOCUS.search(s):
            continue
        pack = ""
        res = ""
        if s.lower().endswith(".mrp"):
            pack = Path(s.replace("\\", "/")).name
        if "@" in s and "." in s:
            res = s
        rows.append(
            {
                "source_file": source,
                "string_or_record": s[:200],
                "pack_name": pack,
                "referenced_resource": res,
                "version_or_appid": "",
                "relation": classify(s),
                "candidate_event_code": "",
            }
        )


def scan_mrp_members(path: Path, rows: list[dict]) -> None:
    if not path.is_file():
        return
    src = rel(path)
    try:
        rep = parse(path)
    except Exception as e:
        rows.append(
            {
                "source_file": src,
                "string_or_record": f"<parse_error:{e}>",
                "pack_name": path.name,
                "referenced_resource": "",
                "version_or_appid": "",
                "relation": "unknown",
                "candidate_event_code": "",
            }
        )
        return
    appid = rep.get("appid", "")
    appver = rep.get("appver", "")
    rows.append(
        {
            "source_file": src,
            "string_or_record": f"mrp_header appid={appid} appver={appver} members={len(rep.get('members') or [])}",
            "pack_name": path.stem,
            "referenced_resource": "",
            "version_or_appid": f"appid={appid};appver={appver}",
            "relation": classify(path.name),
            "candidate_event_code": "",
        }
    )
    data = path.read_bytes()
    for mem in rep.get("members") or []:
        name = mem["name"]
        keep = PAT_FOCUS.search(name) or name.endswith(".mr") or name.endswith(".ext")
        if not keep:
            continue
        rows.append(
            {
                "source_file": src,
                "string_or_record": name,
                "pack_name": path.stem,
                "referenced_resource": name,
                "version_or_appid": f"appid={appid};appver={appver}",
                "relation": classify(name),
                "candidate_event_code": "",
            }
        )
        # Decode small text-ish members for update/manifest strings
        if mem["stored_length"] <= 256000 and (
            name.endswith(".mr")
            or name.endswith(".cfg")
            or name.endswith(".txt")
            or "update" in name.lower()
            or "list" in name.lower()
        ):
            raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
            decoded, _ = try_decode(raw)
            extract_strings(decoded if decoded else raw, f"{src}::{name}", rows)


def scan_cfg_gamelist(rows: list[dict]) -> None:
    for p in [
        ROOT / "game_files" / "mythroad" / "320x480" / "gwy.cfg",
        ROOT / "game_files" / "mythroad" / "gwy.cfg",
        ROOT / "game_files" / "mythroad" / "320x480" / "gamelist",
        ROOT / "game_files" / "mythroad" / "gamelist",
    ]:
        if not p.is_file():
            continue
        extract_strings(p.read_bytes(), rel(p), rows)


def scan_ext(path: Path, rows: list[dict]) -> None:
    if not path.is_file():
        return
    data = path.read_bytes()
    extract_strings(data, rel(path), rows)
    # Hex-ish event codes near focus strings (loose)
    for m in re.finditer(rb"(downimage|update|resource|@downimage)", data, re.I):
        off = m.start()
        window = data[max(0, off - 32) : off + 64]
        for hm in re.finditer(rb"0x[0-9A-Fa-f]{2,8}", window):
            rows.append(
                {
                    "source_file": rel(path),
                    "string_or_record": m.group().decode("ascii", "ignore"),
                    "pack_name": "",
                    "referenced_resource": "",
                    "version_or_appid": "",
                    "relation": "unknown",
                    "candidate_event_code": hm.group().decode("ascii"),
                }
            )


def main() -> int:
    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    scan_mrp_members(MAIN, rows)
    if JJFBOL.is_dir():
        for p in sorted(JJFBOL.glob("downimage*.mrp")):
            scan_mrp_members(p, rows)
        # Sample a few other packs' start.mr for update strings
        for p in sorted(JJFBOL.glob("*.mrp"))[:8]:
            if "downimage" in p.name.lower():
                continue
            scan_mrp_members(p, rows)
    scan_cfg_gamelist(rows)
    scan_ext(EXT_ROBOTOL, rows)
    scan_ext(EXT_MRC, rows)
    # Historical contract note from E9Y: evt 0x14 @ 0x30D300
    rows.append(
        {
            "source_file": "reports/e9y_event_contract_trace.csv",
            "string_or_record": "observed_dispatcher_event",
            "pack_name": "",
            "referenced_resource": "",
            "version_or_appid": "",
            "relation": "unknown",
            "candidate_event_code": "0x14",
        }
    )

    fields = [
        "source_file",
        "string_or_record",
        "pack_name",
        "referenced_resource",
        "version_or_appid",
        "relation",
        "candidate_event_code",
    ]
    # Dedup
    seen = set()
    uniq = []
    for r in rows:
        key = (
            r["source_file"],
            r["string_or_record"],
            r.get("candidate_event_code", ""),
        )
        if key in seen:
            continue
        seen.add(key)
        uniq.append(r)

    with OUT_CSV.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(uniq)
    print(f"Wrote {OUT_CSV} rows={len(uniq)}")
    print("GWY_UPDATE_MANIFEST_PARSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
