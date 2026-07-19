#!/usr/bin/env python3
"""Shared helpers for Phase 6J static publication audits."""
from __future__ import annotations

import gzip
import hashlib
import struct
import zlib
from pathlib import Path
from typing import Iterator

FIELD_IMMS = {0x0, 0x4, 0x8, 0xC, 0x10}


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


def iter_mrp_members(path: Path) -> Iterator[tuple[str, bytes, bytes]]:
    data = path.read_bytes()
    if len(data) < 240 or data[:4] != b"MRPG":
        return
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
        stored = data[off : off + slen]
        yield name, try_decode(stored), stored


def member_blob(path: Path, want: str) -> bytes | None:
    for name, blob, _ in iter_mrp_members(path):
        if want in name or name.endswith(want) or name == want:
            return blob
    return None


def sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def scan_thumb_str_imm(blob: bytes, imms: set[int] | None = None) -> list[dict]:
    """Find Thumb STR Rt,[Rn,#imm] for selected word offsets."""
    want = imms if imms is not None else FIELD_IMMS
    hits: list[dict] = []
    for off in range(0, len(blob) - 1, 2):
        h = struct.unpack_from("<H", blob, off)[0]
        if (h & 0xF800) != 0x6000:
            continue
        rt = h & 7
        rn = (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        if imm not in want:
            continue
        hits.append(
            {
                "file_offset": off,
                "insn": f"0x{h:04X}",
                "disasm": f"STR r{rt},[r{rn},#0x{imm:X}]",
                "rt": rt,
                "rn": rn,
                "imm": imm,
            }
        )
    return hits


def scan_thumb_ldr_imm(blob: bytes, imms: set[int]) -> list[dict]:
    hits: list[dict] = []
    for off in range(0, len(blob) - 1, 2):
        h = struct.unpack_from("<H", blob, off)[0]
        if (h & 0xF800) != 0x6800:
            continue
        rt = h & 7
        rn = (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        if imm not in imms:
            continue
        hits.append(
            {
                "file_offset": off,
                "insn": f"0x{h:04X}",
                "disasm": f"LDR r{rt},[r{rn},#0x{imm:X}]",
                "rt": rt,
                "rn": rn,
                "imm": imm,
            }
        )
    return hits


def cluster_str_sites(hits: list[dict], window: int = 0x40) -> list[dict]:
    """Group STR sites that write multiple P-like fields within a window."""
    by_off = sorted(hits, key=lambda h: h["file_offset"])
    clusters: list[dict] = []
    used = set()
    for i, h in enumerate(by_off):
        if i in used:
            continue
        group = [h]
        used.add(i)
        for j in range(i + 1, len(by_off)):
            if by_off[j]["file_offset"] - h["file_offset"] > window:
                break
            group.append(by_off[j])
            used.add(j)
        imms = sorted({g["imm"] for g in group})
        clusters.append(
            {
                "start": group[0]["file_offset"],
                "end": group[-1]["file_offset"],
                "imms": imms,
                "has_0c": 0xC in imms,
                "has_0_4_8": all(x in imms for x in (0, 4, 8)),
                "sites": group,
            }
        )
    return clusters


def mrpgcmap_header(blob: bytes) -> dict:
    """Decode MRPGCMAP 8-byte prefix; entry candidate = image+8 (DOCUMENTED)."""
    out = {
        "magic": None,
        "prefix_hex": None,
        "code_size": len(blob),
        "entry_offset_candidate": 8 if len(blob) >= 12 else None,
        "evidence": "DOCUMENTED image+8 / 8-byte MRPGCMAP prefix",
    }
    if len(blob) >= 8 and blob[:8] == b"MRPGCMAP":
        out["magic"] = "MRPGCMAP"
        out["prefix_hex"] = blob[:8].hex()
    elif len(blob) >= 8:
        out["magic"] = blob[:8].decode("latin1", "replace")
        out["prefix_hex"] = blob[:8].hex()
    return out


def load_ext_targets(gwy: Path) -> list[tuple[str, bytes]]:
    """Collect primary EXT blobs for shell + jjfb + wxjwq."""
    targets: list[tuple[str, bytes]] = []
    shell = [
        ("gbrwcore.mrp", "gbrwcore.ext"),
        ("gamelist.mrp", "gamelist.ext"),
        ("gbrwshell.mrp", "gbrwshell.ext"),
    ]
    for pkg, mem in shell:
        p = gwy / pkg
        if not p.is_file():
            continue
        blob = member_blob(p, mem)
        if blob:
            targets.append((f"{pkg}:{mem}", blob))
        reg = member_blob(p, "reg.ext")
        if reg:
            targets.append((f"{pkg}:reg.ext", reg))

    for game in ("jjfb.mrp", "wxjwq.mrp"):
        p = gwy / game
        if not p.is_file():
            continue
        for mem in ("robotol.ext", "cfunction.ext", "mrc_loader.ext", "start.mr"):
            blob = member_blob(p, mem)
            if blob:
                targets.append((f"{game}:{mem}", blob))
    return targets
