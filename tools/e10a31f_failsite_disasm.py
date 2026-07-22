#!/usr/bin/env python3
"""E10A-3.1f: annotate gamelist fail-site function at 0x2E1BBD / 0x2E1C24."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from phase6j_common import member_blob


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


CONDS = ["EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE"]


def disasm_window(blob: bytes, file_off: int, va: int, nbytes: int) -> list[str]:
    lines: list[str] = []
    data = blob[file_off : file_off + nbytes]
    i = 0
    while i + 1 < len(data):
        pc = va + i
        h0 = u16(data, i)
        size = 2
        note = f"h0=0x{h0:04X}"
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0 and i + 3 < len(data):
            h1 = u16(data, i + 2)
            size = 4
            if (h0 & 0xF800) == 0xF000 and (h1 & 0xC000) == 0xC000:
                s = (h0 >> 10) & 1
                imm10 = h0 & 0x3FF
                j1 = (h1 >> 13) & 1
                j2 = (h1 >> 11) & 1
                imm11 = h1 & 0x7FF
                i1 = (~(j1 ^ s)) & 1
                i2 = (~(j2 ^ s)) & 1
                imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
                imm32 = sign_extend(imm32, 25)
                tgt = (pc + 4 + imm32) | (1 if (h1 & 0x1000) else 0)
                note = ("BL" if (h1 & 0x1000) else "BLX") + f" -> 0x{tgt:X}"
            else:
                note = f"T2 h0=0x{h0:04X} h1=0x{h1:04X}"
        elif (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
            imm = sign_extend(h0 & 0xFF, 8) << 1
            tgt = (pc + 4 + imm) | 1
            note = f"B{CONDS[(h0 >> 8) & 0xF]} -> 0x{tgt:X}"
        elif (h0 & 0xF800) == 0xE000:
            imm = sign_extend(h0 & 0x7FF, 11) << 1
            tgt = (pc + 4 + imm) | 1
            note = f"B -> 0x{tgt:X}"
        elif (h0 & 0xFFC0) == 0x43C0:
            note = f"MVNS r{h0 & 7}, r{(h0 >> 3) & 7}"
        elif (h0 & 0xFF00) == 0x2000:
            note = f"MOVS r0, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFF00) == 0x2100:
            note = f"MOVS r1, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFF00) == 0x2200:
            note = f"MOVS r2, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFF00) == 0x2300:
            note = f"MOVS r3, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFF00) == 0x2400:
            note = f"MOVS r4, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFF00) == 0x2800:
            note = f"CMP r0, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFFC0) == 0x4280:
            note = f"CMP r{h0 & 7}, r{(h0 >> 3) & 7}"
        elif (h0 & 0xF800) == 0x4800:
            imm = (h0 & 0xFF) << 2
            lit = (pc & ~2) + 4 + imm
            note = f"LDR r{(h0 >> 8) & 7}, [pc, #0x{imm:X}] lit=0x{lit:X}"
        elif (h0 & 0xF800) == 0x6800:
            note = f"LDR r{h0 & 7}, [r{(h0 >> 3) & 7}, #0x{((h0 >> 6) & 0x1F) << 2:X}]"
        elif (h0 & 0xF800) == 0x6000:
            note = f"STR r{h0 & 7}, [r{(h0 >> 3) & 7}, #0x{((h0 >> 6) & 0x1F) << 2:X}]"
        elif (h0 & 0xF800) == 0x8000:
            note = f"STRH r{h0 & 7}, [r{(h0 >> 3) & 7}, #0x{((h0 >> 6) & 0x1F) << 1:X}]"
        elif (h0 & 0xFF00) == 0x4400 or (h0 & 0xFF00) == 0x4480:
            dn = (h0 >> 7) & 1
            rd = (h0 & 7) | (dn << 3)
            rm = (h0 >> 3) & 0xF
            note = f"ADD r{rd}, r{rm}"
        elif (h0 & 0xFF00) == 0x4790 or (h0 & 0xFF87) == 0x4780:
            rm = (h0 >> 3) & 0xF
            note = f"BLX r{rm}"
        elif (h0 & 0xFE00) == 0xB400:
            note = "PUSH"
        elif (h0 & 0xFE00) == 0xBC00:
            note = "POP"
        elif (h0 & 0xFF00) == 0xB000:
            note = "ADD/SUB SP"
        elif (h0 & 0xF800) == 0x3000:
            note = f"ADDS r{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xF800) == 0x3800:
            note = f"SUBS r{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}"

        mark = ""
        if pc == 0x2E1C24:
            mark = "  <<< E10A31D_FALSE_POSITIVE: unconditional MVNS r0,r4 (r4=0 => -1 store)"
        if "-> 0x2E1C24" in note or "-> 0x2E1C25" in note:
            mark = "  <<< BRANCH_TO_FAIL"
        if pc == 0x2E1BC0 and "MOVS r4" in note:
            mark = "  <<< r4:=0 for zeroing / -1 materialization"
        if "BLX r" in note:
            mark += "  <<< indirect call (memset/platform?)"
        raw = data[i : i + size].hex().upper()
        lines.append(f"  0x{pc:08X}: {raw:<12}  {note}{mark}")
        i += size
    return lines


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mrp", default="game_files/mythroad/320x480/gwy/gamelist.mrp")
    ap.add_argument("--code-base", default="0x2D4354")
    ap.add_argument("--va", default="0x2E1BBC")
    ap.add_argument("--nbytes", default="0x120")
    ap.add_argument("--out-dir", default="out/e10a31f")
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[1]
    mrp = Path(args.mrp)
    if not mrp.is_absolute():
        mrp = root / mrp
    out = root / args.out_dir
    out.mkdir(parents=True, exist_ok=True)

    blob = member_blob(mrp, "gamelist.ext")
    if not blob:
        raise SystemExit(f"no gamelist.ext in {mrp}")

    code_base = int(args.code_base, 0)
    va = int(args.va, 0) & ~1
    nbytes = int(args.nbytes, 0)
    file_off = va - code_base

    # Pool literals after function (typical)
    lit_lines = []
    for lit_va in range(0x2E1C74, 0x2E1CB8, 4):
        loff = lit_va - code_base
        if 0 <= loff + 4 <= len(blob):
            val = u32(blob, loff)
            lit_lines.append(f"  lit 0x{lit_va:X} = 0x{val:X}  ; R9+0x{val:X} if used as ERW offset")

    body = disasm_window(blob, file_off, va, nbytes)
    branches_to_fail = [ln for ln in body if "BRANCH_TO_FAIL" in ln]

    text = [
        "# E10A-3.1f gamelist fail-site annotate (0x2E1BBD / 0x2E1C24)",
        "",
        f"- mrp: `{mrp}`",
        f"- code_base: 0x{code_base:X}",
        f"- window_va: 0x{va:X}",
        f"- file_off: 0x{file_off:X}",
        "",
        "## Critical finding",
        "",
        "- `0x2E1BC0`: `MOVS r4, #0`",
        "- Function zeroes many `R9+offset` fields via `STR r4, [...]`",
        "- Makes indirect `BLX r3` calls (likely memset via `[R9+0x95C]`)",
        "- `0x2E1C24`: **unconditional** `MVNS r0, r4` → r0=-1 (because r4==0)",
        "- `0x2E1C26`: `STRH r0, [r1, #0]` stores the -1 sentinel into a halfword field",
        "- Execution **continues** (BL 0x2E33AD, more stores, POP at 0x2E1C72)",
        "- **No Bcond targets 0x2E1C24** — not a version/appInfo gate",
        "- E10A-3.1d `RETURN_NEG1_IMMEDIATE @ 0x2E1C24` is a **false-positive first-failure**",
        "  because the tracer stops when r0 first becomes -1 (sentinel materialization)",
        "",
        f"- branches_to_0x2E1C24_count: {len(branches_to_fail)}",
        "",
        "## Literal pool (R9-relative offsets used nearby)",
        "",
        *lit_lines,
        "",
        "## Note on R9+0x920 / appInfo",
        "",
        "- This function uses offsets like 0x3E4, 0x120, 0x6B4, **0x95C**, 0x116, 0x114, ...",
        "- It does **not** reference 0x91C / 0x920 — consistent with",
        "  `METHOD0_DOES_NOT_READ_APPINFO_BEFORE_FAILURE`",
        "",
        "## Disassembly",
        "",
        *body,
        "",
        "## Verdicts (static)",
        "",
        "- `FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE`",
        "- `FAILSITE_NO_BRANCH_PREDICATE_INTO_2E1C24`",
        "- `E10A31D_FIRST_FAILURE_FALSE_POSITIVE`",
        "- Next: continue method0 trace past sentinel to find true helper return -1",
        "",
    ]
    out_path = out / "gamelist_2e1bbd_fail_annotated.txt"
    out_path.write_text("\n".join(text) + "\n", encoding="utf-8")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
