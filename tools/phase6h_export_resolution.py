#!/usr/bin/env python3
"""Phase 6H: resolve gbrwcore lib.startGame/lib.runapp beyond string offsets."""
from __future__ import annotations

import argparse
import struct
import zlib
import gzip
from pathlib import Path

EXPORTS = [
    b"lib.startGame",
    b"lib.runapp",
    b"lib.isFileOnServerNewer",
    b"lib.checkmrpver",
    b"lib.download",
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
    if len(data) < 240 or data[:4] != b"MRPG":
        return None
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


def find_ldr_xrefs(blob: bytes, str_off: int) -> list[dict]:
    """ARM LDR Rd,[PC,#imm] that land on/near the string."""
    hits = []
    for pc in range(0, len(blob) - 4, 4):
        w = struct.unpack_from("<I", blob, pc)[0]
        # Encoding: cond | 01 | I=0 | P=1 | U | B=0 | W=0 | L=1 | Rn=PC(15) | Rd | imm12
        if (w & 0x0C70F000) != 0x0410F000:
            continue
        if ((w >> 16) & 0xF) != 15:
            continue
        imm = w & 0xFFF
        u = (w >> 23) & 1
        tgt = (pc + 8 + imm) if u else (pc + 8 - imm)
        if abs(tgt - str_off) <= 3:
            hits.append(
                {
                    "xref_pc": f"0x{pc:X}",
                    "load_target": f"0x{tgt:X}",
                    "insn": f"0x{w:08X}",
                    "kind": "ldr_pc_rel",
                }
            )
    return hits


def find_ptr_table_near(blob: bytes, str_off: int, window: int = 0x80) -> list[dict]:
    """Words near string that look like code pointers into this image."""
    out = []
    lo = max(0, (str_off - window) & ~3)
    hi = min(len(blob), (str_off + window + 3) & ~3)
    for off in range(lo, hi, 4):
        if off == str_off:
            continue
        v = struct.unpack_from("<I", blob, off)[0]
        if 0x20 <= v < len(blob) and (v & 1) == 0:
            # Prefer values that land on plausible ARM prologues
            if v + 4 <= len(blob):
                insn = struct.unpack_from("<I", blob, v)[0]
                if (insn & 0xFFFF0000) in (0xE92D0000, 0xE52D0000) or (insn & 0xFF000000) == 0xEA000000:
                    out.append(
                        {
                            "table_off": f"0x{off:X}",
                            "candidate_fn": f"0x{v:X}",
                            "prologue": f"0x{insn:08X}",
                        }
                    )
    return out[:12]


def scan_export_name_table(blob: bytes) -> list[dict]:
    """Collect contiguous lib.* C strings (export name table evidence)."""
    rows = []
    i = 0
    while True:
        j = blob.find(b"lib.", i)
        if j < 0:
            break
        end = blob.find(b"\0", j)
        if end < 0 or end - j > 48:
            i = j + 4
            continue
        name = blob[j:end].decode("latin1", "replace")
        if all(32 <= ord(c) < 127 for c in name):
            rows.append({"name": name, "string_offset": f"0x{j:X}"})
        i = end + 1
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gwy_root")
    ap.add_argument("out_md")
    args = ap.parse_args()
    root = Path(args.gwy_root)
    blob = member_blob(root / "gbrwcore.mrp", "gbrwcore.ext")
    lines = [
        "# Phase 6H — gbrwcore export / dispatcher resolution",
        "",
        "Evidence: **TARGET_OBSERVED** static analysis of `gbrwcore.ext`.",
        "String offsets are **not** function entries.",
        "",
    ]
    if blob is None:
        lines.append("ERROR: gbrwcore.ext missing")
        Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
        return 1

    lines.extend(
        [
            f"- member_size: `{len(blob)}`",
            f"- header_magic: `{blob[:8]!r}`",
            "",
            "## Export name table (`lib.*` strings)",
            "",
            "| export name | string offset | xref pc | candidate dispatcher | candidate function pointer | runtime VA | called | args |",
            "|---|---|---|---|---|---|---|---|",
        ]
    )

    name_table = scan_export_name_table(blob)
    focus = {e.decode("latin1") for e in EXPORTS}
    for row in name_table:
        if row["name"] not in focus and not any(k in row["name"] for k in ("runapp", "startGame")):
            continue
        off = int(row["string_offset"], 16)
        xrefs = find_ldr_xrefs(blob, off)
        ptrs = find_ptr_table_near(blob, off)
        xref_s = xrefs[0]["xref_pc"] if xrefs else "none_found"
        disp = "lib_name_table_or_strcmp_dispatcher (HYPOTHESIS)"
        fn = ptrs[0]["candidate_fn"] if ptrs else "unknown"
        lines.append(
            f"| `{row['name']}` | `{row['string_offset']}` | `{xref_s}` | `{disp}` | "
            f"`{fn}` | `load_base+{row['string_offset']}` | live_see_phase6h | pending |"
        )

    lines.extend(
        [
            "",
            "## Notes",
            "",
            "1. `lib.startGame` / `lib.runapp` live in a packed C-string export table "
            "inside `gbrwcore.ext` (CROSS_TARGET pattern across GWY shell packages).",
            "2. No absolute pointer xrefs to the string offsets were found; resolution is "
            "likely via strcmp against the name table + function-pointer side table "
            "(HYPOTHESIS until guest export-call is observed).",
            "3. Phase 6H runtime must log `[JJFB_SHELL_EXPORT]` as `kind=string_va_not_entry` "
            "and only mark `called=yes` when guest args/PC prove a real call.",
            "",
            f"- lib.* string count in image: **{len(name_table)}**",
            "",
        ]
    )
    Path(args.out_md).write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
