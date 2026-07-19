#!/usr/bin/env python3
"""Stage E8G: bootstrap writer caller provenance + fault 0x2D92B0 + 10102 jump-table."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

# Priority bootstrap callers (from E8F writer_callpath).
BOOTSTRAP_EDGES: List[Tuple[str, int, List[int]]] = [
    ("C44_2F4E82", 0x2F4E82, [0x302340, 0x302362]),
    ("C44_2FEDFA", 0x2FEDFA, [0x2FC048]),
    ("C44_2FEE4E", 0x2FEE4E, [0x2FC048]),
    ("CF5_2E7DBC", 0x2E7DBC, [0x2E32A2]),
    ("C9D_2F097A", 0x2F097A, [0x2EFF1C, 0x2F08A4, 0x2F0D6A, 0x2F1FF0]),
    ("C9D_2FB008", 0x2FB008, [0x30D9EE]),
    ("C9D_30AA42", 0x30AA42, [0x30AF8A, 0x30DF78]),
    ("C44_2FB286", 0x2FB286, [0x2DA64A, 0x2DE7EC, 0x30FCEE]),
]

FAULT_PC = 0x2D92B0
HANDLER_10102 = 0x30D301


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
            meta.update({"kind": "thumb2", "size": 4, "h1": h1})
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
    if (h0 & 0xFF00) == 0x4400:
        rd = (h0 & 7) | ((h0 >> 4) & 8)
        rm = (h0 >> 3) & 0xF
        meta.update({"kind": "ADD", "rd": rd, "rm": rm})
        return 2, f"ADD r{rd}, r{rm}", meta
    if (h0 & 0xFF80) == 0x4700:
        rm = (h0 >> 3) & 0xF
        meta.update({"kind": "BX" if (h0 & 0x80) == 0 else "BLX_reg", "rm": rm})
        return 2, f"{'BLX' if h0 & 0x80 else 'BX'} r{rm}", meta
    if (h0 & 0xF800) == 0x6800:
        meta["kind"] = "LDR"
        return 2, f"LDR r{h0 & 7},[r{(h0 >> 3) & 7},#0x{((h0 >> 6) & 0x1F) << 2:X}]", meta
    if (h0 & 0xF800) == 0x7800:
        meta["kind"] = "LDRB"
        return 2, f"LDRB r{h0 & 7},[r{(h0 >> 3) & 7},#0x{(h0 >> 6) & 0x1F:X}]", meta
    meta["kind"] = "raw"
    return 2, f"h0=0x{h0:04X}", meta


def find_fn_bounds(blob: bytes, code_base: int, pc: int) -> Tuple[int, int]:
    va = pc & ~1
    start = va
    for back in range(0, 0x600, 2):
        cur = va - back
        if cur < code_base:
            break
        h0 = u16(blob, cur - code_base)
        if (h0 & 0xFF00) == 0xB500:
            start = cur
            break
    end = va + 2
    cur = start
    for _ in range(0x600):
        size, note, meta = decode_one(blob, code_base, cur)
        if meta.get("kind") == "POP_PC":
            end = cur + size
            break
        cur += size
        end = cur
        if cur > start + 0x800:
            break
    return start, end


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


def scan_fn(blob: bytes, code_base: int, fn_s: int, fn_e: int) -> Dict[str, Any]:
    bls = []
    preds = []
    r9_offs = []
    platish = []
    va = fn_s
    while va < fn_e:
        size, note, meta = decode_one(blob, code_base, va)
        if meta.get("kind") == "BL":
            t = meta["target"]
            bls.append({"pc": f"0x{va:X}", "target": f"0x{t:X}"})
            if t in (0x304558, 0x304559, 0x3046A8, 0x305604):
                platish.append({"pc": f"0x{va:X}", "helper": f"0x{t:X}"})
        if meta.get("kind") in ("CMP_imm", "Bcond", "LDR_lit", "ADD"):
            preds.append(f"0x{va:X}: {note}")
        if meta.get("kind") == "LDR_lit" and "lit_val" in meta:
            v = meta["lit_val"]
            if 0x100 <= v <= 0x2000:
                r9_offs.append({"pc": f"0x{va:X}", "off": f"0x{v:X}"})
        va += size
    return {
        "bl_targets": bls[:40],
        "predicates": preds[:30],
        "r9_offset_lits": r9_offs[:20],
        "plat_helpers": platish,
    }


def disasm_window(blob: bytes, code_base: int, entry: int, n: int = 64) -> List[str]:
    lines = []
    va = entry & ~1
    for _ in range(n):
        size, note, meta = decode_one(blob, code_base, va)
        hx = blob[va - code_base : va - code_base + size].hex().upper()
        lines.append(f"  0x{va:08X}: {hx:<10}  {note}")
        if meta.get("kind") == "POP_PC":
            break
        va += size
    return lines


def analyze_fault(blob: bytes, code_base: int, fault_pc: int) -> Dict[str, Any]:
    fn_s, fn_e = find_fn_bounds(blob, code_base, fault_pc)
    # prior 32 insns approx 64 bytes
    start = max(fn_s, (fault_pc - 0x40) & ~1)
    prior = []
    va = start
    while va < (fault_pc & ~1):
        size, note, meta = decode_one(blob, code_base, va)
        prior.append(f"0x{va:X}: {note}")
        va += size
    size, note, meta = decode_one(blob, code_base, fault_pc & ~1)
    callers = find_bl_callers(blob, code_base, fn_s)
    return {
        "fault_pc": f"0x{fault_pc:X}",
        "fault_insn": note,
        "fault_kind": meta.get("kind"),
        "fn_start": f"0x{fn_s:X}",
        "fn_end": f"0x{fn_e:X}",
        "upstream_bl_callers": [f"0x{c:X}" for c in callers[:20]],
        "prior_insns": prior[-32:],
        "hypothesis": (
            "CF tick2 hit this PC with UC_ERR_EXCEPTION r0=3; "
            "classify live: unmapped vs trap vs bad target"
        ),
        "evidence": "TARGET_OBSERVED+HYPOTHESIS",
    }


def analyze_10102(blob: bytes, code_base: int) -> Dict[str, Any]:
    fn_s, fn_e = find_fn_bounds(blob, code_base, HANDLER_10102)
    body = scan_fn(blob, code_base, fn_s, fn_e)
    # Look for TBH/TBB or LDR lit jump tables / CMP ranges
    cases = []
    va = fn_s
    while va < fn_e:
        size, note, meta = decode_one(blob, code_base, va)
        if meta.get("kind") == "CMP_imm" and meta.get("rn") in (0, 1, 2, 3):
            cases.append({"pc": f"0x{va:X}", "note": note})
        if meta.get("kind") == "BL":
            t = meta["target"]
            interesting = {
                0x302340,
                0x302362,
                0x2FC048,
                0x2E32A2,
                0x30D9EE,
                0x2F4E64,
                0x2F4E82,
            }
            if t in interesting or any(abs(t - x) < 8 for x in interesting):
                cases.append({"pc": f"0x{va:X}", "bl_interesting": f"0x{t:X}"})
        va += size
    # Cross: does 10102 fn BL any bootstrap callers?
    bl_set = {int(x["target"], 16) for x in body["bl_targets"]}
    bootstrap_targets = set()
    for _, writer, callers in BOOTSTRAP_EDGES:
        bootstrap_targets.add(writer & ~1)
        for c in callers:
            bootstrap_targets.add(c & ~1)
            fs, _ = find_fn_bounds(blob, code_base, c)
            bootstrap_targets.add(fs)
    hits = sorted(bl_set & bootstrap_targets)
    return {
        "handler": f"0x{HANDLER_10102:X}",
        "fn_start": f"0x{fn_s:X}",
        "fn_end": f"0x{fn_e:X}",
        "cmp_sites": cases[:40],
        "bl_to_bootstrap": [f"0x{h:X}" for h in hits],
        "body_summary": body,
        "note": "ZERO_ARGS insufficient; need case index from R0/R1 before firing",
        "evidence": "TARGET_OBSERVED",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument("-o", "--out-dir", default="out/e8g_tmp")
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    records = []
    md = ["# E8G bootstrap caller provenance", f"code_base=0x{args.code_base:X}", ""]
    bp_pcs: Set[int] = set()
    fn_entries: Set[int] = set()

    for tag, writer, callers in BOOTSTRAP_EDGES:
        md.append(f"## {tag} writer 0x{writer:X}")
        for cpc in callers:
            fn_s, fn_e = find_fn_bounds(blob, args.code_base, cpc)
            up = find_bl_callers(blob, args.code_base, fn_s)
            body = scan_fn(blob, args.code_base, fn_s, fn_e)
            # Does this caller site actually BL the writer?
            bls_to_writer = [
                b for b in body["bl_targets"] if int(b["target"], 16) == (writer & ~1)
            ]
            # Also BL to writer fn start
            wfn_s, _ = find_fn_bounds(blob, args.code_base, writer)
            bls_to_wfn = [
                b for b in body["bl_targets"] if int(b["target"], 16) == (wfn_s & ~1)
            ]
            rec = {
                "tag": tag,
                "writer_pc": f"0x{writer:X}",
                "caller_pc": f"0x{cpc:X}",
                "fn_start": f"0x{fn_s:X}",
                "fn_end": f"0x{fn_e:X}",
                "upstream_bl_callers": [f"0x{u:X}" for u in up[:16]],
                "upstream_count": len(up),
                "bl_to_writer": bls_to_writer or bls_to_wfn,
                "body": body,
                "evidence": "TARGET_OBSERVED",
            }
            records.append(rec)
            bp_pcs.add(cpc & ~1)
            bp_pcs.add(fn_s & ~1)
            fn_entries.add(fn_s & ~1)
            md.append(f"### caller 0x{cpc:X}")
            md.append(f"- function: `0x{fn_s:X}` .. `0x{fn_e:X}`")
            md.append(
                f"- upstream BL callers ({len(up)}): "
                + (", ".join(f"0x{u:X}" for u in up[:8]) or "none")
            )
            md.append(f"- BL to writer/fn: {rec['bl_to_writer'] or 'none-in-fn-scan'}")
            md.append(f"- plat helpers: {body['plat_helpers']}")
            md.append(f"- R9 offset lits: {body['r9_offset_lits'][:8]}")
            md.append("- key predicates:")
            for p in body["predicates"][:12]:
                md.append(f"  - {p}")
            md.append("")
        md.append("")

    (out / "bootstrap_caller_xref.json").write_text(
        json.dumps(records, indent=2), encoding="utf-8"
    )
    (out / "bootstrap_caller_xref.md").write_text("\n".join(md) + "\n", encoding="utf-8")

    # Fault site
    fault = analyze_fault(blob, args.code_base, FAULT_PC)
    (out / "fault_2d92b0.json").write_text(json.dumps(fault, indent=2), encoding="utf-8")
    fmd = ["# E8G counterfactual fault site 0x2D92B0", ""]
    for k, v in fault.items():
        if k == "prior_insns":
            fmd.append("## Prior instructions")
            for line in v:
                fmd.append(f"- {line}")
        else:
            fmd.append(f"- **{k}**: {v}")
    fmd.append("")
    fmd.append("## Disasm window")
    fmd.extend(disasm_window(blob, args.code_base, int(fault["fn_start"], 16), 80))
    (out / "fault_2d92b0.md").write_text("\n".join(fmd) + "\n", encoding="utf-8")

    # 10102
    h102 = analyze_10102(blob, args.code_base)
    (out / "handler_10102_abi.json").write_text(json.dumps(h102, indent=2), encoding="utf-8")
    hmd = [
        "# E8G 0x10102 handler ABI (static)",
        f"handler={h102['handler']} fn={h102['fn_start']}..{h102['fn_end']}",
        f"bl_to_bootstrap={h102['bl_to_bootstrap']}",
        "",
        "## CMP / case sites",
    ]
    for c in h102["cmp_sites"]:
        hmd.append(f"- {c}")
    hmd.append("")
    hmd.append("## Disasm")
    hmd.extend(disasm_window(blob, args.code_base, HANDLER_10102, 80))
    (out / "handler_10102_abi.md").write_text("\n".join(hmd) + "\n", encoding="utf-8")

    # BP CSV for runtime
    bp_list = sorted(bp_pcs)
    # also fault fn entry + fault pc
    bp_list = sorted(set(bp_list) | {int(fault["fn_start"], 16), FAULT_PC & ~1})
    csv = ",".join(f"0x{p:X}" for p in bp_list)
    (out / "caller_bp_csv.txt").write_text(csv + "\n", encoding="utf-8")
    (out / "caller_breakpoints.json").write_text(
        json.dumps(
            {
                "caller_pcs": [f"0x{p:X}" for p in bp_list],
                "fn_entries": [f"0x{p:X}" for p in sorted(fn_entries)],
                "fault_pc": f"0x{FAULT_PC:X}",
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    print(f"wrote {out}/bootstrap_caller_xref.md ({len(records)} callers)")
    print(f"wrote {out}/fault_2d92b0.md")
    print(f"wrote {out}/handler_10102_abi.md")
    print(f"bp_csv count={len(bp_list)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
