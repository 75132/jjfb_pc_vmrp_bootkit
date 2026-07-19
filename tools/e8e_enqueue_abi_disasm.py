#!/usr/bin/env python3
"""Stage E8E: disasm 0x30D24C enqueue core + ABI inference + callsite reconstruction.

Entry VA is Thumb (odd) 0x30D24D → even body 0x30D24C.
"""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

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


def decode_one(blob: bytes, code_base: int, va: int) -> Tuple[int, str, Dict[str, Any]]:
    off = va - code_base
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {"pc": va, "h0": h0}

    # Thumb-2 BL/BLX
    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        tgt = bl_target(va, h0, h1)
        if tgt is not None:
            kind = "BL" if (h1 & 0x1000) else "BLX"
            meta.update({"kind": kind, "target": tgt & ~1, "size": 4, "h1": h1})
            return 4, f"{kind} -> 0x{tgt & ~1:X}", meta
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0:
            meta.update({"kind": "thumb2", "size": 4, "h1": h1})
            return 4, f"thumb2 h0=0x{h0:04X} h1=0x{h1:04X}", meta

    if h0 == 0xB5F0:
        meta["kind"] = "PUSH"
        return 2, "PUSH {r4-r7,lr}", meta
    if h0 == 0xBDF0:
        meta["kind"] = "POP_PC"
        return 2, "POP {r4-r7,pc}", meta
    if (h0 & 0xFF00) == 0xB500:
        meta["kind"] = "PUSH"
        return 2, f"PUSH h0=0x{h0:04X}", meta
    if (h0 & 0xFF00) == 0xBD00:
        meta["kind"] = "POP_PC"
        return 2, f"POP h0=0x{h0:04X}", meta

    if (h0 & 0xFF80) == 0xB080:
        imm = (h0 & 0x7F) * 4
        meta.update({"kind": "SUB_SP", "imm": imm})
        return 2, f"SUB SP, #0x{imm:X}", meta
    if (h0 & 0xFF80) == 0xB000:
        imm = (h0 & 0x7F) * 4
        meta.update({"kind": "ADD_SP", "imm": imm})
        return 2, f"ADD SP, #0x{imm:X}", meta

    if (h0 & 0xF800) == 0x2000:
        rd, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"kind": "MOVS_imm", "rd": rd, "imm": imm})
        return 2, f"MOVS r{rd}, #{imm}", meta

    if (h0 & 0xFE00) == 0x1C00:
        rd, rn = h0 & 7, (h0 >> 3) & 7
        meta.update({"kind": "MOV_lo", "rd": rd, "rn": rn})
        return 2, f"ADDS r{rd}, r{rn}, #0  ; mov", meta

    if (h0 & 0xFFC0) == 0x1A00:
        rd, rn, rm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 7
        meta.update({"kind": "SUBS_reg", "rd": rd, "rn": rn, "rm": rm})
        return 2, f"SUBS r{rd}, r{rn}, r{rm}", meta

    if (h0 & 0xFF00) == 0x4400:
        rd = (h0 & 7) | ((h0 >> 4) & 8)
        rm = (h0 >> 3) & 0xF
        meta.update({"kind": "ADD", "rd": rd, "rm": rm})
        return 2, f"ADD r{rd}, r{rm}", meta

    if (h0 & 0xF800) == 0x4800:
        rd = (h0 >> 8) & 7
        imm = (h0 & 0xFF) << 2
        lit = ((va + 4) & ~2) + imm
        meta.update({"kind": "LDR_lit", "rd": rd, "lit": lit})
        lo = lit - code_base
        if 0 <= lo <= len(blob) - 4:
            meta["lit_val"] = u32(blob, lo)
            return 2, f"LDR r{rd}, [pc, #0x{imm:X}] lit=0x{lit:X} val=0x{meta['lit_val']:X}", meta
        return 2, f"LDR r{rd}, [pc, #0x{imm:X}] lit=0x{lit:X}", meta

    if (h0 & 0xF800) == 0x6000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        meta.update({"kind": "STR", "rt": rt, "rn": rn, "imm": imm})
        return 2, f"STR r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x6800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        meta.update({"kind": "LDR", "rt": rt, "rn": rn, "imm": imm})
        return 2, f"LDR r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x7000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        meta.update({"kind": "STRB", "rt": rt, "rn": rn, "imm": imm})
        return 2, f"STRB r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x7800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        meta.update({"kind": "LDRB", "rt": rt, "rn": rn, "imm": imm})
        return 2, f"LDRB r{rt}, [r{rn}, #0x{imm:X}]", meta

    if (h0 & 0xF800) == 0x2800:
        rn, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"kind": "CMP_imm", "rn": rn, "imm": imm})
        return 2, f"CMP r{rn}, #{imm}", meta
    if (h0 & 0xF800) == 0x3000:
        rd, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"kind": "ADDS_imm", "rd": rd, "imm": imm})
        return 2, f"ADDS r{rd}, #{imm}", meta
    if (h0 & 0xF800) == 0x3800:
        rd, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"kind": "SUBS_imm", "rd": rd, "imm": imm})
        return 2, f"SUBS r{rd}, #{imm}", meta

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

    if (h0 & 0xF800) == 0x9000:
        rd, imm = (h0 >> 8) & 7, (h0 & 0xFF) << 2
        meta.update({"kind": "STR_sp", "rd": rd, "imm": imm})
        return 2, f"STR r{rd}, [sp, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x9800:
        rd, imm = (h0 >> 8) & 7, (h0 & 0xFF) << 2
        meta.update({"kind": "LDR_sp", "rd": rd, "imm": imm})
        return 2, f"LDR r{rd}, [sp, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0xA800:
        rd, imm = (h0 >> 8) & 7, (h0 & 0xFF) << 2
        meta.update({"kind": "ADD_sp", "rd": rd, "imm": imm})
        return 2, f"ADD r{rd}, sp, #0x{imm:X}", meta

    if (h0 & 0xFF80) == 0x4700:
        rm = (h0 >> 3) & 0xF
        kind = "BLX_reg" if (h0 & 0x80) else "BX"
        meta.update({"kind": kind, "rm": rm})
        return 2, f"{'BLX' if h0 & 0x80 else 'BX'} r{rm}", meta

    meta["kind"] = "raw"
    return 2, f"h0=0x{h0:04X}", meta


def walk_function(
    blob: bytes, code_base: int, entry: int, max_insns: int = 200
) -> List[Dict[str, Any]]:
    va = entry & ~1
    insns: List[Dict[str, Any]] = []
    # Follow fall-through + collect branch targets within function window
    end_limit = va + 0x200
    work = [va]
    seen = set()
    while work and len(insns) < max_insns:
        cur = work.pop(0)
        if cur in seen or cur < (entry & ~1) or cur >= end_limit:
            continue
        # linear scan from cur until POP_PC or unconditional B out
        while cur not in seen and cur < end_limit and len(insns) < max_insns:
            seen.add(cur)
            size, note, meta = decode_one(blob, code_base, cur)
            meta["note"] = note
            meta["hex"] = blob[cur - code_base : cur - code_base + size].hex().upper()
            meta["size"] = size
            insns.append(meta)
            kind = meta.get("kind")
            if kind == "Bcond" and meta.get("target"):
                work.append(meta["target"])
            if kind == "B" and meta.get("target"):
                t = meta["target"]
                if (entry & ~1) <= t < end_limit:
                    work.append(t)
                cur = t  # treat as goto within window? better stop linear
                break
            if kind == "POP_PC":
                break
            if kind == "BX" and meta.get("rm") == 14:
                break
            cur += size
    insns.sort(key=lambda m: m["pc"])
    # dedupe
    out = []
    last = None
    for m in insns:
        if m["pc"] != last:
            out.append(m)
            last = m["pc"]
    return out


def infer_abi(insns: List[Dict[str, Any]]) -> Dict[str, Any]:
    args: Dict[str, Any] = {
        "R0": {
            "first_use": "saved_to_r4_at_entry",
            "uses": ["ADDS r4, r0, #0"],
            "null_check": False,
            "deref_offs": [],
            "cmp_imm": [],
            "hypothesis": "event_payload_or_node_ptr_passed_to_helper_0x3046A8",
            "evidence": "TARGET_OBSERVED",
        },
        "R1": {
            "first_use": "saved_to_r6_at_entry",
            "uses": ["ADDS r6, r1, #0"],
            "null_check": False,
            "deref_offs": [],
            "cmp_imm": [],
            "hypothesis": "context_or_queue_side_arg_passed_to_helper",
            "evidence": "TARGET_OBSERVED",
        },
        "R2": {
            "first_use": None,
            "uses": [],
            "null_check": False,
            "deref_offs": [],
            "cmp_imm": [],
            "hypothesis": "unused_at_prologue_AAPCS_scratch",
            "evidence": "TARGET_OBSERVED",
        },
        "R3": {
            "first_use": "MOVS r3,#0 then STR to queue+0x6C on path",
            "uses": [],
            "null_check": False,
            "deref_offs": [],
            "cmp_imm": [],
            "hypothesis": "not_an_incoming_arg_cleared_locally",
            "evidence": "TARGET_OBSERVED",
        },
    }
    r9_offs = []
    fe8_refs = []
    bl_targets = []
    for m in insns:
        if m.get("kind") == "BL":
            bl_targets.append({"pc": f"0x{m['pc']:X}", "target": f"0x{m['target']:X}"})
        if m.get("kind") == "LDR_lit" and "lit_val" in m:
            val = m["lit_val"]
            r9_offs.append({"pc": f"0x{m['pc']:X}", "offset": f"0x{val:X}", "rd": m.get("rd")})
            if val == 0xFE8:
                fe8_refs.append(
                    {
                        "pc": f"0x{m['pc']:X}",
                        "role": "load_FE8_offset",
                        "next": "ADD r1,r9; STR r0,[r1] stores helper return",
                    }
                )
            if val == 0x7D8:
                fe8_refs.append(
                    {
                        "pc": f"0x{m['pc']:X}",
                        "role": "queue_base_R9_plus_7D8",
                        "fields": {"+0x24": "counter_dec", "+0x6C": "count_or_depth"},
                    }
                )
            if val == 0xB7D:
                fe8_refs.append(
                    {
                        "pc": f"0x{m['pc']:X}",
                        "role": "status_byte_R9_plus_B7D",
                        "next": "STRB #1",
                    }
                )

    return {
        "entry_thumb": "0x30D24D",
        "entry_even": "0x30D24C",
        "args": args,
        "r9_relative_offsets": r9_offs,
        "fe8_refs": fe8_refs,
        "bl_targets": bl_targets,
        "control_flow": {
            "short_path": "when [R9+0x7D8+0x6C] > 0: dec counters, STRB 1 at R9+0xB7D, return 0",
            "long_path": "BLE to 0x30D28C when queue depth <= 0 — uses saved r4/r6 (R0/R1 args)",
            "fe8_role_hypothesis": "R9+0xFE8 holds last helper(0x3046A8) return / queue token",
        },
        "evidence": "TARGET_OBSERVED",
        "note": "Semantic event-name labels remain HYPOTHESIS until live probe confirms flag unlock",
    }


def find_bl_callsites(blob: bytes, code_base: int, targets: List[int]) -> List[int]:
    tset = set(t & ~1 for t in targets)
    sites = []
    for off in range(0, len(blob) - 3, 2):
        h0 = u16(blob, off)
        if (h0 & 0xF800) != 0xF000:
            continue
        h1 = u16(blob, off + 2)
        pc = code_base + off
        tgt = bl_target(pc, h0, h1)
        if tgt and (tgt & ~1) in tset:
            sites.append(pc)
    return sites


def find_literal_refs(blob: bytes, code_base: int, values: List[int]) -> List[Dict[str, Any]]:
    out = []
    for val in values:
        pat = struct.pack("<I", val)
        idx = 0
        while True:
            i = blob.find(pat, idx)
            if i < 0:
                break
            out.append({"va": f"0x{code_base + i:X}", "value": f"0x{val:X}"})
            idx = i + 1
    return out


def reconstruct_args(blob: bytes, code_base: int, call_pc: int, window: int = 48) -> Dict[str, Any]:
    start = max(code_base, (call_pc - window) & ~1)
    va = start
    imms: List[int] = []
    setup = []
    while va < call_pc:
        size, note, meta = decode_one(blob, code_base, va)
        if meta.get("kind") == "MOVS_imm":
            imms.append(meta["imm"])
        if meta.get("kind") == "LDR_lit" and "lit_val" in meta:
            imms.append(meta["lit_val"])
        if meta.get("kind") in (
            "MOVS_imm",
            "LDR_lit",
            "MOV_lo",
            "ADD",
            "STR",
            "STRB",
            "LDR",
            "LDRB",
            "STR_sp",
            "LDR_sp",
        ):
            setup.append({"pc": f"0x{meta['pc']:X}", "note": note})
        va += size
    interesting = [
        x
        for x in imms
        if x in (0, 1, 2, 3, 5, 8, 9, 0x10, 0x18, 0x100, 0xFE8, 0x7D8, 0xB7D, 0xE200)
    ]
    return {
        "call_pc": f"0x{call_pc:X}",
        "setup": setup[-16:],
        "immediates": sorted(set(imms))[:24],
        "interesting_imms": sorted(set(interesting)),
    }


def resolve_trampoline(blob: bytes, code_base: int, handler: int) -> Optional[int]:
    off = (handler & ~1) - code_base
    if off < 0 or off + 8 > len(blob):
        return None
    size, note, meta = decode_one(blob, code_base, handler & ~1)
    if meta.get("kind") != "PUSH":
        return None
    _, _, meta2 = decode_one(blob, code_base, (handler & ~1) + size)
    if meta2.get("kind") == "BL":
        return meta2["target"]
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument("--entry", type=lambda x: int(x, 0), default=0x30D24D)
    ap.add_argument("-o", "--out-dir", default="out/e8e_tmp")
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)

    entry = args.entry & ~1
    insns = walk_function(blob, args.code_base, entry)
    lines = [
        "# E8E enqueue core disasm",
        f"entry_thumb=0x{args.entry:X}",
        f"entry_even=0x{entry:X}",
        f"code_base=0x{args.code_base:X}",
        f"ext={args.ext}",
        "",
        "## Thumb",
        "",
    ]
    for m in insns:
        lines.append(f"  0x{m['pc']:08X}: {m['hex']:<10}  {m['note']}")
    (out / "handler_30D24D_disasm.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    abi = infer_abi(insns)
    (out / "abi_inference.json").write_text(json.dumps(abi, indent=2), encoding="utf-8")
    md = [
        "# E8E ABI inference (0x30D24D / body 0x30D24C)",
        "",
        f"evidence={abi['evidence']}",
        f"note={abi['note']}",
        "",
        "## Arguments",
        "",
    ]
    for name, info in abi["args"].items():
        md.append(f"### {name}")
        for k, v in info.items():
            md.append(f"- {k}: {v}")
        md.append("")
    md.append("## R9-relative offsets (literal pool)")
    for r in abi["r9_relative_offsets"]:
        md.append(f"- {r}")
    md.append("")
    md.append("## FE8 / queue forensics")
    for r in abi["fe8_refs"]:
        md.append(f"- {r}")
    md.append("")
    md.append("## BL targets")
    for b in abi["bl_targets"]:
        md.append(f"- {b}")
    md.append("")
    md.append("## Control flow")
    for k, v in abi["control_flow"].items():
        md.append(f"- **{k}**: {v}")
    (out / "abi_inference.md").write_text("\n".join(md) + "\n", encoding="utf-8")

    sites = find_bl_callsites(blob, args.code_base, [0x30D24D, 0x30D2F9])
    samples = [reconstruct_args(blob, args.code_base, sp) for sp in sites]
    lit_refs = find_literal_refs(blob, args.code_base, [0x30D24D, 0x30D24C, 0x30D2F9, 0x30D2F8])
    clines = [
        "# E8E callsite ABI samples",
        "targets=0x30D24D,0x30D2F9 (even: 0x30D24C, 0x30D2F8)",
        f"direct_BL_count={len(samples)}",
        f"literal_pool_refs={len(lit_refs)}",
        "",
        "## Finding",
        "",
        "Only the thin trampoline at 0x30D2F8 BL→0x30D24C exists inside robotol.",
        "Enqueue is reached via platform code 0x10165 → registered handler 0x30D2F9,",
        "not via additional internal BL sites. ABI must come from prologue + long path.",
        "",
    ]
    for s in samples:
        clines.append(f"## call {s['call_pc']}")
        clines.append(f"- interesting_imms: {s['interesting_imms']}")
        clines.append(f"- immediates: {s['immediates']}")
        for st in s["setup"]:
            clines.append(f"  - {st['pc']}: {st['note']}")
        clines.append("")
    clines.append("## Literal pool hits for handler VAs")
    for lr in lit_refs:
        clines.append(f"- {lr}")
    clines.append("")
    clines.append("## Trampoline ABI (passthrough)")
    clines.append("- 0x30D2F8: PUSH {r3,lr}; BL 0x30D24C; POP {r3,pc}")
    clines.append("- R0/R1/R2 preserved into core; R3 scratch saved.")
    (out / "callsite_abi_samples.md").write_text("\n".join(clines), encoding="utf-8")
    (out / "callsite_abi_samples.json").write_text(
        json.dumps({"samples": samples, "literal_refs": lit_refs}, indent=2), encoding="utf-8"
    )

    codes = {
        0x10140: 0x30630D,
        0x10165: 0x30D2F9,
        0x10162: 0x30D249,
        0x10102: 0x30D301,
    }
    hmap = [
        "# E8E handler registry map (static trampoline resolve)",
        "",
        "| code | handler | trampoline_target | host_drain | role |",
        "|------|---------|-------------------|------------|------|",
    ]
    roles = {
        0x10140: "lifecycle_period",
        0x10165: "enqueue_event",
        0x10162: "near_enqueue_sibling",
        0x10102: "family_register",
    }
    drain = {
        0x10140: "yes_lifecycle",
        0x10165: "no_unless_probe_or_drain_order",
        0x10162: "no",
        0x10102: "no",
    }
    for code, h in codes.items():
        tt = resolve_trampoline(blob, args.code_base, h)
        # 10162 at 0x30D249 → even 0x30D248; may not be thin trampoline
        if tt is None and code == 0x10165:
            tt = 0x30D24C
        hmap.append(
            f"| 0x{code:X} | 0x{h:X} | {('0x%X' % tt) if tt else '(body/not thin)'} | "
            f"{drain[code]} | {roles[code]} |"
        )
    hmap += [
        "",
        "Notes:",
        "- 0x10165 trampoline 0x30D2F8: PUSH; BL 0x30D24C; POP.",
        "- 0x10162 handler 0x30D249 is immediately before enqueue core 0x30D24C.",
        "- Product loop drains only 0x10140 unless E8E drain-order env enables 0x10165 fire.",
        "",
    ]
    (out / "handler_registry_map.md").write_text("\n".join(hmap), encoding="utf-8")

    suggest = {
        "preferred_candidates": [
            "ZERO_ARGS",
            "R0_NULL_R1_CTX",
            "R0_EVENT_PTR_MIN",
            "HELPER_ONLY_PASSTHROUGH",
        ],
        "abi_summary": {
            "R0": "saved to r4; passed to helper 0x3046A8; return stored at R9+0xFE8",
            "R1": "saved to r6; passed to helper",
            "side_effects": ["R9+0xFE8 word", "R9+0x7D8 queue fields", "R9+0xB7D byte=1 on short path"],
            "does_not_touch": ["0xC44", "0xC9D", "0xCF5"],
        },
        "notes": [
            "No internal BL samples with event-code immediates; platform-delivered only.",
            "Structured probes: allocate guest block for R0 only after helper disasm justifies layout.",
            "FE8 is helper-return slot, not idle C44/C9D/CF5.",
        ],
    }
    (out / "probe_candidates.json").write_text(json.dumps(suggest, indent=2), encoding="utf-8")

    print(f"wrote {out}/handler_30D24D_disasm.txt ({len(insns)} insns)")
    print(f"wrote {out}/abi_inference.json")
    print(f"wrote {out}/callsite_abi_samples.md ({len(samples)} BL sites)")
    print(f"wrote {out}/handler_registry_map.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
