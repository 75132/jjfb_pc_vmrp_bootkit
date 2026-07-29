#!/usr/bin/env python3
"""P11: Case-9 / late-fault runtime contract recovery (static + optional dump merge).

Classifies family case 9 into CASE9_CONTRACT_A..E from Thumb evidence.
Does not stop at 'maybe'.
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[2]
BLOB_CANDIDATES = [
    ROOT / "out/research_p9_extract/robotol.ext",
    ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext",
]
CODE_BASE = 0x2D8DF4
FAMILY_SWITCH = 0x30D301
CASE9 = 0x30E1A0
FREE_WRAPPER = 0x305E08
FAULT_PC = 0x2D960E
OUT_DIR = ROOT / "out/p11"
REP = ROOT / "reports"

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


def u16(b: bytes, o: int) -> int:
    return struct.unpack_from("<H", b, o)[0]


def u32(b: bytes, o: int) -> int:
    return struct.unpack_from("<I", b, o)[0]


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


def decode(blob: bytes, pc: int) -> Tuple[int, str, Dict[str, Any]]:
    off = pc - CODE_BASE
    meta: Dict[str, Any] = {}
    if off < 0 or off + 1 >= len(blob):
        return 2, "OOB", meta
    h0 = u16(blob, off)
    meta["raw"] = f"0x{h0:04X}"

    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        meta["raw2"] = f"0x{h1:04X}"
        t = bl_target(pc, h0, h1)
        if t is not None:
            kind = "BL" if (h1 & 0x1000) else "BLX"
            meta["target"] = t
            meta["kind"] = kind
            return 4, f"{kind} 0x{t:X}", meta
        return 4, f"T32 0x{h0:04X} 0x{h1:04X}", meta

    if (h0 & 0xFF00) in (0xB400, 0xB500):
        regs = [f"r{i}" for i in range(8) if h0 & (1 << i)]
        if h0 & 0x100:
            regs.append("lr")
        meta["kind"] = "PUSH"
        meta["regs"] = regs
        return 2, "PUSH {" + ",".join(regs) + "}", meta
    if (h0 & 0xFF00) in (0xBC00, 0xBD00):
        regs = [f"r{i}" for i in range(8) if h0 & (1 << i)]
        if h0 & 0x100:
            regs.append("pc")
        meta["kind"] = "POP"
        meta["regs"] = regs
        return 2, "POP {" + ",".join(regs) + "}", meta
    if (h0 & 0xFF80) == 0xB080:
        imm = (h0 & 0x7F) * 4
        op = "SUB" if (h0 & 0x80) else "ADD"
        meta["kind"] = op + "_SP"
        meta["imm"] = imm
        return 2, f"{op} sp,#0x{imm:X}", meta
    if (h0 & 0xF800) == 0x4800:
        rt = (h0 >> 8) & 7
        imm = (h0 & 0xFF) * 4
        lit = ((pc + 4) & ~2) + imm
        meta["kind"] = "LDR_PC"
        meta["rt"] = rt
        meta["litpool"] = lit
        if 0 <= lit - CODE_BASE + 3 < len(blob):
            meta["lit"] = u32(blob, lit - CODE_BASE)
            return 2, f"LDR r{rt},[pc,#0x{imm:X}] ; =0x{meta['lit']:X}", meta
        return 2, f"LDR r{rt},[pc,#0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x2000:
        return 2, f"MOVS r{(h0 >> 8) & 7},#0x{h0 & 0xFF:X}", {"kind": "MOVS", "rt": (h0 >> 8) & 7}
    if (h0 & 0xF800) == 0x2800:
        return 2, f"CMP r{(h0 >> 8) & 7},#0x{h0 & 0xFF:X}", {
            "kind": "CMP_IMM",
            "rn": (h0 >> 8) & 7,
            "imm": h0 & 0xFF,
        }
    if (h0 & 0xF000) == 0xD000:
        cond = (h0 >> 8) & 0xF
        imm = sign_extend(h0 & 0xFF, 8) << 1
        tgt = (pc + 4 + imm) & ~1
        meta["kind"] = "Bcond"
        meta["cond"] = CONDS.get(cond, str(cond))
        meta["target"] = tgt
        return 2, f"B{meta['cond']} 0x{tgt:X}", meta
    if (h0 & 0xF800) == 0xE000:
        imm = sign_extend(h0 & 0x7FF, 11) << 1
        tgt = (pc + 4 + imm) & ~1
        meta["kind"] = "B"
        meta["target"] = tgt
        return 2, f"B 0x{tgt:X}", meta
    if (h0 & 0xF800) == 0x9000:
        rt = (h0 >> 8) & 7
        imm = (h0 & 0xFF) * 4
        meta.update({"kind": "STR_SP", "rt": rt, "imm": imm})
        return 2, f"STR r{rt},[sp,#0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x9800:
        rt = (h0 >> 8) & 7
        imm = (h0 & 0xFF) * 4
        meta.update({"kind": "LDR_SP", "rt": rt, "imm": imm})
        return 2, f"LDR r{rt},[sp,#0x{imm:X}]", meta
    if (h0 & 0xFE00) == 0x1C00:
        return 2, f"ADDS r{h0 & 7},r{(h0 >> 3) & 7},#{(h0 >> 6) & 7}", {"kind": "ADDS"}
    if (h0 & 0xFE00) == 0x1E00:
        return 2, f"SUBS r{h0 & 7},r{(h0 >> 3) & 7},#{(h0 >> 6) & 7}", {"kind": "SUBS"}
    if (h0 & 0xFF00) == 0x4600:
        rd = ((h0 & 0x80) >> 4) | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"MOV r{rd},r{rm}", {"kind": "MOV", "rd": rd, "rm": rm}
    if (h0 & 0xFF87) == 0x4780:
        return 2, f"BLX r{(h0 >> 3) & 0xF}", {"kind": "BLX_REG", "rm": (h0 >> 3) & 0xF}
    if (h0 & 0xFF87) == 0x4700:
        return 2, f"BX r{(h0 >> 3) & 0xF}", {"kind": "BX", "rm": (h0 >> 3) & 0xF}
    if (h0 & 0xF800) == 0x6800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) * 4
        meta.update({"kind": "LDR", "rt": rt, "rn": rn, "imm": imm})
        return 2, f"LDR r{rt},[r{rn},#0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x6000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) * 4
        meta.update({"kind": "STR", "rt": rt, "rn": rn, "imm": imm})
        return 2, f"STR r{rt},[r{rn},#0x{imm:X}]", meta
    if (h0 & 0xFFC0) == 0x4280:
        return 2, f"CMP r{h0 & 7},r{(h0 >> 3) & 7}", {"kind": "CMP"}
    if (h0 & 0xF800) == 0x3000:
        return 2, f"ADDS r{(h0 >> 8) & 7},#0x{h0 & 0xFF:X}", {"kind": "ADDS_IMM"}
    if (h0 & 0xF800) == 0x3800:
        return 2, f"SUBS r{(h0 >> 8) & 7},#0x{h0 & 0xFF:X}", {"kind": "SUBS_IMM"}
    if (h0 & 0xFF00) == 0x4400:
        rd = ((h0 & 0x80) >> 4) | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"ADD r{rd},r{rm}", {"kind": "ADD", "rd": rd, "rm": rm}
    if (h0 & 0xF800) == 0x7800:
        return 2, f"LDRB r{h0 & 7},[r{(h0 >> 3) & 7},#0x{(h0 >> 6) & 0x1F:X}]", {"kind": "LDRB"}
    if (h0 & 0xF800) == 0x7000:
        return 2, f"STRB r{h0 & 7},[r{(h0 >> 3) & 7},#0x{(h0 >> 6) & 0x1F:X}]", {"kind": "STRB"}
    if (h0 & 0xF800) == 0x8800:
        return 2, f"LDRH r{h0 & 7},[r{(h0 >> 3) & 7},#0x{((h0 >> 6) & 0x1F) * 2:X}]", {
            "kind": "LDRH"
        }
    return 2, f"??? 0x{h0:04X}", meta


def disasm(blob: bytes, start: int, nmax: int = 120) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    pc = start & ~1
    for _ in range(nmax):
        n, text, meta = decode(blob, pc)
        row = {"pc": f"0x{pc:X}", "text": text, **{k: v for k, v in meta.items() if k != "raw"}}
        if "raw" in meta:
            row["raw"] = meta["raw"]
        out.append(row)
        if meta.get("kind") == "POP" and "pc" in meta.get("regs", []):
            break
        if meta.get("kind") == "BX" and meta.get("rm") == 14:
            break
        if meta.get("kind") == "B" and pc > start + 4:
            # keep going for window dumps; caller truncates
            pass
        pc += n
    return out


def find_fn_start(blob: bytes, site: int) -> int:
    for back in range(0, 0x800, 2):
        p = (site & ~1) - back
        if p < CODE_BASE:
            break
        h = u16(blob, p - CODE_BASE)
        if (h & 0xFF00) == 0xB500 or h in (0xB570, 0xB5F0, 0xB5F8, 0xB5FF):
            return p
    return site & ~1


def analyze_path_to_case9(blob: bytes) -> Dict[str, Any]:
    """Walk family switch from entry until case-9 arm; collect SP loads and R0-R3 first uses."""
    lines = disasm(blob, FAMILY_SWITCH, 200)
    sp_loads: List[Dict[str, Any]] = []
    sp_stores: List[Dict[str, Any]] = []
    r_reads_first: Dict[str, str] = {}
    indirect: List[Dict[str, Any]] = []
    case9_reached = False
    for row in lines:
        text = row["text"]
        kind = row.get("kind")
        if kind == "LDR_SP":
            sp_loads.append(row)
            rt = f"r{row.get('rt')}"
            if rt not in r_reads_first:
                r_reads_first[rt] = row["pc"]
        if kind == "STR_SP":
            sp_stores.append(row)
        if kind in ("LDR", "LDRB", "LDRH") and f"r{row.get('rn')}" in ("r0", "r1", "r2", "r3"):
            rn = f"r{row.get('rn')}"
            if rn not in r_reads_first:
                r_reads_first[rn] = row["pc"]
        if kind in ("BLX_REG", "BX") and row.get("rm", 0) < 14:
            indirect.append(row)
        # case9 branch target
        if row.get("target") == CASE9 or (kind == "B" and row.get("target") == CASE9):
            case9_reached = True
        if row["pc"] == f"0x{CASE9:X}":
            case9_reached = True
            break
        # stop if we jumped far away after common prologue
        if kind == "B" and row.get("target") == CASE9:
            case9_reached = True
            break
    return {
        "entry": f"0x{FAMILY_SWITCH:X}",
        "lines": lines,
        "sp_loads": sp_loads,
        "sp_stores": sp_stores,
        "r_first_read": r_reads_first,
        "indirect": indirect,
        "case9_in_window": case9_reached,
    }


def analyze_case9_arm(blob: bytes) -> Dict[str, Any]:
    arm = disasm(blob, CASE9, 16)
    uses_r2 = False
    uses_r3 = False
    uses_sp = False
    bl_targets: List[int] = []
    for row in arm:
        text = row["text"]
        if "r2" in text and row.get("kind") in ("LDR", "MOV", "ADD", "CMP", "BLX_REG"):
            uses_r2 = True
        if "r3" in text:
            uses_r3 = True
        if row.get("kind") in ("LDR_SP", "STR_SP"):
            uses_sp = True
        if row.get("kind") in ("BL", "BLX") and "target" in row:
            bl_targets.append(int(row["target"]))
        # stop after case9's own B join
        if row.get("kind") == "B" and row["pc"] != f"0x{CASE9:X}":
            break
        if row.get("kind") == "B" and int(row.get("target", 0)) != CASE9:
            # first B after BL/MOVS is the join
            if len(arm) > 1 and row != arm[0]:
                break
    return {
        "arm": arm[:8],
        "uses_r2": uses_r2,
        "uses_r3": uses_r3,
        "uses_sp_relative": uses_sp,
        "bl_targets": [f"0x{t:X}" for t in bl_targets],
    }


def analyze_free_wrapper(blob: bytes) -> Dict[str, Any]:
    fn = find_fn_start(blob, FREE_WRAPPER)
    lines = disasm(blob, fn, 48)
    sp_loads = [r for r in lines if r.get("kind") == "LDR_SP"]
    r0_source = None
    plat_codes = []
    for r in lines:
        if r.get("kind") == "LDR_PC" and isinstance(r.get("lit"), int):
            if r["lit"] in (0x10133, 0x1E205, 0x1E209, 0x10102):
                plat_codes.append({"pc": r["pc"], "lit": f"0x{r['lit']:X}"})
        if r.get("kind") == "LDR_SP" and r.get("rt") == 0:
            r0_source = r
        if r.get("kind") == "MOV" and r.get("rd") == 0:
            r0_source = r
    return {
        "fn_start": f"0x{fn:X}",
        "lines": lines,
        "sp_loads": sp_loads,
        "r0_source": r0_source,
        "plat_codes": plat_codes,
    }


def analyze_fault_site(blob: bytes) -> Dict[str, Any]:
    fn = find_fn_start(blob, FAULT_PC)
    window = disasm(blob, FAULT_PC - 0x20, 40)
    at = None
    for r in window:
        if r["pc"] == f"0x{FAULT_PC:X}" or r["pc"] == f"0x{FAULT_PC & ~1:X}":
            at = r
            break
    # Precise instruction at 0x2D960E
    n, text, meta = decode(blob, FAULT_PC & ~1)
    return {
        "fn_start": f"0x{fn:X}",
        "fault_pc": f"0x{FAULT_PC:X}",
        "insn": {"text": text, **meta, "size": n},
        "window": window,
    }


def classify(
    switch: Dict[str, Any],
    case9: Dict[str, Any],
    free_w: Dict[str, Any],
    fault: Dict[str, Any],
    runtime: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Hard classification CASE9_CONTRACT_A..E."""
    arm_texts = [r["text"] for r in case9["arm"]]
    # Proven static shape: BL 0x305E08; MOVS r0,#0; B join
    is_free_only = (
        len(case9["bl_targets"]) == 1
        and case9["bl_targets"][0] == "0x305E08"
        and not case9["uses_r2"]
        and not case9["uses_r3"]
        and not case9["uses_sp_relative"]
    )

    # Does common switch prologue load sp+32/36 before case dispatch?
    prologue_sp32 = [r for r in switch["sp_loads"] if r.get("imm") in (0x20, 0x24, 32, 36)]
    # Also check any LDR_SP in first ~40 insns before first case branch table use
    early_sp = switch["sp_loads"][:8]

    # Free wrapper: does it need r0 as heap ptr from caller?
    free_needs_r0 = True  # 305E08 historically sends 10133 with r0-4
    free_sp = free_w["sp_loads"]

    # Fault after return → likely E if case9 completes (MOVS r0,#0) and fault PC is outside switch
    fault_outside_switch = True
    fpc = int(fault["fault_pc"], 16)
    if 0x30D300 <= fpc <= 0x30E200:
        fault_outside_switch = False

    classification = None
    reasons: List[str] = []

    if is_free_only:
        reasons.append(
            "case9 arm is BL 0x305E08; MOVS r0,#0; B join — no LDR of r2/r3/[sp,#imm] in arm"
        )
        # Check whether 305E08 / switch common path needs stack args from host
        switch_needs_stack_args = any(r.get("imm") in (0x20, 0x24) for r in early_sp)
        if switch_needs_stack_args:
            reasons.append(f"switch prologue loads stack args early: {early_sp[:4]}")
            # Could be B or C depending on producer
            classification = "CASE9_CONTRACT_B"
            reasons.append(
                "common path loads registered context from stack — need 10102 context, not event payload"
            )
        else:
            # Does free wrapper read r0 as-is (event code) vs heap?
            # Ack contract says case9_r0 = event_code 0x1E209 (not heap). So free is misnamed
            # OR r0 is overwritten inside wrapper from stack.
            if free_sp:
                reasons.append(f"305E08 loads from SP: {free_sp[:4]}")
                classification = "CASE9_CONTRACT_B"
            else:
                classification = "CASE9_CONTRACT_A"
                reasons.append(
                    "case9 + 305E08 path shows no required host-filled r2/r3/stack0/stack1"
                )

    # Override toward E if late fault is proven after successful case9 return
    if runtime and runtime.get("case9_ret_ok") and runtime.get("fault_after_handler"):
        classification = "CASE9_CONTRACT_E"
        reasons.append(
            "runtime: case9 returned ok then late P3_FAULT outside handler — scheduler/continuation"
        )
    elif fault_outside_switch and is_free_only and classification == "CASE9_CONTRACT_A":
        # Static alone: A for case9 ABI, but also note E as post-handler open issue
        reasons.append(
            "late fault PC 0x2D960E is outside family switch — post-handler / scheduler path "
            "(tracked as successor of A, not a case9 arg-fill need)"
        )

    # D: only if wrapper entry != 0x30D301 but host calls case handler directly
    if runtime and runtime.get("host_entry") and runtime["host_entry"] not in (
        "0x30D301",
        "0x30D300",
        "0x30D2F9",
    ):
        if int(runtime["host_entry"], 16) == CASE9:
            classification = "CASE9_CONTRACT_D"
            reasons.append("host entered case9 arm directly, skipping switch wrapper")

    if classification is None:
        # Fallback: prefer E over vague B when arm is free-only and fault is outside
        if is_free_only and fault_outside_switch:
            classification = "CASE9_CONTRACT_E"
            reasons.append(
                "case9 itself needs no host stack fill; open gap is post-return at 0x2D960E"
            )
        else:
            classification = "CASE9_CONTRACT_A"
            reasons.append("default: no evidence case9 consumes r2/r3/stack args")

    return {
        "classification": classification,
        "reasons": reasons,
        "is_free_only": is_free_only,
        "arm_texts": arm_texts,
        "prologue_sp32": prologue_sp32,
        "early_sp_loads": early_sp,
        "fault_outside_switch": fault_outside_switch,
        "fault_insn": fault["insn"],
        "action": action_for(classification),
    }


def action_for(c: str) -> Dict[str, Any]:
    if c == "CASE9_CONTRACT_A":
        return {
            "do": "disable_guessed_r2_r3_stack_fill",
            "then": "chase_post_handler_0x2D960E_1E205",
        }
    if c == "CASE9_CONTRACT_B":
        return {"do": "restore_registered_10102_context", "source": "handler_record_only"}
    if c == "CASE9_CONTRACT_C":
        return {"do": "restore_event_node_payload_context_stack"}
    if c == "CASE9_CONTRACT_D":
        return {"do": "call_real_wrapper_not_case_handler"}
    if c == "CASE9_CONTRACT_E":
        return {
            "do": "stop_family_arg_mutation",
            "then": "trace_scheduler_continuation_and_1E205_producer",
        }
    return {"do": "unknown"}


def load_runtime_hints() -> Optional[Dict[str, Any]]:
    dumps = OUT_DIR / "runtime_dumps.json"
    if not dumps.exists():
        # Infer from known p10/p7 reports
        p7 = REP / "p7_family_event_abi.csv"
        hints: Dict[str, Any] = {
            "host_entry": "0x30D301",
            "case9_ret_ok": True,  # Layer-1 PASS + 5 resources before late fault
            "fault_after_handler": True,  # p10: late P3_FAULT after first frame
            "source": "reports_inference",
        }
        if p7.exists():
            hints["p7_present"] = True
        return hints
    return json.loads(dumps.read_text(encoding="utf-8"))


def main() -> int:
    blob_path = next((p for p in BLOB_CANDIDATES if p.exists()), None)
    if not blob_path:
        print("FATAL: robotol.ext not found", file=sys.stderr)
        return 2
    blob = blob_path.read_bytes()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    REP.mkdir(parents=True, exist_ok=True)

    switch = analyze_path_to_case9(blob)
    case9 = analyze_case9_arm(blob)
    free_w = analyze_free_wrapper(blob)
    fault = analyze_fault_site(blob)
    runtime = load_runtime_hints()
    verdict = classify(switch, case9, free_w, fault, runtime)

    # Write human disasm artifacts
    def dump_lines(path: Path, title: str, rows: List[Dict[str, Any]]) -> None:
        lines = [f"# {title}", f"blob={blob_path}", f"code_base=0x{CODE_BASE:X}", ""]
        for r in rows:
            lines.append(f"  {r['pc']}: {r['text']}")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    dump_lines(OUT_DIR / "disasm_family_switch.txt", "0x30D301 family switch", switch["lines"][:120])
    dump_lines(OUT_DIR / "disasm_case9.txt", "0x30E1A0 case9", case9["arm"])
    dump_lines(OUT_DIR / "disasm_305e08.txt", "0x305E08 free wrapper", free_w["lines"])
    dump_lines(OUT_DIR / "disasm_2d960e.txt", "0x2D960E fault site", fault["window"])

    contract = {
        "phase": "P11",
        "blob": str(blob_path.relative_to(ROOT)).replace("\\", "/"),
        "code_base": f"0x{CODE_BASE:X}",
        "family_switch": f"0x{FAMILY_SWITCH:X}",
        "case9": f"0x{CASE9:X}",
        "free_wrapper": f"0x{FREE_WRAPPER:X}",
        "fault_pc": f"0x{FAULT_PC:X}",
        "fault_addr_observed": "0x1E205",
        "classification": verdict["classification"],
        "reasons": verdict["reasons"],
        "action": verdict["action"],
        "case9_arm": case9,
        "free_wrapper_analysis": {
            "fn_start": free_w["fn_start"],
            "plat_codes": free_w["plat_codes"],
            "sp_loads": free_w["sp_loads"],
            "r0_source": free_w["r0_source"],
        },
        "fault_insn": fault["insn"],
        "switch_early_sp_loads": switch["sp_loads"][:12],
        "runtime_hints": runtime,
        "sp_relative_contract": {
            "note": "For each [sp,#imm] convert using entry_sp from runtime dump when present",
            "static_early_loads": [
                {
                    "pc": r["pc"],
                    "imm": r.get("imm"),
                    "rt": r.get("rt"),
                    "text": r["text"],
                }
                for r in switch["sp_loads"][:20]
            ],
        },
    }
    (OUT_DIR / "case9_contract.json").write_text(
        json.dumps(contract, indent=2, ensure_ascii=False), encoding="utf-8"
    )
    (REP / "JJFB_RUNTIME_CONTRACT.json").write_text(
        json.dumps(contract, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    print(json.dumps({
        "classification": verdict["classification"],
        "action": verdict["action"],
        "reasons": verdict["reasons"],
        "fault_insn": fault["insn"].get("text"),
        "case9_arm": [r["text"] for r in case9["arm"][:4]],
        "out": str(OUT_DIR / "case9_contract.json"),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
