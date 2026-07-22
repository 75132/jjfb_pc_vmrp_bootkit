#!/usr/bin/env python3
"""E10A-3.1m: annotate gamelist method0 failure block at 0x2E5404."""
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
                tgt = (pc + 4 + imm32) | (1 if (h1 & 0x1000) else 0)
                note = ("BL" if (h1 & 0x1000) else "BLX") + f" -> 0x{tgt:X}"
            else:
                note = f"T2 h0=0x{h0:04X} h1=0x{h1:04X}"
        elif (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
            imm = sign_extend(h0 & 0xFF, 8) << 1
            tgt = (pc + 4 + imm) & ~1
            note = f"B{CONDS[(h0 >> 8) & 0xF]} -> 0x{tgt:X}"
        elif (h0 & 0xF800) == 0xE000:
            imm = sign_extend(h0 & 0x7FF, 11) << 1
            tgt = (pc + 4 + imm) & ~1
            note = f"B -> 0x{tgt:X}"
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
            rm = (h0 >> 3) & 0xF
            note = f"ADD r{rd}, r{rm}"
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
            note = "POP"
        elif (h0 & 0xF800) == 0x9800:
            note = f"LDR r{(h0 >> 8) & 7}, [SP, #0x{(h0 & 0xFF) << 2:X}]"
        elif (h0 & 0xF800) == 0x9000:
            note = f"STR r{(h0 >> 8) & 7}, [SP, #0x{(h0 & 0xFF) << 2:X}]"

        mark = ""
        if pc == 0x2E5404:
            mark = "  <<< FAILURE_PC: ADDS r0,r5,#0 (r5=-1 via prior MVNS) COMMON_EPILOGUE"
        if pc == 0x2E53F4:
            mark = "  <<< PREDICATE_A: CMP r0,#0 after LDRSH of SMSCFG+0x355"
        if pc == 0x2E53F6:
            mark = "  <<< BRANCH_A: BLE -> fail if int16_le <= 0"
        if pc == 0x2E5400:
            mark = "  <<< PREDICATE_B: CMP r0,r1 (r1=0xFF+0xB3=0x1B2)"
        if pc == 0x2E5402:
            mark = "  <<< BRANCH_B: BLE -> success; fallthrough fail if value > 0x1B2"
        if pc == 0x2E53EA:
            mark = "  <<< copy SMSCFG+0x355 len=2 -> SP+0x48"
        if pc == 0x2E53A8:
            mark = "  <<< MVNS r5,r4  (r4==0 => r5=0xFFFFFFFF for failure returns)"
        if "-> 0x2E5404" in note or "-> 0x2E5405" in note:
            mark += "  <<< TO_FAIL_EPILOGUE"
        raw = data[i : i + size].hex().upper()
        lines.append(f"  0x{pc:08X}: {raw:<12}  {note}{mark}")
        i += size
    return lines


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mrp", default="game_files/mythroad/320x480/gwy/gamelist.mrp")
    ap.add_argument("--code-base", default="0x2D4354")
    ap.add_argument("--va", default="0x2E5380")
    ap.add_argument("--nbytes", default="0xB0")
    ap.add_argument("--out-dir", default="out/e10a31m")
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
    body = disasm_window(blob, file_off, va, nbytes)

    # Literal pool used by this block
    lit_lines = []
    for lit_va in (0x2E5708, 0x2E570C, 0x2E5710, 0x2E5714, 0x2E5718):
        loff = lit_va - code_base
        if 0 <= loff + 4 <= len(blob):
            lit_lines.append(f"  lit 0x{lit_va:X} = 0x{u32(blob, loff):X}")

    text = [
        "# E10A-3.1m fail block annotate (0x2E5404 predecessor)",
        "",
        f"- mrp: `{mrp}`",
        f"- code_base: 0x{code_base:X}",
        f"- window_va: 0x{va:X} .. 0x{va + nbytes:X}",
        f"- file_off: 0x{file_off:X}",
        "",
        "## Critical finding",
        "",
        "- After gwy strcmp passes, guest copies **2 bytes** from SMSCFG+**0x355** to SP+0x48.",
        "- Loads them as **signed halfword** (`LDRSH`) via `ADD r3,SP,#0x40` + `MOVS r0,#8`.",
        "- Predicate A @ 0x2E53F4: `CMP r0, #0` / `BLE 0x2E5404` → fail if value <= 0.",
        "- Predicate B @ 0x2E5400: `MOVS r1,#0xFF; ADDS r1,#0xB3` → r1=**0x1B2**;",
        "  `CMP r0,r1` / `BLE success`; fallthrough to 0x2E5404 if value > 0x1B2.",
        "- Required range (from immediates, not brute force): **1 <= int16_le <= 0x1B2**.",
        "- 0x2E5404 is **common failure epilogue**: `ADDS r0, r5, #0` with r5=-1",
        "  (materialized earlier by `MVNS r5, r4` at 0x2E53A8), then `B` to function exit.",
        "- 0x355/0x356 are **one little-endian int16**, not two independent uint8 fields.",
        "",
        "## Literal pool",
        "",
        *lit_lines,
        "",
        "## Disassembly",
        "",
        *body,
        "",
        "## Verdicts (static)",
        "",
        "- `METHOD0_2E5404_PREDECESSOR_IDENTIFIED`",
        "- `METHOD0_2E5404_COMMON_FAILURE_EPILOGUE`",
        "- `METHOD0_2E5404_RETURN_NEG1_IMMEDIATE` (via r5==-1 MOV)",
        "- Field type: int16_le @ SMSCFG+0x355 (covers bytes 0x355 and 0x356)",
        "",
    ]
    out_path = out / "fail_2e5404_annotated.txt"
    out_path.write_text("\n".join(text) + "\n", encoding="utf-8")

    dot = """digraph fail_2e5404 {
  rankdir=TB;
  node [shape=box, fontname="Consolas"];
  gwy_cmp [label="0x2E53DA BLX strcmp(gwy)\\nCMP r0,#0"];
  gwy_fail [label="0x2E53E0 ADDS r0,r5,#0\\nB epilogue", style=filled, fillcolor="#ffcccc"];
  copy355 [label="0x2E53EA BL sms_get\\noff=0x355 len=2 -> SP+0x48"];
  load [label="0x2E53EE ADD r3,SP,#0x40\\n0x2E53F0 MOVS r0,#8\\n0x2E53F2 LDRSH r0,[r3,r0]"];
  pred_a [label="0x2E53F4 CMP r0,#0\\n0x2E53F6 BLE fail\\n(fail if <=0)", style=filled, fillcolor="#ffe0a0"];
  pred_b [label="0x2E53FC..02 CMP vs 0x1B2\\nBLE success / fallthrough fail\\n(fail if >0x1B2)", style=filled, fillcolor="#ffe0a0"];
  fail [label="0x2E5404 ADDS r0,r5,#0\\n(r5=-1)\\n0x2E5406 B epilogue", style=filled, fillcolor="#ff6666"];
  ok [label="0x2E5408..10 value+1\\nBL 0x2E2FED", style=filled, fillcolor="#ccffcc"];
  epi [label="0x2E53B0 ADD SP / POP return -1"];

  gwy_cmp -> gwy_fail [label="NE"];
  gwy_cmp -> copy355 [label="EQ"];
  gwy_fail -> epi;
  copy355 -> load -> pred_a;
  pred_a -> fail [label="LE (<=0)"];
  pred_a -> pred_b [label="GT"];
  pred_b -> ok [label="LE (<=0x1B2)"];
  pred_b -> fail [label="GT"];
  fail -> epi;
}
"""
    (out / "fail_2e5404_cfg.dot").write_text(dot, encoding="utf-8")
    print(f"wrote {out_path}")
    print(f"wrote {out / 'fail_2e5404_cfg.dot'}")
    print("METHOD0_2E5404_PREDECESSOR_IDENTIFIED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
