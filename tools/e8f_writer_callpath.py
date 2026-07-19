#!/usr/bin/env python3
"""Stage E8F: function-level callpath xref for C44/C9D/CF5 idle-flag writers."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

# Priority writers from E8D writer_class (deduped).
TOP_WRITERS: Dict[str, List[int]] = {
    "C44": [
        0x2E87F2,
        0x2F4E82,
        0x2F7F56,
        0x2FB286,
        0x2FC8CA,
        0x2FEDFA,
        0x2FEE4E,
        0x30CC72,
        0x311C3E,
    ],
    "C9D": [
        0x2E3A68,
        0x2F097A,
        0x2F7772,
        0x2FB008,
        0x307796,
        0x30AA42,
        0x3115B4,
        # memcpy-like cluster endpoints (sample)
        0x2D9B68,
        0x2D9CE6,
        0x2DBA82,
    ],
    "CF5": [0x2E7DBC, 0x2F7F2C],
}

SIBLINGS = {
    0x10162: 0x30D249,
    0x10102: 0x30D301,
    0x10165: 0x30D2F9,
}


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
    return (pc + 4 + imm32) | (1 if (h1 & 0x1000) else 0)


CONDS = {
    0: "EQ",
    1: "NE",
    2: "CS",
    3: "CC",
    4: "MI",
    5: "PL",
    6: "VS",
    7: "VC",
    8: "HI",
    9: "LS",
    10: "GE",
    11: "LT",
    12: "GT",
    13: "LE",
}


def decode_one(blob: bytes, code_base: int, va: int) -> Tuple[int, str, Dict[str, Any]]:
    off = va - code_base
    if off < 0 or off + 1 >= len(blob):
        return 2, "oob", {"pc": va, "kind": "oob"}
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {"pc": va, "h0": h0}
    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        tgt = bl_target(va, h0, h1)
        if tgt is not None:
            kind = "BL" if (h1 & 0x1000) else "BLX"
            meta.update({"kind": kind, "target": tgt & ~1, "size": 4})
            return 4, f"{kind} -> 0x{tgt & ~1:X}", meta
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0:
            meta.update({"kind": "thumb2", "size": 4})
            return 4, f"thumb2 0x{h0:04X}_{h1:04X}", meta
    if (h0 & 0xFF00) == 0xB500:
        meta["kind"] = "PUSH"
        return 2, f"PUSH 0x{h0:04X}", meta
    if (h0 & 0xFF00) == 0xBD00:
        meta["kind"] = "POP_PC"
        return 2, f"POP 0x{h0:04X}", meta
    if (h0 & 0xF800) == 0x2000:
        rd, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"kind": "MOVS_imm", "rd": rd, "imm": imm})
        return 2, f"MOVS r{rd}, #{imm}", meta
    if (h0 & 0xF800) == 0x2800:
        rn, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"kind": "CMP_imm", "rn": rn, "imm": imm})
        return 2, f"CMP r{rn}, #{imm}", meta
    if (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
        imm = sign_extend(h0 & 0xFF, 8) << 1
        tgt = va + 4 + imm
        cond = (h0 >> 8) & 0xF
        meta.update({"kind": "Bcond", "target": tgt, "cond": cond})
        return 2, f"B{CONDS.get(cond, '?')} -> 0x{tgt:X}", meta
    if (h0 & 0xF800) == 0xE000:
        imm = sign_extend(h0 & 0x7FF, 11) << 1
        tgt = va + 4 + imm
        meta.update({"kind": "B", "target": tgt})
        return 2, f"B -> 0x{tgt:X}", meta
    if (h0 & 0xF800) == 0x4800:
        rd = (h0 >> 8) & 7
        imm = (h0 & 0xFF) << 2
        lit = ((va + 4) & ~2) + imm
        meta.update({"kind": "LDR_lit", "rd": rd, "lit": lit})
        lo = lit - code_base
        if 0 <= lo <= len(blob) - 4:
            meta["lit_val"] = u32(blob, lo)
            return 2, f"LDR r{rd},[pc,#0x{imm:X}] =0x{meta['lit_val']:X}", meta
        return 2, f"LDR r{rd},[pc,#0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x7000:
        meta["kind"] = "STRB"
        return 2, f"STRB r{h0 & 7},[r{(h0 >> 3) & 7},#0x{(h0 >> 6) & 0x1F:X}]", meta
    if (h0 & 0xF800) == 0x6000:
        meta["kind"] = "STR"
        return 2, f"STR r{h0 & 7},[r{(h0 >> 3) & 7},#0x{((h0 >> 6) & 0x1F) << 2:X}]", meta
    if (h0 & 0xF800) == 0x8000:
        meta["kind"] = "STRH"
        return 2, f"STRH r{h0 & 7},[r{(h0 >> 3) & 7},#0x{((h0 >> 6) & 0x1F) << 1:X}]", meta
    if (h0 & 0xFF00) == 0x4400:
        rd = (h0 & 7) | ((h0 >> 4) & 8)
        rm = (h0 >> 3) & 0xF
        meta.update({"kind": "ADD", "rd": rd, "rm": rm})
        return 2, f"ADD r{rd}, r{rm}", meta
    meta["kind"] = "raw"
    return 2, f"h0=0x{h0:04X}", meta


def find_fn_bounds(blob: bytes, code_base: int, writer: int) -> Tuple[int, int]:
    """Heuristic: walk back for PUSH {..,lr}, forward for POP {..,pc}."""
    va = writer & ~1
    start = va
    for back in range(0, 0x400, 2):
        cur = va - back
        if cur < code_base:
            break
        off = cur - code_base
        h0 = u16(blob, off)
        if (h0 & 0xFF00) == 0xB500:  # PUSH with lr bit typically set in B5xx
            start = cur
            break
    end = va + 2
    cur = va
    for _ in range(0x400):
        size, note, meta = decode_one(blob, code_base, cur)
        if meta.get("kind") == "POP_PC":
            end = cur + size
            break
        cur += size
        end = cur
        if cur > va + 0x400:
            break
    return start, end


def prior_predicates(blob: bytes, code_base: int, writer: int, nbytes: int = 40) -> List[str]:
    start = max(code_base, (writer - nbytes) & ~1)
    out = []
    va = start
    while va < (writer & ~1):
        size, note, meta = decode_one(blob, code_base, va)
        if meta.get("kind") in ("CMP_imm", "Bcond", "MOVS_imm", "LDR_lit", "ADD", "BL"):
            out.append(f"0x{va:X}: {note}")
        va += size
    return out[-12:]


def find_bl_callers(blob: bytes, code_base: int, target: int) -> List[int]:
    t = target & ~1
    sites = []
    for off in range(0, len(blob) - 3, 2):
        h0 = u16(blob, off)
        if (h0 & 0xF800) != 0xF000:
            continue
        h1 = u16(blob, off + 2)
        pc = code_base + off
        tgt = bl_target(pc, h0, h1)
        if tgt and (tgt & ~1) == t:
            sites.append(pc)
    return sites


def analyze_writer(blob: bytes, code_base: int, flag: str, writer: int) -> Dict[str, Any]:
    fn_s, fn_e = find_fn_bounds(blob, code_base, writer)
    size, note, meta = decode_one(blob, code_base, writer & ~1)
    callers = find_bl_callers(blob, code_base, fn_s)
    preds = prior_predicates(blob, code_base, writer)
    # value source: look for MOVS imm in prior window
    val_src = []
    for p in preds:
        if "MOVS" in p or "LDR" in p:
            val_src.append(p)
    return {
        "flag": flag,
        "writer_pc": f"0x{writer:X}",
        "store_note": note,
        "store_kind": meta.get("kind"),
        "fn_start": f"0x{fn_s:X}",
        "fn_end": f"0x{fn_e:X}",
        "fn_size": fn_e - fn_s,
        "upstream_bl_callers": [f"0x{c:X}" for c in callers[:16]],
        "caller_count": len(callers),
        "prior_predicates": preds,
        "value_source_hints": val_src[-6:],
        "reach_class_static": "UNREACHED_INTERNAL_STATE",  # filled live later
        "evidence": "TARGET_OBSERVED",
    }


def disasm_window(blob: bytes, code_base: int, entry: int, max_insns: int = 48) -> List[str]:
    lines = []
    va = entry & ~1
    for _ in range(max_insns):
        size, note, meta = decode_one(blob, code_base, va)
        hx = blob[va - code_base : va - code_base + size].hex().upper()
        lines.append(f"  0x{va:08X}: {hx:<10}  {note}")
        if meta.get("kind") == "POP_PC":
            break
        va += size
    return lines


def analyze_long_path(blob: bytes, code_base: int) -> Dict[str, Any]:
    """Document 0x30D24C queue / long-path / 0x101AB."""
    return {
        "enqueue_core": "0x30D24C",
        "short_path_ret": "0x30D28A",
        "long_path_entry": "0x30D28C",
        "queue_base_off": "0x7D8",
        "queue_depth_off": "0x7D8+0x6C=0x844",
        "long_path_gate": "BLE when [R9+0x844] <= 0",
        "plat_101ab_lit_pc": "0x30D2AA",
        "plat_101ab_call": "BL 0x304558 with r0=0x101AB r1=saved_R0 r3=2",
        "fe8_store_pc": "0x30D262",
        "b7d_store_pc": "0x30D284",
        "note": "Normal R0_EVENTCODE_2 took short path (b7d=1); long path needs depth<=0 after FE8 store",
        "evidence": "TARGET_OBSERVED",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument("-o", "--out-dir", default="out/e8f_tmp")
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    records = []
    md = [
        "# E8F writer callpath xref",
        f"code_base=0x{args.code_base:X}",
        f"ext={args.ext}",
        "",
    ]
    for flag, pcs in TOP_WRITERS.items():
        md.append(f"## {flag}")
        md.append("")
        for pc in pcs:
            rec = analyze_writer(blob, args.code_base, flag, pc)
            records.append(rec)
            md.append(f"### writer {rec['writer_pc']}")
            md.append(f"- store: `{rec['store_note']}` kind={rec['store_kind']}")
            md.append(f"- function: `{rec['fn_start']}` .. `{rec['fn_end']}` size=0x{rec['fn_size']:X}")
            md.append(f"- BL callers ({rec['caller_count']}): {', '.join(rec['upstream_bl_callers'][:8]) or 'none'}")
            md.append("- prior predicates:")
            for p in rec["prior_predicates"]:
                md.append(f"  - {p}")
            md.append("")

    (out / "writer_callpath.json").write_text(json.dumps(records, indent=2), encoding="utf-8")
    (out / "writer_callpath.md").write_text("\n".join(md) + "\n", encoding="utf-8")

    # Breakpoint list for runtime
    bps = []
    for flag, pcs in TOP_WRITERS.items():
        for pc in pcs:
            bps.append({"flag": flag, "pc": pc, "pc_hex": f"0x{pc:X}"})
    # Also fn entries (unique)
    fn_entries: Set[int] = set()
    for rec in records:
        fn_entries.add(int(rec["fn_start"], 16))
    bp_payload = {
        "writer_pcs": bps,
        "fn_entry_pcs": [f"0x{x:X}" for x in sorted(fn_entries)],
        "long_path_pc": "0x30D28C",
        "fe8_store_pc": "0x30D262",
    }
    (out / "writer_breakpoints.json").write_text(json.dumps(bp_payload, indent=2), encoding="utf-8")

    # Sibling handler disasm
    sib_md = ["# E8F sibling handlers 10162 / 10102", ""]
    for code, h in SIBLINGS.items():
        sib_md.append(f"## 0x{code:X} handler 0x{h:X}")
        sib_md.extend(disasm_window(blob, args.code_base, h, 40))
        sib_md.append("")
    (out / "sibling_handler_disasm.md").write_text("\n".join(sib_md), encoding="utf-8")

    longp = analyze_long_path(blob, args.code_base)
    (out / "long_path_101ab.md").write_text(
        "# E8F long path / 0x101AB\n\n"
        + "\n".join(f"- **{k}**: {v}" for k, v in longp.items())
        + "\n",
        encoding="utf-8",
    )
    (out / "long_path_101ab.json").write_text(json.dumps(longp, indent=2), encoding="utf-8")

    # CSV of writer PCs for C env
    csv = ",".join(f"0x{p:X}" for pcs in TOP_WRITERS.values() for p in pcs)
    (out / "writer_bp_csv.txt").write_text(csv + "\n", encoding="utf-8")

    print(f"wrote {out}/writer_callpath.md ({len(records)} writers)")
    print(f"wrote {out}/writer_breakpoints.json")
    print(f"wrote {out}/sibling_handler_disasm.md")
    print(f"bp_csv={csv[:60]}...")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
