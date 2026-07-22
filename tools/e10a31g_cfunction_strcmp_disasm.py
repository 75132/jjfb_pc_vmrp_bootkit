#!/usr/bin/env python3
"""E10A-3.1g: annotate cfunction.ext strcmp fail at 0xAC2D0 / 0xAC2E8."""
from __future__ import annotations

import struct
from pathlib import Path

from phase6j_common import member_blob


def arm_word(b: bytes) -> int:
    return int.from_bytes(b, "little")


def disasm_arm_word(pc: int, w: int) -> str:
    # Minimal ARM decoder for the strcmp window.
    if w == 0xE4D02001:
        return "LDRB r2, [r0], #1"
    if w == 0xE4D13001:
        return "LDRB r3, [r1], #1"
    if w == 0xE1520003:
        return "CMP r2, r3"
    if (w & 0xFF000000) == 0x0A000000:
        imm = w & 0xFFFFFF
        if imm & 0x800000:
            imm -= 0x1000000
        tgt = pc + 8 + imm * 4
        return f"BEQ -> 0x{tgt:X}"
    if (w & 0xFF000000) == 0x1A000000:
        imm = w & 0xFFFFFF
        if imm & 0x800000:
            imm -= 0x1000000
        tgt = pc + 8 + imm * 4
        return f"BNE -> 0x{tgt:X}"
    if w == 0xE3A00001:
        return "MOV r0, #1"
    if w == 0x23A00001:
        return "MOVCS r0, #1"
    if w == 0x33E00000:
        return "MVNCC r0, #0  ; r0=-1"
    if w == 0xE3A00000:
        return "MOV r0, #0"
    if w == 0xE2700000:
        return "RSBS r0, r0, #0"
    if w == 0xE12FFF1E:
        return "BX lr"
    if (w & 0xFFFF0000) == 0xE3500000:
        return f"CMP r0, #0x{w & 0xFF:X}"
    if (w & 0xFFF00000) == 0xE3500000:
        return f"CMP r0, #imm 0x{w & 0xFFF:X}"
    # MVN rd, rm
    if (w & 0x0FE00FF0) == 0x01E00000:
        rd = (w >> 12) & 0xF
        rm = w & 0xF
        return f"MVN r{rd}, r{rm}"
    # RSB rd, rn, #0
    if (w & 0x0FF0F000) == 0x02600000 and (w & 0xFFF) == 0:
        rd = (w >> 12) & 0xF
        rn = (w >> 16) & 0xF
        return f"RSB r{rd}, r{rn}, #0"
    return f"ARM 0x{w:08X}"


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    out = root / "out" / "e10a31g"
    out.mkdir(parents=True, exist_ok=True)

    # Live-traced bytes from e10a31d insn CSV (LE memory order).
    traced = [
        (0xAC2D0, "0120D0E4"),
        (0xAC2D4, "0130D1E4"),
        (0xAC2D8, "030052E1"),
        (0xAC2DC, "0300000A"),
        (0xAC2E0, "030052E1"),
        (0xAC2E4, "0100A023"),  # MOVCS r0,#1
        (0xAC2E8, "0000E033"),  # MVNCC r0,#0
        (0xAC2EC, "1EFF2FE1"),
    ]

    # Prefer reading real image from mapped cfunction.ext (DSM at 0x80000).
    # Known sha from log: robotol/cfunction from jjfb — but DSM image is separate.
    # Try common locations.
    cfn_candidates = [
        root / "out/vmrp_run/cfunction.ext",
        root / "third_party/vmrp_upstream/mythroad/cfunction.ext",
        root / "game_files/mythroad/cfunction.ext",
    ]
    cfn = None
    for p in cfn_candidates:
        if p.exists():
            cfn = p.read_bytes()
            break
    # Also search overlay / mythroad for large cfunction
    if cfn is None:
        for p in (root / "out/vmrp_run").rglob("cfunction.ext"):
            data = p.read_bytes()
            if len(data) > 100000:
                cfn = data
                cfn_candidates.insert(0, p)
                break

    code_base = 0x80000
    lines = [
        "# E10A-3.1g cfunction.ext true-fail annotate (0xAC2D0 / 0xAC2E8)",
        "",
        "## Live register snapshot (method0 insn trace)",
        "",
        "| PC | r0 | r1 | r2 | r3 | r9 | note |",
        "|----|----|----|----|----|----|------|",
        "| 0x2E53A6 BLX | 0x27FA30 | 0x2E845C | 0xAC2D0 | 0x27FA33 | 0x280400 | gamelist calls strcmp |",
        "| 0xAC2D0 enter | 0x27FA30 | 0x2E845C | (fn) | 0x27FA33 | 0x280400 | ARM mode |",
        "| 0xAC2D4 | 0x27FA31 | 0x2E845C | **0x00** | … | | first byte of A = NUL |",
        "| 0xAC2D8 CMP | 0x27FA31 | 0x2E845D | 0x00 | **0x47='G'** | | mismatch |",
        "| 0xAC2E8 | … | … | 0 | 0x47 | | r0 becomes -1 |",
        "| 0xAC2EC BX lr | **-1** | | | | | return to gamelist |",
        "",
        "## Static interpretation",
        "",
        "Classic ARM `strcmp` returning `-1/0/+1` via conditional MOV/MVN:",
        "",
        "```",
        "LDRB r2, [r0], #1",
        "LDRB r3, [r1], #1",
        "CMP  r2, r3",
        "BEQ  equal_continue     ; -> 0xAC2F0",
        "CMP  r2, r3",
        "MOVCS r0, #1            ; r2 > r3",
        "MVNCC r0, #0            ; r2 < r3 => r0 = -1",
        "BX   lr",
        "```",
        "",
        "Observed first-byte compare:",
        "- A = `*(uint8*)0x27FA30` = **0x00** (stack field empty)",
        "- B = `*(uint8*)0x2E845C` = **`'G'` of literal `\"GPT\"`**",
        "",
        "## Gamelist call chain (Thumb @ 0x2E5380)",
        "",
        "```",
        "memset(sp+0x30, 0, 0x10)                 ; R9+0x95C -> cfunction",
        "BL  0x2E3180(id=0x349, dst=sp+0x30, len=3)",
        "  BLX memcpy(dst, src=0x28101D, len=3)   ; src = base+0x349",
        "strcmp(sp+0x30, \"GPT\")                   ; R9+0x94C -> 0xAC2D0",
        "  TRUE FAIL @ 0xAC2E8",
        "```",
        "",
        "Live memcpy (insn seq 5023): dst=`0x27FA30` src=`0x28101D` len=3;",
        "src_base=`0x280CD4` (inside cfunction ERW `0x280400`+0x8D4).",
        "strcmp then sees NUL vs `GPT` — appInfo is not on this path.",
        "",
        "## Disassembly (from live-traced / image bytes)",
        "",
    ]

    for pc, hx in traced:
        b = bytes.fromhex(hx)
        w = arm_word(b)
        note = disasm_arm_word(pc, w)
        mark = ""
        if pc == 0xAC2E8:
            mark = "  <<< TRUE_FAIL"
        if pc == 0xAC2EC:
            mark = "  <<< return"
        lines.append(f"  0x{pc:08X}: {hx}  {note}{mark}")

    # Dump B string from gamelist.ext
    mrp = root / "game_files/mythroad/320x480/gwy/gamelist.mrp"
    blob = member_blob(mrp, "gamelist.ext")
    gbase = 0x2D4354
    bva = 0x2E845C
    boff = bva - gbase
    lines += ["", "## Operand B string @ gamelist VA 0x2E845C", ""]
    if blob and 0 <= boff < len(blob):
        chunk = blob[boff : boff + 80]
        asci = "".join(chr(c) if 32 <= c < 127 else "." for c in chunk)
        # C-string
        end = chunk.find(b"\x00")
        cstr = chunk[: end if end >= 0 else 64]
        lines.append(f"- file_off: 0x{boff:X}")
        lines.append(f"- raw_hex: {chunk[:40].hex()}")
        lines.append(f"- ascii: `{asci[:60]}`")
        lines.append(f"- cstring: `{cstr.decode('latin1', errors='replace')}`")
    else:
        lines.append("- unresolved (VA outside gamelist.ext image)")

    # Dump from cfunction image if present
    lines += ["", "## cfunction.ext image window @ file_off 0x2C2D0 (VA 0xAC2D0 - 0x80000)", ""]
    if cfn is None:
        lines.append("- cfunction.ext image not found on disk for static read")
    else:
        foff = 0xAC2D0 - code_base
        lines.append(f"- source_len: {len(cfn)}")
        if 0 <= foff + 32 <= len(cfn):
            win = cfn[foff : foff + 32]
            lines.append(f"- file_off: 0x{foff:X}")
            lines.append(f"- bytes: {win.hex()}")
            for i in range(0, 32, 4):
                w = arm_word(win[i : i + 4])
                pc = 0xAC2D0 + i
                lines.append(f"  0x{pc:08X}: {disasm_arm_word(pc, w)}")
        else:
            lines.append(f"- file_off 0x{foff:X} out of range")

    lines += [
        "",
        "## Verdicts",
        "",
        "- `TRUE_FAIL_IS_STRCMP_IN_CFUNCTION`",
        "- `STRCMP_EMPTY_VS_GPT_LITERAL`",
        "- `GPT_FIELD_READ_OFF_349_SRC_EMPTY`",
        "- `APPINFO_NOT_ON_TRUE_FAIL_PATH`",
        "- Next: identify who owns buffer base `0x280CD4` / ERW+0x8D4 and why",
        "  offset `0x349` is not populated with `GPT` before method0.",
        "",
    ]

    path = out / "cfunction_ac2d0_strcmp_annotated.txt"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {path}")

    # Also print B string to stdout for quick view
    if blob and 0 <= boff < len(blob):
        chunk = blob[boff : boff + 64]
        end = chunk.find(b"\x00")
        print("B cstring:", chunk[: end if end >= 0 else 32])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
