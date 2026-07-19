#!/usr/bin/env python3
"""Stage E8P-Fast: 0x3020C8 → C44 writer dependency cracking (static)."""
from __future__ import annotations

import argparse
import json
import struct
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

CODE_BASE = 0x2D8DF4
FN_3020C8 = 0x3020C8
FN_END = 0x302400  # approx; stop before next PUSH cluster
SITE_3021FA = 0x3021FA
SITE_302340 = 0x302340
SITE_302360 = 0x302360
SITE_302362 = 0x302362
FN_2F4E64 = 0x2F4E64
WRITER_2F4E82 = 0x2F4E82

OFF_C6C = 0xC6C
OFF_EEC = 0xEEC
OFF_11D0 = 0x11D0
TARGET_OFFS = {OFF_C6C, OFF_EEC, OFF_11D0}


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
            return p
        if h == 0xE92D:
            return p
    return site


def pc_rel_lit(pc: int, imm8: int) -> int:
    return ((pc + 4) & ~2) + (imm8 << 2)


def decode_insn(blob: bytes, code_base: int, pc: int) -> Tuple[int, str, Dict[str, Any]]:
    """Return (size, text, meta)."""
    off = pc - code_base
    if off < 0 or off + 1 >= len(blob):
        return 2, "???", {}
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {"raw": f"0x{h0:04X}"}

    # BL
    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        t = bl_target(pc, h0, h1)
        if t is not None:
            meta["bl"] = t
            return 4, f"BL 0x{t:X}", meta
        return 4, f"t2 0x{h0:04X}_{h1:04X}", meta

    # PUSH
    if (h0 & 0xFE00) == 0xB400:
        return 2, f"PUSH 0x{h0:04X}", meta
    if (h0 & 0xFF00) == 0xB500:
        return 2, f"PUSH {{...,lr}} 0x{h0:04X}", meta

    # POP
    if (h0 & 0xFE00) == 0xBC00:
        return 2, f"POP 0x{h0:04X}", meta

    # SVC
    if (h0 & 0xFF00) == 0xDF00:
        meta["svc"] = h0 & 0xFF
        return 2, f"SVC #0x{h0 & 0xFF:X}", meta

    # MOVS Rd,#imm8
    if (h0 & 0xF800) == 0x2000:
        rd, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"movs": True, "rd": rd, "imm": imm})
        return 2, f"MOVS r{rd}, #{imm}", meta

    # CMP Rn,#imm8
    if (h0 & 0xF800) == 0x2800:
        rn, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"cmp_imm": True, "rn": rn, "imm": imm})
        return 2, f"CMP r{rn}, #{imm}", meta

    # CMP Rm,Rn (low)
    if (h0 & 0xFFC0) == 0x4280:
        meta.update({"cmp_reg": True, "rn": h0 & 7, "rm": (h0 >> 3) & 7})
        return 2, f"CMP r{h0 & 7}, r{(h0 >> 3) & 7}", meta

    # B unconditional
    if (h0 & 0xF800) == 0xE000:
        imm = h0 & 0x7FF
        if imm >= 0x400:
            imm -= 0x800
        tgt = (pc + 4 + imm * 2) & ~1
        meta["b"] = tgt
        return 2, f"B 0x{tgt:X}", meta

    # Bcond
    if (h0 & 0xF000) == 0xD000 and (h0 & 0xF00) != 0xF00:
        imm = h0 & 0xFF
        if imm >= 0x80:
            imm -= 0x100
        cond = (h0 >> 8) & 0xF
        tgt = (pc + 4 + imm * 2) & ~1
        names = {
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
        meta.update({"bcond": cond, "btgt": tgt})
        return 2, f"B{names.get(cond, str(cond))} 0x{tgt:X}", meta

    # LDR Rd,[pc,#imm]
    if (h0 & 0xF800) == 0x4800:
        rd, imm8 = (h0 >> 8) & 7, h0 & 0xFF
        lit = pc_rel_lit(pc, imm8)
        val = None
        lo = lit - code_base
        if 0 <= lo + 3 < len(blob):
            val = u32(blob, lo)
        meta.update({"ldr_pc": True, "rd": rd, "lit": lit, "lit_val": val})
        return 2, f"LDR r{rd}, [pc,#0x{imm8 << 2:X}] ; =0x{val:X}" if val is not None else f"LDR r{rd}, [pc,#imm]", meta

    # ADD Rd, Rn, #imm (add sp / etc) — ADD Rd, #imm8
    if (h0 & 0xF800) == 0x3000:
        rd, imm = (h0 >> 8) & 7, h0 & 0xFF
        meta.update({"add_imm": True, "rd": rd, "imm": imm})
        return 2, f"ADDS r{rd}, #{imm}", meta

    # SUBS Rd,#imm8
    if (h0 & 0xF800) == 0x3800:
        rd, imm = (h0 >> 8) & 7, h0 & 0xFF
        return 2, f"SUBS r{rd}, #{imm}", meta

    # ADD Rd, Rm (high regs incl r9)
    if (h0 & 0xFF00) == 0x4400:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        meta.update({"add_reg": True, "rd": rd, "rm": rm})
        return 2, f"ADD r{rd}, r{rm}", meta

    # MOV Rd, Rm (high)
    if (h0 & 0xFF00) == 0x4600:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        meta.update({"mov_reg": True, "rd": rd, "rm": rm})
        return 2, f"MOV r{rd}, r{rm}", meta

    # MOVS Rd, Rm (low)
    if (h0 & 0xFFC0) == 0x1C00:
        rd, rm = h0 & 7, (h0 >> 3) & 7
        meta.update({"movs_reg": True, "rd": rd, "rm": rm})
        return 2, f"MOVS r{rd}, r{rm}", meta
    if (h0 & 0xFFC0) == 0x0000 and (h0 & 0x3F) != 0:  # LSLS often
        pass

    # LDR Rd,[Rn,#imm5*4]
    if (h0 & 0xF800) == 0x6800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        meta.update({"ldr_imm": True, "rt": rt, "rn": rn, "imm": imm})
        return 2, f"LDR r{rt}, [r{rn}, #0x{imm:X}]", meta

    # STR Rd,[Rn,#imm5*4]
    if (h0 & 0xF800) == 0x6000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        meta.update({"str_imm": True, "rt": rt, "rn": rn, "imm": imm})
        return 2, f"STR r{rt}, [r{rn}, #0x{imm:X}]", meta

    # LDRB
    if (h0 & 0xF800) == 0x7800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        meta.update({"ldrb": True, "rt": rt, "rn": rn, "imm": imm})
        return 2, f"LDRB r{rt}, [r{rn}, #0x{imm:X}]", meta

    # STRB
    if (h0 & 0xF800) == 0x7000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        meta.update({"strb": True, "rt": rt, "rn": rn, "imm": imm})
        return 2, f"STRB r{rt}, [r{rn}, #0x{imm:X}]", meta

    # LDRH
    if (h0 & 0xF800) == 0x8800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 1
        meta.update({"ldrh": True, "rt": rt, "rn": rn, "imm": imm})
        return 2, f"LDRH r{rt}, [r{rn}, #0x{imm:X}]", meta

    # STRH
    if (h0 & 0xF800) == 0x8000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 1
        meta.update({"strh": True, "rt": rt, "rn": rn, "imm": imm})
        return 2, f"STRH r{rt}, [r{rn}, #0x{imm:X}]", meta

    # LDR Rd,[SP,#imm]
    if (h0 & 0xF800) == 0x9800:
        rd, imm = (h0 >> 8) & 7, (h0 & 0xFF) << 2
        return 2, f"LDR r{rd}, [sp, #0x{imm:X}]", meta

    # STR Rd,[SP,#imm]
    if (h0 & 0xF800) == 0x9000:
        rd, imm = (h0 >> 8) & 7, (h0 & 0xFF) << 2
        meta.update({"str_sp": True, "rd": rd, "imm": imm})
        return 2, f"STR r{rd}, [sp, #0x{imm:X}]", meta

    # ADD SP,#imm / SUB SP
    if (h0 & 0xFF80) == 0xB080:
        return 2, f"SUB sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xFF80) == 0xB000:
        return 2, f"ADD sp, #0x{(h0 & 0x7F) << 2:X}", meta

    # BX lr
    if h0 == 0x4770:
        return 2, "BX lr", meta

    # LSLS / etc fallback
    return 2, f"raw 0x{h0:04X}", meta


def disasm_range(blob: bytes, code_base: int, start: int, end: int) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    pc = start
    while pc < end:
        size, text, meta = decode_insn(blob, code_base, pc)
        row = {"pc": pc, "text": text, **meta}
        out.append(row)
        pc += size
    return out


def track_r9_slots(insns: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Lightweight symbolic: reg -> ('addr', off) | ('val', name) | ('imm', n) | None."""
    reg: Dict[int, Any] = {}
    events: List[Dict[str, Any]] = []
    for ins in insns:
        pc = ins["pc"]
        if ins.get("ldr_pc") and ins.get("lit_val") is not None:
            reg[ins["rd"]] = ("imm_off", ins["lit_val"])
            if ins["lit_val"] in TARGET_OFFS:
                events.append({"pc": pc, "kind": "load_offset_lit", "rd": ins["rd"], "off": ins["lit_val"]})
        elif ins.get("add_reg") and ins["rm"] == 9:
            rd = ins["rd"]
            cur = reg.get(rd)
            if cur and cur[0] == "imm_off" and cur[1] in TARGET_OFFS:
                reg[rd] = ("addr", cur[1])
                events.append({"pc": pc, "kind": "form_r9_addr", "rd": rd, "off": cur[1]})
        elif ins.get("movs_reg") or ins.get("mov_reg"):
            rd, rm = ins["rd"], ins["rm"]
            if rm in reg:
                reg[rd] = reg[rm]
        elif ins.get("ldr_imm"):
            rn, rt, imm = ins["rn"], ins["rt"], ins["imm"]
            base = reg.get(rn)
            if base and base[0] == "addr" and base[1] in TARGET_OFFS and imm == 0:
                # load pointer value from R9+off
                reg[rt] = ("ptr_val", base[1])
                events.append({"pc": pc, "kind": "deref_slot_ptr", "rt": rt, "off": base[1], "field": 0})
            elif base and base[0] == "ptr_val":
                events.append(
                    {
                        "pc": pc,
                        "kind": "deref_obj_field",
                        "rt": rt,
                        "obj_off": base[1],
                        "field": imm,
                    }
                )
                reg[rt] = ("field", base[1], imm)
            elif base and base[0] == "addr":
                events.append({"pc": pc, "kind": "load_from_r9_addr", "rt": rt, "off": base[1], "field": imm})
        elif ins.get("ldrb") or ins.get("ldrh"):
            rn, rt, imm = ins["rn"], ins["rt"], ins["imm"]
            base = reg.get(rn)
            if base and base[0] == "ptr_val":
                events.append(
                    {
                        "pc": pc,
                        "kind": "deref_obj_field_b",
                        "rt": rt,
                        "obj_off": base[1],
                        "field": imm,
                        "width": "b" if ins.get("ldrb") else "h",
                    }
                )
            elif base and base[0] == "addr" and base[1] in TARGET_OFFS:
                events.append({"pc": pc, "kind": "load_slot_byte", "rt": rt, "off": base[1], "field": imm})
        elif ins.get("str_imm"):
            rn, rt, imm = ins["rn"], ins["rt"], ins["imm"]
            base = reg.get(rn)
            if base and base[0] == "addr" and base[1] == (0x800 + 0xD0):
                events.append({"pc": pc, "kind": "state_write", "rt": rt, "field": imm})
            elif base and base[0] == "addr":
                events.append({"pc": pc, "kind": "store_r9", "off": base[1], "field": imm, "rt": rt})
        elif ins.get("cmp_imm"):
            rn, imm = ins["rn"], ins["imm"]
            cur = reg.get(rn)
            events.append({"pc": pc, "kind": "cmp_imm", "rn": rn, "imm": imm, "sym": cur})
        elif ins.get("cmp_reg"):
            events.append({"pc": pc, "kind": "cmp_reg", "rn": ins["rn"], "rm": ins["rm"], "sym_n": reg.get(ins["rn"]), "sym_m": reg.get(ins["rm"])})
        elif "bl" in ins:
            events.append({"pc": pc, "kind": "bl", "tgt": ins["bl"], "r0": reg.get(0), "r1": reg.get(1), "r4": reg.get(4)})
        elif "bcond" in ins:
            events.append({"pc": pc, "kind": "bcond", "cond": ins["bcond"], "tgt": ins["btgt"]})
        elif "b" in ins:
            events.append({"pc": pc, "kind": "b", "tgt": ins["b"]})
    return events


def find_literal_offset_uses(blob: bytes, code_base: int, offset_val: int) -> List[Dict[str, Any]]:
    """All LDR pc-lit of offset + following ADD rN,r9 and nearby LDR/STR/CMP."""
    lit_vas = set()
    for off in range(0, len(blob) - 3, 4):
        if u32(blob, off) == offset_val:
            lit_vas.add(code_base + off)
    hits: List[Dict[str, Any]] = []
    for i in range(0, len(blob) - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = code_base + i
        lit = pc_rel_lit(pc, h & 0xFF)
        if lit not in lit_vas:
            continue
        rd = (h >> 8) & 7
        window = []
        j = i + 2
        formed = False
        for _ in range(16):
            if j + 1 >= len(blob):
                break
            pc2 = code_base + j
            size, text, meta = decode_insn(blob, code_base, pc2)
            window.append(f"0x{pc2:X}: {text}")
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                formed = True
            if formed and (meta.get("ldr_imm") or meta.get("str_imm") or meta.get("cmp_imm") or meta.get("ldrb")):
                hits.append(
                    {
                        "ldr_pc": f"0x{pc:X}",
                        "rd": rd,
                        "off": f"0x{offset_val:X}",
                        "use_pc": f"0x{pc2:X}",
                        "use": text,
                        "window": window[:12],
                    }
                )
                break
            j += size
        else:
            hits.append(
                {
                    "ldr_pc": f"0x{pc:X}",
                    "rd": rd,
                    "off": f"0x{offset_val:X}",
                    "use_pc": None,
                    "use": None,
                    "window": window[:12],
                }
            )
    return hits


def find_direct_stores(blob: bytes, code_base: int, offset_val: int) -> List[Dict[str, Any]]:
    lit_vas = {code_base + off for off in range(0, len(blob) - 3, 4) if u32(blob, off) == offset_val}
    hits: List[Dict[str, Any]] = []
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
        for _ in range(12):
            if j + 1 >= len(blob):
                break
            pc2 = code_base + j
            size, text, meta = decode_insn(blob, code_base, pc2)
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                saw_add = True
            if saw_add and (meta.get("str_imm") or meta.get("strb") or meta.get("strh")):
                if meta.get("rn") == rd or meta.get("str_imm") and meta.get("rn") == rd:
                    fn = find_fn_start(blob, code_base, pc2)
                    callers = find_bl_callers(blob, code_base, fn)[:12]
                    hits.append(
                        {
                            "kind": text.split()[0],
                            "store_pc": f"0x{pc2:X}",
                            "ldr_pc": f"0x{pc:X}",
                            "fn": f"0x{fn:X}",
                            "callers": [f"0x{c:X}" for c in callers],
                            "text": text,
                        }
                    )
                    break
            j += size
    return hits


def path_trace_notes(insns: List[Dict[str, Any]], case_r4: int) -> Dict[str, Any]:
    """Document branch decisions for a given r4 case value (18 or 20)."""
    # Build CFG edges from B/Bcond/BL-fallthrough
    lines = []
    for ins in insns:
        lines.append(f"0x{ins['pc']:X}: {ins['text']}")

    # Find CMP r4, #N near head and record taken targets for this case
    decisions = []
    for i, ins in enumerate(insns):
        if ins.get("cmp_imm") and ins.get("rn") == 4:
            imm = ins["imm"]
            # look ahead for Bcond
            for j in range(i + 1, min(i + 4, len(insns))):
                n = insns[j]
                if "bcond" in n:
                    cond = n["bcond"]
                    tgt = n["btgt"]
                    # EQ=0, NE=1, GT=12, LE=13, GE=10, LT=11
                    taken = None
                    if cond == 0:  # EQ
                        taken = case_r4 == imm
                    elif cond == 1:  # NE
                        taken = case_r4 != imm
                    elif cond == 12:  # GT
                        taken = case_r4 > imm
                    elif cond == 13:  # LE
                        taken = case_r4 <= imm
                    elif cond == 10:  # GE
                        taken = case_r4 >= imm
                    elif cond == 11:  # LT
                        taken = case_r4 < imm
                    decisions.append(
                        {
                            "cmp_pc": f"0x{ins['pc']:X}",
                            "cmp": f"r4,#{imm}",
                            "bcond_pc": f"0x{n['pc']:X}",
                            "cond": cond,
                            "tgt": f"0x{tgt:X}",
                            "taken_for_case": taken,
                        }
                    )
                    break
    return {"case_r4": case_r4, "cmp_decisions": decisions, "disasm_excerpt": lines}


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

    insns = disasm_range(blob, cb, FN_3020C8, FN_END)
    events = track_r9_slots(insns)

    # Focus windows
    arms = {
        "r1_18_region": disasm_range(blob, cb, 0x302170, 0x302220),
        "r1_20_region": disasm_range(blob, cb, 0x302300, 0x302380),
        "head": disasm_range(blob, cb, FN_3020C8, 0x302140),
        "writer_call": disasm_range(blob, cb, 0x302330, 0x302380),
        "fn_2f4e64": disasm_range(blob, cb, FN_2F4E64, FN_2F4E64 + 0x80),
    }

    uses = {f"0x{o:X}": find_literal_offset_uses(blob, cb, o) for o in TARGET_OFFS}
    stores = {f"0x{o:X}": find_direct_stores(blob, cb, o) for o in TARGET_OFFS}

    # Also word-cover stores
    for o in list(TARGET_OFFS):
        wo = o & ~3
        if wo != o:
            stores[f"0x{o:X}_wordcover"] = find_direct_stores(blob, cb, wo)

    path18 = path_trace_notes(insns, 18)
    path20 = path_trace_notes(insns, 20)

    # Collect BL targets inside 3020C8
    bls = [i for i in insns if "bl" in i]

    # Null-check heuristic: LDR from addr-slot then CMP #0
    null_checks = []
    for i, ev in enumerate(events):
        if ev["kind"] == "deref_slot_ptr":
            for ev2 in events[i + 1 : i + 8]:
                if ev2["kind"] == "cmp_imm" and ev2["imm"] == 0 and ev2["rn"] == ev.get("rt"):
                    null_checks.append({"load": ev, "cmp": ev2})
                    break

    report = {
        "fn": f"0x{FN_3020C8:X}",
        "events_head": [e for e in events if e["pc"] < 0x302140][:80],
        "null_checks": null_checks,
        "bls": [{"pc": f"0x{i['pc']:X}", "tgt": f"0x{i['bl']:X}"} for i in bls],
        "uses": uses,
        "stores": stores,
        "path18_decisions": path18["cmp_decisions"],
        "path20_decisions": path20["cmp_decisions"],
        "writer_callers_2f4e64": [f"0x{c:X}" for c in find_bl_callers(blob, cb, FN_2F4E64)],
        "writer_callers_2f4e82_fn": [f"0x{c:X}" for c in find_bl_callers(blob, cb, find_fn_start(blob, cb, WRITER_2F4E82))],
    }

    (out / "e8p_deps.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    def fmt_ins(rows: List[Dict[str, Any]]) -> List[str]:
        return [f"0x{r['pc']:X}: {r['text']}" for r in rows]

    md: List[str] = [
        "# E8P-Fast: 0x3020C8 → C44 dependency static map",
        "",
        "## Function head (R9 slot address formation)",
        "",
        "```",
        *fmt_ins(arms["head"]),
        "```",
        "",
        "## Symbolic events (slot/obj)",
        "",
    ]
    for e in events:
        if e["pc"] > 0x302380:
            continue
        md.append(f"- `0x{e['pc']:X}` `{e['kind']}` { {k: v for k, v in e.items() if k not in ('pc', 'kind')} }")

    md += [
        "",
        "## Null checks on C6C/EEC/11D0 pointer loads",
        "",
    ]
    if not null_checks:
        md.append("- (none matched heuristic LDR-slot → CMP #0 on same rt)")
    for nc in null_checks:
        md.append(f"- load `{nc['load']}` cmp `{nc['cmp']}`")

    md += [
        "",
        "## Case r4=18 decisions (CMP/Bcond)",
        "",
    ]
    for d in path18["cmp_decisions"]:
        md.append(
            f"- CMP `{d['cmp']}` @ `{d['cmp_pc']}` → Bcond@{d['bcond_pc']} tgt `{d['tgt']}` taken={d['taken_for_case']}"
        )

    md += ["", "## Case r4=20 decisions", ""]
    for d in path20["cmp_decisions"]:
        md.append(
            f"- CMP `{d['cmp']}` @ `{d['cmp_pc']}` → Bcond@{d['bcond_pc']} tgt `{d['tgt']}` taken={d['taken_for_case']}"
        )

    md += [
        "",
        "## Region near R1=18 clear (0x302170..)",
        "",
        "```",
        *fmt_ins(arms["r1_18_region"]),
        "```",
        "",
        "## Region near R1=20 / C44 arm (0x302300..)",
        "",
        "```",
        *fmt_ins(arms["r1_20_region"]),
        "```",
        "",
        "## 0x2F4E64 head",
        "",
        "```",
        *fmt_ins(arms["fn_2f4e64"]),
        "```",
        "",
        "## Offset uses / stores",
        "",
    ]
    for o in ("0xC6C", "0xEEC", "0x11D0"):
        md.append(f"### {o} uses ({len(uses[o])})")
        for u in uses[o][:20]:
            md.append(f"- LDR@{u['ldr_pc']} r{u['rd']} use@{u['use_pc']}: `{u['use']}`")
        md.append(f"### {o} stores ({len(stores[o])})")
        for s in stores[o][:25]:
            md.append(
                f"- `{s['store_pc']}` `{s['text']}` fn=`{s['fn']}` callers={s['callers'][:6]}"
            )

    md += [
        "",
        "## BL sites inside 0x3020C8..0x302400",
        "",
    ]
    for b in report["bls"]:
        md.append(f"- `{b['pc']}` → `{b['tgt']}`")

    (out / "e8p_deps.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print(f"wrote {out / 'e8p_deps.md'}")
    print(f"wrote {out / 'e8p_deps.json'}")
    print(f"null_checks={len(null_checks)} stores_c6c={len(stores['0xC6C'])} uses_c6c={len(uses['0xC6C'])}")


if __name__ == "__main__":
    main()
