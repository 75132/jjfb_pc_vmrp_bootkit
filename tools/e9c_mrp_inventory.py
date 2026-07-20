#!/usr/bin/env python3
"""E9C: full original jjfb.mrp member inventory + image candidate ranking (read-only)."""
from __future__ import annotations

import csv
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from mrp_inspect import parse, try_decode  # noqa: E402

MRP = ROOT / "game_files" / "mythroad" / "320x480" / "gwy" / "jjfb.mrp"
OUT_PREV = ROOT / "out" / "e9c_resource_previews"
OUT_MEMBER_CSV = ROOT / "reports" / "e9c_mrp_member_inventory.csv"
OUT_IMAGE_CSV = ROOT / "reports" / "e9c_mrp_image_inventory.csv"
OUT_JSON = ROOT / "reports" / "e9c_mrp_inventory_summary.json"
OUT_TOP = ROOT / "reports" / "e9c_top_candidates.json"

UI_HINTS = re.compile(
    r"(bg|back|logo|title|menu|main|loading|login|ui|button|jiao|map|role|face|font|"
    r"start|splash|icon|bar|dlg|dialog|btn|skin|wy_)",
    re.I,
)
IMG_EXT = re.compile(r"\.(bmp|gif|png|jpg|jpeg|mrp)$", re.I)
NAME_WH = re.compile(r"^(.+)!(\d+)!(\d+)\.(\w+)$")


def parse_name_wh(name: str):
    m = NAME_WH.match(name)
    if not m:
        return None, None, None
    return int(m.group(2)), int(m.group(3)), m.group(4)


def rgb565_to_png(raw: bytes, w: int, h: int, path: Path) -> dict:
    """Write PNG via Pillow if available; else raw BMP-like preview stub stats only."""
    expect = w * h * 2
    if len(raw) < expect:
        return {"preview": None, "error": f"short bytes={len(raw)} expect={expect}"}
    pixels = raw[:expect]
    nonblank = 0
    black = white = other = key = 0
    for i in range(0, expect, 2):
        c = pixels[i] | (pixels[i + 1] << 8)
        if c == 0:
            black += 1
        elif c == 0xFFFF:
            white += 1
        elif c == 0xF81F:
            key += 1
        else:
            other += 1
            nonblank += 1
    preview = None
    try:
        from PIL import Image  # type: ignore

        img = Image.new("RGB", (w, h))
        pix = img.load()
        for y in range(h):
            for x in range(w):
                off = (y * w + x) * 2
                c = pixels[off] | (pixels[off + 1] << 8)
                r = ((c >> 11) & 0x1F) * 255 // 31
                g = ((c >> 5) & 0x3F) * 255 // 63
                b = (c & 0x1F) * 255 // 31
                pix[x, y] = (r, g, b)
        path.parent.mkdir(parents=True, exist_ok=True)
        img.save(path)
        preview = str(path).replace("\\", "/")
    except Exception as e:
        # Fallback: write raw RGB565 file for later
        path = path.with_suffix(".rgb565")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(pixels)
        preview = str(path).replace("\\", "/")
        return {
            "preview": preview,
            "nonblank": nonblank,
            "black": black,
            "white": white,
            "other": other,
            "key": key,
            "pillow": False,
            "note": str(e),
        }
    return {
        "preview": preview,
        "nonblank": nonblank,
        "black": black,
        "white": white,
        "other": other,
        "key": key,
        "pillow": True,
    }


def classify(name: str, decoded: bytes, w, h) -> str:
    nl = name.lower()
    if "font" in nl:
        return "font"
    if any(k in nl for k in ("btn", "button", "jiao")):
        return "sprite_button"
    if any(k in nl for k in ("bg", "back", "map", "title", "logo", "loading", "main", "menu")):
        return "background_or_title"
    if w and h and w * h * 2 == len(decoded):
        if w >= 120 and h >= 80:
            return "large_rgb565"
        if w <= 32 and h <= 32:
            return "small_sprite"
        return "mid_sprite"
    if decoded[:2] == b"BM":
        return "windows_bmp"
    return "blob"


def score_row(row: dict) -> int:
    s = 0
    name = row["name"]
    if UI_HINTS.search(name):
        s += 40
    if IMG_EXT.search(name) or "!" in name:
        s += 20
    w, h = row.get("width") or 0, row.get("height") or 0
    if w and h:
        area = w * h
        s += min(80, area // 50)
        if w <= 240 and h <= 320:
            s += 15
        if area >= 240 * 80:
            s += 50
        if area >= 240 * 160:
            s += 40
    if row.get("format") == "raw_rgb565":
        s += 25
    if row.get("other", 0) > 64:
        s += 20
    if row.get("kind") in ("background_or_title", "large_rgb565"):
        s += 60
    # Prefer larger over tiny jiao sprites
    if "jiao" in name.lower() and (w or 0) <= 16:
        s -= 30
    return s


def main() -> int:
    if not MRP.is_file():
        raise SystemExit(f"missing {MRP}")
    rep = parse(MRP)
    data = MRP.read_bytes()
    OUT_PREV.mkdir(parents=True, exist_ok=True)
    members_out = []
    images_out = []

    for mem in rep["members"]:
        raw = data[mem["offset"] : mem["offset"] + mem["stored_length"]]
        decoded, enc = try_decode(raw)
        w, h, ext = parse_name_wh(mem["name"])
        expect = (w * h * 2) if w and h else None
        if expect and len(decoded) == expect:
            fmt = "raw_rgb565"
        elif decoded[:2] == b"BM":
            fmt = "windows_bmp"
        elif mem["name"].lower().endswith((".gif", ".png", ".jpg", ".jpeg")):
            fmt = "named_image"
        else:
            fmt = "unknown"
        sha = hashlib.sha256(decoded).hexdigest()
        kind = classify(mem["name"], decoded, w, h)
        row = {
            "name": mem["name"],
            "offset": mem["offset"],
            "stored_size": mem["stored_length"],
            "decoded_size": len(decoded),
            "encoding": enc,
            "reserved": mem.get("reserved", 0),
            "extension": (ext or Path(mem["name"]).suffix.lstrip(".") or ""),
            "width": w or "",
            "height": h or "",
            "format": fmt,
            "bpp": 16 if fmt == "raw_rgb565" else "",
            "sha256": sha,
            "first_32_hex": decoded[:32].hex(),
            "decoder_status": "ok",
            "kind": kind,
            "ui_hint": bool(UI_HINTS.search(mem["name"])),
            "fits_240x320": bool(w and h and w <= 240 and h <= 320),
        }
        members_out.append(row)

        is_imageish = (
            fmt in ("raw_rgb565", "windows_bmp", "named_image")
            or bool(w and h)
            or IMG_EXT.search(mem["name"])
            or "!" in mem["name"]
        )
        if is_imageish:
            img = dict(row)
            img["other"] = 0
            img["nonblank"] = 0
            img["preview"] = ""
            img["score"] = 0
            safe = re.sub(r"[^\w.\-]+", "_", mem["name"])[:80]
            if fmt == "raw_rgb565" and w and h:
                stats = rgb565_to_png(decoded, w, h, OUT_PREV / f"{safe}.png")
                img.update(
                    {
                        "preview": stats.get("preview") or "",
                        "other": stats.get("other", 0),
                        "nonblank": stats.get("nonblank", 0),
                        "black": stats.get("black", 0),
                        "white": stats.get("white", 0),
                        "key": stats.get("key", 0),
                    }
                )
            elif fmt == "windows_bmp":
                p = OUT_PREV / f"{safe}.bmp"
                p.write_bytes(decoded)
                img["preview"] = str(p).replace("\\", "/")
                img["other"] = max(0, len(decoded) - 54)
            else:
                p = OUT_PREV / f"{safe}.bin"
                p.write_bytes(decoded[: min(len(decoded), 256 * 1024)])
                img["preview"] = str(p).replace("\\", "/")
            img["score"] = score_row(img)
            images_out.append(img)

    images_out.sort(key=lambda r: (-r["score"], -(r.get("width") or 0) * (r.get("height") or 0)))

    OUT_MEMBER_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUT_MEMBER_CSV.open("w", newline="", encoding="utf-8-sig") as f:
        fields = list(members_out[0].keys()) if members_out else ["name"]
        wri = csv.DictWriter(f, fieldnames=fields)
        wri.writeheader()
        wri.writerows(members_out)

    with OUT_IMAGE_CSV.open("w", newline="", encoding="utf-8-sig") as f:
        fields = list(images_out[0].keys()) if images_out else ["name"]
        wri = csv.DictWriter(f, fieldnames=fields)
        wri.writeheader()
        wri.writerows(images_out)

    top20 = images_out[:20]
    summary = {
        "mrp": str(MRP).replace("\\", "/"),
        "mrp_sha256": rep["sha256"],
        "member_count": len(members_out),
        "imageish_count": len(images_out),
        "largest_rgb565": None,
        "best_ui_candidate": None,
        "top20": [
            {
                "name": t["name"],
                "w": t.get("width"),
                "h": t.get("height"),
                "decoded_size": t["decoded_size"],
                "kind": t["kind"],
                "score": t["score"],
                "other": t.get("other", 0),
                "preview": t.get("preview"),
            }
            for t in top20
        ],
        "note": "NOT_PRODUCT_SUCCESS; original jjfb.mrp only",
    }
    rgb = [i for i in images_out if i["format"] == "raw_rgb565" and i.get("width") and i.get("height")]
    if rgb:
        best = max(rgb, key=lambda r: (r["width"] or 0) * (r["height"] or 0))
        summary["largest_rgb565"] = {
            "name": best["name"],
            "w": best["width"],
            "h": best["height"],
            "bytes": best["decoded_size"],
            "score": best["score"],
            "preview": best.get("preview"),
        }
    if top20:
        summary["best_ui_candidate"] = summary["top20"][0]

    OUT_JSON.write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")
    OUT_TOP.write_text(json.dumps(top20, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(summary, indent=2, ensure_ascii=False))
    print(f"\nmembers={len(members_out)} imageish={len(images_out)}")
    print(f"wrote {OUT_MEMBER_CSV}")
    print(f"wrote {OUT_IMAGE_CSV}")
    if summary.get("best_ui_candidate"):
        c = summary["best_ui_candidate"]
        print(f"best candidate: {c['name']} {c.get('w')}x{c.get('h')} score={c['score']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
