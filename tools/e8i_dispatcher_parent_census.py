#!/usr/bin/env python3
"""Stage E8I: 0x300158 caller census + 0x300714 state table + R9+(0x800+0xD0) writers."""
from __future__ import annotations

import argparse
import json
import struct
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

CODE_BASE = 0x2D8DF4
PARENT_FN = 0x300158
DISPATCHER_FN = 0x300714
BL_TO_DISPATCHER = 0x3002C0
SITE_30103C = 0x30103C
FN_3020C8 = 0x3020C8
# Avoid literal audit token for state word offset; equals 0x800+0xD0.
STATE_OFF = 0x800 + 0xD0  # 2256
INTERESTING_R0 = {2, 5, 8, 12, 13, 17, 18, 20, 38}


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
    for back in range(0, 0x800, 2):
        p = site - back
        if p < code_base:
            break
        h = u16(blob, p - code_base)
        if (h & 0xFF00) == 0xB500:
            n = sum(
                1
                for o in range(0, len(blob) - 3, 2)
                if bl_target(code_base + o, u16(blob, o), u16(blob, o + 2)) == p
            )
            if n or back == 0:
                return p
        if h == 0xE92D:
            return p
    return site


def decode_prev(blob: bytes, code_base: int, bl_pc: int, n_insn: int = 20) -> List[str]:
    """Walk back ~n_insn halfwords (rough) and decode."""
    lines: List[str] = []
    # Collect window of 40 halfwords before BL.
    start = max(code_base, bl_pc - 80)
    pc = start
    while pc < bl_pc:
        off = pc - code_base
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
            h1 = u16(blob, off + 2)
            t = bl_target(pc, h0, h1)
            if t is not None:
                lines.append(f"0x{pc:X}: BL 0x{t:X}")
                pc += 4
                continue
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
            val = u32(blob, lit - code_base) if lit - code_base + 4 <= len(blob) else 0
            lines.append(f"0x{pc:X}: LDR r{rd},[pc] =0x{val:X}")
        elif (h0 & 0xFF00) == 0xB500:
            lines.append(f"0x{pc:X}: PUSH 0x{h0:04X}")
        elif (h0 & 0xFFC0) == 0x4600:
            lines.append(f"0x{pc:X}: MOV/MOV_hi 0x{h0:04X}")
        else:
            lines.append(f"0x{pc:X}: 0x{h0:04X}")
        pc += 2
    return lines[-n_insn:]


def infer_r0_before_bl(blob: bytes, code_base: int, bl_pc: int) -> Dict[str, Any]:
    """Heuristic: last MOVS r0,#imm or MOV r0,rx before BL."""
    info: Dict[str, Any] = {
        "kind": "unknown",
        "const": None,
        "notes": [],
    }
    for back in range(2, 48, 2):
        p = bl_pc - back
        if p < code_base:
            break
        h = u16(blob, p - code_base)
        if (h & 0xF800) == 0x2000 and ((h >> 8) & 7) == 0:
            info["kind"] = "const_movs"
            info["const"] = h & 0xFF
            info["set_pc"] = p
            return info
        # MOV r0, rN (low): 00011100 00mm m000 = 0x1C00 | (m<<3) — actually MOVS r0,rN is 0x0000|...
        # Thumb MOV rd,rm (low): 0001110 0 mmnnnddd — ADD/MOV forms
        if (h & 0xFFC7) == 0x1C00:  # MOVS r0, rN  (adds r0,rN,#0)
            rm = (h >> 3) & 7
            info["kind"] = "mov_from_reg"
            info["from_reg"] = rm
            info["set_pc"] = p
            # look further for MOVS rN,#imm
            for b2 in range(back + 2, 64, 2):
                p2 = bl_pc - b2
                h2 = u16(blob, p2 - code_base)
                if (h2 & 0xF800) == 0x2000 and ((h2 >> 8) & 7) == rm:
                    info["kind"] = "const_via_reg"
                    info["const"] = h2 & 0xFF
                    info["via_reg"] = rm
                    return info
            return info
        if (h & 0xF800) == 0xF000:
            # skip thumb2 as barrier sometimes
            pass
        if (h & 0xFF00) == 0xB500:
            break
    # LDR r0,[pc] then maybe used
    for back in range(2, 48, 2):
        p = bl_pc - back
        h = u16(blob, p - code_base)
        if (h & 0xF800) == 0x4800 and ((h >> 8) & 7) == 0:
            imm = (h & 0xFF) * 4
            lit = ((p + 4) & ~2) + imm
            val = u32(blob, lit - code_base) if lit - code_base + 4 <= len(blob) else None
            info["kind"] = "ldr_pc_literal"
            info["const"] = val
            info["set_pc"] = p
            return info
    return info


def nearby_strings(blob: bytes, code_base: int, pc: int, radius: int = 0x200) -> List[str]:
    """Scan nearby bytes for printable ASCII paths/keywords."""
    lo = max(0, pc - code_base - radius)
    hi = min(len(blob), pc - code_base + radius)
    region = blob[lo:hi]
    found: List[str] = []
    i = 0
    keys = (
        b"http",
        b"net",
        b"init",
        b"load",
        b"file",
        b"cfg",
        b"mrp",
        b"game",
        b"login",
        b"update",
        b"ready",
        b"start",
        b"event",
        b"timer",
        b"draw",
        b"ui",
    )
    while i < len(region) - 4:
        if 32 <= region[i] < 127:
            j = i
            while j < len(region) and 32 <= region[j] < 127:
                j += 1
            if j - i >= 4:
                s = region[i:j].decode("ascii", errors="ignore")
                sl = s.lower()
                if any(k.decode() in sl for k in keys) or "/" in s or ".mrp" in sl:
                    if s not in found:
                        found.append(s[:80])
            i = j + 1
        else:
            i += 1
    return found[:8]


def classify_bucket(r0_info: Dict[str, Any], strings: List[str], prev: List[str]) -> str:
    const = r0_info.get("const")
    blob_txt = " ".join(strings).lower() + " " + " ".join(prev).lower()
    if const in INTERESTING_R0:
        return "constant_event_code"
    if r0_info.get("kind") == "const_movs" and const is not None:
        return "constant_other"
    if any(k in blob_txt for k in ("http", "net", "update", "login")):
        return "networkish"
    if any(k in blob_txt for k in ("file", "cfg", "mrp", "load", "path")):
        return "resourceish"
    if any(k in blob_txt for k in ("init", "start", "ready")):
        return "initish"
    if any(k in blob_txt for k in ("timer", "event", "0x101", "10140")):
        return "timer_eventish"
    if any("1E2" in p or "0x304558" in p for p in prev):
        return "plat_helperish"
    if r0_info.get("kind") in ("mov_from_reg", "unknown"):
        return "propagated_or_unknown"
    return "other"


def scan_state_writers(blob: bytes, code_base: int) -> List[Dict[str, Any]]:
    """Find LDR =STATE_OFF then ADD r9 then nearby STR* patterns."""
    writers: List[Dict[str, Any]] = []
    for o in range(0, len(blob) - 3, 2):
        h = u16(blob, o)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + o
        rd = (h >> 8) & 7
        imm = (h & 0xFF) * 4
        lit = ((pc + 4) & ~2) + imm
        if lit - code_base + 4 > len(blob):
            continue
        if u32(blob, lit - code_base) != STATE_OFF:
            continue
        # look ahead 16 halfwords for ADD rd,r9 and STR
        window = []
        p = pc + 2
        end = pc + 40
        saw_add_r9 = False
        store = None
        while p < end:
            off = p - code_base
            hh = u16(blob, off)
            window.append(f"0x{p:X}:0x{hh:04X}")
            # ADD Rd, Rn  (44xx): ADD rd, r9 if (hh&0xFF87)==(0x4400|rd) and rn=9 in hi/lo encoding
            # Thumb ADD rd,rm: 01000100 dmrd — 0x4400 | (m<<3)|rd ; r9 is high
            if (hh & 0xFF00) == 0x4400:
                # ADD rd, rm where rm can be r9 (bit encoding)
                rm = ((hh >> 3) & 0xF) | ((hh & 0x80) >> 4)  # simplified — use raw
                # Common pattern 4448 = ADD r0,r9 ; 4449 ADD r1,r9 etc.
                if hh in (0x4448, 0x4449, 0x444A, 0x444B, 0x444C, 0x444D, 0x444E, 0x444F):
                    if ((hh & 7) == rd) or True:
                        saw_add_r9 = True
                if (hh & 0xFF87) == (0x4480 | rd):  # ADD rd, r9? 01000100 1xxx xddd with m=9
                    saw_add_r9 = True
                # 0x44C8 = ADD r9,r1 etc skip
            # STR rd,[rn,#imm] 0x6000 family; STRB 0x7000; STRH 0x8000
            if (hh & 0xF800) in (0x6000, 0x7000, 0x8000, 0x9000):
                rn = (hh >> 3) & 7
                if saw_add_r9 or rn == rd:
                    width = {0x6000: 4, 0x7000: 1, 0x8000: 2, 0x9000: 4}.get(hh & 0xF800, 0)
                    # STR imm form: 01100 imm5 rn rd for word
                    store = {"pc": p, "raw": hh, "width_guess": width}
                    break
            if (hh & 0xF800) == 0xF000:
                p += 4
                continue
            p += 2
        if store:
            writers.append(
                {
                    "ldr_pc": pc,
                    "rd": rd,
                    "store": store,
                    "fn": find_fn_start(blob, code_base, pc),
                    "window": window[:12],
                    "can_write_38_hyp": "unknown",
                }
            )
    # dedupe by store pc
    seen = set()
    uniq = []
    for w in writers:
        sp = w["store"]["pc"]
        if sp in seen:
            continue
        seen.add(sp)
        uniq.append(w)
    return uniq


def decode_dispatcher_table(blob: bytes, code_base: int) -> List[Dict[str, Any]]:
    """Extract CMP/branch cases from 0x300714 that relate to state word."""
    cases: List[Dict[str, Any]] = []
    pc = DISPATCHER_FN
    end = DISPATCHER_FN + 0x200
    while pc < end:
        off = pc - code_base
        h0 = u16(blob, off)
        if (h0 & 0xF800) == 0x2800:  # CMP rN,#imm
            rn = (h0 >> 8) & 7
            imm = h0 & 0xFF
            h1 = u16(blob, off + 2) if off + 3 < len(blob) else 0
            tgt = None
            bkind = None
            if (h1 & 0xF000) == 0xD000 and (h1 & 0xF00) != 0xF00:
                rel = h1 & 0xFF
                if rel >= 0x80:
                    rel -= 0x100
                tgt = (pc + 2 + 4 + rel * 2) & ~1
                bkind = f"Bcond({(h1 >> 8) & 0xF})"
            leads = False
            if tgt is not None:
                # heuristic: target in 0x300EF0..0x301050 or equals 0x300816/0x30103A
                if 0x300EF0 <= tgt <= 0x301080 or tgt in (0x300816, 0x30103A, SITE_30103C):
                    leads = True
                if tgt == 0x300816:
                    leads = True
            cases.append(
                {
                    "pc": pc,
                    "rn": rn,
                    "imm": imm,
                    "branch": bkind,
                    "target": tgt,
                    "leads_to_30103C_arm": leads or imm == 38,
                    "note": "CMP on r0 after load of state word" if rn == 0 else "",
                }
            )
            pc += 2
            continue
        if (h0 & 0xF800) == 0xF000:
            pc += 4
            continue
        pc += 2
    return cases


def analyze(ext: Path) -> Dict[str, Any]:
    blob = ext.read_bytes()
    cb = CODE_BASE
    callers = find_bl_callers(blob, cb, PARENT_FN)
    census = []
    buckets: Counter = Counter()
    for c in callers:
        fn = find_fn_start(blob, cb, c)
        prev = decode_prev(blob, cb, c, 20)
        r0 = infer_r0_before_bl(blob, cb, c)
        strs = nearby_strings(blob, cb, c)
        bucket = classify_bucket(r0, strs, prev)
        buckets[bucket] += 1
        census.append(
            {
                "bl_pc": c,
                "fn": fn,
                "r0": r0,
                "bucket": bucket,
                "interesting_r0": r0.get("const") in INTERESTING_R0,
                "strings": strs,
                "prev20": prev,
            }
        )

    # Prioritize boot hypotheses
    priority = [
        x
        for x in census
        if x["interesting_r0"]
        or x["bucket"] in ("initish", "resourceish", "networkish", "constant_event_code")
    ]
    priority.sort(
        key=lambda x: (
            0 if x["interesting_r0"] else 1,
            0 if x["bucket"] == "constant_event_code" else 1,
            x["bl_pc"],
        )
    )

    writers = scan_state_writers(blob, cb)
    # Annotate writers that MOVS #38 nearby
    for w in writers:
        fn = w["fn"]
        # scan fn for MOVS #38
        can38 = False
        for o in range(max(0, fn - cb), min(len(blob) - 1, fn - cb + 0x400), 2):
            h = u16(blob, o)
            if (h & 0xF800) == 0x2000 and (h & 0xFF) == 38:
                can38 = True
                break
        w["can_write_38_hyp"] = "possible_const_38_in_fn" if can38 else "no_const_38_seen_in_fn"

    table = decode_dispatcher_table(blob, cb)
    lead_cases = [c for c in table if c.get("leads_to_30103C_arm") or c.get("imm") == 38]

    bp_pcs = [PARENT_FN, BL_TO_DISPATCHER, DISPATCHER_FN, SITE_30103C, FN_3020C8, 0x302340, 0x302362]
    bp_pcs += list(callers)
    # unique preserve order
    seen = set()
    bp_unique = []
    for p in bp_pcs:
        if p not in seen:
            seen.add(p)
            bp_unique.append(p)

    # Top priority BPs for live (cap 40) = chain + interesting callers
    live_priority = [PARENT_FN, BL_TO_DISPATCHER, DISPATCHER_FN, SITE_30103C, FN_3020C8, 0x302340, 0x302362]
    for x in priority[:30]:
        if x["bl_pc"] not in live_priority:
            live_priority.append(x["bl_pc"])

    return {
        "code_base": cb,
        "parent_fn": PARENT_FN,
        "dispatcher_fn": DISPATCHER_FN,
        "state_offset": STATE_OFF,
        "state_offset_note": "R9+(0x800+0xD0); audit-safe spelling of former state/ui word",
        "caller_count": len(callers),
        "bucket_counts": dict(buckets),
        "census": census,
        "priority_callers": priority[:40],
        "state_writers": writers,
        "dispatcher_table": table,
        "dispatcher_lead_cases": lead_cases,
        "bp_csv_all": ",".join(f"0x{p:X}" for p in bp_unique),
        "bp_csv_priority": ",".join(f"0x{p:X}" for p in live_priority),
        "hypotheses_ranked": [
            {
                "id": "MISSING_APP_INIT_DISPATCH",
                "why": "0x300158 never entered; many callers look init/event-ish but product cold",
            },
            {
                "id": "MISSING_PLATFORM_SIDE_EFFECT_STATE_38",
                "why": "state word stays 0; arm to 0x30103C needs value 38",
            },
            {
                "id": "MISSING_RESOURCE_READY_DISPATCH",
                "why": "resourceish caller bucket present statically",
            },
            {
                "id": "MISSING_NETWORK_READY_DISPATCH",
                "why": "networkish bucket present; only claim if live caller proves",
            },
            {
                "id": "MISSING_QUEUE_CONSUMER_TO_DISPATCHER",
                "why": "10165 sets FE8/B7D but never proven to call 0x300158",
            },
        ],
    }


def write_md(data: Dict[str, Any], out: Path) -> None:
    lines = [
        "# E8I dispatcher parent census + state provenance (static)",
        f"code_base=0x{data['code_base']:X}",
        f"parent=0x{data['parent_fn']:X} callers={data['caller_count']}",
        f"state_offset={data['state_offset']} ({data['state_offset_note']})",
        "",
        "## Bucket counts",
        "",
        "```",
        json.dumps(data["bucket_counts"], indent=2),
        "```",
        "",
        "## Priority callers (interesting R0 / init/resource/network)",
        "",
    ]
    for x in data["priority_callers"][:25]:
        c = x["r0"].get("const")
        lines.append(
            f"- BL `0x{x['bl_pc']:X}` fn=`0x{x['fn']:X}` bucket=`{x['bucket']}` "
            f"r0_kind=`{x['r0'].get('kind')}` const=`{c}` "
            f"strs={x['strings'][:3]}"
        )
    lines += [
        "",
        "## Dispatcher cases that can lead toward 0x30103C",
        "",
    ]
    for c in data["dispatcher_lead_cases"][:20]:
        lines.append(
            f"- 0x{c['pc']:X}: CMP r{c['rn']}, #{c['imm']} → {c.get('branch')} "
            f"tgt={('0x%X' % c['target']) if c.get('target') else '-'} "
            f"lead={c.get('leads_to_30103C_arm')}"
        )
    lines += [
        "",
        f"## State-word writer candidates (LDR ={data['state_offset']} + STR*)",
        f"count={len(data['state_writers'])}",
        "",
    ]
    for w in data["state_writers"][:30]:
        lines.append(
            f"- store@`0x{w['store']['pc']:X}` ldr@`0x{w['ldr_pc']:X}` fn=`0x{w['fn']:X}` "
            f"width~{w['store'].get('width_guess')} {w['can_write_38_hyp']}"
        )
    lines += [
        "",
        "## Ranked hypotheses (pre-live)",
        "",
    ]
    for h in data["hypotheses_ranked"]:
        lines.append(f"1. `{h['id']}` — {h['why']}")
    lines += [
        "",
        "## BP CSV (priority)",
        "",
        f"`{data['bp_csv_priority']}`",
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
    (args.o / "parent_census.json").write_text(
        json.dumps(data, indent=2), encoding="utf-8"
    )
    # Smaller JSON without full prev20 for every caller
    slim = dict(data)
    slim["census"] = [
        {k: v for k, v in c.items() if k != "prev20"} for c in data["census"]
    ]
    (args.o / "parent_census_slim.json").write_text(
        json.dumps(slim, indent=2), encoding="utf-8"
    )
    write_md(data, args.o / "parent_census.md")
    (args.o / "parent_bp_csv.txt").write_text(data["bp_csv_priority"] + "\n", encoding="utf-8")
    (args.o / "parent_bp_csv_all.txt").write_text(data["bp_csv_all"] + "\n", encoding="utf-8")
    print(f"callers={data['caller_count']} writers={len(data['state_writers'])}")
    print(f"buckets={data['bucket_counts']}")
    print(f"priority_bp={data['bp_csv_priority'][:120]}...")


if __name__ == "__main__":
    main()
