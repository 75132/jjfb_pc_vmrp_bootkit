#!/usr/bin/env python3
"""Static recover of robotol 0x305E09 + case-9 + unfinished poll site 0x304589."""
from __future__ import annotations

import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
BLOB_PATH = ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext"
CODE_BASE = 0x2D8DF4
OUT_DIR = ROOT / "out/product_event"
REP = ROOT / "reports"

TARGET = 0x305E08  # even
CASE9 = 0x30E1A0
POLL = 0x304588  # even of 0x304589


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def bl_target(pc: int, h0: int, h1: int) -> Optional[int]:
    if (h0 & 0xF800) != 0xF000 or (h1 & 0xC000) != 0xC000:
        return None
    s = (h0 >> 10) & 1
    imm10 = h0 & 0x3FF
    j1 = (h1 >> 13) & 1
    j2 = (h1 >> 11) & 1
    imm11 = h1 & 0x7FF
    i1 = (~(j1 ^ s)) & 1
    i2 = (~(j2 ^ s)) & 1
    imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
    imm32 = sign_extend(imm32, 25)
    return (pc + 4 + imm32) & ~1


def find_bl_callers(blob: bytes, target: int) -> List[int]:
    out: List[int] = []
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(CODE_BASE + o, u16(blob, o), u16(blob, o + 2))
        if t == (target & ~1):
            out.append(CODE_BASE + o)
    return out


def find_fn_start(blob: bytes, site: int) -> int:
    for back in range(0, 0x800, 2):
        p = (site & ~1) - back
        if p < CODE_BASE:
            break
        h = u16(blob, p - CODE_BASE)
        # PUSH {...,lr} flavors
        if (h & 0xFF00) == 0xB500 or h == 0xB570 or h == 0xB5F0 or h == 0xB5F8:
            return p
        if h == 0xE92D:  # ARM PUSH — unlikely in Thumb region
            return p
    return site & ~1


def decode(blob: bytes, pc: int) -> Tuple[int, str, Dict[str, Any]]:
    off = pc - CODE_BASE
    if off < 0 or off + 1 >= len(blob):
        return 2, "???", {}
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {"h0": f"0x{h0:04X}"}
    # 32-bit Thumb
    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        meta["h1"] = f"0x{h1:04X}"
        t = bl_target(pc, h0, h1)
        if t is not None:
            meta["bl"] = f"0x{t:X}"
            return 4, f"BL 0x{t:X}", meta
        if (h1 & 0xD000) == 0x8000:
            return 4, f"B.W/cond h1=0x{h1:04X}", meta
        if (h0 & 0xFFF0) == 0xF240 or (h0 & 0xFBF0) == 0xF240:
            return 4, f"MOVW/MOVT-ish 0x{h0:04X} 0x{h1:04X}", meta
        return 4, f"THUMB32 0x{h0:04X} 0x{h1:04X}", meta
    # common 16-bit
    if (h0 & 0xFF00) == 0xB500:
        return 2, f"PUSH {{{h0 & 0xFF:02X},lr}}", meta
    if (h0 & 0xFF00) == 0xBD00:
        return 2, f"POP {{{h0 & 0xFF:02X},pc}}", meta
    if (h0 & 0xFF00) == 0xB000:
        imm = (h0 & 0x7F) << 2
        if h0 & 0x80:
            return 2, f"SUB sp,#0x{imm:X}", meta
        return 2, f"ADD sp,#0x{imm:X}", meta
    if (h0 & 0xF800) == 0x4800:
        rt = (h0 >> 8) & 7
        imm = h0 & 0xFF
        lit = ((pc + 4) & ~2) + (imm << 2)
        meta["litpool"] = f"0x{lit:X}"
        if lit - CODE_BASE + 3 < len(blob):
            meta["lit"] = f"0x{u32(blob, lit - CODE_BASE):X}"
        return 2, f"LDR r{rt},[pc,#0x{imm << 2:X}] ; ->0x{lit:X}", meta
    if (h0 & 0xF800) == 0x6800:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        return 2, f"LDR r{rt},[r{rn},#0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x6000:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        return 2, f"STR r{rt},[r{rn},#0x{imm:X}]", meta
    if (h0 & 0xFFC0) == 0x4280:
        return 2, f"CMP r{h0 & 7},r{(h0 >> 3) & 7}", meta
    if (h0 & 0xF800) == 0x2000:
        return 2, f"MOVS r{(h0 >> 8) & 7},#0x{h0 & 0xFF:X}", meta
    if (h0 & 0xFF00) == 0x2800:
        return 2, f"CMP r{(h0 >> 8) & 7},#0x{h0 & 0xFF:X}", meta
    if (h0 & 0xF000) == 0xD000:
        cond = (h0 >> 8) & 0xF
        imm = sign_extend(h0 & 0xFF, 8) << 1
        tgt = (pc + 4 + imm) & ~1
        meta["bcond"] = f"0x{tgt:X}"
        return 2, f"Bcond({cond}) 0x{tgt:X}", meta
    if (h0 & 0xF800) == 0xE000:
        imm = sign_extend(h0 & 0x7FF, 11) << 1
        tgt = (pc + 4 + imm) & ~1
        meta["b"] = f"0x{tgt:X}"
        return 2, f"B 0x{tgt:X}", meta
    if (h0 & 0xFF87) == 0x4700:
        return 2, f"BX r{(h0 >> 3) & 0xF}", meta
    if (h0 & 0xFF87) == 0x4780:
        return 2, f"BLX r{(h0 >> 3) & 0xF}", {"blx_reg": (h0 >> 3) & 0xF}
    if (h0 & 0xFFC0) == 0x0000:
        return 2, f"LSLS r{h0 & 7},r{(h0 >> 3) & 7},#?", meta
    if (h0 & 0xFFC0) == 0x1C00:
        return 2, f"ADDS r{h0 & 7},r{(h0 >> 3) & 7},#?", meta
    if (h0 & 0xFF00) == 0x1C00 or (h0 & 0xFE00) == 0x1C00:
        return 2, f"ADD/SUB 0x{h0:04X}", meta
    # MOV low
    if (h0 & 0xFFC0) == 0x0000:
        pass
    if (h0 & 0xFF00) == 0x4600:
        rd = ((h0 >> 3) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"MOV r{rd},r{rm}", meta
    return 2, f"h0=0x{h0:04X}", meta


def disasm_fn(blob: bytes, start: int, limit: int = 0x200) -> List[str]:
    lines: List[str] = []
    pc = start & ~1
    end = min(start + limit, CODE_BASE + len(blob))
    seen_ret = 0
    while pc < end:
        n, text, meta = decode(blob, pc)
        extra = ""
        if "lit" in meta:
            extra = f"  lit={meta['lit']}"
        if "bl" in meta:
            extra = f"  -> {meta['bl']}"
        lines.append(f"  0x{pc:08X}: {text}{extra}")
        if "POP" in text and "pc" in text:
            seen_ret += 1
            if seen_ret >= 1 and pc > start + 0x20:
                break
        if text.startswith("BX lr") or text == "BX r14":
            break
        pc += n
    return lines


def disasm_window(blob: bytes, start: int, nbytes: int) -> List[str]:
    lines: List[str] = []
    pc = start & ~1
    end = min(start + nbytes, CODE_BASE + len(blob))
    while pc < end:
        n, text, meta = decode(blob, pc)
        extra = ""
        for k in ("bl", "lit", "bcond", "b", "blx_reg"):
            if k in meta:
                extra += f" {k}={meta[k]}"
        lines.append(f"  0x{pc:08X}: {text}{extra}")
        pc += n
    return lines


def collect_mem_ops(blob: bytes, start: int, limit: int = 0x180) -> List[Dict[str, Any]]:
    ops = []
    pc = start & ~1
    end = min(start + limit, CODE_BASE + len(blob))
    while pc < end:
        n, text, meta = decode(blob, pc)
        if text.startswith("LDR") or text.startswith("STR"):
            ops.append({"pc": f"0x{pc:X}", "op": text, **{k: v for k, v in meta.items() if k in ("lit", "litpool")}})
        if "bl" in meta:
            ops.append({"pc": f"0x{pc:X}", "op": text, "target": meta["bl"]})
        if "blx_reg" in meta:
            ops.append({"pc": f"0x{pc:X}", "op": text, "blx_reg": meta["blx_reg"]})
        if "POP" in text and "pc" in text:
            break
        pc += n
    return ops


def cfg_dot(blob: bytes, start: int, limit: int = 0x120) -> str:
    nodes = set()
    edges = []
    pc = start & ~1
    end = min(start + limit, CODE_BASE + len(blob))
    while pc < end:
        n, text, meta = decode(blob, pc)
        nodes.add(pc)
        nxt = pc + n
        if "bcond" in meta:
            t = int(meta["bcond"], 16)
            edges.append((pc, t, "cond"))
            edges.append((pc, nxt, "fall"))
        elif "b" in meta and text.startswith("B "):
            t = int(meta["b"], 16)
            edges.append((pc, t, "b"))
        elif "bl" in meta:
            edges.append((pc, nxt, "after_bl"))
        elif "POP" in text and "pc" in text:
            break
        else:
            if nxt < end:
                edges.append((pc, nxt, "seq"))
        pc = nxt
    lines = ["digraph G {", "  rankdir=TB;"]
    for n in sorted(nodes):
        lines.append(f'  n{n:X} [label="0x{n:X}"];')
    for a, b, k in edges:
        lines.append(f'  n{a:X} -> n{b:X} [label="{k}"];')
    lines.append("}")
    return "\n".join(lines)


def main() -> None:
    blob = BLOB_PATH.read_bytes()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    REP.mkdir(parents=True, exist_ok=True)

    fn = find_fn_start(blob, TARGET)
    callers = find_bl_callers(blob, TARGET)
    case9_callers = find_bl_callers(blob, CASE9)  # unlikely direct BL to case arm
    poll_fn = find_fn_start(blob, POLL)

    ann = []
    ann.append(f"# 0x305E09 annotated (static)")
    ann.append(f"code_base=0x{CODE_BASE:X}")
    ann.append(f"live_entry=0x305E09 thumb=yes")
    ann.append(f"fn_start=0x{fn:X}")
    ann.append(f"callers_bl={len(callers)}")
    for c in callers:
        ann.append(f"  caller_bl=0x{c:X}")
    ann.append("")
    ann.append("## Function body")
    ann.extend(disasm_fn(blob, fn, 0x220))
    ann.append("")
    ann.append("## Case-9 arm window @ 0x30E1A0")
    ann.extend(disasm_window(blob, CASE9, 0x40))
    ann.append("")
    ann.append("## Poll / sendAppEvent site @ 0x304589")
    ann.append(f"poll_fn_start=0x{poll_fn:X}")
    ann.extend(disasm_window(blob, poll_fn, 0x120))
    ann.append("")
    ann.append("## Window around 0x304589")
    ann.extend(disasm_window(blob, 0x304560, 0x80))

    (OUT_DIR / "305e09_annotated.txt").write_text("\n".join(ann) + "\n", encoding="utf-8")
    (OUT_DIR / "305e09_cfg.dot").write_text(cfg_dot(blob, fn), encoding="utf-8")

    # callers csv
    rows = ["pc,insn,note"]
    for c in callers:
        n, text, meta = decode(blob, c)
        rows.append(f"0x{c:X},{text},bl_to_305E08")
    (REP / "product_event_305e09_callers.csv").write_text("\n".join(rows) + "\n", encoding="utf-8")

    ops = collect_mem_ops(blob, fn)
    mem_rows = ["pc,op,detail"]
    for o in ops:
        detail = o.get("target") or o.get("lit") or o.get("blx_reg") or ""
        mem_rows.append(f"{o['pc']},{o['op']},{detail}")
    (REP / "product_event_305e09_static_ops.csv").write_text("\n".join(mem_rows) + "\n", encoding="utf-8")

    summary = {
        "code_base": f"0x{CODE_BASE:X}",
        "entry": "0x305E09",
        "fn_start": f"0x{fn:X}",
        "callers": [f"0x{c:X}" for c in callers],
        "poll_fn": f"0x{poll_fn:X}",
        "case9": f"0x{CASE9:X}",
        "static_ops": ops[:40],
    }
    (OUT_DIR / "305e09_static_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
