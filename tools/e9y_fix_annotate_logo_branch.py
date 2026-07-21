#!/usr/bin/env python3
"""E9Y-Fix Task 2: annotate 0x2EF86C splash logo/loading branch (Thumb).

Output: out/e9y_fix/2ef86c_logo_branch_annotated.txt
"""
from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def bl_target(pc: int, h0: int, h1: int):
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
    return (pc + 4 + imm32) | (1 if (h1 & 0x1000) else 0)


LANDMARKS = {
    0x2EF86C: "SPLASH_ENTRY",
    0x2EF8AC: "AC8_LDR_SITE",
    0x2EF8AE: "AC8_COMPARE_BRANCH",
    0x2EF992: "LOGO_MID",
    0x2EF9AA: "LOGO_BLIT_CALLSITE?",
    0x2EF9DE: "LOGO_TAIL",
    0x2D96F6: "NAME_WORKBUF_COPY",
    0x2D92E4: "RESOURCE_LOAD",
    0x2EC6B8: "BLIT",
    0x2EFA33: "LOADING_BRANCH",
    0x2EFA46: "LOADING_BAR",
}


def annotate_insn(pc: int, h0: int, h1: int | None, size: int) -> tuple[str, list[str]]:
    notes: list[str] = []
    desc = f"h0=0x{h0:04X}"
    if size == 4 and h1 is not None:
        desc = f"h0=0x{h0:04X} h1=0x{h1:04X}"
        tgt = bl_target(pc, h0, h1)
        if tgt is not None:
            kind = "BL" if (h1 & 0x1000) else "BLX"
            bare = tgt & ~1
            desc = f"{kind} -> 0x{bare:X}"
            if bare in LANDMARKS:
                notes.append(f"target={LANDMARKS[bare]}")
            if bare == 0x2D96F6:
                notes.append("name/workbuf copy (showN construction)")
            if bare == 0x2D92E4:
                notes.append("resource load")
            if bare == 0x2EC6B8:
                notes.append("blit")
            return desc, notes

    # Bcond
    if (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
        imm = sign_extend(h0 & 0xFF, 8) << 1
        tgt = (pc + 4 + imm) | 1
        cond = (h0 >> 8) & 0xF
        conds = [
            "EQ",
            "NE",
            "CS",
            "CC",
            "MI",
            "PL",
            "VS",
            "VC",
            "HI",
            "LS",
            "GE",
            "LT",
            "GT",
            "LE",
        ]
        cname = conds[cond] if cond < len(conds) else f"c{cond}"
        bare = tgt & ~1
        desc = f"B{cname} -> 0x{bare:X}"
        notes.append("COND_BRANCH")
        if pc == 0x2EF8AE or bare in (0x2EF992, 0x2EFA33, 0x2EF9F4):
            notes.append("AC8_GATE_OR_LOGO_LOADING_SPLIT")
        return desc, notes

    if (h0 & 0xF800) == 0xE000:
        imm = sign_extend(h0 & 0x7FF, 11) << 1
        tgt = (pc + 4 + imm) | 1
        desc = f"B -> 0x{tgt & ~1:X}"
        notes.append("UNCOND_BRANCH")
        return desc, notes

    # LDR Rt,[Rn,#imm] 01101
    if (h0 & 0xF800) == 0x6800:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        desc = f"LDR r{rt},[r{rn},#{imm}]"
        if rn == 9 or rn == 4:  # often r4 holds r9 base after mov
            notes.append(f"R9_REL_LOAD? rn=r{rn} imm=0x{imm:X}")
        return desc, notes

    if (h0 & 0xF800) == 0x6000:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        desc = f"STR r{rt},[r{rn},#{imm}]"
        notes.append(f"STORE rn=r{rn} imm=0x{imm:X}")
        return desc, notes

    # LDR lit
    if (h0 & 0xF800) == 0x4800:
        rt = (h0 >> 8) & 7
        imm = (h0 & 0xFF) << 2
        lit = ((pc + 4) & ~2) + imm
        desc = f"LDR r{rt},[PC,#0x{imm:X}] lit@0x{lit:X}"
        notes.append("LDR_LITERAL")
        return desc, notes

    # ADD/SUB rd,rn,#imm / high regs ADD
    if (h0 & 0xFF00) == 0x4400:
        rd = ((h0 >> 7) & 1) << 3 | (h0 & 7)
        rm = (h0 >> 3) & 0xF
        desc = f"ADD r{rd},r{rm}"
        if rm == 9 or rd == 9:
            notes.append("R9_TOUCH")
        return desc, notes

    if (h0 & 0xFF80) == 0x4700:
        rm = (h0 >> 3) & 0xF
        desc = f"{'BLX' if (h0 & 0x80) else 'BX'} r{rm}"
        return desc, notes

    if (h0 & 0xF800) == 0x2000:
        rd = (h0 >> 8) & 7
        imm = h0 & 0xFF
        desc = f"MOV r{rd},#{imm}"
        return desc, notes

    if (h0 & 0xF800) == 0x2800:
        rn = (h0 >> 8) & 7
        imm = h0 & 0xFF
        desc = f"CMP r{rn},#{imm}"
        if pc in (0x2EF8AC, 0x2EF8AE, 0x2EF8B0) or imm == 0:
            notes.append("CMP_FOR_AC8_BOOL_GATE?")
        return desc, notes

    return desc, notes


def enrich_literal(blob: bytes, code_base: int, lit_va: int) -> str:
    off = lit_va - code_base
    if 0 <= off <= len(blob) - 4:
        v = u32(blob, off)
        extra = f" =0x{v:X}"
        if v in (0xAC8, 0x8D8, 0x8D0, 0xA80, 0xA90, 0xABC, 0xBA0):
            extra += f"  ; R9_OFFSET"
            if v == 0xAC8:
                extra += " AC8_LOGO_GATE"
            if v == 0x8D8:
                extra += " STR_WORKBUF"
        return extra
    return ""


def disasm_range(blob: bytes, code_base: int, start: int, end: int) -> list[str]:
    lines: list[str] = []
    off = start - code_base
    end_off = end - code_base
    if off < 0 or end_off > len(blob):
        return [f"ERROR window out of range 0x{start:X}..0x{end:X}"]
    i = off
    while i + 1 < end_off:
        pc = code_base + i
        h0 = u16(blob, i)
        size = 2
        h1 = None
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0 and i + 3 < len(blob):
            h1 = u16(blob, i + 2)
            size = 4
        desc, notes = annotate_insn(pc, h0, h1, size)
        if (h0 & 0xF800) == 0x4800:
            imm = (h0 & 0xFF) << 2
            lit = ((pc + 4) & ~2) + imm
            desc += enrich_literal(blob, code_base, lit)
            # If lit is offset, next ADD rN,r9 then LDR/STR is R9+off
            notes.append(f"lit_va=0x{lit:X}")
        raw = blob[i : i + size].hex().upper()
        mark = LANDMARKS.get(pc, "")
        tag = f"  ; << {mark}" if mark else ""
        extra = ("  ; " + "; ".join(notes)) if notes else ""
        # Detect R9+imm via preceding lit load of offset — look back one LDR lit pattern in notes
        lines.append(f"  0x{pc:08X}: {raw:<10}  {desc}{tag}{extra}")
        i += size
    return lines


def scan_format_strings(blob: bytes) -> list[str]:
    hits = []
    for m in re.finditer(rb"show%?[0-9a-zA-Z!@._-]{0,40}", blob):
        hits.append(f"off=0x{m.start():X} {m.group()!r}")
    for m in re.finditer(rb"downimage%?[0-9a-zA-Z!@._-]{0,40}", blob, re.I):
        hits.append(f"off=0x{m.start():X} {m.group()!r}")
    for m in re.finditer(rb"[ -~]{3,}![0-9]+![0-9]+@[ -~]{1,32}\.[a-zA-Z0-9]+", blob):
        hits.append(f"off=0x{m.start():X} fmtish={m.group()!r}")
    return hits[:80]


def find_ext(args_ext: str | None) -> Path:
    if args_ext:
        return Path(args_ext)
    cands = [
        ROOT / "out" / "JJFB_E8A_delivery" / "02_mrp_extracted" / "jjfb" / "robotol.ext",
        ROOT / "out" / "e9y_fix" / "extracted" / "robotol.ext",
    ]
    for p in cands:
        if p.is_file():
            return p
    raise SystemExit("robotol.ext not found; run dump_jjfb_downimage_contract.py first or pass --ext")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", default=None)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument(
        "-o",
        "--out",
        default=str(ROOT / "out" / "e9y_fix" / "2ef86c_logo_branch_annotated.txt"),
    )
    args = ap.parse_args()
    ext = find_ext(args.ext)
    blob = ext.read_bytes()
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    windows = [
        ("splash_body_0x2EF86C", 0x2EF86C, 0x2EFD00),
        ("name_copy_0x2D96F6", 0x2D96BC, 0x2D9780),
        ("resource_load_0x2D92E4", 0x2D92E0, 0x2D9380),
        ("blit_0x2EC6B8", 0x2EC6B0, 0x2EC720),
    ]

    lines = [
        "# E9Y-Fix: 0x2EF86C logo branch annotated",
        f"ext={ext.as_posix()}",
        f"code_base=0x{args.code_base:X}",
        "",
        "## Semantics summary (static)",
        "- AC8 (R9+0xAC8): read near 0x2EF8AC, compare at 0x2EF8AE — acts as logo-path BOOL gate",
        "  (nonzero → logo/show@downimage; zero → loadingbar-only). Not progress count.",
        "- R9+0x8D8: workbuf pointer for name construction at 0x2D96F6 (showN!W!H@pack.bmp).",
        "- Static AC8 stores only clear/keep-zero (e.g. 0x2FB28C); no STR AC8=1 found.",
        "- show1/downimage names are built at runtime via format/copy — do not hardcode.",
        "",
        "## Format / downimage string hits in robotol",
    ]
    fmt = scan_format_strings(blob)
    lines.extend(fmt if fmt else ["(none)"])
    lines.append("")

    for title, lo, hi in windows:
        lines.append(f"## {title}  0x{lo:X}..0x{hi:X}")
        lines.extend(disasm_range(blob, args.code_base, lo, hi))
        lines.append("")

    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
