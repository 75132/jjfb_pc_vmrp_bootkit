#!/usr/bin/env python3
"""Stage E8N: R9+(0x800+0xD0) state writer xref + 0x300158 state ladder."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

CODE_BASE = 0x2D8DF4
STATE_OFF = 0x800 + 0xD0  # 0x8D0
E6C_OFF = 0xE6C
PARENT = 0x300158
PARENT_END = 0x3004F6
DISPATCHER = 0x300714
BL_714 = 0x3002C0
STATE0_ARM = 0x3004C8


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


def find_fn_start(blob: bytes, cb: int, site: int) -> int:
    for back in range(0, 0x800, 2):
        p = site - back
        if p < cb:
            break
        h = u16(blob, p - cb)
        if (h & 0xFF00) == 0xB500:
            return p
        if h == 0xE92D:
            return p
    return site


def find_bl_callers(blob: bytes, cb: int, target: int) -> List[int]:
    out: List[int] = []
    tgt = target & ~1
    for o in range(0, len(blob) - 3, 2):
        t = bl_target(cb + o, u16(blob, o), u16(blob, o + 2))
        if t == tgt:
            out.append(cb + o)
    return out


def infer_store_value(blob: bytes, cb: int, store_pc: int, rt: int) -> Dict[str, Any]:
    """Look back for MOVS rt,#imm or MOV rt,rn near store."""
    for back in range(2, 64, 2):
        p = store_pc - back
        if p < cb:
            break
        h = u16(blob, p - cb)
        if (h & 0xF800) == 0xF000:
            continue
        if (h & 0xF800) == 0x2000 and ((h >> 8) & 7) == rt:
            imm = h & 0xFF
            return {
                "kind": "imm8",
                "imm": imm,
                "pc": p,
                "can_nonzero": imm != 0,
                "can_ge20": imm >= 20,
                "can_eq38": imm == 38,
            }
        # MOV rd,rm low: 00011100 mm m dd d = ADD rd,rm,#0 alias 1Cxx
        if (h & 0xFFE0) == (0x1C00 | (rt & 7)) and ((h >> 3) & 7) != rt:
            return {"kind": "mov_reg", "pc": p, "raw": h}
    return {"kind": "unknown"}


def scan_direct_writers(blob: bytes, cb: int) -> List[Dict[str, Any]]:
    """LDR =STATE_OFF; ADD rd,r9; STR* rt,[rd,#0]."""
    writers: List[Dict[str, Any]] = []
    for o in range(0, len(blob) - 3, 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = cb + o
        rd = (h >> 8) & 7
        imm = h & 0xFF
        lit = ((pc + 4) & ~2) + imm * 4
        if lit - cb + 4 > len(blob):
            continue
        if u32(blob, lit - cb) != STATE_OFF:
            continue
        p = pc + 2
        saw_add = False
        for _ in range(28):
            if p - cb + 1 >= len(blob):
                break
            hh = u16(blob, p - cb)
            if (hh & 0xF800) == 0xF000:
                p += 4
                continue
            if hh in (0x4448, 0x4449, 0x444A, 0x444B, 0x444C, 0x444D, 0x444E, 0x444F):
                if (hh & 7) == rd:
                    saw_add = True
            if saw_add and (hh & 0xF800) in (0x6000, 0x7000, 0x8000):
                rn = (hh >> 3) & 7
                imm5 = (hh >> 6) & 0x1F
                rt = hh & 7
                width = {0x6000: 4, 0x7000: 1, 0x8000: 2}[hh & 0xF800]
                scale = width
                off = imm5 * scale
                if rn == rd and off == 0:
                    vs = infer_store_value(blob, cb, p, rt)
                    fn = find_fn_start(blob, cb, pc)
                    callers = find_bl_callers(blob, cb, fn)[:12]
                    writers.append(
                        {
                            "kind": "direct_STR_off0",
                            "ldr_pc": pc,
                            "str_pc": p,
                            "width": width,
                            "rt": rt,
                            "value_source": vs,
                            "fn": fn,
                            "callers": callers,
                            "caller_fns": sorted({find_fn_start(blob, cb, c) for c in callers})[:12],
                            "can_nonzero": vs.get("can_nonzero", "unknown"),
                            "can_ge20": vs.get("can_ge20", "unknown"),
                            "can_eq38": vs.get("can_eq38", "unknown"),
                        }
                    )
            p += 2
    # dedupe by str_pc
    seen: Set[int] = set()
    uniq = []
    for w in writers:
        if w["str_pc"] in seen:
            continue
        seen.add(w["str_pc"])
        uniq.append(w)
    return uniq


def scan_7d8_math_writers(blob: bytes, cb: int) -> List[Dict[str, Any]]:
    """R9+0x7D8 + 0x80 + 0x78 = 0x8D0 alternate addressing (parent load path)."""
    # Find LDR =0x7D8 then ADD r9 then ADDS #0x80 then STR to [r,#0x78]
    hits: List[Dict[str, Any]] = []
    for o in range(0, len(blob) - 3, 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = cb + o
        rd = (h >> 8) & 7
        imm = h & 0xFF
        lit = ((pc + 4) & ~2) + imm * 4
        if lit - cb + 4 > len(blob):
            continue
        if u32(blob, lit - cb) != 0x7D8:
            continue
        p = pc + 2
        saw_add = False
        saw_80 = False
        for _ in range(32):
            if p - cb + 1 >= len(blob):
                break
            hh = u16(blob, p - cb)
            if (hh & 0xF800) == 0xF000:
                p += 4
                continue
            if hh in (0x4448, 0x4449, 0x444A, 0x444B, 0x444C, 0x444D, 0x444E, 0x444F):
                if (hh & 7) == rd:
                    saw_add = True
            # ADDS rd,#0x80 = 0x3080 | (rd<<8) for rd in low? Thumb ADDS rd,#imm8 = 0x3000|(rd<<8)|imm
            if (hh & 0xF800) == 0x3000 and ((hh >> 8) & 7) == rd and (hh & 0xFF) == 0x80:
                saw_80 = True
            # STR word [rn,#0x78]: imm5=0x1E, width4
            if saw_add and saw_80 and (hh & 0xF800) == 0x6000:
                rn = (hh >> 3) & 7
                imm5 = (hh >> 6) & 0x1F
                if rn == rd and imm5 * 4 == 0x78:
                    hits.append(
                        {
                            "kind": "via_7D8_plus_80_plus_78",
                            "ldr_7d8_pc": pc,
                            "str_pc": p,
                            "fn": find_fn_start(blob, cb, pc),
                            "note": "aliases R9+0x8D0",
                        }
                    )
            p += 2
    return hits


def decode_state_ladder(blob: bytes, cb: int) -> List[Dict[str, Any]]:
    """Map CMP imm arms in 0x300158 that key off loaded state."""
    COND = {
        0: "EQ",
        1: "NE",
        2: "CS",
        3: "CC",
        8: "HI",
        9: "LS",
        10: "GE",
        11: "LT",
        12: "GT",
        13: "LE",
    }
    arms: List[Dict[str, Any]] = []
    pc = PARENT
    while pc < PARENT_END:
        off = pc - cb
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0xF000:
            pc += 4
            continue
        if (h0 & 0xF800) == 0x2800 and ((h0 >> 8) & 7) == 0:
            imm = h0 & 0xFF
            h1 = u16(blob, off + 2)
            tgt = None
            kind = None
            if (h1 & 0xF000) == 0xD000 and (h1 & 0xF00) != 0xF00:
                cond = (h1 >> 8) & 0xF
                rel = h1 & 0xFF
                if rel >= 0x80:
                    rel -= 0x100
                tgt = (pc + 2 + 4 + rel * 2) & ~1
                kind = f"B{COND.get(cond, str(cond))}"
            reaches_714 = False
            if tgt is not None:
                # BFS-ish: if tgt is 3002BA/3002C0/300272 or known 714 path
                if tgt in (0x3002BA, BL_714, 0x300272):
                    reaches_714 = True
                if imm == 0 and tgt in (0x30026E, STATE0_ARM):
                    reaches_714 = False
            arms.append(
                {
                    "cmp_pc": pc,
                    "state_imm": imm,
                    "branch": kind,
                    "target": tgt,
                    "reaches_714_hyp": reaches_714
                    if imm not in (0, 1, 5)
                    else False,
                    "note": (
                        "state0 early arm"
                        if imm == 0
                        else (
                            "gate CMP#20 before BL714"
                            if pc == 0x3002BA
                            else ""
                        )
                    ),
                }
            )
        pc += 2
    # Explicit gate
    arms.append(
        {
            "cmp_pc": 0x3002BA,
            "state_imm": 20,
            "branch": "BLT→epilogue else BL 0x300714",
            "target": BL_714,
            "reaches_714_hyp": True,
            "note": "default-arm gate: state>=20 required",
        }
    )
    return arms


def analyze_state0_arm(blob: bytes, cb: int) -> Dict[str, Any]:
    pc = STATE0_ARM
    end = 0x3004DE
    bls = []
    while pc < end:
        off = pc - cb
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0xF000:
            h1 = u16(blob, off + 2)
            t = bl_target(pc, h0, h1)
            if t:
                bls.append({"pc": pc, "target": t})
            pc += 4
            continue
        if (h0 & 0xF800) == 0xE000:
            rel = h0 & 0x7FF
            if rel >= 0x400:
                rel -= 0x800
            tgt = (pc + 4 + rel * 2) & ~1
            if tgt == 0x3001B6 or tgt == 0x3001B8:
                break
            pc += 2
            continue
        pc += 2
    return {
        "entry": STATE0_ARM,
        "bls": bls,
        "summary": (
            "MOVS args; BL 0x304558 (plat helper); B epilogue — "
            "no STR to state, no BL 0x300714, no E6C write"
        ),
        "side_effect": "platform/helper call only (0x304558)",
        "advances_state": False,
    }


def find_e6c_writers(blob: bytes, cb: int) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    for o in range(0, len(blob) - 3, 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = cb + o
        rd = (h >> 8) & 7
        imm = h & 0xFF
        lit = ((pc + 4) & ~2) + imm * 4
        if lit - cb + 4 > len(blob):
            continue
        if u32(blob, lit - cb) != E6C_OFF:
            continue
        p = pc + 2
        saw = False
        for _ in range(20):
            hh = u16(blob, p - cb)
            if (hh & 0xF800) == 0xF000:
                p += 4
                continue
            if hh in (0x4448, 0x4449, 0x444A, 0x444B, 0x444C, 0x444D, 0x444E, 0x444F):
                if (hh & 7) == rd:
                    saw = True
            if saw and (hh & 0xF800) == 0x6000:
                rn = (hh >> 3) & 7
                imm5 = (hh >> 6) & 0x1F
                if rn == rd and imm5 == 0:
                    fn = find_fn_start(blob, cb, pc)
                    out.append({"ldr_pc": pc, "str_pc": p, "fn": fn})
            p += 2
    seen: Set[int] = set()
    uniq = []
    for w in out:
        if w["str_pc"] in seen:
            continue
        seen.add(w["str_pc"])
        uniq.append(w)
    return uniq


def ladder_hypothesis() -> List[Dict[str, Any]]:
    return [
        {
            "from": 0,
            "to": "special arms (1,5,...) or stay",
            "via": "state0 arm 0x3004C8 does NOT advance; needs external writer",
        },
        {
            "from": "unlisted >=20 (e.g. 20..37,39..)",
            "to": "BL 0x300714",
            "via": "default arm + CMP#20 not LT",
        },
        {
            "from": 38,
            "to": "0x30103C path inside dispatcher",
            "via": "0x300714 CMP #38",
        },
        {
            "note": "Do not assume 0→38 single step; E8N maps writers that can produce intermediate values",
        },
    ]


def analyze(ext: Path) -> Dict[str, Any]:
    blob = ext.read_bytes()
    cb = CODE_BASE
    direct = scan_direct_writers(blob, cb)
    via7 = scan_7d8_math_writers(blob, cb)
    ladder = decode_state_ladder(blob, cb)
    s0 = analyze_state0_arm(blob, cb)
    e6c = find_e6c_writers(blob, cb)

    # Shared fns between state writers and E6C writers
    state_fns = {w["fn"] for w in direct}
    e6c_fns = {w["fn"] for w in e6c}
    shared = sorted(state_fns & e6c_fns)

    # Imm-capable writers summary
    imm38 = [w for w in direct if w.get("can_eq38") is True]
    imm_ge20 = [w for w in direct if w.get("can_ge20") is True]
    imm_nz = [w for w in direct if w.get("can_nonzero") is True]

    bp = [
        "p:0x300158",
        "p:0x3004C8",
        "p:0x3002BA",
        "p:0x3002C0",
        "p:0x300714",
        "p:0x30103C",
        "e:0x2DFC3C",
        "e:0x2DFCAC",
        "e:0x30D300",
    ]
    # Add top writer STR sites as BPs (cap)
    for w in direct[:40]:
        bp.append(f"u:0x{w['str_pc']:X}")

    return {
        "state_off": STATE_OFF,
        "direct_writers": direct,
        "direct_writer_count": len(direct),
        "via_7d8_alias_writers": via7,
        "imm_nonzero_count": len(imm_nz),
        "imm_ge20_count": len(imm_ge20),
        "imm_eq38_count": len(imm38),
        "imm38_writers": imm38[:20],
        "state_ladder_cmps": ladder,
        "ladder_hypothesis": ladder_hypothesis(),
        "state0_arm": s0,
        "e6c_writers": e6c,
        "shared_state_e6c_fns": [f"0x{x:X}" for x in shared],
        "bp_spec": ",".join(bp),
        "cf_ladder_values": [1, 2, 19, 20, 38],
        "hypotheses": [
            {
                "id": "R9_8D0_WRITER_NEVER_REACHED",
                "why": "E8I product + E8M probes: state_writes=0; writers exist statically",
            },
            {
                "id": "STATE_LADDER_DERIVED_NEXT_GAP",
                "why": "0→714 blocked; need writer path that produces >=20 then 38",
            },
            {
                "id": "R9_8D0_REQUIRES_OBJECT_INIT",
                "why": "shared init with E6C if common fn found",
            },
        ],
    }


def write_md(data: Dict[str, Any], out: Path) -> None:
    lines = [
        "# E8N R9+0x8D0 state writer xref + ladder (static)",
        "",
        f"- STATE_OFF = `0x{data['state_off']:X}` (= 0x800+0xD0)",
        f"- direct writers: **{data['direct_writer_count']}**",
        f"- imm nonzero / >=20 / ==38: {data['imm_nonzero_count']} / "
        f"{data['imm_ge20_count']} / {data['imm_eq38_count']}",
        f"- via 7D8 alias writers: {len(data['via_7d8_alias_writers'])}",
        f"- E6C writers: {len(data['e6c_writers'])}",
        f"- shared state∩E6C fns: {data['shared_state_e6c_fns'] or '(none)'}",
        "",
        "## State0 arm `0x3004C8`",
        "",
        json.dumps(data["state0_arm"], indent=2),
        "",
        "## Ladder hypothesis",
        "",
    ]
    for x in data["ladder_hypothesis"]:
        lines.append(f"- {x}")
    lines += ["", "## Imm#38 writer sites (if any)", ""]
    if not data["imm38_writers"]:
        lines.append("- (none with adjacent MOVS #38 — value likely computed/copied)")
    for w in data["imm38_writers"]:
        lines.append(
            f"- STR@`0x{w['str_pc']:X}` fn`0x{w['fn']:X}` val={w['value_source']}"
        )
    lines += ["", "## Direct writers (first 25)", ""]
    for w in data["direct_writers"][:25]:
        lines.append(
            f"- STR@`0x{w['str_pc']:X}` w={w['width']} fn`0x{w['fn']:X}` "
            f"val={w['value_source']} callers={len(w['callers'])} "
            f"nz={w['can_nonzero']} ge20={w['can_ge20']} eq38={w['can_eq38']}"
        )
    lines += ["", f"BP: `{data['bp_spec'][:200]}...`", ""]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", type=Path, required=True)
    ap.add_argument("-o", type=Path, required=True)
    args = ap.parse_args()
    args.o.mkdir(parents=True, exist_ok=True)
    data = analyze(args.ext)
    (args.o / "state_writers.json").write_text(
        json.dumps(data, indent=2, default=str), encoding="utf-8"
    )
    write_md(data, args.o / "state_writers.md")
    (args.o / "e8n_bp_spec.txt").write_text(data["bp_spec"] + "\n", encoding="utf-8")
    # Slim writer PC list for BP
    pcs = [f"0x{w['str_pc']:X}" for w in data["direct_writers"][:64]]
    (args.o / "writer_str_pcs.txt").write_text(",".join(pcs) + "\n", encoding="utf-8")
    print(
        f"writers={data['direct_writer_count']} imm38={data['imm_eq38_count']} "
        f"shared_e6c={data['shared_state_e6c_fns']} bp_n={len(data['bp_spec'].split(','))}"
    )


if __name__ == "__main__":
    main()
