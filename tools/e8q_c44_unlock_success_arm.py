#!/usr/bin/env python3
"""Stage E8Q-Fast: C44 nonzero unlock + R1=20 success arm (0x301848 / 0x304558)."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CODE_BASE = 0x2D8DF4
FN_301848 = 0x301848
FN_304558 = 0x304558
FN_2FC8B8 = 0x2FC8B8
C44_UNLOCK = 0x2FC8CE  # STRB #1
C44_RESET = 0x2F4E82
SITE_30213E = 0x30213E


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
        if (h & 0xFF00) == 0xB500 or h == 0xE92D:
            return p
    return site


def pc_rel_lit(pc: int, imm8: int) -> int:
    return ((pc + 4) & ~2) + (imm8 << 2)


def decode_insn(blob: bytes, code_base: int, pc: int) -> Tuple[int, str, Dict[str, Any]]:
    off = pc - code_base
    if off < 0 or off + 1 >= len(blob):
        return 2, "???", {}
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {}
    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        t = bl_target(pc, h0, h1)
        if t is not None:
            meta["bl"] = t
            return 4, f"BL 0x{t:X}", meta
        return 4, f"t2 0x{h0:04X}_{h1:04X}", meta
    if (h0 & 0xFF00) == 0xB500:
        return 2, f"PUSH {{...,lr}} 0x{h0:04X}", meta
    if (h0 & 0xFE00) == 0xBC00:
        return 2, f"POP 0x{h0:04X}", meta
    if h0 == 0x4770:
        return 2, "BX lr", meta
    if (h0 & 0xFF00) == 0xDF00:
        return 2, f"SVC #0x{h0 & 0xFF:X}", meta
    if (h0 & 0xF800) == 0x2000:
        return 2, f"MOVS r{(h0 >> 8) & 7}, #{h0 & 0xFF}", {"movs": h0 & 0xFF, "rd": (h0 >> 8) & 7}
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
        tgt = (pc + 4 + imm * 2) & ~1
        return 2, f"Bcond({(h0 >> 8) & 0xF}) 0x{tgt:X}", {"btgt": tgt}
    if (h0 & 0xF800) == 0x4800:
        rd, imm8 = (h0 >> 8) & 7, h0 & 0xFF
        lit = pc_rel_lit(pc, imm8)
        val = u32(blob, lit - code_base) if 0 <= lit - code_base + 3 < len(blob) else None
        meta = {"ldr_pc": True, "rd": rd, "lit_val": val}
        return 2, f"LDR r{rd}, [pc] ; =0x{val:X}" if val is not None else f"LDR r{rd}, [pc]", meta
    if (h0 & 0xFF00) == 0x4400:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        return 2, f"ADD r{rd}, r{rm}", {"add_reg": True, "rd": rd, "rm": rm}
    if (h0 & 0xF800) == 0x6800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"LDR r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x6000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, ((h0 >> 6) & 0x1F) << 2
        return 2, f"STR r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x7000:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        return 2, f"STRB r{rt}, [r{rn}, #0x{imm:X}]", {"strb": True, "rt": rt}
    if (h0 & 0xF800) == 0x7800:
        rt, rn, imm = h0 & 7, (h0 >> 3) & 7, (h0 >> 6) & 0x1F
        return 2, f"LDRB r{rt}, [r{rn}, #0x{imm:X}]", meta
    if (h0 & 0xFFC0) == 0x1C00:
        return 2, f"MOVS r{h0 & 7}, r{(h0 >> 3) & 7}", meta
    if (h0 & 0xF800) == 0x3000:
        return 2, f"ADDS r{(h0 >> 8) & 7}, #{h0 & 0xFF}", meta
    if (h0 & 0xFF80) == 0xB080:
        return 2, f"SUB sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xFF80) == 0xB000:
        return 2, f"ADD sp, #0x{(h0 & 0x7F) << 2:X}", meta
    if (h0 & 0xF800) == 0x9800:
        return 2, f"LDR r{(h0 >> 8) & 7}, [sp, #0x{(h0 & 0xFF) << 2:X}]", meta
    if (h0 & 0xF800) == 0x9000:
        return 2, f"STR r{(h0 >> 8) & 7}, [sp, #0x{(h0 & 0xFF) << 2:X}]", meta
    return 2, f"raw 0x{h0:04X}", meta


def disasm_range(blob: bytes, code_base: int, start: int, end: int) -> List[str]:
    lines: List[str] = []
    pc = start
    while pc < end:
        size, text, _ = decode_insn(blob, code_base, pc)
        lines.append(f"0x{pc:X}: {text}")
        pc += size
    return lines


def classify_c44_stores(blob: bytes, code_base: int) -> List[Dict[str, Any]]:
    lit_vas = {code_base + o for o in range(0, len(blob) - 3, 4) if u32(blob, o) == 0xC44}
    out: List[Dict[str, Any]] = []
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
        for _ in range(20):
            if j + 1 >= len(blob):
                break
            pc2 = code_base + j
            size, text, meta = decode_insn(blob, code_base, pc2)
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                saw_add = True
            if saw_add and meta.get("strb") and "STRB" in text:
                # value source: scan back for MOVS into rt
                src = "unknown"
                val = None
                for back in range(2, 32, 2):
                    p = pc2 - back
                    if p < code_base:
                        break
                    _, t2, m2 = decode_insn(blob, code_base, p)
                    if m2.get("movs") is not None and m2.get("rd") == meta.get("rt"):
                        src = f"MOVS#{m2['movs']}"
                        val = m2["movs"]
                        break
                fn = find_fn_start(blob, code_base, pc2)
                callers = find_bl_callers(blob, code_base, fn)[:16]
                kind = "write_0" if val == 0 else ("write_1" if val == 1 else "write_other_or_reg")
                out.append(
                    {
                        "store_pc": f"0x{pc2:X}",
                        "fn": f"0x{fn:X}",
                        "src": src,
                        "kind": kind,
                        "text": text,
                        "callers": [f"0x{c:X}" for c in callers],
                        "n_callers": len(find_bl_callers(blob, code_base, fn)),
                    }
                )
                break
            j += size
    return out


def field_writers(blob: bytes, code_base: int, base_off: int, field: int, width: int) -> List[Dict[str, Any]]:
    """Find stores to R9+base_off+field via LDR lit base_off + ADD r9 + STR/STRB at field."""
    lit_vas = {code_base + o for o in range(0, len(blob) - 3, 4) if u32(blob, o) == base_off}
    out: List[Dict[str, Any]] = []
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
        for _ in range(24):
            if j + 1 >= len(blob):
                break
            pc2 = code_base + j
            size, text, meta = decode_insn(blob, code_base, pc2)
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                saw_add = True
            if saw_add and (text.startswith("STR") or text.startswith("STRB") or text.startswith("STRH")):
                # parse imm from text roughly
                if f"#0x{field:X}" in text or (field < 10 and f"#{field}" in text):
                    fn = find_fn_start(blob, code_base, pc2)
                    out.append(
                        {
                            "store_pc": f"0x{pc2:X}",
                            "fn": f"0x{fn:X}",
                            "text": text,
                            "callers": [f"0x{c:X}" for c in find_bl_callers(blob, code_base, fn)[:10]],
                        }
                    )
                    break
            j += size
    return out


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

    c44 = classify_c44_stores(blob, cb)
    unlock = [x for x in c44 if x["kind"] == "write_1"]
    reset = [x for x in c44 if x["kind"] == "write_0"]

    fn_2fc8 = find_fn_start(blob, cb, FN_2FC8B8)
    callers_2fc8 = find_bl_callers(blob, cb, fn_2fc8)
    callers_301848 = find_bl_callers(blob, cb, FN_301848)
    callers_304558 = find_bl_callers(blob, cb, FN_304558)

    # BLs inside 301848 / 304558 / 2FC8B8
    def bls_in(start: int, nbytes: int) -> List[Dict[str, str]]:
        rows = []
        for line in disasm_range(blob, cb, start, start + nbytes):
            if ": BL " in line:
                pc_s, rest = line.split(": ", 1)
                rows.append({"pc": pc_s, "text": rest})
        return rows

    report = {
        "c44_stores": c44,
        "c44_unlock_writers": unlock,
        "c44_reset_writers": reset,
        "fn_2fc8b8": f"0x{fn_2fc8:X}",
        "callers_2fc8b8": [f"0x{c:X}" for c in callers_2fc8],
        "callers_301848": [f"0x{c:X}" for c in callers_301848[:30]],
        "n_callers_301848": len(callers_301848),
        "callers_304558_sample": [f"0x{c:X}" for c in callers_304558[:20]],
        "n_callers_304558": len(callers_304558),
        "bls_301848": bls_in(FN_301848, 0x120),
        "bls_304558": bls_in(FN_304558, 0x80),
        "bls_2fc8b8": bls_in(fn_2fc8, 0x60),
        "field_c6c22": field_writers(blob, cb, 0xC6C, 0x22, 1),
        "field_dec30": field_writers(blob, cb, 0xDEC, 0x30, 4),
        "field_eec7c": field_writers(blob, cb, 0xEEC, 0x7C, 4),
        "field_1a8": field_writers(blob, cb, 0x1A8, 0, 1),
    }
    (out / "e8q_deps.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    md = [
        "# E8Q-Fast: C44 nonzero unlock + R1=20 success arm",
        "",
        "## C44 store classification",
        "",
        "| store_pc | kind | src | fn | n_callers |",
        "| --- | --- | --- | --- | --- |",
    ]
    for x in c44:
        md.append(
            f"| `{x['store_pc']}` | `{x['kind']}` | `{x['src']}` | `{x['fn']}` | {x['n_callers']} |"
        )

    md += [
        "",
        "## Unlock writers (MOVS#1 → C44)",
        "",
    ]
    for x in unlock:
        md.append(f"- `{x['store_pc']}` in `{x['fn']}` callers={x['callers'][:12]}")

    md += [
        "",
        f"## Function `0x{fn_2fc8:X}` (historical C44 enable / around 0x2FC8B8)",
        "",
        f"- BL callers ({len(callers_2fc8)}): " + ", ".join(f"`0x{c:X}`" for c in callers_2fc8[:24]),
        "",
        "```",
        *disasm_range(blob, cb, fn_2fc8, fn_2fc8 + 0x50),
        "```",
        "",
        "## 0x301848",
        "",
        f"- BL callers: {len(callers_301848)} (sample: "
        + ", ".join(f"`0x{c:X}`" for c in callers_301848[:12])
        + ")",
        "",
        "```",
        *disasm_range(blob, cb, FN_301848, FN_301848 + 0x100),
        "```",
        "",
        "### BL targets inside 0x301848",
        "",
    ]
    for b in report["bls_301848"]:
        md.append(f"- `{b['pc']}` {b['text']}")

    md += [
        "",
        "## 0x304558",
        "",
        f"- BL callers: {len(callers_304558)}",
        "",
        "```",
        *disasm_range(blob, cb, FN_304558, FN_304558 + 0x60),
        "```",
        "",
        "## Success arm site 0x30213E",
        "",
        "```",
        *disasm_range(blob, cb, SITE_30213E, 0x302178),
        "```",
        "",
        "## Embedded field writers",
        "",
        "### C6C+0x22",
        "",
    ]
    for x in report["field_c6c22"][:20]:
        md.append(f"- `{x['store_pc']}` `{x['text']}` fn=`{x['fn']}` callers={x['callers'][:6]}")
    md += ["", "### DEC+0x30", ""]
    for x in report["field_dec30"][:20]:
        md.append(f"- `{x['store_pc']}` `{x['text']}` fn=`{x['fn']}` callers={x['callers'][:6]}")
    md += ["", "### EEC+0x7C", ""]
    for x in report["field_eec7c"][:15]:
        md.append(f"- `{x['store_pc']}` `{x['text']}` fn=`{x['fn']}` callers={x['callers'][:6]}")
    md += ["", "### R9+0x1A8", ""]
    for x in report["field_1a8"][:15]:
        md.append(f"- `{x['store_pc']}` `{x['text']}` fn=`{x['fn']}` callers={x['callers'][:6]}")

    (out / "e8q_deps.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print(f"wrote {out / 'e8q_deps.md'}")
    print(f"unlock={[x['store_pc'] for x in unlock]}")
    print(f"2fc8_callers={len(callers_2fc8)} 301848_callers={len(callers_301848)}")


if __name__ == "__main__":
    main()
