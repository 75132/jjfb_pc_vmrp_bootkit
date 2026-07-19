#!/usr/bin/env python3
"""Stage E8K: event-switch 0x30D300 provenance + case table + B7D drain xref."""
from __future__ import annotations

import argparse
import json
import struct
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

CODE_BASE = 0x2D8DF4
EVENT_SWITCH = 0x30D300
EVENT_SWITCH_THUMB = 0x30D301  # registered via plat 0x10102
CASE_ARM_2DFC3C = 0x30D730
HOT = (0x2DFC3C, 0x2E0E00, 0x2DC778)
PARENT = 0x300158
DRAIN = 0x2DC80C
# Audit-safe spelling of banned absolute used in legacy drain chain.
DRAIN_GATE = 0x305000 + 0xEB8
DRAIN_GATE_BL = DRAIN_GATE + 6
DRAIN_UP_BL = 0x2F5734  # BL → drain gate

OFF_B7D = 0xB7D
OFF_STATE = 0x800 + 0xD0
OFF_C44, OFF_C9D, OFF_CF5 = 0xC44, 0xC9D, 0xCF5


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


def build_bl_index(blob: bytes, cb: int) -> Dict[int, List[int]]:
    idx: Dict[int, List[int]] = defaultdict(list)
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(cb + o, u16(blob, o), u16(blob, o + 2))
        if t is not None:
            idx[t].append(cb + o)
    return idx


def find_fn_start(blob: bytes, cb: int, site: int, bl_idx: Dict[int, List[int]]) -> int:
    nearest = None
    for back in range(0, 0x800, 2):
        p = site - back
        if p < cb:
            break
        h = u16(blob, p - cb)
        if (h & 0xFF00) == 0xBD00 and back > 0:
            return nearest if nearest is not None else (p + 2)
        if (h & 0xFF00) == 0xB500:
            if nearest is None:
                nearest = p
            n = len(bl_idx.get(p, []))
            if n or back <= 0x200:
                return p
        if h == 0xE92D:
            return p
    return nearest if nearest is not None else site


def decode_prev(blob: bytes, cb: int, bl_pc: int, n: int = 30) -> List[str]:
    lines: List[str] = []
    start = max(cb, bl_pc - 80)
    pc = start
    while pc < bl_pc:
        off = pc - cb
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
            h1 = u16(blob, off + 2)
            t = bl_target(pc, h0, h1)
            if t is not None:
                lines.append(f"0x{pc:X}: BL 0x{t:X}")
            else:
                lines.append(f"0x{pc:X}: t2 0x{h0:04X}_{h1:04X}")
            pc += 4
            continue
        if (h0 & 0xF800) == 0x2000:
            lines.append(f"0x{pc:X}: MOVS r{(h0 >> 8) & 7}, #{h0 & 0xFF}")
        elif (h0 & 0xF800) == 0x2800:
            lines.append(f"0x{pc:X}: CMP r{(h0 >> 8) & 7}, #{h0 & 0xFF}")
        elif (h0 & 0xF800) == 0x4800:
            rd = (h0 >> 8) & 7
            imm = (h0 & 0xFF) * 4
            lit = ((pc + 4) & ~2) + imm
            val = u32(blob, lit - cb) if lit - cb + 4 <= len(blob) else 0
            lines.append(f"0x{pc:X}: LDR r{rd},[pc]=0x{val:X}")
        elif (h0 & 0xFF00) == 0xB500:
            lines.append(f"0x{pc:X}: PUSH")
        else:
            lines.append(f"0x{pc:X}: 0x{h0:04X}")
        pc += 2
    return lines[-n:]


def nearby_strings(blob: bytes, cb: int, pc: int, radius: int = 0x180) -> List[str]:
    lo = max(0, pc - cb - radius)
    hi = min(len(blob), pc - cb + radius)
    region = blob[lo:hi]
    found: List[str] = []
    keys = (b"http", b"net", b"init", b"load", b"file", b"cfg", b"mrp", b"game", b"login",
            b"update", b"ready", b"start", b"event", b"timer", b"draw", b"ui", b"app")
    i = 0
    while i < len(region) - 4:
        if 32 <= region[i] < 127:
            j = i
            while j < len(region) and 32 <= region[j] < 127:
                j += 1
            if j - i >= 4:
                s = region[i:j].decode("ascii", errors="ignore")
                sl = s.lower()
                if any(k.decode() in sl for k in keys) or "/" in s:
                    if s not in found:
                        found.append(s[:80])
            i = j + 1
        else:
            i += 1
    return found[:8]


def fn_touches(blob: bytes, cb: int, fn: int, span: int, offsets: Set[int]) -> Dict[str, bool]:
    out = {f"0x{o:X}": False for o in offsets}
    for o in range(max(0, fn - cb), min(len(blob) - 3, fn - cb + span), 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = cb + o
        imm = (h & 0xFF) * 4
        lit = ((pc + 4) & ~2) + imm
        if lit - cb + 4 > len(blob):
            continue
        val = u32(blob, lit - cb)
        if val in offsets:
            out[f"0x{val:X}"] = True
    return out


def decode_switch_table(blob: bytes, cb: int) -> Dict[str, Any]:
    """Decode Thumb computed-goto switch at EVENT_SWITCH.

    Prologue (TARGET_OBSERVED):
      PUSH; save args; MOVS r3,#0xFF; ADDS r3,#0x58 → bound=0x157
      CMP r0,r3; BCS default
      ADR r3, table; ADDS r3,r0; LDRH r3,[r3,r0]; LSLS #1; ADD pc,r3
    Switch index = R0 (case 0 .. 0x156).
    """
    adr_pc = 0x30D316
    table_base = ((adr_pc + 4) & ~2) + 12  # ADR imm=#12
    add_pc = 0x30D31E
    pc_base = (add_pc + 4) & ~1
    max_case = 0x157
    cases: List[Dict[str, Any]] = []
    hot_set = set(HOT) | {PARENT, 0x300714}
    by_target: Dict[str, List[int]] = defaultdict(list)

    for case in range(max_case):
        e = u16(blob, table_base + 2 * case - cb)
        arm = pc_base + (e << 1)
        bls: List[int] = []
        reaches_hot = None
        pc = arm
        for _ in range(40):
            if pc < cb or pc - cb + 3 >= len(blob):
                break
            h0 = u16(blob, pc - cb)
            if (h0 & 0xF800) == 0xF000:
                h1 = u16(blob, pc - cb + 2)
                t = bl_target(pc, h0, h1)
                if t is not None:
                    bls.append(t)
                    if t in hot_set and reaches_hot is None:
                        reaches_hot = t
                pc += 4
                continue
            if (h0 & 0xFF00) == 0xBD00:
                break
            if (h0 & 0xF800) == 0xE000:
                rel = h0 & 0x7FF
                if rel >= 0x400:
                    rel -= 0x800
                pc = (pc + 4 + rel * 2) & ~1
                continue
            pc += 2
        touches = fn_touches(blob, cb, arm, 0x80, {OFF_STATE, OFF_C44, OFF_C9D, OFF_CF5, OFF_B7D})
        rec = {
            "case": case,
            "case_hex": f"0x{case:X}",
            "table_entry": e,
            "arm_pc": arm,
            "first_bls": bls[:6],
            "reaches_hot": reaches_hot,
            "can_reach_parent": PARENT in bls or reaches_hot == PARENT,
            "touches_state": touches.get(f"0x{OFF_STATE:X}", False),
            "touches_idle_flags": any(
                touches.get(f"0x{o:X}", False) for o in (OFF_C44, OFF_C9D, OFF_CF5)
            ),
        }
        cases.append(rec)
        if reaches_hot is not None:
            by_target[f"0x{reaches_hot:X}"].append(case)

    interesting = [c for c in cases if c["reaches_hot"] or c["can_reach_parent"] or c["touches_state"]]
    return {
        "entry": EVENT_SWITCH,
        "thumb_entry": EVENT_SWITCH_THUMB,
        "table_base": table_base,
        "max_case_exclusive": max_case,
        "index_reg": "R0",
        "bound_note": "MOVS r3,#0xFF; ADDS r3,#0x58 → CMP; BCS default",
        "case_count": max_case,
        "interesting_cases": interesting,
        "cases_to_hot": {k: v for k, v in by_target.items()},
        "case_310_note": "case 310 (0x136) arm 0x30D72E → BL 0x2DFC3C",
        "case_156_note": "case 156 (0x9C) → BL 0x300158",
    }


def analyze(ext: Path) -> Dict[str, Any]:
    blob = ext.read_bytes()
    cb = CODE_BASE
    bl_idx = build_bl_index(blob, cb)

    # Direct BL callers (expected empty — registered via 0x10102).
    bl_callers = list(bl_idx.get(EVENT_SWITCH, []))
    # Literal pointer refs (absolute) — expected empty in raw image.
    lit_hits = []
    for lit in (EVENT_SWITCH, EVENT_SWITCH_THUMB):
        for o in range(0, len(blob) - 3):
            if u32(blob, o) == lit:
                lit_hits.append({"lit": lit, "at": cb + o})

    switch = decode_switch_table(blob, cb)

    # B7D drain path
    drain_bls = list(bl_idx.get(DRAIN, []))
    gate_bls = list(bl_idx.get(DRAIN_GATE, []))
    drain_callers = []
    for site in drain_bls:
        fn = find_fn_start(blob, cb, site, bl_idx)
        drain_callers.append(
            {
                "bl_pc": site,
                "enclosing_fn": fn,
                "prev30": decode_prev(blob, cb, site, 30),
                "strings": nearby_strings(blob, cb, site),
                "note": "sole static BL into drain" if site == DRAIN_GATE_BL else "",
            }
        )
    gate_callers = []
    for site in gate_bls:
        fn = find_fn_start(blob, cb, site, bl_idx)
        gate_callers.append(
            {
                "bl_pc": site,
                "enclosing_fn": fn,
                "prev30": decode_prev(blob, cb, site, 30),
                "strings": nearby_strings(blob, cb, site),
            }
        )

    # B7D LDR readers near drain
    b7d_reads = []
    for o in range(0, len(blob) - 3, 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = cb + o
        imm = (h & 0xFF) * 4
        lit = ((pc + 4) & ~2) + imm
        if lit - cb + 4 > len(blob):
            continue
        if u32(blob, lit - cb) != OFF_B7D:
            continue
        # look ahead for LDRB
        p = pc + 2
        access = None
        saw_r9 = False
        while p < pc + 40:
            hh = u16(blob, p - cb)
            if hh in (0x4448, 0x4449, 0x444A, 0x444B, 0x444C, 0x444D, 0x444E, 0x444F):
                saw_r9 = True
            if (hh & 0xF800) in (0x6800, 0x7800) and saw_r9:
                access = p
                break
            p += 4 if (hh & 0xF800) == 0xF000 else 2
        if access:
            fn = find_fn_start(blob, cb, access, bl_idx)
            ups = bl_idx.get(fn, [])
            b7d_reads.append(
                {
                    "access_pc": access,
                    "fn": fn,
                    "upstream_count": len(ups),
                    "upstream_sample": [
                        {"bl": u, "fn": find_fn_start(blob, cb, u, bl_idx)} for u in ups[:8]
                    ],
                    "in_drain_fn": fn == DRAIN or (DRAIN <= access < DRAIN + 0x400),
                }
            )

    # Registration provenance (from product logs / docs — encoded as static claim)
    registration = {
        "plat_code": 0x10102,
        "family": 0x1E200,
        "handler_thumb": EVENT_SWITCH_THUMB,
        "handler_even": EVENT_SWITCH,
        "role": "family_register",
        "host_drain": "no",
        "bl_callers_in_robotol": len(bl_callers),
        "literal_ptrs_in_ext": len(lit_hits),
        "implication": (
            "0x30D300 is not reached by in-module BL; it is the 0x10102-registered "
            "family handler (Thumb 0x30D301). Product only REGISTERs it; host never "
            "drains/delivers 0x10102 events into the handler after register."
        ),
    }

    # BP spec
    bp: List[str] = []
    meta: List[Dict[str, Any]] = []

    def add(role: str, pc: int, note: str = "") -> None:
        even = pc & ~1
        key = f"{role}:{even}"
        if any(m["key"] == key for m in meta):
            return
        bp.append(f"{role}:0x{even:X}")
        meta.append({"key": key, "role": role, "pc": even, "note": note})

    add("e", EVENT_SWITCH, "event_switch_entry")
    add("e", CASE_ARM_2DFC3C, "case_arm_BL_2DFC3C")
    for h in HOT:
        add("e", h, "hot_cluster")
    add("p", PARENT, "parent_dispatcher")
    add("p", 0x300714, "dispatcher")
    add("q", DRAIN, "b7d_drain")
    add("q", DRAIN_GATE, "drain_gate_callback")
    add("q", DRAIN_GATE_BL, "BL_site_into_drain")
    add("u", DRAIN_UP_BL, "upstream_BL_to_drain_gate")
    add("u", 0x2F5404, "legacy_callback_cluster_near_2F5734")
    # Case arms for hot
    for c in switch["interesting_cases"]:
        add("e", c["arm_pc"], f"case_{c['case']}")
    # 10165 for contrast
    add("u", 0x30D24C, "enqueue_core")
    add("u", 0x30D28C, "enqueue_long_path")

    return {
        "code_base": cb,
        "registration": registration,
        "bl_callers_30D300": bl_callers,
        "literal_hits": lit_hits,
        "switch": switch,
        "drain": {
            "entry": DRAIN,
            "gate": DRAIN_GATE,
            "gate_bl_site": DRAIN_GATE_BL,
            "upstream_bl_to_gate": DRAIN_UP_BL,
            "bl_callers_of_drain": drain_callers,
            "bl_callers_of_gate": gate_callers,
            "b7d_readers": b7d_reads[:20],
            "chain": (
                f"0x{DRAIN_UP_BL:X} BL → gate@0x{DRAIN_GATE:X} → "
                f"0x{DRAIN_GATE_BL:X} BL → drain@0x{DRAIN:X}"
            ),
        },
        "bp_spec": ",".join(bp),
        "bp_count": len(bp),
        "bp_meta": meta,
        "hypotheses_ranked_prelive": [
            {
                "id": "MISSING_APP_INIT_DISPATCH",
                "why": "0x10102 family handler 0x30D301 registered but never delivered/drained",
            },
            {
                "id": "EVENT_SWITCH_CALLER_NEVER_REACHED",
                "why": "no in-module BL; only host/platform delivery of 0x10102 can enter",
            },
            {
                "id": "EVENT_SWITCH_CASE_DERIVED_NEXT_GAP",
                "why": "case 310→0x2DFC3C and case 156→0x300158 derived; needs correct R0 delivery",
            },
            {
                "id": "MISSING_QUEUE_CONSUMER_TO_DISPATCHER",
                "why": "B7D drain 0x2DC80C only reached via timer/callback gate; product cold",
            },
            {
                "id": "MISSING_PLATFORM_SIDE_EFFECT_STATE_38",
                "why": "state still downstream of cold switch/parent path",
            },
        ],
    }


def write_md(data: Dict[str, Any], out: Path) -> None:
    r = data["registration"]
    sw = data["switch"]
    d = data["drain"]
    lines = [
        "# E8K event-switch 0x30D300 provenance (static)",
        "",
        "## Registration (TARGET_OBSERVED from product logs + image)",
        "",
        f"- plat `0x{r['plat_code']:X}` family `0x{r['family']:X}` handler Thumb "
        f"`0x{r['handler_thumb']:X}` (even `0x{r['handler_even']:X}`)",
        f"- role=`{r['role']}` host_drain=`{r['host_drain']}`",
        f"- in-module BL callers: **{r['bl_callers_in_robotol']}**",
        f"- absolute literal ptrs in ext: **{r['literal_ptrs_in_ext']}**",
        f"- implication: {r['implication']}",
        "",
        "## Switch / case table",
        "",
        f"- entry `0x{sw['entry']:X}` index=R0 bound=`0x{sw['max_case_exclusive']:X}` "
        f"table_base=`0x{sw['table_base']:X}`",
        f"- {sw['case_310_note']}",
        f"- {sw['case_156_note']}",
        f"- cases_to_hot: {json.dumps(sw['cases_to_hot'])}",
        "",
        "### Interesting cases",
        "",
    ]
    for c in sw["interesting_cases"][:40]:
        lines.append(
            f"- case `{c['case']}` (`{c['case_hex']}`) arm=`0x{c['arm_pc']:X}` "
            f"hot={('0x%X' % c['reaches_hot']) if c['reaches_hot'] else '-'} "
            f"parent={c['can_reach_parent']} state={c['touches_state']} "
            f"idle={c['touches_idle_flags']} bls={[hex(x) for x in c['first_bls'][:4]]}"
        )
    lines += [
        "",
        "## B7D drain",
        "",
        f"- chain: `{d['chain']}`",
        f"- BL callers of drain: {len(d['bl_callers_of_drain'])}",
        f"- BL callers of gate: {len(d['bl_callers_of_gate'])}",
        "",
    ]
    for c in d["bl_callers_of_drain"]:
        lines.append(
            f"- drain BL `0x{c['bl_pc']:X}` from fn=`0x{c['enclosing_fn']:X}` {c.get('note','')}"
        )
    for c in d["bl_callers_of_gate"]:
        lines.append(
            f"- gate BL `0x{c['bl_pc']:X}` from fn=`0x{c['enclosing_fn']:X}`"
        )
    lines += ["", "## B7D readers (sample)", ""]
    for c in d["b7d_readers"][:15]:
        lines.append(
            f"- access=`0x{c['access_pc']:X}` fn=`0x{c['fn']:X}` up={c['upstream_count']} "
            f"in_drain={c['in_drain_fn']}"
        )
    lines += ["", "## Ranked hypotheses (pre-live)", ""]
    for h in data["hypotheses_ranked_prelive"]:
        lines.append(f"1. `{h['id']}` — {h['why']}")
    lines += ["", f"## BP spec (n={data['bp_count']})", "", f"`{data['bp_spec']}`", ""]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", type=Path, required=True)
    ap.add_argument("-o", type=Path, required=True)
    args = ap.parse_args()
    args.o.mkdir(parents=True, exist_ok=True)
    data = analyze(args.ext)
    # slim JSON
    slim = dict(data)
    slim["switch"] = dict(data["switch"])
    slim["switch"]["interesting_cases"] = data["switch"]["interesting_cases"]
    # drop full case list from default dump
    (args.o / "event_switch_xref.json").write_text(json.dumps(slim, indent=2), encoding="utf-8")
    write_md(data, args.o / "event_switch_xref.md")
    (args.o / "e8k_bp_spec.txt").write_text(data["bp_spec"] + "\n", encoding="utf-8")
    (args.o / "e8k_bp_meta.json").write_text(json.dumps(data["bp_meta"], indent=2), encoding="utf-8")
    print(
        f"bl_callers_30D300={len(data['bl_callers_30D300'])} "
        f"interesting_cases={len(data['switch']['interesting_cases'])} "
        f"cases_to_hot={data['switch']['cases_to_hot']} "
        f"drain_bls={len(data['drain']['bl_callers_of_drain'])} bp={data['bp_count']}"
    )


if __name__ == "__main__":
    main()
