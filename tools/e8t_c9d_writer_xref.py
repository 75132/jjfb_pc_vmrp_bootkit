#!/usr/bin/env python3
"""Stage E8T Lane A: complete xref for writes covering R9+0xC9D."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CODE_BASE = 0x2D8DF4
C9D = 0xC9D


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


def find_fn_start(blob: bytes, site: int) -> int:
    for back in range(0, 0x1800, 2):
        p = site - back
        if p < CODE_BASE:
            break
        h = u16(blob, p - CODE_BASE)
        if (h & 0xFF00) == 0xB500 or h == 0xE92D:
            return p
    return site


def find_bl_callers(blob: bytes, target: int) -> List[int]:
    out: List[int] = []
    tgt = target & ~1
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(CODE_BASE + o, u16(blob, o), u16(blob, o + 2))
        if t == tgt:
            out.append(CODE_BASE + o)
    return out


def lit_refs(blob: bytes) -> List[Tuple[int, int, int]]:
    """Return (pc, rd, lit_val) for LDR rn,[pc,#imm] with interesting lits."""
    hits = []
    for i in range(0, len(blob) - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = CODE_BASE + i
        lit = ((pc + 4) & ~2) + ((h & 0xFF) << 2)
        if not (0 <= lit - CODE_BASE + 3 < len(blob)):
            continue
        L = u32(blob, lit - CODE_BASE)
        if 0xC70 <= L <= 0xCD0 or L == C9D:
            hits.append((pc, (h >> 8) & 7, L))
    return hits


def value_source(blob: bytes, store_pc: int, rt: int) -> str:
    for back in range(2, 64, 2):
        p = store_pc - back
        if p < CODE_BASE:
            break
        hh = u16(blob, p - CODE_BASE)
        if (hh & 0xF800) == 0x2000 and ((hh >> 8) & 7) == rt:
            return f"MOVS#{hh & 0xFF}"
        if (hh & 0xF800) == 0x0000 and (hh & 7) == rt:
            # LSLS/ADDS etc — keep generic
            pass
    return f"r{rt}"


def classify_src(src: str) -> str:
    if src.startswith("MOVS#0"):
        return "writes_0"
    if src.startswith("MOVS#1"):
        return "writes_1"
    if src.startswith("MOVS#"):
        imm = int(src.split("#", 1)[1])
        if imm == 0:
            return "writes_0"
        return "writes_variable_nonzero" if imm != 1 else "writes_1"
    return "writes_variable_nonzero"


def scan_stores_from_lit(blob: bytes) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    for pc_lit, rd, L in lit_refs(blob):
        j = (pc_lit + 2) - CODE_BASE
        base_reg: Optional[int] = None
        base_off = L
        for _ in range(56):
            if j + 1 >= len(blob):
                break
            pc2 = CODE_BASE + j
            h2 = u16(blob, j)
            if (h2 & 0xFF00) == 0x4400:
                rdd = (((h2 >> 7) & 1) << 3) | (h2 & 7)
                rm = (h2 >> 3) & 0xF
                if rm == 9 and rdd == rd:
                    base_reg = rd
            if base_reg is not None and (h2 & 0xF800) == 0x3000 and ((h2 >> 8) & 7) == base_reg:
                base_off = L + (h2 & 0xFF)
            if base_reg is not None and (h2 & 0xFE00) == 0x1C00:
                rdn = h2 & 7
                rn = (h2 >> 3) & 7
                imm = (h2 >> 6) & 7
                if rdn == base_reg and rn == base_reg:
                    base_off = L + imm
            if base_reg is not None and (h2 & 0xF800) == 0x7000:
                rt, rn, imm = h2 & 7, (h2 >> 3) & 7, (h2 >> 6) & 0x1F
                if rn == base_reg and base_off + imm == C9D:
                    src = value_source(blob, pc2, rt)
                    out.append(
                        {
                            "kind": "STRB",
                            "pc": f"0x{pc2:X}",
                            "insn": f"STRB r{rt},[r{rn},#0x{imm:X}]",
                            "lit_base": f"0x{L:X}",
                            "effective": f"0x{C9D:X}",
                            "value_source": src,
                            "class": classify_src(src),
                        }
                    )
            if base_reg is not None and (h2 & 0xF800) == 0x8000:
                rt, rn, imm = h2 & 7, (h2 >> 3) & 7, ((h2 >> 6) & 0x1F) * 2
                if rn == base_reg and base_off + imm <= C9D <= base_off + imm + 1:
                    src = value_source(blob, pc2, rt)
                    out.append(
                        {
                            "kind": "STRH",
                            "pc": f"0x{pc2:X}",
                            "insn": f"STRH r{rt},[r{rn},#0x{imm:X}]",
                            "lit_base": f"0x{L:X}",
                            "effective_range": [f"0x{base_off+imm:X}", f"0x{base_off+imm+1:X}"],
                            "value_source": src,
                            "class": classify_src(src),
                            "note": "covers C9D only if store value byte1 nonzero (LE)",
                        }
                    )
            if base_reg is not None and (h2 & 0xF800) == 0x6000:
                rt, rn, imm = h2 & 7, (h2 >> 3) & 7, ((h2 >> 6) & 0x1F) * 4
                if rn == base_reg and base_off + imm <= C9D <= base_off + imm + 3:
                    src = value_source(blob, pc2, rt)
                    byte_idx = C9D - (base_off + imm)
                    out.append(
                        {
                            "kind": "STR",
                            "pc": f"0x{pc2:X}",
                            "insn": f"STR r{rt},[r{rn},#0x{imm:X}]",
                            "lit_base": f"0x{L:X}",
                            "c9d_byte_index": byte_idx,
                            "value_source": src,
                            "class": classify_src(src),
                            "note": f"word store; C9D is byte{byte_idx}; MOVS#1 only sets byte0",
                        }
                    )
            if base_reg is not None and (h2 & 0xFE00) == 0x5400:
                rm = (h2 >> 6) & 7
                rn = (h2 >> 3) & 7
                rt = h2 & 7
                if rn == base_reg:
                    out.append(
                        {
                            "kind": "STRB_regoff",
                            "pc": f"0x{pc2:X}",
                            "insn": f"STRB r{rt},[r{rn},r{rm}]",
                            "lit_base": f"0x{L:X}",
                            "value_source": value_source(blob, pc2, rt),
                            "class": "writes_variable_nonzero",
                            "note": "computed offset; verify dynamically",
                        }
                    )
            if (h2 & 0xF800) == 0xF000:
                j += 4
            else:
                j += 2
    return out


def annotate(blob: bytes, writers: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    for w in writers:
        pc = int(w["pc"], 16)
        fn = find_fn_start(blob, pc)
        cs = find_bl_callers(blob, fn)
        w["fn"] = f"0x{fn:X}"
        w["callers"] = [f"0x{c:X}" for c in cs[:12]]
        w["caller_count"] = len(cs)
        # crude path tags
        tags = []
        if fn == 0x2E4788 or any(0x2E4788 <= int(c, 16) <= 0x2E4C00 for c in w["callers"]):
            tags.append("ui_init_path")
        if any(int(c, 16) in (0x30AF8A, 0x30DF78) for c in w["callers"]) or fn == 0x30A9EC:
            tags.append("clear_reset_path")
        if w["class"] == "writes_0":
            tags.append("clears_reset")
        w["path_tags"] = tags
    return writers


def decode_ui_init_states(blob: bytes) -> Dict[str, Any]:
    """Lane C: accepted/rejected states for 0x2E4788."""
    fn = 0x2E4788
    end = min(fn + 0x400, CODE_BASE + len(blob))
    cmps: List[Dict[str, Any]] = []
    bls: List[str] = []
    reads: List[str] = []
    pc = fn
    while pc < end:
        off = pc - CODE_BASE
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
            h1 = u16(blob, off + 2)
            t = bl_target(pc, h0, h1)
            if t is not None:
                bls.append(f"0x{pc:X}->0x{t:X}")
                if t == 0x2FC8C0:
                    bls.append(f"UNLOCK_BL@{pc:X}")
            pc += 4
            continue
        if (h0 & 0xF800) == 0x4800:
            lit = ((pc + 4) & ~2) + ((h0 & 0xFF) << 2)
            if 0 <= lit - CODE_BASE + 3 < len(blob):
                L = u32(blob, lit - CODE_BASE)
                if L in (0x8D0, 0xC44, 0xC9D, 0xCF5, 0xCA3, 0xED8, 0xC9C):
                    reads.append(f"0x{pc:X}:LDR=0x{L:X}")
        if (h0 & 0xF800) == 0x2800:
            cmps.append({"pc": f"0x{pc:X}", "reg": (h0 >> 8) & 7, "imm": h0 & 0xFF})
        if (h0 & 0xFF00) == 0xBD00 and (h0 & 0x0100):
            break
        pc += 2

    # Known early-out cluster from prior E8S: rejects {38,46,69,252} and state 300 via SUB chain
    rejected = sorted({c["imm"] for c in cmps if c["imm"] in (20, 30, 37, 38, 46, 69, 252)})
    # Also scan SUBS #0xFF / #0x2D pattern near start (reject 300)
    reject_300 = False
    for a in range(fn, fn + 0x80, 2):
        h = u16(blob, a - CODE_BASE)
        if (h & 0xF800) == 0x3800 and (h & 0xFF) == 0xFF:
            h2 = u16(blob, a + 2 - CODE_BASE)
            if (h2 & 0xF800) == 0x3800 and (h2 & 0xFF) == 0x2D:
                reject_300 = True
    if reject_300:
        rejected = sorted(set(rejected) | {300})

    return {
        "fn": f"0x{fn:X}",
        "state_cmps": cmps,
        "rejected_states_observed": rejected,
        "reject_300_via_sub_chain": reject_300,
        "r9_reads": reads,
        "bls": [b for b in bls if "UNLOCK" in b or "2FC8C0" in b or "2E" in b][:40],
        "callers": [f"0x{c:X}" for c in find_bl_callers(blob, fn)],
        "ed8_gate": "CMP #0; BGT early-out — continue only if ED8<=0 (typically 0)",
        "ca3_gate": "CMP #1; BNE early-out — need CA3==1",
        "c8e_gate": "CMP #0; BNE early-out — need C8E==0",
        "note": (
            "UI-init rejects state {38,46,69,252,300} via BEQ early-out; "
            "ED8 must be 0 (BGT rejects ED8>0); CA3==1; C8E==0. "
            "FAST state=38 is in reject set — product needs different state at UI-init time"
        ),
    }


def adjacent_c9c_write1(blob: bytes) -> List[Dict[str, Any]]:
    """Historical misclassified C9C=#1 writers (do NOT set C9D)."""
    sites = [0x2E3A68, 0x2FB008]
    out = []
    for pc in sites:
        fn = find_fn_start(blob, pc)
        out.append(
            {
                "pc": f"0x{pc:X}",
                "fn": f"0x{fn:X}",
                "effective": "0xC9C",
                "class": "writes_1_ADJACENT_NOT_C9D",
                "callers": [f"0x{c:X}" for c in find_bl_callers(blob, fn)[:8]],
                "note": "STRB #1 @ C9C; gate reads C9D — does not unlock idle C9D",
            }
        )
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()
    blob = Path(args.ext).read_bytes()
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    writers = annotate(blob, scan_stores_from_lit(blob))
    # dedupe by pc+kind
    seen = set()
    uniq = []
    for w in writers:
        k = (w["pc"], w["kind"])
        if k in seen:
            continue
        seen.add(k)
        uniq.append(w)

    exact = [w for w in uniq if w["kind"] == "STRB" and w.get("effective") == "0xC9D"]
    # 0x3115BA: MOVS r7,#0 earlier in fn — reclassify as clear.
    for w in exact:
        if w["pc"] == "0x3115BA":
            w["value_source"] = "MOVS#0 (r7)"
            w["class"] = "writes_0"
            w["path_tags"] = list(set(w.get("path_tags", []) + ["clears_reset"]))
    # Drop STRB_regoff false positives that are not r9+C9D (e.g. 0x2DA3EA).
    uniq = [w for w in uniq if not (w.get("kind") == "STRB_regoff" and w.get("pc") == "0x2DA3EA")]
    nonzero = [
        w
        for w in uniq
        if w["class"] in ("writes_1", "writes_variable_nonzero")
        and w.get("kind") != "STR"
        or (
            w["class"] in ("writes_1", "writes_variable_nonzero")
            and w.get("kind") == "STR"
            and w.get("c9d_byte_index") == 0
        )
    ]
    # No exact STRB write_1 exists in EXT.
    exact_write1 = [w for w in exact if w["class"] == "writes_1"]
    ui = decode_ui_init_states(blob)
    adj = adjacent_c9c_write1(blob)

    report = {
        "code_base": f"0x{CODE_BASE:X}",
        "gate_offset": "0xC9D",
        "exact_strb_c9d": exact,
        "all_covering_stores": uniq,
        "nonzero_capable_candidates": nonzero,
        "adjacent_c9c_write1_misclassified": adj,
        "ui_init": ui,
        "verdict_hint": (
            "C9D_NONZERO_WRITER_FOUND_NEXT_GAP"
            if exact_write1
            else "C9D_NONZERO_WRITER_NEVER_REACHED"
        ),
        "summary": {
            "exact_strb_count": len(exact),
            "exact_strb_classes": {w["pc"]: w["class"] for w in exact},
            "covering_store_count": len(uniq),
            "nonzero_candidate_count": len(nonzero),
            "exact_write1_count": len(exact_write1),
            "note": (
                "Exact STRB @ C9D are clears only (0x30AA46, 0x3115BA). "
                "No MOVS#1 STRB @ C9D in robotol.ext. "
                "C9C=#1 sites (0x2E3A68, 0x2FB008) do not satisfy gate LDRSB C9D. "
                "Nonzero C9D may require memcpy/other-module/path not yet found."
            ),
        },
    }

    (out_dir / "e8t_c9d_xref.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    lines = [
        "# E8T C9D writer xref",
        "",
        f"Gate: R9+0xC9D must be ==1 (LDRSB @ 0x3066BC).",
        "",
        "## Exact STRB @ C9D",
    ]
    for w in exact:
        lines.append(
            f"- `{w['pc']}` `{w['insn']}` src={w['value_source']} class={w['class']} "
            f"fn={w['fn']} callers={w['caller_count']} {w.get('callers', [])[:4]}"
        )
    lines += ["", "## Covering STR/STRH (may or may not set C9D)", ""]
    for w in uniq:
        if w["kind"] == "STRB":
            continue
        lines.append(
            f"- `{w['pc']}` {w['kind']} src={w['value_source']} class={w['class']} "
            f"note={w.get('note','')}"
        )
    lines += ["", "## Adjacent C9C=#1 (NOT C9D)", ""]
    for w in adj:
        lines.append(f"- `{w['pc']}` fn={w['fn']} {w['note']}")
    lines += ["", "## UI-init 0x2E4788 state requirement", ""]
    lines.append(f"- rejected_states: {ui['rejected_states_observed']}")
    lines.append(f"- reject_300: {ui['reject_300_via_sub_chain']}")
    lines.append(f"- state CMPs: {ui['state_cmps'][:20]}")
    lines.append(f"- callers: {ui['callers']}")
    lines.append(f"- unlock BLs: {[b for b in ui['bls'] if 'UNLOCK' in b or '2FC8C0' in b]}")
    lines += ["", f"## Verdict hint: `{report['verdict_hint']}`", ""]
    lines.append(report["summary"]["note"])
    (out_dir / "e8t_c9d_xref.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(json.dumps(report["summary"], indent=2))
    print("ui_rejected", ui["rejected_states_observed"])
    print("wrote", out_dir / "e8t_c9d_xref.json")


if __name__ == "__main__":
    main()
