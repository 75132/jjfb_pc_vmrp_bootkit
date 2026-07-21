#!/usr/bin/env python3
"""E9Y-Fix Task 1: inventory downimage splash contract (readonly MRP/EXT).

Outputs:
  reports/e9y_downimage_contract_inventory.csv
  out/e9y_fix/downimage_string_xrefs.txt
"""
from __future__ import annotations

import csv
import hashlib
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mrp_inspect import parse, try_decode  # noqa: E402

GWY = ROOT / "game_files" / "mythroad" / "320x480" / "gwy"
MAIN_MRP = GWY / "jjfb.mrp"
JJFBOL = GWY / "jjfbol"
OUT_CSV = ROOT / "reports" / "e9y_downimage_contract_inventory.csv"
OUT_XREF = ROOT / "out" / "e9y_fix" / "downimage_string_xrefs.txt"

NAME_WH = re.compile(r"^(.+)!(\d+)!(\d+)(?:@([^.]+))?\.(\w+)$", re.I)
NAME_WH_SIMPLE = re.compile(r"^(.+)!(\d+)!(\d+)\.(\w+)$", re.I)

# Must appear in inventory (exact member or pack path)
REQUIRED = [
    "downimage1.mrp",
    "show1!232!100@downimage1.bmp",
    "loadingbar!201!29.bmp",
    "bar!16!18.bmp",
    "textbar!120!30.bmp",
]

FOCUS_PAT = re.compile(
    r"(downimage|show\d|loadingbar|textbar|^bar!|@downimage|slogo)",
    re.I,
)


def parse_dims(name: str):
    m = NAME_WH.match(name)
    if m:
        return int(m.group(2)), int(m.group(3)), m.group(4) or "", m.group(5)
    m = NAME_WH_SIMPLE.match(name)
    if m:
        return int(m.group(2)), int(m.group(3)), "", m.group(4)
    return None, None, "", ""


def name_pattern(name: str) -> str:
    if "@" in name:
        return "name!W!H@pack.ext"
    if "!" in name:
        return "name!W!H.ext"
    if name.lower().endswith(".mrp"):
        return "pack_mrp"
    return "other"


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def invent_pack_row(pack_path: Path, source: str) -> dict:
    data = pack_path.read_bytes()
    return {
        "pack_path": str(pack_path.relative_to(ROOT)).replace("\\", "/"),
        "member_name": pack_path.name,
        "width": "",
        "height": "",
        "decoded_size": len(data),
        "sha256": sha256_bytes(data),
        "name_pattern": "pack_mrp",
        "referenced_by_pc_if_known": "",
        "source": source,
        "note": "pack_file",
    }


def invent_members(pack_path: Path, source: str, force_all: bool = False) -> list[dict]:
    rows: list[dict] = []
    try:
        rep = parse(pack_path)
    except Exception as e:
        rows.append(
            {
                "pack_path": str(pack_path.relative_to(ROOT)).replace("\\", "/"),
                "member_name": f"<parse_error:{e}>",
                "width": "",
                "height": "",
                "decoded_size": "",
                "sha256": "",
                "name_pattern": "error",
                "referenced_by_pc_if_known": "",
                "source": source,
                "note": "parse_fail",
            }
        )
        return rows
    data = pack_path.read_bytes()
    for mem in rep["members"]:
        name = mem["name"]
        keep = force_all or FOCUS_PAT.search(name) or name in REQUIRED
        if not keep:
            continue
        raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
        decoded, _enc = try_decode(raw)
        w, h, _pack, _ext = parse_dims(name)
        rows.append(
            {
                "pack_path": str(pack_path.relative_to(ROOT)).replace("\\", "/"),
                "member_name": name,
                "width": w if w is not None else "",
                "height": h if h is not None else "",
                "decoded_size": len(decoded),
                "sha256": sha256_bytes(decoded),
                "name_pattern": name_pattern(name),
                "referenced_by_pc_if_known": "",
                "source": source,
                "note": "member",
            }
        )
    return rows


def find_ext_paths() -> list[Path]:
    cands = [
        ROOT / "out" / "JJFB_E8A_delivery" / "02_mrp_extracted" / "jjfb" / "robotol.ext",
        ROOT / "out" / "JJFB_E8A_delivery" / "02_mrp_extracted" / "jjfb" / "mrc_loader.ext",
        ROOT / "game_files" / "mythroad" / "320x480" / "gwy" / "robotol.ext",
        ROOT / "game_files" / "mythroad" / "320x480" / "gwy" / "mrc_loader.ext",
    ]
    # Also search inside jjfb.mrp members if already extracted elsewhere
    out = []
    for p in cands:
        if p.is_file():
            out.append(p)
    # Extract from main MRP if missing
    if not any(p.name == "robotol.ext" for p in out) and MAIN_MRP.is_file():
        try:
            rep = parse(MAIN_MRP)
            data = MAIN_MRP.read_bytes()
            for mem in rep["members"]:
                if mem["name"] in ("robotol.ext", "mrc_loader.ext"):
                    dest = ROOT / "out" / "e9y_fix" / "extracted" / mem["name"]
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
                    decoded, _ = try_decode(raw)
                    dest.write_bytes(decoded)
                    out.append(dest)
        except Exception:
            pass
    return out


def scan_strings(blob: bytes, base_va: int = 0x2D8DF4) -> list[str]:
    """Find printable C strings mentioning downimage/show/loading patterns."""
    lines: list[str] = []
    i = 0
    n = len(blob)
    pats = (
        b"downimage",
        b"show",
        b"loadingbar",
        b"textbar",
        b"@down",
        b"slogo",
    )
    while i < n:
        if blob[i] < 0x20 or blob[i] >= 0x7F:
            i += 1
            continue
        j = i
        while j < n and 0x20 <= blob[j] < 0x7F:
            j += 1
        if j - i >= 4:
            s = blob[i:j]
            low = s.lower()
            if any(p in low for p in pats):
                # try find nearby code xref via literal address (best-effort)
                va = base_va + i
                lines.append(f"off=0x{i:X} va≈0x{va:X} str={s.decode('latin1', 'replace')!r}")
        i = j + 1 if j > i else i + 1
    return lines


def ensure_required(rows: list[dict]) -> None:
    have = {(r["member_name"], r["source"]) for r in rows}
    names = {r["member_name"] for r in rows}
    for req in REQUIRED:
        if req.endswith(".mrp") and req not in names:
            # pack row may use full filename
            if not any(r["member_name"] == req or r["pack_path"].endswith(req) for r in rows):
                rows.append(
                    {
                        "pack_path": "",
                        "member_name": req,
                        "width": "",
                        "height": "",
                        "decoded_size": "",
                        "sha256": "",
                        "name_pattern": name_pattern(req),
                        "referenced_by_pc_if_known": "",
                        "source": "missing",
                        "note": "REQUIRED_MISSING",
                    }
                )
        elif req not in names:
            rows.append(
                {
                    "pack_path": "",
                    "member_name": req,
                    "width": "",
                    "height": "",
                    "decoded_size": "",
                    "sha256": "",
                    "name_pattern": name_pattern(req),
                    "referenced_by_pc_if_known": "",
                    "source": "missing",
                    "note": "REQUIRED_MISSING",
                }
            )
    _ = have


def main() -> int:
    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    OUT_XREF.parent.mkdir(parents=True, exist_ok=True)

    rows: list[dict] = []

    if MAIN_MRP.is_file():
        rows.append(invent_pack_row(MAIN_MRP, "main_mrp"))
        # Prefer splash-related members from main; also force-load known loading assets
        rows.extend(invent_members(MAIN_MRP, "main_mrp", force_all=False))
        # Force exact required members from main if present
        try:
            rep = parse(MAIN_MRP)
            data = MAIN_MRP.read_bytes()
            want = set(REQUIRED)
            have_names = {r["member_name"] for r in rows}
            for mem in rep["members"]:
                if mem["name"] in want and mem["name"] not in have_names:
                    raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
                    decoded, _ = try_decode(raw)
                    w, h, _, _ = parse_dims(mem["name"])
                    rows.append(
                        {
                            "pack_path": str(MAIN_MRP.relative_to(ROOT)).replace("\\", "/"),
                            "member_name": mem["name"],
                            "width": w if w is not None else "",
                            "height": h if h is not None else "",
                            "decoded_size": len(decoded),
                            "sha256": sha256_bytes(decoded),
                            "name_pattern": name_pattern(mem["name"]),
                            "referenced_by_pc_if_known": "",
                            "source": "main_mrp",
                            "note": "required_member",
                        }
                    )
        except Exception:
            pass

    if JJFBOL.is_dir():
        for p in sorted(JJFBOL.glob("downimage*.mrp")):
            rows.append(invent_pack_row(p, "downimage_pack"))
            rows.extend(invent_members(p, "downimage_pack", force_all=True))

    # EXT string scan
    xref_lines = [
        "# E9Y-Fix downimage string xrefs (readonly)",
        "# Patterns: downimage / show / loadingbar / textbar / @down / slogo",
        "",
    ]
    for ext in find_ext_paths():
        blob = ext.read_bytes()
        xref_lines.append(f"## file={ext.relative_to(ROOT).as_posix()} size={len(blob)}")
        hits = scan_strings(blob)
        if not hits:
            xref_lines.append("(no matching printable strings)")
        else:
            xref_lines.extend(hits[:500])
        xref_lines.append("")
        # Also list EXT as source row for key strings found
        for h in hits:
            if "downimage" in h.lower() or "show" in h.lower():
                m = re.search(r"str='([^']+)'", h) or re.search(r'str="([^"]+)"', h)
                if m:
                    s = m.group(1)
                    if FOCUS_PAT.search(s) or "@" in s:
                        rows.append(
                            {
                                "pack_path": str(ext.relative_to(ROOT)).replace("\\", "/"),
                                "member_name": s,
                                "width": "",
                                "height": "",
                                "decoded_size": len(s),
                                "sha256": sha256_bytes(s.encode("latin1", "replace")),
                                "name_pattern": "ext_string",
                                "referenced_by_pc_if_known": "",
                                "source": "ext",
                                "note": "string_literal",
                            }
                        )

    ensure_required(rows)

    # Dedup
    seen = set()
    uniq = []
    for r in rows:
        key = (r["pack_path"], r["member_name"], r["source"], r.get("note", ""))
        if key in seen:
            continue
        seen.add(key)
        uniq.append(r)

    fields = [
        "pack_path",
        "member_name",
        "width",
        "height",
        "decoded_size",
        "sha256",
        "name_pattern",
        "referenced_by_pc_if_known",
        "source",
        "note",
    ]
    with OUT_CSV.open("w", newline="", encoding="utf-8-sig") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(uniq)

    OUT_XREF.write_text("\n".join(xref_lines) + "\n", encoding="utf-8")

    missing = [r for r in uniq if r.get("note") == "REQUIRED_MISSING"]
    present_req = []
    names = {r["member_name"] for r in uniq if r.get("note") != "REQUIRED_MISSING"}
    for req in REQUIRED:
        if req in names or any(r["pack_path"].endswith(req) for r in uniq):
            present_req.append(req)

    print(f"DOWNIMAGE_CONTRACT_PARSED rows={len(uniq)} required_hit={len(present_req)}/{len(REQUIRED)}")
    print(f"wrote {OUT_CSV}")
    print(f"wrote {OUT_XREF}")
    if missing:
        print(f"WARNING required still missing: {[m['member_name'] for m in missing]}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
