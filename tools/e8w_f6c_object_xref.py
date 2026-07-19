#!/usr/bin/env python3
"""Stage E8W: decode 0x2E88CC F6C struct gate + xref writers to R9+F6C/F70/F74."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CODE_BASE = 0x2D8DF4
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


def u16(b: bytes, o: int) -> int:
    return struct.unpack_from("<H", b, o)[0]


def u32(b: bytes, o: int) -> int:
    return struct.unpack_from("<I", b, o)[0]


def sx(v: int, bits: int) -> int:
    s = 1 << (bits - 1)
    return (v & (s - 1)) - (v & s)


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
    imm32 = sx(imm32, 25)
    return (pc + 4 + imm32) & ~1


def decode(blob: bytes, pc: int) -> Tuple[int, str, Dict[str, Any]]:
    off = pc - CODE_BASE
    if off < 0 or off + 1 >= len(blob):
        return 2, "???", {}
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {}
    if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        t = bl_target(pc, h0, h1)
        if t is not None:
            kind = "BL" if (h1 & 0x1000) else "BLX"
            meta["bl"] = t
            return 4, f"{kind} 0x{t:X}", meta
        if (h0 & 0xFFF0) == 0xF8D0:
            rn = h0 & 0xF
            rt = (h1 >> 12) & 0xF
            imm = h1 & 0xFFF
            return 4, f"LDR.W r{rt}, [r{rn}, #0x{imm:X}]", {
                "ldr": True,
                "rt": rt,
                "rn": rn,
                "imm": imm,
            }
        if (h0 & 0xFFF0) == 0xF8C0:
            rn = h0 & 0xF
            rt = (h1 >> 12) & 0xF
            imm = h1 & 0xFFF
            return 4, f"STR.W r{rt}, [r{rn}, #0x{imm:X}]", {
                "str": True,
                "rt": rt,
                "rn": rn,
                "imm": imm,
            }
        return 4, f"t2 0x{h0:04X}_{h1:04X}", meta
    if (h0 & 0xFF00) == 0xB500:
        return 2, f"PUSH {{...,lr}} 0x{h0:04X}", meta
    if (h0 & 0xFE00) == 0xBC00:
        return 2, f"POP 0x{h0:04X}", {"pop_pc": bool(h0 & 0x100)}
    if h0 == 0x4770:
        return 2, "BX lr", meta
    if (h0 & 0xF800) == 0x2000:
        return 2, f"MOVS r{(h0 >> 8) & 7}, #{h0 & 0xFF}", {
            "movs": h0 & 0xFF,
            "rd": (h0 >> 8) & 7,
        }
    if (h0 & 0xF800) == 0x2800:
        return 2, f"CMP r{(h0 >> 8) & 7}, #{h0 & 0xFF}", {"cmp": True}
    if (h0 & 0xF800) == 0xE000:
        imm = h0 & 0x7FF
        if imm >= 0x400:
            imm -= 0x800
        tgt = (pc + 4 + imm * 2) & ~1
        return 2, f"B 0x{tgt:X}", {"b": tgt}
    if (h0 & 0xF000) == 0xD000 and (h0 & 0xF00) != 0xF00:
        imm = h0 & 0xFF
        if imm >= 0x80:
            imm -= 0x100
        cond = (h0 >> 8) & 0xF
        tgt = (pc + 4 + imm * 2) & ~1
        return 2, f"B{COND.get(cond, '?')} 0x{tgt:X}", {"btgt": tgt, "cond": cond}
    if (h0 & 0xF800) == 0x4800:
        rd, imm8 = (h0 >> 8) & 7, h0 & 0xFF
        lit = ((pc + 4) & ~2) + (imm8 << 2)
        val = u32(blob, lit - CODE_BASE) if 0 <= lit - CODE_BASE + 3 < len(blob) else None
        return (
            2,
            f"LDR r{rd}, [pc] ; =0x{val:X}" if val is not None else f"LDR r{rd}, [pc]",
            {"ldr_pc": True, "rd": rd, "lit_val": val},
        )
    if (h0 & 0xFF00) == 0x4400:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"ADD r{rd}, r{rm}", {"add_reg": True, "rd": rd, "rm": rm}
    if (h0 & 0xF800) == 0x7000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        return 2, f"STRB r{rt}, [r{rn}, #0x{imm:X}]", {
            "strb": True,
            "rt": rt,
            "rn": rn,
            "imm": imm,
        }
    if (h0 & 0xF800) == 0x6000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"STR r{rt}, [r{rn}, #0x{imm:X}]", {
            "str": True,
            "rt": rt,
            "rn": rn,
            "imm": imm,
        }
    if (h0 & 0xF800) == 0x6800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"LDR r{rt}, [r{rn}, #0x{imm:X}]", {
            "ldr": True,
            "rt": rt,
            "rn": rn,
            "imm": imm,
        }
    if (h0 & 0xFE00) == 0x5E00:
        rt, rn, rm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 7
        return 2, f"LDRSH r{rt}, [r{rn}, r{rm}]", {"ldrsh": True}
    if (h0 & 0xFF80) == 0xB080:
        return 2, f"SUB sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xFF80) == 0xB000:
        return 2, f"ADD sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xFF00) == 0x4600:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"MOV r{rd}, r{rm}", meta
    if (h0 & 0xFE00) == 0x1C00:
        imm = (h0 >> 6) & 7
        rd, rn = h0 & 7, (h0 >> 3) & 7
        return 2, f"ADDS r{rd}, r{rn}, #{imm}", meta
    if (h0 & 0xF800) == 0x3000:
        return 2, f"ADDS r{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}", meta
    if (h0 & 0xF800) == 0x3800:
        return 2, f"SUBS r{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}", meta
    if (h0 & 0xFFC0) == 0x4240:
        return 2, f"NEGS r{h0 & 7}, r{(h0 >> 3) & 7}", meta
    if (h0 & 0xFFC0) == 0x4340:
        return 2, f"MULS r{h0 & 7}, r{(h0 >> 3) & 7}", meta
    if (h0 & 0xFFC0) == 0x4280:
        return 2, f"CMP r{h0 & 7}, r{(h0 >> 3) & 7}", meta
    if (h0 & 0xFE00) == 0x1800:
        return 2, f"ADDS r{h0 & 7}, r{(h0 >> 3) & 7}, r{(h0 >> 6) & 7}", meta
    if (h0 & 0xFE00) == 0x1A00:
        return 2, f"SUBS r{h0 & 7}, r{(h0 >> 3) & 7}, r{(h0 >> 6) & 7}", meta
    if (h0 & 0xF800) == 0x0000:
        return 2, f"LSLS r{h0 & 7}, r{(h0 >> 3) & 7}, #{(h0 >> 6) & 0x1F}", meta
    if (h0 & 0xF800) == 0x0800:
        return 2, f"LSRS r{h0 & 7}, r{(h0 >> 3) & 7}, #{(h0 >> 6) & 0x1F}", meta
    if (h0 & 0xF800) == 0x1000:
        return 2, f"ASRS r{h0 & 7}, r{(h0 >> 3) & 7}, #{(h0 >> 6) & 0x1F}", meta
    if (h0 & 0xF800) == 0x9000:
        return 2, f"STR r{(h0 >> 8) & 7}, [sp, #0x{(h0 & 0xFF) << 2:X}]", meta
    if (h0 & 0xF800) == 0x9800:
        return 2, f"LDR r{(h0 >> 8) & 7}, [sp, #0x{(h0 & 0xFF) << 2:X}]", meta
    if (h0 & 0xF800) == 0xA800:
        return 2, f"ADD r{(h0 >> 8) & 7}, sp, #0x{(h0 & 0xFF) << 2:X}", meta
    if (h0 & 0xF800) == 0x7800:
        return 2, f"LDRB r{h0 & 7}, [r{(h0 >> 3) & 7}, #0x{(h0 >> 6) & 0x1F:X}]", meta
    return 2, f"raw 0x{h0:04X}", meta


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


def disasm_e88cc(blob: bytes) -> Tuple[List[str], List[Tuple[int, int]]]:
    lines: List[str] = []
    bls: List[Tuple[int, int]] = []
    pc = 0x2E88CC
    while pc < 0x2E8A74:
        sz, text, meta = decode(blob, pc)
        mark = ""
        if meta.get("lit_val") in (0xF6C, 0xF70, 0xF74, 0xCEC, 0x818, 0x81C, 0x1A8, 0x116C):
            mark += "  ; << KEY_LIT"
        if meta.get("bl") in (
            0x2F2854,
            0x305BFC,
            0x2EA058,
            0x305E78,
            0x2F9970,
            0x2F99D0,
            0x2F5B38,
            0x2F9964,
        ):
            mark += "  ; << DRAW/HELPER"
        if pc in (
            0x2E8908,
            0x2E8910,
            0x2E8914,
            0x2E8920,
            0x2E8936,
            0x2E8980,
            0x2E89A8,
            0x2E8A22,
            0x2E8A44,
        ):
            mark += "  ; << GATE/DRAW_SITE"
        lines.append(f"0x{pc:X}: {text}{mark}")
        if meta.get("bl"):
            bls.append((pc, meta["bl"]))
        pc += sz
    return lines, bls


def find_writers(blob: bytes) -> List[Dict[str, Any]]:
    writers: List[Dict[str, Any]] = []
    for i in range(0, len(blob) - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = CODE_BASE + i
        lit = ((pc + 4) & ~2) + ((h & 0xFF) << 2)
        if not (0 <= lit - CODE_BASE + 3 < len(blob)):
            continue
        L = u32(blob, lit - CODE_BASE)
        if L not in (0xF6C, 0xF70, 0xF74):
            continue
        rd = (h >> 8) & 7
        j = i + 2
        saw = False
        base_off = L
        for _ in range(48):
            if j + 1 >= len(blob):
                break
            pc2 = CODE_BASE + j
            sz, text, meta = decode(blob, pc2)
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                saw = True
            if saw and meta.get("str"):
                imm = int(meta.get("imm") or 0)
                abs_off = base_off + imm
                if abs_off in (0xF6C, 0xF70, 0xF74) or (base_off == 0xF6C and imm in (0, 4, 8)):
                    fn = find_fn_start(blob, pc2)
                    callers = find_bl_callers(blob, fn)
                    writers.append(
                        {
                            "abs_off": abs_off,
                            "field": f"R9+0x{abs_off:X}",
                            "store_pc": pc2,
                            "insn": text,
                            "rt": meta.get("rt"),
                            "lit_pc": pc,
                            "lit": L,
                            "fn": fn,
                            "callers": callers[:16],
                            "callers_n": len(callers),
                        }
                    )
            j += sz
            if meta.get("pop_pc") or text == "BX lr":
                break
    seen = set()
    uniq: List[Dict[str, Any]] = []
    for w in writers:
        k = (w["store_pc"], w["abs_off"])
        if k in seen:
            continue
        seen.add(k)
        uniq.append(w)
    rel_map = {
        0x2E2520: "ui_2E2520",
        0x2E2F50: "ui_2E2F50",
        0x2E4788: "ui_2E4788",
        0x2E993C: "ui_2E993C",
        0x305E78: "sched_305E78",
        0x2E88CC: "e88cc_self",
    }
    for w in uniq:
        rel = []
        for c in w["callers"]:
            cfn = find_fn_start(blob, c)
            for t, name in rel_map.items():
                if abs(c - t) < 0x80 or abs(cfn - t) < 0x40:
                    rel.append(name)
            if 0x2E88CC <= c <= 0x2E8A74:
                rel.append("e88cc_self")
        w["relation"] = ",".join(sorted(set(rel))) or "none"
        w["store_pc_h"] = f"0x{w['store_pc']:X}"
        w["fn_h"] = f"0x{w['fn']:X}"
        w["callers_h"] = [f"0x{c:X}" for c in w["callers"]]
    return uniq


def object_layout_note() -> Dict[str, Any]:
    """Derived from 0x2E88CC gate decode (not pointer deref)."""
    return {
        "interpretation": "R9+0xF6C is EMBEDDED struct base, not a heap object pointer",
        "gate": {
            "pc_load_f74": "0x2E8908 LDR r0,[r5,#8] ; r5=R9+F6C => R9+0xF74",
            "pc_bne_f74": "0x2E890E BNE 0x2E8930 ; if F74!=0 skip F70 check",
            "pc_load_f70": "0x2E8910 LDR r0,[r5,#4] ; => R9+0xF70",
            "pc_beq_exit": "0x2E8914 BEQ 0x2E898C ; if F70==0 early exit",
            "open_condition": "R9+0xF74 != 0 OR R9+0xF70 != 0",
        },
        "fields": {
            "R9+0xF6C": "struct word0 (may be written elsewhere; gate does not require nonzero)",
            "R9+0xF70": "pointer/handle checked when F74==0; must be nonzero to pass",
            "R9+0xF74": "pointer/handle; if nonzero, skip F70 check and continue",
        },
        "after_gate": {
            "0x2E891A": "BL 0x2F99D0 then STR r0,[r5,#8] writes F74",
            "0x2E8936": "LDR r0,[r5,#8] uses F74 then BL 0x2F5B38",
        },
        "e8v_correction": "E8V treated [R9+F6C] as object pointer; actual gate reads adjacent words F70/F74",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--ext",
        default="out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext",
    )
    ap.add_argument("--out-dir", default="out/e8w_tmp")
    args = ap.parse_args()
    blob = Path(args.ext).read_bytes()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    lines, bls = disasm_e88cc(blob)
    (out / "e88cc_disasm.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    writers = find_writers(blob)
    report = {
        "code_base": f"0x{CODE_BASE:X}",
        "object_layout": object_layout_note(),
        "bls": [{"pc": f"0x{a:X}", "target": f"0x{b:X}"} for a, b in bls],
        "writers": writers,
        "draw_targets": {
            "0x2F2854": [f"0x{a:X}" for a, b in bls if b == 0x2F2854],
            "0x305BFC": [f"0x{a:X}" for a, b in bls if b == 0x305BFC],
            "0x2EA058": [f"0x{a:X}" for a, b in bls if b == 0x2EA058],
        },
    }
    (out / "f6c_object_report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )
    md = []
    md.append("# E8W F6C object layout / writers")
    md.append("")
    md.append("## Layout (corrected)")
    md.append("")
    md.append("- `R9+0xF6C` is an **embedded struct base**, not a heap object pointer.")
    md.append("- Gate open iff `*(R9+0xF74) != 0` **OR** `*(R9+0xF70) != 0`.")
    md.append("- Early exit `0x2E8914` is BEQ on **F70==0** after F74 was already 0.")
    md.append("")
    md.append("## Draw BLs from 0x2E88CC")
    md.append("")
    for t, pcs in report["draw_targets"].items():
        md.append(f"- `{t}` from: {', '.join(pcs) if pcs else '(none in window)'}")
    md.append("")
    md.append("## Writers")
    md.append("")
    md.append("| Store PC | Field | Fn | Callers | Relation | Insn |")
    md.append("| --- | --- | --- | --- | --- | --- |")
    for w in writers:
        md.append(
            f"| `{w['store_pc_h']}` | `{w['field']}` | `{w['fn_h']}` | {w['callers_n']} | "
            f"{w['relation']} | `{w['insn']}` |"
        )
    md.append("")
    (out / "f6c_object_notes.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print(f"writers={len(writers)} -> {out / 'f6c_object_report.json'}")
    for w in writers:
        print(
            f"  {w['store_pc_h']} {w['field']} fn={w['fn_h']} n={w['callers_n']} "
            f"rel={w['relation']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
