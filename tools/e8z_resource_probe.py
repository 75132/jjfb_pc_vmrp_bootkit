#!/usr/bin/env python3
"""E8Z resource probe: extract wy_jiao1!11!11.bmp from original jjfb.mrp (read-only)."""
from __future__ import annotations
import hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mrp_inspect import parse, try_decode  # noqa: E402

MEMBER = "wy_jiao1!11!11.bmp"
MRP = ROOT / "game_files" / "mythroad" / "320x480" / "gwy" / "jjfb.mrp"
OUT_DIR = ROOT / "out" / "e8z_resources"
OUT_RAW = OUT_DIR / "wy_jiao1_11_11.bmp"
OUT_JSON = ROOT / "reports" / "e8z_resource_probe.json"


def parse_name_wh(name: str) -> tuple[int, int] | tuple[None, None]:
    if name.count("!") < 2:
        return None, None
    base, w, rest = name.split("!", 2)
    h = rest.split(".", 1)[0]
    try:
        return int(w), int(h)
    except ValueError:
        return None, None


def main() -> int:
    if not MRP.is_file():
        raise SystemExit(f"missing {MRP}")
    rep = parse(MRP)
    mem = next((m for m in rep["members"] if m["name"] == MEMBER), None)
    if not mem:
        raise SystemExit(f"member not found: {MEMBER}")
    data = MRP.read_bytes()
    raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
    decoded, enc = try_decode(raw)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OUT_RAW.write_bytes(decoded)
    w, h = parse_name_wh(MEMBER)
    expect = (w * h * 2) if w and h else None
    fmt = "raw_rgb565" if expect and len(decoded) == expect else (
        "windows_bmp" if decoded[:2] == b"BM" else "unknown"
    )
    first16 = list(struct.unpack_from(f"<{min(16, len(decoded)//2)}H", decoded)) if len(decoded) >= 2 else []
    # nontrivial pixel check
    other = sum(1 for c in first16 if c not in (0, 0xFFFF))
    probe = {
        "member_name": MEMBER,
        "mrp_path": str(MRP).replace("\\", "/"),
        "mrp_sha256": rep["sha256"],
        "stored_offset": mem["offset"],
        "stored_length": mem["stored_length"],
        "encoding": enc,
        "decoded_length": len(decoded),
        "sha256": hashlib.sha256(decoded).hexdigest(),
        "out_path": str(OUT_RAW).replace("\\", "/"),
        "name_w": w,
        "name_h": h,
        "expected_rgb565_bytes": expect,
        "format": fmt,
        "bpp_hypothesis": 16 if fmt == "raw_rgb565" else None,
        "first_32_bytes_hex": decoded[:32].hex(),
        "first_16_u16_le": [f"0x{c:04X}" for c in first16],
        "nontrivial_in_first16": other,
        "legacy_decoder_accepts": bool(expect and len(decoded) == expect),
        "note": "pixels from original jjfb.mrp only; not invented",
    }
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(probe, indent=2), encoding="utf-8")
    print(json.dumps(probe, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
