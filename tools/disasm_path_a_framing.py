#!/usr/bin/env python3
"""Disassemble robotol.ext Thumb around Path-A framing 0x2E4E80..0x2E4F10."""
from __future__ import annotations

import struct
import subprocess
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MRP = ROOT / "game_files/mythroad/320x480/gwy/jjfb.mrp"
OUT = ROOT / "out/product_event/path_a_framing_2e4eae.txt"


def extract_robotol() -> bytes:
    # Prefer gwy_launcher decode via temporary member extract if available.
    data = MRP.read_bytes()
    # Simple MRP member walk: after header, entries of name\0 + offsets (repo format varies).
    # Use existing unit-tested approach via subprocess inspect + synthesize is heavy;
    # instead locate gzip-compressed member by scanning for 'robotol.ext' string near index.
    name = b"robotol.ext\0"
    pos = data.find(name)
    if pos < 0:
        raise SystemExit("robotol.ext name not found in mrp")
    # Many GWY MRPs: after name, u32 offset, u32 size (LE) — try nearby
    for off in range(pos + len(name), min(pos + 64, len(data) - 8)):
        o = struct.unpack_from("<I", data, off)[0]
        s = struct.unpack_from("<I", data, off + 4)[0]
        if 0x100 < o < len(data) and 0x1000 < s < 0x80000 and o + s <= len(data):
            blob = data[o : o + s]
            # try raw or zlib
            if blob[:2] == b"\x1f\x8b" or blob[:2] == b"\x78\x9c" or blob[:2] == b"\x78\xda":
                try:
                    return zlib.decompress(blob)
                except Exception:
                    pass
            try:
                return zlib.decompress(blob)
            except Exception:
                pass
            if len(blob) > 100000:
                return blob
    # Fallback: run gwy_launcher and ask for decoded member via tempfile
    import tempfile
    import os

    tmp = Path(tempfile.gettempdir()) / "robotol_extract.bin"
    # Use inspect-member-view --to robotol.ext may synthesize; decode via archive API in a tiny C runner is hard.
    # Try overlays from previous runs.
    for p in (ROOT / "out").rglob("robotol.ext"):
        b = p.read_bytes()
        if len(b) > 100000:
            return b
    raise SystemExit(f"could not extract robotol near name@{pos}")


def disasm_thumb(code: bytes, base: int) -> list[str]:
    lines = []
    # Prefer capstone if present
    try:
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

        md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
        md.detail = False
        for i in md.disasm(code, base):
            lines.append(f"0x{i.address:08X}: {i.mnemonic}\t{i.op_str}")
        return lines
    except Exception as e:
        lines.append(f"# capstone unavailable: {e}")
    # Fallback: raw halfwords
    i = 0
    while i + 2 <= len(code):
        h = struct.unpack_from("<H", code, i)[0]
        addr = base + i
        lines.append(f"0x{addr:08X}: .hword 0x{h:04X}")
        i += 2
    return lines


def main() -> None:
    # robotol mapped at CODE base typically 0x2C0000? In this project guest PCs are absolute like 0x2E4EAE.
    # Extract full robotol and use file offset = pc - load_base.
    # From prior maps, robotol.ext is loaded such that PC 0x2E4EAE is inside it.
    # Try load bases commonly used.
    blob = None
    for p in [
        ROOT / "out/vmrp_run/overlay/mythroad/gwy/robotol.ext",
        ROOT / "out/vmrp_run/mythroad/gwy/robotol.ext",
    ]:
        if p.exists() and p.stat().st_size > 100000:
            blob = p.read_bytes()
            break
    if blob is None:
        # decode via launcher_core isn't trivial; use zlib member via mrp_archive through a small ctypes? 
        # Use gwy_launcher inspect-member-view writing tmp — check if it supports --out
        r = subprocess.run(
            [
                str(ROOT / "build-i686/gwy_launcher.exe"),
                "inspect-member-view",
                "--mrp",
                str(MRP),
                "--member",
                "robotol.ext",
                "--to",
                "robotol.ext",
            ],
            capture_output=True,
            text=True,
            cwd=str(ROOT),
        )
        print(r.stdout[-2000:])
        print(r.stderr[-1000:])
        # Find any dumped view in cwd
        for p in ROOT.rglob("*robotol*"):
            if p.is_file() and p.stat().st_size > 100000:
                blob = p.read_bytes()
                print("using", p)
                break
    if blob is None:
        # Last resort: decompress member using offsets from inspect-mrp
        r = subprocess.run(
            [str(ROOT / "build-i686/gwy_launcher.exe"), "inspect-mrp", str(MRP)],
            capture_output=True,
            text=True,
            cwd=str(ROOT),
        )
        off = size = None
        for line in r.stdout.splitlines():
            if "robotol.ext" in line and "stored_offset=" in line:
                # [n] name=robotol.ext stored_offset=X stored_size=Y
                parts = dict(
                    kv.split("=", 1)
                    for kv in line.replace("[", " ").replace("]", " ").split()
                    if "=" in kv
                )
                off = int(parts.get("stored_offset", "0"))
                size = int(parts.get("stored_size", "0"))
                unpacked = int(parts.get("unpacked_size", "0") or "0")
                print("member", parts)
                raw = MRP.read_bytes()[off : off + size]
                try:
                    blob = zlib.decompress(raw)
                except Exception:
                    # try gzip
                    import gzip
                    from io import BytesIO

                    try:
                        blob = gzip.GzipFile(fileobj=BytesIO(raw)).read()
                    except Exception as e:
                        print("decompress fail", e)
                        blob = raw
                break
    if blob is None:
        raise SystemExit("no robotol blob")

    # Find load base: search for unique insn pattern near known absolute PCs.
    # Known: at 0x2E4EEE is BL 0x312A60. Thumb BL encoding depends on offset.
    # Simpler: assume robotol base = 0x2C0000 or compute from first_data.
    # From prior key maps robotol codes live at 0x2Exxxx — try base candidates.
    target_pc = 0x2E4E80
    end_pc = 0x2E4F20
    base_guess = None
    for b in (0x2C0000, 0x280000, 0x2A0000, 0x2E0000, 0):
        off = target_pc - b
        if 0 <= off < len(blob) - 0x100:
            # check BL at 0x2E4EEE-b looks like bl
            off2 = 0x2E4EEE - b
            h0, h1 = struct.unpack_from("<HH", blob, off2)
            # Thumb BL: high F000..F7FF / F800..FFFF
            if (h0 & 0xF800) == 0xF000 and (h1 & 0xF800) in (0xF800, 0xE800, 0xF000):
                base_guess = b
                print(f"base_guess=0x{b:X} bl_halfs=0x{h0:04X} 0x{h1:04X}")
                break
    if base_guess is None:
        # scan for BL to 0x312A60 relative from each possible pc
        for b in range(0, 0x400000, 0x1000):
            off2 = 0x2E4EEE - b
            if off2 < 0 or off2 + 4 > len(blob):
                continue
            h0, h1 = struct.unpack_from("<HH", blob, off2)
            if (h0 & 0xF800) != 0xF000:
                continue
            # decode bl target
            s = (h0 >> 10) & 1
            imm10 = h0 & 0x3FF
            j1 = (h1 >> 13) & 1
            j2 = (h1 >> 11) & 1
            imm11 = h1 & 0x7FF
            i1 = ~(j1 ^ s) & 1
            i2 = ~(j2 ^ s) & 1
            imm = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm |= ~((1 << 25) - 1)
            dest = (0x2E4EEE + 4 + imm) & 0xFFFFFFFF
            if dest == 0x312A60:
                base_guess = b
                print(f"matched BL target base=0x{b:X}")
                break
    if base_guess is None:
        raise SystemExit("could not locate framing in blob")

    off = target_pc - base_guess
    code = blob[off : off + (end_pc - target_pc)]
    lines = disasm_thumb(code, target_pc)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("wrote", OUT, "lines", len(lines))
    for l in lines:
        if any(x in l for x in ("str", "ldr", "bl", "mov", "add", "sub", "pc", "sp", "r5")):
            print(l)


if __name__ == "__main__":
    main()
