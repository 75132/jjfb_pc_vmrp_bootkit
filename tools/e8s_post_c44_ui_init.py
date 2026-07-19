#!/usr/bin/env python3
"""Stage E8S: post-C44 gate (C9D/CF5) + UI-init 0x2E4788 natural entry."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CODE_BASE = 0x2D8DF4
FN_UI = 0x2E4788
FN_UNLOCK = 0x2FC8C0
UI_UPSTREAM = [0x2E2F50, 0x2E39BE, 0x2E39C8, 0x2E39D2, 0x2E39DC, 0x2E3BB2, 0x2E3F7C]
GATE_PCS = [0x3066AD, 0x3066BD, 0x306745]


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
    for back in range(0, 0x1400, 2):
        p = site - back
        if p < code_base:
            break
        h = u16(blob, p - code_base)
        if (h & 0xFF00) == 0xB500 or h == 0xE92D:
            return p
    return site


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
        return 2, f"POP 0x{h0:04X}", {"pop_pc": bool(h0 & 0x0100)}
    if h0 == 0x4770:
        return 2, "BX lr", meta
    if (h0 & 0xF800) == 0x2000:
        return 2, f"MOVS r{(h0 >> 8) & 7}, #{h0 & 0xFF}", {"movs": h0 & 0xFF, "rd": (h0 >> 8) & 7}
    if (h0 & 0xF800) == 0x2800:
        return 2, f"CMP r{(h0 >> 8) & 7}, #{h0 & 0xFF}", {"cmp": True}
    if (h0 & 0xF800) == 0xE000:
        imm = h0 & 0x7FF
        if imm >= 0x400:
            imm -= 0x800
        return 2, f"B 0x{(pc + 4 + imm * 2) & ~1:X}", {"b": (pc + 4 + imm * 2) & ~1}
    if (h0 & 0xF000) == 0xD000 and (h0 & 0xF00) != 0xF00:
        imm = h0 & 0xFF
        if imm >= 0x80:
            imm -= 0x100
        return 2, f"Bcond({(h0 >> 8) & 0xF}) 0x{(pc + 4 + imm * 2) & ~1:X}", {
            "btgt": (pc + 4 + imm * 2) & ~1
        }
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
    if (h0 & 0xF800) == 0x7000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        return 2, f"STRB r{rt}, [r{rn}, #0x{imm:X}]", {"strb": True, "rt": rt, "rn": rn, "imm": imm}
    if (h0 & 0xF800) == 0x6000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"STR r{rt}, [r{rn}, #0x{imm:X}]", {"str": True, "rt": rt, "imm": imm}
    if (h0 & 0xF800) == 0x6800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"LDR r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x7800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        return 2, f"LDRB r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xFF80) == 0xB080:
        return 2, f"SUB sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xFF80) == 0xB000:
        return 2, f"ADD sp, #0x{(h0 & 0x7F) << 2:X}", meta
    return 2, f"raw 0x{h0:04X}", meta


def disasm_range(blob: bytes, code_base: int, start: int, end: int) -> List[str]:
    lines: List[str] = []
    pc = start
    while pc < end:
        size, text, _ = decode_insn(blob, code_base, pc)
        lines.append(f"0x{pc:X}: {text}")
        pc += size
    return lines


def classify_flag_stores(blob: bytes, code_base: int, off: int) -> List[Dict[str, Any]]:
    """Find STRB/STR to R9+off via LDR lit + ADD r9."""
    lit_vas = {code_base + o for o in range(0, len(blob) - 3, 4) if u32(blob, o) == off}
    out: List[Dict[str, Any]] = []
    for i in range(0, len(blob) - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + i
        lit = pc_rel_lit(pc, h & 0xFF)
        if lit not in lit_vas:
            continue
        rd = (h >> 8) & 7
        j = i + 2
        saw_add = False
        for _ in range(24):
            if j + 1 >= len(blob):
                break
            pc2 = code_base + j
            size, text, meta = decode_insn(blob, code_base, pc2)
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                saw_add = True
            if saw_add and (meta.get("strb") or meta.get("str")):
                # only stores with imm 0 to the flag byte base (or STRB #0)
                imm = meta.get("imm", -1)
                if meta.get("strb") and imm not in (0, None):
                    # might be off+imm if lit was base-imm; for exact lit==off require imm0
                    if imm != 0:
                        j += size
                        continue
                src, val = "unknown", None
                for back in range(2, 40, 2):
                    p = pc2 - back
                    if p < code_base:
                        break
                    _, t2, m2 = decode_insn(blob, code_base, p)
                    if m2.get("movs") is not None and m2.get("rd") == meta.get("rt"):
                        src = f"MOVS#{m2['movs']}"
                        val = m2["movs"]
                        break
                kind = "write_0" if val == 0 else ("write_1" if val == 1 else "write_other_or_reg")
                fn = find_fn_start(blob, code_base, pc2)
                callers = find_bl_callers(blob, code_base, fn)
                out.append(
                    {
                        "store_pc": f"0x{pc2:X}",
                        "fn": f"0x{fn:X}",
                        "src": src,
                        "kind": kind,
                        "text": text,
                        "callers": [f"0x{c:X}" for c in callers[:12]],
                        "n_callers": len(callers),
                        "in_ui_init": FN_UI <= pc2 <= FN_UI + 0x400,
                        "near_unlock": abs(pc2 - FN_UNLOCK) < 0x200,
                    }
                )
                break
            j += size
    # dedupe by store_pc
    seen = set()
    uniq = []
    for x in out:
        if x["store_pc"] in seen:
            continue
        seen.add(x["store_pc"])
        uniq.append(x)
    return uniq


def scan_r9_lits(blob: bytes, code_base: int, start: int, end: int) -> List[int]:
    offs: List[int] = []
    pc = start
    while pc < end:
        size, _, meta = decode_insn(blob, code_base, pc)
        if meta.get("ldr_pc") and meta.get("lit_val") is not None:
            v = meta["lit_val"]
            p2 = pc + size
            for _ in range(6):
                if p2 >= end:
                    break
                s2, _, m2 = decode_insn(blob, code_base, p2)
                if m2.get("add_reg") and m2.get("rm") == 9 and m2.get("rd") == meta.get("rd"):
                    offs.append(v)
                    break
                p2 += s2
        pc += size
    return sorted(set(offs))


def analyze_ui_init(blob: bytes, code_base: int) -> Dict[str, Any]:
    fn = FN_UI
    # find end roughly at next PUSH after unlock BLs
    end = fn + 0x400
    lines = disasm_range(blob, code_base, fn, min(fn + 0x200, end))
    bls = []
    preds = []
    pc = fn
    while pc < fn + 0x400 and pc - code_base + 1 < len(blob):
        size, text, meta = decode_insn(blob, code_base, pc)
        if meta.get("bl") is not None:
            bls.append({"pc": f"0x{pc:X}", "target": f"0x{meta['bl']:X}"})
        if meta.get("btgt") or meta.get("b"):
            preds.append({"pc": f"0x{pc:X}", "text": text})
        if meta.get("pop_pc") and pc > fn + 0x40:
            end = pc + size
            break
        pc += size
    r9 = scan_r9_lits(blob, code_base, fn, end)
    unlock_bls = [b for b in bls if int(b["target"], 16) == FN_UNLOCK]
    upstream = find_bl_callers(blob, code_base, fn)
    up_detail = []
    for u in upstream[:16]:
        ufn = find_fn_start(blob, code_base, u)
        up_detail.append(
            {
                "bl": f"0x{u:X}",
                "fn": f"0x{ufn:X}",
                "r9": [f"0x{o:X}" for o in scan_r9_lits(blob, code_base, ufn, u + 4)[:20]],
            }
        )
    # upstream of upstream sample
    deep = []
    for u in UI_UPSTREAM:
        callers = find_bl_callers(blob, code_base, u)
        deep.append(
            {
                "pc": f"0x{u:X}",
                "fn": f"0x{find_fn_start(blob, code_base, u):X}",
                "n_callers": len(callers),
                "callers": [f"0x{c:X}" for c in callers[:10]],
            }
        )
    return {
        "fn": f"0x{fn:X}",
        "end": f"0x{end:X}",
        "r9_offsets": [f"0x{o:X}" for o in r9],
        "depends_8d0": 0x8D0 in r9,
        "depends_c6c": 0xC6C in r9,
        "depends_dec": 0xDEC in r9,
        "depends_e6c": 0xE6C in r9,
        "depends_1a8": 0x1A8 in r9,
        "depends_queue": any(o in (0xFE8, 0xB7D, 0x844) for o in r9),
        "depends_c9d": 0xC9D in r9,
        "depends_cf5": 0xCF5 in r9,
        "bls": bls[:60],
        "unlock_bls": unlock_bls,
        "branch_preds_sample": preds[:30],
        "upstream": up_detail,
        "n_upstream": len(upstream),
        "upstream_nodes": deep,
        "disasm_head": lines,
        "safe_for_fast_call": True,  # R9-heavy; args may be needed — runtime will reveal
        "note": "UI-init; multiple BL 0x2FC8C0; reads state 0x8D0",
    }


def analyze_gate_region(blob: bytes, code_base: int) -> Dict[str, Any]:
    out = {}
    for pc in GATE_PCS:
        if pc < code_base or pc - code_base >= len(blob):
            out[f"0x{pc:X}"] = {"present": False}
            continue
        fn = find_fn_start(blob, code_base, pc)
        out[f"0x{pc:X}"] = {
            "present": True,
            "fn": f"0x{fn:X}",
            "disasm": disasm_range(blob, code_base, pc - 0x10, pc + 0x40),
            "r9": [f"0x{o:X}" for o in scan_r9_lits(blob, code_base, fn, fn + 0x120)],
        }
    return out


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

    c9d = classify_flag_stores(blob, cb, 0xC9D)
    cf5 = classify_flag_stores(blob, cb, 0xCF5)
    ui = analyze_ui_init(blob, cb)
    gate = analyze_gate_region(blob, cb)

    report = {
        "c9d_stores": c9d,
        "cf5_stores": cf5,
        "c9d_write1": [x for x in c9d if x["kind"] == "write_1"],
        "cf5_write1": [x for x in cf5 if x["kind"] == "write_1"],
        "ui_init": ui,
        "gate_region": gate,
        "bp_sites": sorted(
            set(
                [FN_UI, FN_UNLOCK, 0x2FC8CE, 0x2F4E82]
                + UI_UPSTREAM
                + GATE_PCS
                + [int(x["store_pc"], 16) for x in c9d if x["kind"] == "write_1"]
                + [int(x["store_pc"], 16) for x in cf5 if x["kind"] == "write_1"]
                + [int(x["fn"], 16) for x in c9d if x["kind"] == "write_1"]
                + [int(x["fn"], 16) for x in cf5 if x["kind"] == "write_1"]
            )
        ),
    }
    (out / "e8s_deps.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    md: List[str] = [
        "# E8S: post-C44 gate + UI-init natural entry",
        "",
        "## C9D stores (R9+0xC9D)",
        "",
        "| store_pc | kind | src | fn | n_callers |",
        "| --- | --- | --- | --- | --- |",
    ]
    for x in c9d:
        md.append(
            f"| `{x['store_pc']}` | `{x['kind']}` | `{x['src']}` | `{x['fn']}` | {x['n_callers']} |"
        )
    md += ["", "### C9D write_1", ""]
    for x in report["c9d_write1"]:
        md.append(f"- `{x['store_pc']}` fn=`{x['fn']}` callers={x['callers'][:8]}")

    md += [
        "",
        "## CF5 stores (R9+0xCF5)",
        "",
        "| store_pc | kind | src | fn | n_callers |",
        "| --- | --- | --- | --- | --- |",
    ]
    for x in cf5:
        md.append(
            f"| `{x['store_pc']}` | `{x['kind']}` | `{x['src']}` | `{x['fn']}` | {x['n_callers']} |"
        )
    md += ["", "### CF5 write_1", ""]
    for x in report["cf5_write1"]:
        md.append(f"- `{x['store_pc']}` fn=`{x['fn']}` callers={x['callers'][:8]}")

    md += [
        "",
        f"## UI-init `0x{FN_UI:X}`",
        "",
        f"- end=`{ui['end']}` upstream_n={ui['n_upstream']}",
        f"- R9 offs: {ui['r9_offsets']}",
        f"- deps: 8D0={ui['depends_8d0']} C6C={ui['depends_c6c']} DEC={ui['depends_dec']} "
        f"E6C={ui['depends_e6c']} 1A8={ui['depends_1a8']} queue={ui['depends_queue']} "
        f"C9D={ui['depends_c9d']} CF5={ui['depends_cf5']}",
        f"- unlock BLs: {ui['unlock_bls']}",
        "",
        "### Upstream BLs into UI-init",
        "",
    ]
    for u in ui["upstream"]:
        md.append(f"- `{u['bl']}` fn=`{u['fn']}` r9={u['r9'][:10]}")
    md += ["", "### Upstream nodes", ""]
    for d in ui["upstream_nodes"]:
        md.append(f"- `{d['pc']}` fn=`{d['fn']}` callers({d['n_callers']}): {d['callers']}")
    md += ["", "```", *ui["disasm_head"][:50], "```", "", "## Gate region 0x3066xx", ""]
    for k, v in gate.items():
        md.append(f"### {k} present={v.get('present')} fn={v.get('fn')}")
        if v.get("disasm"):
            md += ["```", *v["disasm"][:20], "```", ""]

    (out / "e8s_deps.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print(f"wrote {out / 'e8s_deps.md'}")
    print(
        f"c9d_w1={len(report['c9d_write1'])} cf5_w1={len(report['cf5_write1'])} "
        f"ui_ups={ui['n_upstream']} unlock_bls={len(ui['unlock_bls'])}"
    )


if __name__ == "__main__":
    main()
