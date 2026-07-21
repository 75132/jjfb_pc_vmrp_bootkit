#!/usr/bin/env python3
"""E9Z Task 1: generic GWY side-pack registry CSV (no show1/downimage1 hardcode).

Outputs:
  reports/e9z_gwy_pack_registry.csv
"""
from __future__ import annotations

import csv
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mrp_inspect import parse  # noqa: E402

OUT_CSV = ROOT / "reports" / "e9z_gwy_pack_registry.csv"


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def target_from_env_or_default() -> Path:
    import os

    env = os.environ.get("JJFB_REAL_MRP_PATH") or os.environ.get("E9Z_TARGET_MRP")
    if env:
        p = Path(env)
        if not p.is_absolute():
            p = ROOT / p
        return p
    return ROOT / "game_files" / "mythroad" / "320x480" / "gwy" / "jjfb.mrp"


def side_pack_dirs(gwy_dir: Path, stem: str) -> list[Path]:
    dirs: list[Path] = []
    # Convention: foo.mrp → fool/ (stem + "ol")
    primary = gwy_dir / f"{stem}ol"
    if primary.is_dir():
        dirs.append(primary)
    for d in sorted(gwy_dir.glob("*ol")):
        if d.is_dir() and d not in dirs:
            dirs.append(d)
    return dirs


def _rel(p: Path) -> str:
    try:
        return str(p.resolve().relative_to(ROOT.resolve())).replace("\\", "/")
    except Exception:
        return str(p).replace("\\", "/")


def invent_rows(target: Path) -> list[dict]:
    rows: list[dict] = []
    gwy_dir = target.parent
    stem = target.stem
    # Prefer target-local side-pack dir only: <stem>ol (e.g. jjfb → jjfbol).
    # Do NOT index peer game MRPs under gwy/*.mrp.
    side_dirs = side_pack_dirs(gwy_dir, stem)
    # Keep only dirs that match stem+"ol" first; if missing, keep other *ol under same gwy.
    primary = gwy_dir / f"{stem}ol"
    if primary.is_dir():
        side_dirs = [primary]
    packs: list[Path] = []
    for sd in side_dirs:
        packs.extend(sorted(sd.glob("*.mrp")))
    # Dedup
    seen = set()
    uniq: list[Path] = []
    for p in packs:
        key = str(p.resolve())
        if key in seen:
            continue
        seen.add(key)
        uniq.append(p)

    side_s = ";".join(_rel(d) for d in side_dirs)
    tgt_rel = _rel(target)

    for pack in uniq:
        try:
            rep = parse(pack)
            members = len(rep.get("members") or [])
            appid = rep.get("appid", 0) or 0
            appver = rep.get("appver", 0) or 0
        except Exception:
            members = 0
            appid = 0
            appver = 0
        name = pack.stem
        rel = _rel(pack)
        print(
            f"[GWY_PACK_REGISTRY] target={tgt_rel} pack={name} members={members} "
            f"sha256={sha256_file(pack)[:16]}..."
        )
        rows.append(
            {
                "target_mrp": tgt_rel,
                "side_pack_dir": side_s,
                "pack_name": name,
                "pack_path": rel,
                "member_count": members,
                "appid": f"0x{appid:X}" if isinstance(appid, int) else appid,
                "appver": f"0x{appver:X}" if isinstance(appver, int) else appver,
                "sha256": sha256_file(pack),
                "is_downimage": 1 if "downimage" in name.lower() else 0,
                "ready": 1 if uniq else 0,
            }
        )
        # Per-member inventory rows (member_name column for resolve map)
        try:
            rep = parse(pack)
            data = pack.read_bytes()
            for mem in rep.get("members") or []:
                mname = mem["name"]
                raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
                rows.append(
                    {
                        "target_mrp": tgt_rel,
                        "side_pack_dir": side_s,
                        "pack_name": name,
                        "pack_path": rel,
                        "member_count": members,
                        "appid": "",
                        "appver": "",
                        "sha256": hashlib.sha256(raw).hexdigest(),
                        "is_downimage": 1 if "downimage" in name.lower() else 0,
                        "ready": 1,
                        "member_name": mname,
                        "stored_size": mem["stored_length"],
                    }
                )
        except Exception:
            pass
    return rows


def main() -> int:
    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    target = target_from_env_or_default()
    if not target.is_file():
        print(f"ERROR: target MRP missing: {target}", file=sys.stderr)
        return 1
    rows = invent_rows(target)
    fields = [
        "target_mrp",
        "side_pack_dir",
        "pack_name",
        "pack_path",
        "member_name",
        "member_count",
        "stored_size",
        "appid",
        "appver",
        "sha256",
        "is_downimage",
        "ready",
    ]
    with OUT_CSV.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow(r)
    print(f"Wrote {OUT_CSV} rows={len(rows)} packs_ready={1 if rows else 0}")
    print("GWY_PACK_REGISTRY_BUILT")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
