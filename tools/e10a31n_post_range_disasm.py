#!/usr/bin/env python3
"""E10A-3.1n: annotate post-range path 0x2E5410 -> 0x2E3FBA."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from phase6j_common import member_blob

CONDS = ["EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE"]


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


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
                tgt = (pc + 4 + imm32) & ~1
                note = ("BL" if (h1 & 0x1000) else "BLX") + f" -> 0x{tgt:X}"
            else:
                note = f"T2 h0=0x{h0:04X} h1=0x{h1:04X}"
        elif (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
            imm = sign_extend(h0 & 0xFF, 8) << 1
            note = f"B{CONDS[(h0 >> 8) & 0xF]} -> 0x{(pc + 4 + imm) & ~1:X}"
        elif (h0 & 0xF800) == 0xE000:
            imm = sign_extend(h0 & 0x7FF, 11) << 1
            note = f"B -> 0x{(pc + 4 + imm) & ~1:X}"
        elif (h0 & 0xF800) == 0xA800:
            note = f"ADD r{(h0 >> 8) & 7}, SP, #0x{(h0 & 0xFF) << 2:X}"
        elif (h0 & 0xFFC0) == 0x5E00:
            note = f"LDRSH r{h0 & 7}, [r{(h0 >> 3) & 7}, r{(h0 >> 6) & 7}]"
        elif (h0 & 0xFF00) == 0x2800:
            note = f"CMP r0, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFFC0) == 0x4280:
            note = f"CMP r{h0 & 7}, r{(h0 >> 3) & 7}"
        elif (h0 & 0xF800) == 0x2000:
            note = f"MOVS r{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xF800) == 0x3000:
            note = f"ADDS r{(h0 >> 8) & 7}, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFF80) == 0x1C00:
            note = f"ADDS r{h0 & 7}, r{(h0 >> 3) & 7}, #0x{(h0 >> 6) & 7:X}"
        elif (h0 & 0xFFC0) == 0x43C0:
            note = f"MVNS r{h0 & 7}, r{(h0 >> 3) & 7}"
        elif (h0 & 0xFF00) == 0x4400 or (h0 & 0xFF00) == 0x4480:
            dn = (h0 >> 7) & 1
            rd = (h0 & 7) | (dn << 3)
            note = f"ADD r{rd}, r{(h0 >> 3) & 0xF}"
        elif (h0 & 0xFF87) == 0x4780:
            note = f"BLX r{(h0 >> 3) & 0xF}"
        elif (h0 & 0xFF87) == 0x4700:
            note = f"BX r{(h0 >> 3) & 0xF}"
        elif (h0 & 0xF800) == 0x4800:
            imm = (h0 & 0xFF) << 2
            lit = (pc & ~2) + 4 + imm
            note = f"LDR r{(h0 >> 8) & 7}, [pc, #0x{imm:X}] lit=0x{lit:X}"
        elif (h0 & 0xF800) == 0x6800:
            note = f"LDR r{h0 & 7}, [r{(h0 >> 3) & 7}, #0x{((h0 >> 6) & 0x1F) << 2:X}]"
        elif (h0 & 0xF800) == 0x6000:
            note = f"STR r{h0 & 7}, [r{(h0 >> 3) & 7}, #0x{((h0 >> 6) & 0x1F) << 2:X}]"
        elif (h0 & 0xFF00) == 0xB000:
            note = ("ADD SP" if (h0 & 0x80) == 0 else "SUB SP") + f", #0x{(h0 & 0x7F) << 2:X}"
        elif (h0 & 0xFE00) == 0xB400:
            note = "PUSH"
        elif (h0 & 0xFE00) == 0xBC00:
            note = "POP" + (" PC" if (h0 & 0x100) else "")
        elif (h0 & 0xF800) == 0x9800:
            note = f"LDR r{(h0 >> 8) & 7}, [SP, #0x{(h0 & 0xFF) << 2:X}]"

        mark = ""
        if pc == 0x2E5410:
            mark = "  <<< CALL_SITE BL alloc/dispatch 0x2E2FED (NOT a function entry)"
        if pc == 0x2E2FEC:
            mark = "  <<< CALLEE entry (thumb)"
        if pc == 0x2E5440:
            mark = "  <<< SMSCFG copy off=0x377 (from dynamic)"
        if pc == 0x2E5486:
            mark = "  <<< BL into fail-fn 0x2E3F85"
        if pc == 0x2E3FB4:
            mark = "  <<< BLX whose R0==0 causes fallthrough to MVNS"
        if pc == 0x2E3FBA:
            mark = "  <<< DIRECT -1: MVNS r0,r0 after zero return (NOT shared epilogue)"
        if pc == 0x2E3FB8:
            mark = "  <<< BNE skip MVNS if callee nonzero"
        if "-> 0x2E3FBA" in note:
            mark += "  <<< TO_FAIL"
        raw = data[i : i + size].hex().upper()
        lines.append(f"  0x{pc:08X}: {raw:<12}  {note}{mark}")
        i += size
        if note.startswith("POP") and (h0 & 0x100):
            break
    return lines


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mrp", default="game_files/mythroad/320x480/gwy/gamelist.mrp")
    ap.add_argument("--code-base", default="0x2D4354")
    ap.add_argument("--out-dir", default="out/e10a31n")
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
    cb = int(args.code_base, 0)

    caller = disasm_window(blob, 0x2E5408 - cb, 0x2E5408, 0x120)
    callee = disasm_window(blob, 0x2E2FEC - cb, 0x2E2FEC, 0x40)
    failfn = disasm_window(blob, 0x2E3F84 - cb, 0x2E3F84, 0x50)

    lit_lines = []
    for lit_va in (0x2E5700, 0x2E5704, 0x2E571C, 0x2E3018, 0x2E4028, 0x2E402C, 0x2E4030):
        loff = lit_va - cb
        if 0 <= loff + 4 <= len(blob):
            lit_lines.append(f"  lit 0x{lit_va:X} = 0x{u32(blob, loff):X}")

    fn_text = [
        "# E10A-3.1n function / call-site annotate around 0x2E5410",
        "",
        f"- mrp: `{mrp}`",
        f"- code_base: 0x{cb:X}",
        "",
        "## Critical clarification",
        "",
        "- `0x2E5410` is a **BL call site**, not a function entry.",
        "- Target = `0x2E2FED` (PUSH at 0x2E2FEC): thin wrapper that calls `[R9+off]` with r0=value+1.",
        "- After return: `CMP r0,#0`; zero → shared -1 epilogue; nonzero → continue.",
        "- Dynamic path then copies SMSCFG+**0x377**, calls `0x2E2FED` again, then `BL 0x2E3F85`.",
        "",
        "## Literals",
        "",
        *lit_lines,
        "",
        "## Call site + continuation (0x2E5408..)",
        "",
        *caller,
        "",
        "## Callee 0x2E2FED",
        "",
        *callee,
        "",
        "## Verdicts (static)",
        "",
        "- `METHOD0_2E5410_FUNCTION_IDENTIFIED` (as CALL_SITE_TO_0x2E2FED)",
        "",
    ]
    (out / "function_2e5410_annotated.txt").write_text("\n".join(fn_text) + "\n", encoding="utf-8")

    fail_text = [
        "# E10A-3.1n fail-site annotate 0x2E3FBA",
        "",
        f"- mrp: `{mrp}`",
        "",
        "## Role of 0x2E3FBA",
        "",
        "- Function entry: `0x2E3F85` (PUSH at 0x2E3F84).",
        "- Sequence:",
        "  1. BLX platform/helper (slot)",
        "  2. optional BLX",
        "  3. BLX at `0x2E3FB4` → result in r0",
        "  4. `ADDS r7,r0,#0`; `BNE 0x2E3FC0`",
        "  5. fallthrough `0x2E3FBA`: **`MVNS r0,r0`** — when r0 was 0, becomes 0xFFFFFFFF",
        "  6. ADD SP; POP PC — return -1 to caller",
        "- Therefore 0x2E3FBA is **DIRECT failure construction** after a **zero callee return**,",
        "  not the shared GPT/gwy/0x355 epilogue at 0x2E5404.",
        "",
        "## Disassembly",
        "",
        *failfn,
        "",
        "## Verdicts (static)",
        "",
        "- `METHOD0_2E3FBA_ROLE_IDENTIFIED`",
        "- `METHOD0_2E3FBA_DIRECT_FAILURE`",
        "",
    ]
    (out / "fail_2e3fba_annotated.txt").write_text("\n".join(fail_text) + "\n", encoding="utf-8")

    dot = """digraph post_range {
  rankdir=TB;
  node [shape=box, fontname="Consolas"];
  gate [label="0x355 range check PASS\\nvalue in 1..0x1B2"];
  call1 [label="0x2E5410 BL 0x2E2FED\\nr0=value+1"];
  chk1 [label="0x2E5416 CMP r0,#0"];
  epi [label="shared -1 epilogue\\n0x2E541E", style=filled, fillcolor="#ffcccc"];
  copy377 [label="0x2E5440 sms_get\\noff=0x377 (dynamic)", style=filled, fillcolor="#ffe0a0"];
  call2 [label="0x2E5474 BL 0x2E2FED"];
  call_failfn [label="0x2E5486 BL 0x2E3F85"];
  blx [label="0x2E3FB4 BLX helper\\n(strcmp/memcmp/platform?)"];
  pred [label="0x2E3FB8 BNE\\nif r0!=0 skip", style=filled, fillcolor="#ffe0a0"];
  mvn [label="0x2E3FBA MVNS r0,r0\\n0 -> -1 DIRECT", style=filled, fillcolor="#ff6666"];
  ok [label="0x2E3FC0 success path", style=filled, fillcolor="#ccffcc"];

  gate -> call1 -> chk1;
  chk1 -> epi [label="EQ"];
  chk1 -> copy377 [label="NE"];
  copy377 -> call2 -> call_failfn -> blx -> pred;
  pred -> ok [label="NE"];
  pred -> mvn [label="EQ (r0==0)"];
}
"""
    (out / "post_range_cfg.dot").write_text(dot, encoding="utf-8")
    print(f"wrote {out / 'function_2e5410_annotated.txt'}")
    print(f"wrote {out / 'fail_2e3fba_annotated.txt'}")
    print(f"wrote {out / 'post_range_cfg.dot'}")
    print("METHOD0_2E5410_FUNCTION_IDENTIFIED")
    print("METHOD0_2E3FBA_ROLE_IDENTIFIED")
    print("METHOD0_2E3FBA_DIRECT_FAILURE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
