#!/usr/bin/env python3
"""Stage E8M: static analysis of 0x300158 → 0x300714 prerequisites."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CODE_BASE = 0x2D8DF4
PARENT = 0x300158
DISPATCHER = 0x300714
PARENT_END = 0x3004F6  # last epilogue POP path before literal pool
BL_300714 = 0x3002C0
GATE_CMP20 = 0x3002BA
STATE_OFF = 0x8D0  # R9+(0x800+0xD0)
E6C_OFF = 0xE6C


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


COND = {
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


def analyze_parent(blob: bytes) -> Dict[str, Any]:
    cb = CODE_BASE
    bls: List[Dict[str, Any]] = []
    branches: List[Dict[str, Any]] = []
    r9_ops: List[Dict[str, Any]] = []
    cmps: List[Dict[str, Any]] = []

    pc = PARENT
    while pc < PARENT_END + 0x20:
        off = pc - cb
        if off < 0 or off + 1 >= len(blob):
            break
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0xF000:
            h1 = u16(blob, off + 2)
            t = bl_target(pc, h0, h1)
            if t is not None:
                bls.append({"pc": pc, "target": t, "is_300714": t == DISPATCHER})
            pc += 4
            continue
        if (h0 & 0xF000) == 0xD000 and (h0 & 0xF00) != 0xF00:
            cond = (h0 >> 8) & 0xF
            rel = h0 & 0xFF
            if rel >= 0x80:
                rel -= 0x100
            tgt = (pc + 4 + rel * 2) & ~1
            branches.append(
                {"pc": pc, "kind": f"B{COND.get(cond, str(cond))}", "target": tgt, "cond": cond}
            )
            pc += 2
            continue
        if (h0 & 0xF800) == 0xE000:
            rel = h0 & 0x7FF
            if rel >= 0x400:
                rel -= 0x800
            tgt = (pc + 4 + rel * 2) & ~1
            branches.append({"pc": pc, "kind": "B", "target": tgt, "cond": None})
            pc += 2
            continue
        if (h0 & 0xF800) == 0x4800:
            rd = (h0 >> 8) & 7
            imm = h0 & 0xFF
            lit = ((pc + 4) & ~2) + imm * 4
            if 0 <= lit - cb <= len(blob) - 4:
                val = u32(blob, lit - cb)
                r9_ops.append({"pc": pc, "op": "LDR_lit", "rd": rd, "imm": val})
            pc += 2
            continue
        if (h0 & 0xF800) == 0x2800:
            cmps.append({"pc": pc, "rn": (h0 >> 8) & 7, "imm": h0 & 0xFF})
            pc += 2
            continue
        if h0 in (0x4448, 0x4449, 0x444A, 0x444B, 0x444C, 0x444D, 0x444E, 0x444F):
            r9_ops.append({"pc": pc, "op": "ADD_rN_r9", "rd": h0 & 7})
        pc += 2

    # Prologue state-load chain (TARGET_OBSERVED)
    state_load = {
        "sequence": [
            "LDR r1, =0x7D8; ADD r1,r9",
            "MOVS r0,#1; STRB r0,[r1,#2]  — touch queue/base flag",
            "LDR r0, =0x7D8; ADD r0,r9; ADDS r0,#0x80; LDR r0,[r0,#0x78]",
            "=> r0 = *[R9+0x7D8+0x80+0x78] = *[R9+0x8D0]  (state word)",
            "MOV r4, <incoming R0> saved before clobber",
        ],
        "switch_subject": "R9+0x8D0 state word (NOT incoming event code)",
        "incoming_r0_role": "saved in r4; restored only on arms that need event code",
        "math": "0x7D8 + 0x80 + 0x78 = 0x8D0",
    }

    path_to_714 = {
        "only_bl_site": BL_300714,
        "gate": {
            "pc": GATE_CMP20,
            "insns": ["CMP r0, #20", "BLT 0x3001E2 (epilogue path)", "MOV r0,r4", "BL 0x300714"],
            "meaning": "state word must be >= 20 (signed) to call dispatcher",
        },
        "reach_gate_via": "default arm 0x300272 → 0x3002BA when state matches no special case",
        "state0_arm": {
            "cmp": "CMP r0,#0; BEQ → 0x30026E → 0x3004C8",
            "effect": "state==0 never reaches 0x3002BA / 0x300714",
            "evidence": "E8L probes all had R9_state=0 → this arm",
        },
        "why_e8l_missed_714": (
            "Parent switch keys off R9+0x8D0; probes left state=0; "
            "state0 arm does plat 0x304558 then return — no BL 0x300714"
        ),
    }

    # Special state cases (from CMP chain) that do NOT go to 300714
    special_states = [
        {"state": 0, "arm": "0x3004C8", "reaches_714": False},
        {"state": 1, "arm": "0x3004E0", "reaches_714": False},
        {"state": 4, "note": "falls toward default if NE path", "reaches_714": "maybe"},
        {"state": 5, "arm": "0x3001A0 early POP", "reaches_714": False},
        {"state": 20, "note": "CMP#20 at gate: EQ not LT → may call 714 if default", "reaches_714": True},
        {"state": 38, "note": "unlisted → default; 38>=20 → BL 300714 with r4", "reaches_714": True},
    ]

    return {
        "fn": {"start": PARENT, "end_approx": PARENT_END, "epilogue_pop": 0x3001B8},
        "state_load": state_load,
        "path_to_714": path_to_714,
        "special_states": special_states,
        "bls": bls,
        "bl_targets_unique": sorted({b["target"] for b in bls}),
        "cmps_on_r0": [c for c in cmps if c["rn"] == 0],
        "r9_literals": r9_ops,
        "branches_sample": branches[:40],
        "event_code_effect": {
            "r4": "incoming R0 (case156 delivery R1 = 0/18/20)",
            "used_when": "arms that MOV r0,r4 before BL helpers / before BL 0x300714",
            "vs_switch": "does NOT select switch arm; switch uses state word",
            "e8l_diff_hyp": (
                "R0=0/18/20 at parent entry only differ after an arm restores r4; "
                "with state=0 all take same state0 arm — paths converge"
            ),
        },
    }


def find_e6c_base_writers(blob: bytes) -> List[Dict[str, Any]]:
    """Find sites that STR a pointer into [R9+0xE6C] (object base), not only field stores."""
    cb = CODE_BASE
    writers: List[Dict[str, Any]] = []
    pc = cb
    end = cb + len(blob) - 8
    while pc < end:
        off = pc - cb
        h = u16(blob, off)
        if (h & 0xF800) == 0x4800:
            rd = (h >> 8) & 7
            imm = h & 0xFF
            lit = ((pc + 4) & ~2) + imm * 4
            if 0 <= lit - cb <= len(blob) - 4 and u32(blob, lit - cb) == E6C_OFF:
                # look ahead for ADD rd,r9 then STR rn,[rd,#0]
                p = pc + 2
                saw_add = False
                for _ in range(20):
                    if p - cb + 1 >= len(blob):
                        break
                    hh = u16(blob, p - cb)
                    if (hh & 0xF800) == 0xF000:
                        p += 4
                        continue
                    if hh in (
                        0x4448,
                        0x4449,
                        0x444A,
                        0x444B,
                        0x444C,
                        0x444D,
                        0x444E,
                        0x444F,
                    ):
                        if (hh & 7) == rd:
                            saw_add = True
                    # STR rt,[rn,#0] word: 0x6000 | (0<<6) | (rn<<3) | rt
                    if saw_add and (hh & 0xF800) == 0x6000:
                        rn = (hh >> 3) & 7
                        imm5 = (hh >> 6) & 0x1F
                        if rn == rd and imm5 == 0:
                            writers.append(
                                {
                                    "ldr_pc": pc,
                                    "str_pc": p,
                                    "raw": hh,
                                    "kind": "STR_word_off0_to_E6C_base",
                                }
                            )
                    p += 2
        pc += 2
    # dedupe by str_pc
    seen = set()
    uniq = []
    for w in writers:
        if w["str_pc"] in seen:
            continue
        seen.add(w["str_pc"])
        uniq.append(w)
    return uniq


def analyze(ext: Path) -> Dict[str, Any]:
    blob = ext.read_bytes()
    parent = analyze_parent(blob)
    e6c = find_e6c_base_writers(blob)
    bp = [
        "p:0x300158",
        "p:0x300182",  # first CMP on state
        "p:0x300194",  # CMP state,#0
        "p:0x30026E",  # state0 branch land
        "p:0x3004C8",  # state0 arm
        "p:0x3002BA",  # gate CMP #20
        "p:0x3002C0",  # BL 300714
        "p:0x300714",
        "p:0x3001B8",  # early POP
        "e:0x2DFC3C",
        "e:0x2DFCAC",
        "e:0x30D300",
        "q:0x2DC80C",
    ]
    return {
        "parent": parent,
        "e6c_base_writers": e6c[:30],
        "e6c_writer_count": len(e6c),
        "bp_spec": ",".join(bp),
        "hypotheses": [
            {
                "id": "PARENT_BRANCH_CONDITION_UNMET",
                "why": "state==0 takes 0x3004C8 arm; never BL 0x300714",
            },
            {
                "id": "PARENT_REQUIRES_QUEUE_SETUP",
                "why": "secondary — 7D8 touched but switch subject is 8D0 via 7D8 math",
            },
            {
                "id": "MISSING_OBJECT_INIT_BEFORE_10102",
                "why": "case310 needs E6C; parent needs non-zero state first — init order",
            },
        ],
        "recommended_seq": [
            "A: 10165 then case156 R1=18",
            "B: 10165 then case156 R1=20",
            "C: case310 then case156 R1=18",
            "D: case310 then case156 R1=20",
            "E: 10165 + case310 + case156 R1=18",
            "F: 10165 + case310 + case156 R1=20",
        ],
    }


def write_md(data: Dict[str, Any], out: Path) -> None:
    p = data["parent"]
    lines = [
        "# E8M 0x300158 parent → 0x300714 prerequisites (static)",
        "",
        f"- fn `0x{PARENT:X}` .. ~`0x{PARENT_END:X}`",
        f"- sole `BL 0x300714` at `0x{BL_300714:X}`",
        "",
        "## State load (critical)",
        "",
    ]
    for s in p["state_load"]["sequence"]:
        lines.append(f"- {s}")
    lines += [
        "",
        f"- switch_subject: **{p['state_load']['switch_subject']}**",
        f"- why E8L missed: {p['path_to_714']['why_e8l_missed_714']}",
        "",
        "## Path to 0x300714",
        "",
        "```",
        json.dumps(p["path_to_714"], indent=2),
        "```",
        "",
        "## Special state arms",
        "",
    ]
    for s in p["special_states"]:
        lines.append(f"- state={s['state']}: reaches_714={s.get('reaches_714')} {s}")
    lines += [
        "",
        "## Event code (r4) vs switch",
        "",
        json.dumps(p["event_code_effect"], indent=2),
        "",
        f"## BL targets ({len(p['bl_targets_unique'])})",
        "",
        ", ".join(f"`0x{t:X}`" for t in p["bl_targets_unique"]),
        "",
        f"## E6C base writers (STR [R9+E6C,#0]): {data['e6c_writer_count']}",
        "",
    ]
    for w in data["e6c_base_writers"][:15]:
        lines.append(f"- LDR@`0x{w['ldr_pc']:X}` STR@`0x{w['str_pc']:X}`")
    lines += ["", f"BP: `{data['bp_spec']}`", ""]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", type=Path, required=True)
    ap.add_argument("-o", type=Path, required=True)
    args = ap.parse_args()
    args.o.mkdir(parents=True, exist_ok=True)
    data = analyze(args.ext)
    (args.o / "parent_prereq.json").write_text(json.dumps(data, indent=2), encoding="utf-8")
    write_md(data, args.o / "parent_prereq.md")
    (args.o / "e8m_bp_spec.txt").write_text(data["bp_spec"] + "\n", encoding="utf-8")
    print(
        f"bl_300714=0x{BL_300714:X} e6c_writers={data['e6c_writer_count']} "
        f"bp={data['bp_spec']}"
    )


if __name__ == "__main__":
    main()
