#!/usr/bin/env python3
"""Stage E8L: 0x10102 / 0x30D300 dispatch ABI + case 156/310 payload analysis."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional

CODE_BASE = 0x2D8DF4
EVENT_SWITCH = 0x30D300
PARENT = 0x300158
HOT_310 = 0x2DFC3C
CASE_156 = 156
CASE_310 = 310


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


def analyze(ext: Path) -> Dict[str, Any]:
    blob = ext.read_bytes()
    cb = CODE_BASE

    abi = {
        "entry": EVENT_SWITCH,
        "thumb": EVENT_SWITCH | 1,
        "regs": {
            "R0": {
                "role": "switch_case_index",
                "first_use": "CMP r0, #0x157; table index",
                "null_ok": False,
                "range": "0 .. 0x156",
            },
            "R1": {
                "role": "primary_payload_or_event_code",
                "first_use": "MOV r4, r1 (saved)",
                "case_forward": "case arms do MOV r0, r4; BL target — so callee R0 = original R1",
                "case310_semantics": (
                    "0x2DFC3C saves R0→r4 then gates on [R9+0xE6C]+0x7C; "
                    "both arms still forward r4 (original R1) into later calls — R1 is payload, "
                    "not the null-check subject"
                ),
                "case156_semantics": (
                    "0x300158 saves R0 to r4 then later MOV r0,r4; "
                    "in-module BL callers pass const #18/#20 — R1 should be event code int"
                ),
            },
            "R2": {
                "role": "secondary_arg_saved_to_r5",
                "first_use": "MOV r5, r2",
                "used_by_case_310_156": False,
                "used_by_other_cases": "arms with MOV r1,r5 forward it",
            },
            "R3": {
                "role": "tertiary_arg_moved_to_r1_then_bound_clobber",
                "first_use": "MOV r1, r3; then r3 reused as 0x157 bound",
                "note": "original R3 preserved only indirectly if needed before clobber; stack args preferred for extras",
            },
            "stack_arg0": {
                "role": "arg4",
                "load": "LDR r6, [sp,#0x20] after PUSH+SUB",
                "aapcs": "first stack arg on entry",
            },
            "stack_arg1": {
                "role": "arg5",
                "load": "LDR r2, [sp,#0x24] after PUSH+SUB",
            },
        },
        "prologue_summary": [
            "PUSH {r4-r6,lr}",
            "r4=R1; r1=R3; r5=R2; SUB sp,#16",
            "r3=0x157; CMP R0,r3; BCS default",
            "load stack arg4→r6, arg5→r2",
            "computed goto via halfword table",
        ],
    }

    cases = {
        "156": {
            "case": CASE_156,
            "case_hex": "0x9C",
            "arm": 0x30DDF4,
            "insns": ["MOV r0, r4", "BL 0x300158", "MOVS r0,#0", "B epilogue"],
            "forwards": {"R0_to_callee": "original R1"},
            "target": PARENT,
            "target_role": "parent_dispatcher_entry",
            "r1_hypothesis": "integer event code (18 or 20 from static parent callers)",
            "requires_pointer_r1": False,
        },
        "310": {
            "case": CASE_310,
            "case_hex": "0x136",
            "arm": 0x30D72E,
            "insns": ["MOV r0, r4", "BL 0x2DFC3C", "MOVS r0,#0", "B epilogue"],
            "forwards": {"R0_to_callee": "original R1"},
            "target": HOT_310,
            "target_role": "hot_cluster",
            "r1_hypothesis": "context/event object forwarded as r4 into later BLs (may be int or ptr)",
            "requires_pointer_r1": "unknown_until_live",
            "r9_e6c_gate": "primary early branch is [R9+0xE6C]+0x7C null, not incoming R1",
        },
    }

    # 2DFC3C: incoming R0 saved; gate on R9+E6C; r4 forwarded on both arms
    fn310 = {
        "entry": HOT_310,
        "saves_incoming_r0_to_r4": "0x2DFC40",
        "e6c_gate": {
            "load": "LDR r2,=#0xE6C; ADD r2,r9; LDR r0,[r2,#0x7C]",
            "branch": "0x2DFC48 CMP r0,#0; BEQ 0x2DFCAC",
            "effect": "R9+0xE6C object missing → alternate arm; both arms still use r4",
        },
        "e6c_present_path": {
            "path": "BL 0x2F5B38 (size 4), walk, BLX, BL 0x30E55C / 0x30C008 / 0x30F0D0",
            "uses_r4": "0x2DFC72 MOV r0,r4 before BLX",
        },
        "e6c_absent_path": {
            "entry": "0x2DFCAC",
            "uses_r4": "0x2DFCB0 MOV r1,r4 then BL",
        },
        "calls_parent": False,
        "note": "does not BL 0x300158; prep/side path; needs R9+E6C and meaningful R1",
    }

    parent = {
        "entry": PARENT,
        "saves_r0_to_r4": "0x30015C MOV r4,r0",
        "restores_r0_from_r4": "0x3001DC MOV r0,r4",
        "early_r0_clobber": "loads from R9+0x7D8 queue before using saved r4",
        "static_caller_r0": [18, 20],
    }

    probes = [
        {"id": "A", "r0": 156, "r1": 0, "r2": 0, "r3": 0, "why": "ZERO payload baseline"},
        {"id": "B", "r0": 156, "r1": 18, "r2": 0, "r3": 0, "why": "event code #18 from parent census"},
        {"id": "C", "r0": 156, "r1": 20, "r2": 0, "r3": 0, "why": "event code #20 from parent census"},
        {
            "id": "D310null",
            "r0": 310,
            "r1": 0,
            "r2": 0,
            "r3": 0,
            "why": "confirm null path (E8K)",
        },
    ]

    bp = [
        "e:0x30D300",
        "e:0x30DDF4",
        "e:0x30D72E",
        "e:0x30D730",
        "e:0x2DFC3C",
        "e:0x2DFCAC",
        "p:0x300158",
        "p:0x3002C0",
        "p:0x300714",
        "p:0x30103C",
        "p:0x3020C8",
        "q:0x2DC80C",
    ]

    return {
        "abi": abi,
        "cases": cases,
        "fn_2DFC3C": fn310,
        "fn_300158": parent,
        "recommended_probes": probes,
        "bp_spec": ",".join(bp),
        "hypotheses": [
            {
                "id": "MISSING_10102_APP_INIT_EVENT",
                "why": "host never delivers into registered 0x30D301",
            },
            {
                "id": "CASE_156_REQUIRES_PAYLOAD",
                "why": "R1 must be event code (18/20); R0=case alone insufficient semantics",
            },
            {
                "id": "CASE_310_REQUIRES_PAYLOAD",
                "why": "R1 must be non-NULL pointer for 2DFC3C main path",
            },
            {
                "id": "CASE_156_REACHED_NEXT_GAP",
                "why": "if R1=18/20 enters parent/dispatcher — next is state=38 / idle flags",
            },
        ],
    }


def write_md(data: Dict[str, Any], out: Path) -> None:
    a = data["abi"]
    lines = [
        "# E8L 0x10102 / 0x30D300 dispatch ABI (static)",
        "",
        "## Register / stack ABI",
        "",
    ]
    for k, v in a["regs"].items():
        lines.append(f"### {k}")
        for kk, vv in v.items():
            lines.append(f"- {kk}: {vv}")
        lines.append("")
    lines += ["## Prologue", ""]
    for s in a["prologue_summary"]:
        lines.append(f"- {s}")
    lines += ["", "## Case 156 / 310", ""]
    for cid, c in data["cases"].items():
        lines.append(f"### case {cid} ({c['case_hex']})")
        lines.append(f"- arm `0x{c['arm']:X}` → `{c['insns']}`")
        lines.append(f"- target `0x{c['target']:X}` ({c['target_role']})")
        lines.append(f"- R1 hyp: {c['r1_hypothesis']}")
        lines.append(f"- requires_pointer_r1={c['requires_pointer_r1']}")
        lines.append("")
    lines += [
        "## 0x2DFC3C null path",
        "",
        json.dumps(data["fn_2DFC3C"], indent=2),
        "",
        "## 0x300158 R0 handling",
        "",
        json.dumps(data["fn_300158"], indent=2),
        "",
        "## Recommended probes",
        "",
    ]
    for p in data["recommended_probes"]:
        lines.append(
            f"- {p['id']}: R0={p['r0']} R1={p['r1']} R2={p['r2']} R3={p['r3']} — {p['why']}"
        )
    lines += ["", "## Hypotheses", ""]
    for h in data["hypotheses"]:
        lines.append(f"1. `{h['id']}` — {h['why']}")
    lines += ["", f"BP: `{data['bp_spec']}`", ""]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", type=Path, required=True)
    ap.add_argument("-o", type=Path, required=True)
    args = ap.parse_args()
    args.o.mkdir(parents=True, exist_ok=True)
    data = analyze(args.ext)
    (args.o / "dispatch_abi.json").write_text(json.dumps(data, indent=2), encoding="utf-8")
    write_md(data, args.o / "dispatch_abi.md")
    (args.o / "e8l_bp_spec.txt").write_text(data["bp_spec"] + "\n", encoding="utf-8")
    print(f"probes={len(data['recommended_probes'])} bp={data['bp_spec']}")


if __name__ == "__main__":
    main()
