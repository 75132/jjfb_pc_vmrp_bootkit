#!/usr/bin/env python3
"""Stage E8H: bootstrap dispatcher provenance (0x30103C) + SVC #0xAB stub scan."""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CODE_BASE = 0x2D8DF4
DISPATCHER_FN = 0x300714
DISPATCHER_BL = 0x3002C0
PARENT_FN = 0x300158
SITE_30103C = 0x30103C
FN_3020C8 = 0x3020C8
SITE_302340 = 0x302340
SITE_302362 = 0x302362
WRITER_2F4E82 = 0x2F4E82
SVC_STUB = 0x2D92A4
SVC_INSN = 0x2D92AE
KNOWN_SVC_CALLERS = [0x2D91E0, 0x2D91EE, 0x2D9202, 0x2D920E]


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


def scan_svc(blob: bytes, code_base: int) -> List[Dict[str, Any]]:
    hits: List[Dict[str, Any]] = []
    for o in range(0, len(blob) - 1, 2):
        h = u16(blob, o)
        if (h & 0xFF00) == 0xDF00:
            hits.append({"pc": code_base + o, "imm": h & 0xFF, "raw": f"0x{h:04X}"})
    return hits


def prev_movs_r0(blob: bytes, code_base: int, bl_pc: int) -> Optional[int]:
    for back in range(2, 32, 2):
        p = bl_pc - back
        if p < code_base:
            break
        h = u16(blob, p - code_base)
        if (h & 0xF800) == 0x2000 and ((h >> 8) & 7) == 0:
            return h & 0xFF
    return None


def disasm_window(blob: bytes, code_base: int, start: int, nbytes: int) -> List[str]:
    lines: List[str] = []
    pc = start
    end = start + nbytes
    while pc < end:
        off = pc - code_base
        if off < 0 or off + 1 >= len(blob):
            break
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
            h1 = u16(blob, off + 2)
            t = bl_target(pc, h0, h1)
            if t is not None:
                lines.append(f"0x{pc:X}: BL 0x{t:X}")
                pc += 4
                continue
            lines.append(f"0x{pc:X}: thumb2 0x{h0:04X}_{h1:04X}")
            pc += 4
            continue
        if (h0 & 0xFF00) == 0xDF00:
            lines.append(f"0x{pc:X}: SVC #0x{h0 & 0xFF:X}")
        elif (h0 & 0xF800) == 0x2800:
            lines.append(f"0x{pc:X}: CMP r{(h0 >> 8) & 7}, #{h0 & 0xFF}")
        elif (h0 & 0xF800) == 0x2000:
            lines.append(f"0x{pc:X}: MOVS r{(h0 >> 8) & 7}, #{h0 & 0xFF}")
        elif (h0 & 0xF800) == 0xE000:
            imm = h0 & 0x7FF
            if imm >= 0x400:
                imm -= 0x800
            lines.append(f"0x{pc:X}: B -> 0x{(pc + 4 + imm * 2) & ~1:X}")
        elif (h0 & 0xF000) == 0xD000 and (h0 & 0xF00) != 0xF00:
            imm = h0 & 0xFF
            if imm >= 0x80:
                imm -= 0x100
            lines.append(
                f"0x{pc:X}: Bcond({(h0 >> 8) & 0xF}) -> 0x{(pc + 4 + imm * 2) & ~1:X}"
            )
        elif (h0 & 0xFF00) == 0xB500:
            lines.append(f"0x{pc:X}: PUSH 0x{h0:04X}")
        else:
            lines.append(f"0x{pc:X}: 0x{h0:04X}")
        pc += 2
    return lines


def r4_cases_3020c8(blob: bytes, code_base: int) -> List[Dict[str, Any]]:
    """Extract CMP r4,#imm branch table near fn entry."""
    cases: List[Dict[str, Any]] = []
    pc = FN_3020C8
    end = FN_3020C8 + 0x80
    while pc < end:
        off = pc - code_base
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0x2C00:  # CMP rn,#imm with rn in low regs via 2Cxx? 
            # Thumb CMP rd,#imm8 is 00101 — 0x2800 family; CMP r4 is 0x2Cimm
            pass
        if (h0 & 0xFF00) == 0x2C00:  # CMP r4, #imm
            imm = h0 & 0xFF
            h1 = u16(blob, off + 2) if off + 3 < len(blob) else 0
            tgt = None
            kind = None
            if (h1 & 0xF000) == 0xD000 and (h1 & 0xF00) != 0xF00:
                rel = h1 & 0xFF
                if rel >= 0x80:
                    rel -= 0x100
                tgt = (pc + 2 + 4 + rel * 2) & ~1
                kind = f"Bcond({(h1 >> 8) & 0xF})"
            cases.append({"pc": pc, "imm": imm, "branch": kind, "target": tgt})
            pc += 2
            continue
        if (h0 & 0xF800) == 0xF000:
            pc += 4
            continue
        pc += 2
    return cases


def path_to_writer(blob: bytes, code_base: int) -> Dict[str, Any]:
    """Document known static path from R9+(0x800+0xD0)==38 to C44 writer."""
    return {
        "evidence": "TARGET_OBSERVED",
        "chain": [
            f"0x{PARENT_FN:X} (many callers) BL@0x{DISPATCHER_BL:X}",
            f"0x{DISPATCHER_FN:X} loads *(R9+(0x800+0xD0)); MOVS r4,r0 (incoming arg)",
            "CMP *(R9+(0x800+0xD0)), #38 → BEQ 0x300816 → 0x300EF0 → 0x30103A",
            "0x30103A: MOVS r0,r4; BL 0x3020C8",
            "0x3020C8: MOVS r4,r0; CMP r4,#13/#2/#5/#8/#12/#17/#18/#20 …",
            "path arm reaches 0x302340 / 0x302362: BL 0x2F4E64 (C44 writer family)",
            f"writer site 0x{WRITER_2F4E82:X}",
        ],
        "r9_state_gate": {"offset": "(0x800+0xD0)", "required_value_for_30103C": 38},
        "note": (
            "0x30103C is a BL site inside dispatcher 0x300714, not a standalone function. "
            "r4 inside 0x3020C8 is the original argument to 0x300714."
        ),
    }


def analyze(ext: Path) -> Dict[str, Any]:
    blob = ext.read_bytes()
    code_base = CODE_BASE
    svc_all = scan_svc(blob, code_base)
    svc_ab = [s for s in svc_all if s["imm"] == 0xAB]
    imm_hist = Counter(s["imm"] for s in svc_all)

    stub_callers = find_bl_callers(blob, code_base, SVC_STUB)
    caller_info = []
    for c in stub_callers:
        caller_info.append(
            {
                "bl_pc": c,
                "original_r0_movs": prev_movs_r0(blob, code_base, c),
                "in_known_list": c in KNOWN_SVC_CALLERS,
            }
        )

    parent_callers = find_bl_callers(blob, code_base, PARENT_FN)
    disp_callers = find_bl_callers(blob, code_base, DISPATCHER_FN)

    bp_pcs = [
        DISPATCHER_FN,
        DISPATCHER_BL,
        SITE_30103C,
        FN_3020C8,
        SITE_302340,
        SITE_302362,
        WRITER_2F4E82,
        SVC_INSN,
    ]

    result: Dict[str, Any] = {
        "code_base": code_base,
        "dispatcher": {
            "fn": DISPATCHER_FN,
            "bl_from_parent": DISPATCHER_BL,
            "parent_fn": PARENT_FN,
            "parent_caller_count": len(parent_callers),
            "parent_callers_sample": parent_callers[:24],
            "dispatcher_callers": disp_callers,
            "site_30103C": SITE_30103C,
            "fn_3020C8": FN_3020C8,
            "path": path_to_writer(blob, code_base),
            "r4_cases_near_3020C8": r4_cases_3020c8(blob, code_base),
            "disasm_30103A": disasm_window(blob, code_base, 0x30103A, 16),
            "disasm_dispatcher_entry": disasm_window(blob, code_base, DISPATCHER_FN, 64),
            "disasm_3020C8_head": disasm_window(blob, code_base, FN_3020C8, 48),
            "disasm_302340": disasm_window(blob, code_base, SITE_302340, 40),
        },
        "svc_ab": {
            "insn_pc": SVC_INSN,
            "stub_fn": SVC_STUB,
            "occurrences_in_robotol": len(svc_ab),
            "sites": svc_ab,
            "stub_disasm": disasm_window(blob, code_base, SVC_STUB, 20),
            "stub_callers": caller_info,
            "abi_hypothesis": {
                "evidence": "TARGET_OBSERVED+HYPOTHESIS",
                "steps": [
                    "PUSH {r3,lr}",
                    "STRB r0,[sp]  ; original request byte",
                    "MOVS r0,#3    ; service selector",
                    "MOV r1,sp     ; arg block",
                    "SVC #0xAB",
                ],
                "known_original_r0_at_callers": {
                    hex(c["bl_pc"]): c["original_r0_movs"] for c in caller_info
                },
            },
        },
        "svc_all_imm_histogram": dict(sorted(imm_hist.items())),
        "dispatcher_bp_csv": ",".join(f"0x{p:X}" for p in bp_pcs),
        "bp_pcs": bp_pcs,
    }
    return result


def write_md(data: Dict[str, Any], out: Path) -> None:
    d = data["dispatcher"]
    s = data["svc_ab"]
    lines = [
        "# E8H bootstrap dispatcher + SVC #0xAB xref",
        f"code_base=0x{data['code_base']:X}",
        "",
        "## Line A — dispatcher provenance",
        "",
        f"- Parent fn: `0x{d['parent_fn']:X}` (BL callers ≈ {d['parent_caller_count']})",
        f"- Dispatcher fn: `0x{d['fn']:X}` via BL `@0x{d['bl_from_parent']:X}`",
        f"- Site `0x{d['site_30103C']:X}`: BL → `0x{d['fn_3020C8']:X}`",
        "",
        "### Static path (TARGET_OBSERVED)",
        "",
    ]
    for step in d["path"]["chain"]:
        lines.append(f"- {step}")
    lines += [
        "",
        f"- R9+(0x800+0xD0) must equal **{d['path']['r9_state_gate']['required_value_for_30103C']}** "
        "to take the 0x30103C arm.",
        f"- Note: {d['path']['note']}",
        "",
        "### CMP r4 cases near 0x3020C8",
        "",
    ]
    for c in d["r4_cases_near_3020C8"]:
        lines.append(
            f"- 0x{c['pc']:X}: CMP r4, #{c['imm']} → {c.get('branch')} "
            f"{('0x%X' % c['target']) if c.get('target') else ''}"
        )
    lines += [
        "",
        "### Disasm snippets",
        "",
        "```",
        *d["disasm_dispatcher_entry"][:20],
        "...",
        *d["disasm_30103A"],
        "...",
        *d["disasm_3020C8_head"][:16],
        "...",
        *d["disasm_302340"][:8],
        "```",
        "",
        "## Line B — SVC #0xAB",
        "",
        f"- Only **{s['occurrences_in_robotol']}** `SVC #0xAB` site in robotol: "
        f"`0x{s['insn_pc']:X}` inside stub `0x{s['stub_fn']:X}`",
        "",
        "```",
        *s["stub_disasm"],
        "```",
        "",
        "### Stub callers + reconstructed original r0",
        "",
    ]
    for c in s["stub_callers"]:
        lines.append(
            f"- BL @ `0x{c['bl_pc']:X}` original_r0_MOVS=#{c['original_r0_movs']} "
            f"known={c['in_known_list']}"
        )
    lines += [
        "",
        "### All Thumb SVC immediates in robotol",
        "",
        "```",
        json.dumps(data["svc_all_imm_histogram"], indent=2),
        "```",
        "",
        "## Observe BP CSV",
        "",
        f"`{data['dispatcher_bp_csv']}`",
        "",
    ]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", type=Path, required=True)
    ap.add_argument("-o", type=Path, required=True)
    args = ap.parse_args()
    args.o.mkdir(parents=True, exist_ok=True)
    data = analyze(args.ext)
    (args.o / "dispatcher_svc_xref.json").write_text(
        json.dumps(data, indent=2), encoding="utf-8"
    )
    write_md(data, args.o / "dispatcher_svc_xref.md")
    (args.o / "dispatcher_bp_csv.txt").write_text(
        data["dispatcher_bp_csv"] + "\n", encoding="utf-8"
    )
    print(f"wrote {args.o / 'dispatcher_svc_xref.md'}")
    print(f"bp_csv={data['dispatcher_bp_csv']}")


if __name__ == "__main__":
    main()
