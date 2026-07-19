#!/usr/bin/env python3
"""Stage E8R: C44 unlock writer 0x2FC8C0 caller provenance."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CODE_BASE = 0x2D8DF4
FN_UNLOCK = 0x2FC8C0
SITE_STRB = 0x2FC8CE
UI_LO, UI_HI = 0x2E4840, 0x2E4B06
KNOWN_CALLERS = [0x2DB9DC, 0x30DDE2]


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


def find_bl_callers(blob: bytes, code_base: int, target: int) -> List[int]:
    out: List[int] = []
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(code_base + o, u16(blob, o), u16(blob, o + 2))
        if t == (target & ~1):
            out.append(code_base + o)
    return out


def find_fn_start(blob: bytes, code_base: int, site: int) -> int:
    for back in range(0, 0x1200, 2):
        p = site - back
        if p < code_base:
            break
        h = u16(blob, p - code_base)
        if (h & 0xFF00) == 0xB500 or h == 0xE92D:
            return p
    return site


def find_fn_end(blob: bytes, code_base: int, start: int, limit: int = 0x800) -> int:
    pc = start
    end = start + limit
    while pc < end and pc - code_base + 1 < len(blob):
        size, text, meta = decode_insn(blob, code_base, pc)
        if text.startswith("POP") and "lr" in text.lower() or (
            text.startswith("POP") and (u16(blob, pc - code_base) & 0x0100)
        ):
            # Thumb POP with PC bit
            h = u16(blob, pc - code_base)
            if (h & 0xFE00) == 0xBC00 and (h & 0x0100):
                return pc + size
        if text == "BX lr":
            return pc + size
        pc += size
    return min(start + 0x80, code_base + len(blob))


def pc_rel_lit(pc: int, imm8: int) -> int:
    return ((pc + 4) & ~2) + (imm8 << 2)


def decode_insn(blob: bytes, code_base: int, pc: int) -> Tuple[int, str, Dict[str, Any]]:
    off = pc - code_base
    if off < 0 or off + 1 >= len(blob):
        return 2, "???", {}
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {}
    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        t = bl_target(pc, h0, h1)
        if t is not None:
            meta["bl"] = t
            return 4, f"BL 0x{t:X}", meta
        return 4, f"t2 0x{h0:04X}_{h1:04X}", meta
    if (h0 & 0xFF00) == 0xB500:
        return 2, f"PUSH {{...,lr}} 0x{h0:04X}", meta
    if (h0 & 0xFE00) == 0xBC00:
        meta["pop_pc"] = bool(h0 & 0x0100)
        return 2, f"POP 0x{h0:04X}", meta
    if h0 == 0x4770:
        return 2, "BX lr", meta
    if (h0 & 0xFF00) == 0xDF00:
        return 2, f"SVC #0x{h0 & 0xFF:X}", meta
    if (h0 & 0xF800) == 0x2000:
        return 2, f"MOVS r{(h0 >> 8) & 7}, #{h0 & 0xFF}", {"movs": h0 & 0xFF, "rd": (h0 >> 8) & 7}
    if (h0 & 0xF800) == 0x2800:
        return 2, f"CMP r{(h0 >> 8) & 7}, #{h0 & 0xFF}", {"cmp": True, "rn": (h0 >> 8) & 7}
    if (h0 & 0xF800) == 0xE000:
        imm = h0 & 0x7FF
        if imm >= 0x400:
            imm -= 0x800
        tgt = (pc + 4 + imm * 2) & ~1
        return 2, f"B 0x{tgt:X}", {"b": tgt}
    if (h0 & 0xF000) == 0xD000 and (h0 & 0xF00) != 0xF00:
        imm = h0 & 0xFF
        if imm >= 0x80:
            imm -= 0x100
        tgt = (pc + 4 + imm * 2) & ~1
        return 2, f"Bcond({(h0 >> 8) & 0xF}) 0x{tgt:X}", {"btgt": tgt, "cond": (h0 >> 8) & 0xF}
    if (h0 & 0xF800) == 0x4800:
        rd, imm8 = (h0 >> 8) & 7, h0 & 0xFF
        lit = pc_rel_lit(pc, imm8)
        val = u32(blob, lit - code_base) if 0 <= lit - code_base + 3 < len(blob) else None
        meta = {"ldr_pc": True, "rd": rd, "lit_val": val}
        return 2, f"LDR r{rd}, [pc] ; =0x{val:X}" if val is not None else f"LDR r{rd}, [pc]", meta
    if (h0 & 0xFF00) == 0x4400:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"ADD r{rd}, r{rm}", {"add_reg": True, "rd": rd, "rm": rm}
    if (h0 & 0xF800) == 0x6800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"LDR r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x6000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"STR r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x7000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        return 2, f"STRB r{rt}, [r{rn}, #0x{imm:X}]", {"strb": True, "rt": rt}
    if (h0 & 0xF800) == 0x7800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        return 2, f"LDRB r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xFFC0) == 0x1C00:
        return 2, f"MOVS r{h0 & 7}, r{(h0 >> 3) & 7}", meta
    if (h0 & 0xF800) == 0x3000:
        return 2, f"ADDS r{(h0 >> 8) & 7}, #{h0 & 0xFF}", meta
    if (h0 & 0xFF80) == 0xB080:
        return 2, f"SUB sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xFF80) == 0xB000:
        return 2, f"ADD sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xF800) == 0x9800:
        return 2, f"LDR r{(h0 >> 8) & 7}, [sp, #0x{(h0 & 0xFF) << 2:X}]", meta
    if (h0 & 0xF800) == 0x9000:
        return 2, f"STR r{(h0 >> 8) & 7}, [sp, #0x{(h0 & 0xFF) << 2:X}]", meta
    return 2, f"raw 0x{h0:04X}", meta


def disasm_range(blob: bytes, code_base: int, start: int, end: int) -> List[str]:
    lines: List[str] = []
    pc = start
    while pc < end:
        size, text, _ = decode_insn(blob, code_base, pc)
        lines.append(f"0x{pc:X}: {text}")
        pc += size
    return lines


def scan_r9_lits(blob: bytes, code_base: int, start: int, end: int) -> List[int]:
    offs: List[int] = []
    pc = start
    while pc < end:
        size, text, meta = decode_insn(blob, code_base, pc)
        if meta.get("ldr_pc") and meta.get("lit_val") is not None:
            v = meta["lit_val"]
            # look ahead for ADD rd, r9
            p2 = pc + size
            for _ in range(6):
                if p2 >= end:
                    break
                s2, t2, m2 = decode_insn(blob, code_base, p2)
                if m2.get("add_reg") and m2.get("rm") == 9 and m2.get("rd") == meta.get("rd"):
                    offs.append(v)
                    break
                p2 += s2
        pc += size
    return sorted(set(offs))


def classify_caller(pc: int, r9_offs: List[int], bls: List[int]) -> str:
    if UI_LO <= pc <= UI_HI + 0x40:
        return "APP_INIT_UI_CLUSTER"
    if 0x304000 <= pc <= 0x30F000:
        return "PLATFORM_SIDE_EFFECT_UI_INIT"
    if any(o in (0xFE8, 0xB7D, 0x844) for o in r9_offs):
        return "QUEUE_CONSUMER_UI_INIT"
    if any(b in (0x304558, 0x1E213) or (0x1E200 <= (b & 0xFFFFF) <= 0x1E300) for b in bls):
        return "PLATFORM_SIDE_EFFECT_UI_INIT"
    if any(o in (0xE6C, 0xC6C, 0xEEC, 0x11D0) for o in r9_offs):
        return "LOADER_INIT_UI_CLUSTER"
    return "EVENT_SWITCH_CASE_UI_INIT"


def analyze_caller(blob: bytes, code_base: int, bl_pc: int) -> Dict[str, Any]:
    fn = find_fn_start(blob, code_base, bl_pc)
    # previous ~40 instructions (~80 bytes) + a bit after
    ctx_lo = max(fn, bl_pc - 0x50)
    ctx_hi = bl_pc + 0x24
    lines = disasm_range(blob, code_base, ctx_lo, ctx_hi)
    fn_lines = disasm_range(blob, code_base, fn, min(fn + 0x120, bl_pc + 0x40))
    bls = []
    preds = []
    pc = ctx_lo
    while pc < ctx_hi:
        size, text, meta = decode_insn(blob, code_base, pc)
        if meta.get("bl") is not None:
            bls.append(meta["bl"])
        if meta.get("btgt") is not None or meta.get("b") is not None:
            preds.append({"pc": f"0x{pc:X}", "text": text})
        pc += size
    r9_offs = scan_r9_lits(blob, code_base, fn, bl_pc + 4)
    upstream = find_bl_callers(blob, code_base, fn)
    # R0-R3 setup: scan last MOVS/LDR before BL
    setup: List[str] = []
    pc = max(fn, bl_pc - 0x30)
    while pc < bl_pc:
        size, text, meta = decode_insn(blob, code_base, pc)
        if any(
            text.startswith(p)
            for p in ("MOVS r0", "MOVS r1", "MOVS r2", "MOVS r3", "LDR r0", "LDR r1", "LDR r2", "LDR r3", "ADD r0", "ADD r1")
        ):
            setup.append(f"0x{pc:X}: {text}")
        pc += size
    return {
        "bl_pc": f"0x{bl_pc:X}",
        "fn": f"0x{fn:X}",
        "class": classify_caller(bl_pc, r9_offs, bls),
        "r9_offsets": [f"0x{o:X}" for o in r9_offs],
        "bl_targets_nearby": [f"0x{b:X}" for b in bls],
        "branch_preds": preds,
        "r0_r3_setup": setup[-12:],
        "upstream_callers": [f"0x{c:X}" for c in upstream[:16]],
        "n_upstream": len(upstream),
        "context": lines,
        "fn_head": fn_lines[:40],
        "in_ui_cluster": UI_LO <= bl_pc <= UI_HI + 0x40,
        "depends_state_8d0": any(o == 0x8D0 for o in r9_offs),
        "depends_c6c": any(o == 0xC6C for o in r9_offs),
        "depends_dec": any(o == 0xDEC for o in r9_offs),
        "depends_1a8": any(o == 0x1A8 for o in r9_offs),
        "depends_e6c": any(o == 0xE6C for o in r9_offs),
        "depends_queue": any(o in (0xFE8, 0xB7D, 0x844) for o in r9_offs),
    }


def analyze_unlock_fn(blob: bytes, code_base: int) -> Dict[str, Any]:
    fn = find_fn_start(blob, code_base, FN_UNLOCK)
    # Prefer exact FN_UNLOCK if it's a PUSH
    if u16(blob, FN_UNLOCK - code_base) & 0xFF00 == 0xB500:
        fn = FN_UNLOCK
    end = FN_UNLOCK + 0x20
    lines = disasm_range(blob, code_base, fn, end)
    bls = []
    r9w = []
    pc = fn
    while pc < end:
        size, text, meta = decode_insn(blob, code_base, pc)
        if meta.get("bl"):
            bls.append({"pc": f"0x{pc:X}", "target": f"0x{meta['bl']:X}"})
        if meta.get("ldr_pc") and meta.get("lit_val") is not None:
            r9w.append(meta["lit_val"])
        pc += size
    return {
        "fn": f"0x{fn:X}",
        "strb_site": f"0x{SITE_STRB:X}",
        "disasm": lines,
        "bl_targets": bls,
        "r9_lits": [f"0x{o:X}" for o in r9w],
        "precondition": "BL 0x3046A8 then STR ret@C44+4; STRB#1@C44; clear C44+8/+C",
        "entry_args": "R0-R3 unused before first BL (uses R9 only); R0 overwritten by 0x3046A8 ret",
        "safe_for_fast_call": True,
        "note": "No unsafe pointer arg; depends on R9 + helper 0x3046A8",
    }


def analyze_helper_3046a8(blob: bytes, code_base: int) -> Dict[str, Any]:
    fn = 0x3046A8
    lines = disasm_range(blob, code_base, fn, fn + 0x80)
    bls = [ln for ln in lines if ": BL " in ln]
    return {"fn": "0x3046A8", "disasm": lines, "bls": bls, "n_callers": len(find_bl_callers(blob, code_base, fn))}


def analyze_ui_cluster(blob: bytes, code_base: int, unlock_bls: List[int]) -> Dict[str, Any]:
    cluster_bls = [c for c in unlock_bls if UI_LO <= c <= UI_HI + 0x80]
    # all BLs inside UI range
    all_bls: List[Dict[str, str]] = []
    pc = UI_LO
    while pc <= UI_HI + 0x40 and pc - code_base + 3 < len(blob):
        size, text, meta = decode_insn(blob, code_base, pc)
        if meta.get("bl") is not None:
            all_bls.append({"pc": f"0x{pc:X}", "target": f"0x{meta['bl']:X}"})
        pc += size
    fns = sorted({find_fn_start(blob, code_base, c) for c in cluster_bls})
    return {
        "range": f"0x{UI_LO:X}..0x{UI_HI:X}",
        "unlock_bl_sites": [f"0x{c:X}" for c in cluster_bls],
        "fn_starts": [f"0x{f:X}" for f in fns],
        "all_bls_in_cluster": all_bls[:80],
        "n_bls": len(all_bls),
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", type=Path, required=True)
    ap.add_argument("--out-dir", type=Path, required=True)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=CODE_BASE)
    args = ap.parse_args()
    blob = args.ext.read_bytes()
    cb = args.code_base
    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)

    unlock = analyze_unlock_fn(blob, cb)
    helper = analyze_helper_3046a8(blob, cb)
    bl_sites = find_bl_callers(blob, cb, FN_UNLOCK)
    callers = [analyze_caller(blob, cb, c) for c in bl_sites]
    ui = analyze_ui_cluster(blob, cb, bl_sites)

    # upstream of each unlock caller fn
    for c in callers:
        fn = int(c["fn"], 16)
        ups = []
        for u in find_bl_callers(blob, cb, fn)[:8]:
            ufn = find_fn_start(blob, cb, u)
            ups.append({"bl": f"0x{u:X}", "fn": f"0x{ufn:X}"})
        c["upstream_detail"] = ups

    report = {
        "unlock_fn": unlock,
        "helper_3046a8": helper,
        "bl_sites": [f"0x{c:X}" for c in bl_sites],
        "callers": callers,
        "ui_cluster": ui,
        "bp_spec_sites": sorted(
            set(
                [FN_UNLOCK, SITE_STRB, 0x2DB9DC, 0x30DDE2, 0x3046A8]
                + [int(c["bl_pc"], 16) for c in callers]
                + [int(c["fn"], 16) for c in callers]
                + [int(x["fn"], 16) for c in callers for x in c.get("upstream_detail", [])][:20]
            )
        ),
    }
    (out / "e8r_deps.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    md: List[str] = [
        "# E8R: C44 unlock caller provenance",
        "",
        "## 0x2FC8C0 unlock function",
        "",
        f"- Entry: `{unlock['fn']}` STRB site `{unlock['strb_site']}`",
        f"- Args: {unlock['entry_args']}",
        f"- Precondition: {unlock['precondition']}",
        f"- FAST-callable: {unlock['safe_for_fast_call']} ({unlock['note']})",
        "",
        "```",
        *unlock["disasm"],
        "```",
        "",
        "## Helper 0x3046A8 (called before C44=1)",
        "",
        f"- Callers of helper: {helper['n_callers']}",
        "",
        "```",
        *helper["disasm"][:40],
        "```",
        "",
        f"## BL sites into 0x2FC8C0 ({len(bl_sites)})",
        "",
    ]
    for c in callers:
        md += [
            f"### `{c['bl_pc']}` fn=`{c['fn']}` class=`{c['class']}`",
            "",
            f"- R9 offs: {c['r9_offsets']}",
            f"- Nearby BLs: {c['bl_targets_nearby']}",
            f"- Upstream n={c['n_upstream']}: {c['upstream_callers'][:10]}",
            f"- deps: state8D0={c['depends_state_8d0']} C6C={c['depends_c6c']} DEC={c['depends_dec']} "
            f"1A8={c['depends_1a8']} E6C={c['depends_e6c']} queue={c['depends_queue']}",
            f"- R0-R3 setup:",
            "",
        ]
        for s in c["r0_r3_setup"]:
            md.append(f"  - `{s}`")
        md += ["", "```", *c["context"], "```", ""]

    md += [
        "## UI-init cluster",
        "",
        f"- Range: `{ui['range']}`",
        f"- Unlock BL sites: {ui['unlock_bl_sites']}",
        f"- Fn starts: {ui['fn_starts']}",
        f"- BLs in cluster (n={ui['n_bls']}):",
        "",
    ]
    for b in ui["all_bls_in_cluster"][:40]:
        md.append(f"- `{b['pc']}` → `{b['target']}`")

    md += [
        "",
        "## Suggested BP list",
        "",
        ", ".join(f"`0x{p:X}`" for p in report["bp_spec_sites"][:40]),
        "",
    ]
    (out / "e8r_deps.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print(f"wrote {out / 'e8r_deps.md'}")
    print(f"unlock_bls={len(bl_sites)} ui={ui['unlock_bl_sites']}")
    print("classes=" + ",".join(sorted({c['class'] for c in callers})))


if __name__ == "__main__":
    main()
