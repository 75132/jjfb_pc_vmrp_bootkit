#!/usr/bin/env python3
"""E10A-3.1d Lane B: annotate gamelist helper dispatcher (method 0/2/6/8)."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from phase6j_common import member_blob


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def disasm_window(blob: bytes, file_off: int, va: int, nbytes: int) -> list[str]:
    lines: list[str] = []
    i = 0
    end = min(len(blob), file_off + nbytes)
    data = blob[file_off:end]
    while i + 1 < len(data):
        pc = va + i
        h0 = u16(data, i)
        size = 2
        note = f"h0=0x{h0:04X}"
        # Thumb-2 BL/BLX
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0 and i + 3 < len(data):
            h1 = u16(data, i + 2)
            size = 4
            note = f"h0=0x{h0:04X} h1=0x{h1:04X}"
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
                kind = "BL" if (h1 & 0x1000) else "BLX"
                note = f"{kind} -> 0x{tgt:X}"
        elif (h0 & 0xF000) == 0xD000 and ((h0 >> 8) & 0xF) != 0xF:
            imm = sign_extend(h0 & 0xFF, 8) << 1
            tgt = (pc + 4 + imm) | 1
            note = f"Bcond -> 0x{tgt:X}"
        elif (h0 & 0xF800) == 0xE000:
            imm = sign_extend(h0 & 0x7FF, 11) << 1
            tgt = (pc + 4 + imm) | 1
            note = f"B -> 0x{tgt:X}"
        elif (h0 & 0xFF80) == 0x4700:
            rm = (h0 >> 3) & 0xF
            note = f"{'BLX' if (h0 & 0x80) else 'BX'} r{rm}"
        elif (h0 & 0xFF00) == 0x2800:
            note = f"CMP r0, #0x{h0 & 0xFF:X}"
        elif (h0 & 0xFF00) == 0x2900:
            note = f"CMP r1, #0x{h0 & 0xFF:X}  ; method?"
        elif (h0 & 0xFF00) == 0x2000:
            note = f"MOVS r0, #0x{h0 & 0xFF:X}"
        elif h0 == 0x43C0:
            note = "MVNS r0, r0  ; possible -1"
        elif (h0 & 0xF800) == 0x4800:
            imm = (h0 & 0xFF) << 2
            lit = (pc & ~2) + 4 + imm
            note = f"LDR r{(h0 >> 8) & 7}, [pc, #0x{imm:X}] lit≈0x{lit:X}"
        elif (h0 & 0xFF00) == 0xB000:
            note = "ADD/SUB SP"
        elif (h0 & 0xFE00) == 0xB400:
            note = "PUSH"
        elif (h0 & 0xFE00) == 0xBC00:
            note = "POP"
        raw = data[i : i + size].hex().upper()
        mark = ""
        if (h0 & 0xFF00) == 0x2900 and (h0 & 0xFF) in (0, 2, 4, 6, 8):
            mark = f"  ; << METHOD CMP imm={(h0 & 0xFF)}"
        if "BL ->" in note or "BLX ->" in note:
            mark += "  ; << CALL"
        lines.append(f"  0x{pc:08X}: {raw:<10}  {note}{mark}")
        i += size
    return lines


def find_method_cmps(blob: bytes, file_off: int, va: int, nbytes: int) -> list[dict]:
    hits = []
    data = blob[file_off : file_off + nbytes]
    i = 0
    while i + 1 < len(data):
        h0 = u16(data, i)
        size = 2
        if (h0 & 0xE000) == 0xE000 and (h0 & 0x1800) != 0 and i + 3 < len(data):
            size = 4
        if (h0 & 0xFF00) == 0x2900:  # CMP r1, #imm
            hits.append({"pc": va + i, "imm": h0 & 0xFF, "insn": f"0x{h0:04X}"})
        i += size
    return hits


def write_cfg_dot(path: Path, helper_va: int, cmps: list[dict], bl_targets: list[int]) -> None:
    lines = [
        "digraph gamelist_method0 {",
        '  rankdir=LR;',
        f'  helper [label="helper\\n0x{helper_va:X}"];',
        '  m0 [label="method 0"];',
        '  m2 [label="method 2"];',
        '  m6 [label="method 6"];',
        '  m8 [label="method 8"];',
        '  mrc [label="mrc_init_candidate"];',
        '  fail [label="return -1"];',
        "  helper -> m0; helper -> m2; helper -> m6; helper -> m8;",
        "  m0 -> mrc;",
        "  mrc -> fail [style=dashed];",
    ]
    for c in cmps:
        if c["imm"] in (0, 2, 6, 8):
            lines.append(
                f'  cmp_{c["imm"]} [label="CMP r1,#{c["imm"]}\\n0x{c["pc"]:X}", shape=diamond];'
            )
            lines.append(f"  helper -> cmp_{c['imm']};")
            lines.append(f"  cmp_{c['imm']} -> m{c['imm']};")
    for i, t in enumerate(bl_targets[:8]):
        lines.append(f'  bl{i} [label="BL 0x{t:X}"];')
        lines.append(f"  m0 -> bl{i};")
    lines.append("}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--mrp",
        default="game_files/mythroad/320x480/gwy/gamelist.mrp",
    )
    ap.add_argument("--helper", default="0x2E3089", help="live helper VA (thumb bit ok)")
    ap.add_argument(
        "--code-base",
        default="",
        help="guest code base; default = helper&~0xFFFF aligned guess from file",
    )
    ap.add_argument("--out-dir", default="out/e10a31d")
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[1]
    mrp = Path(args.mrp)
    if not mrp.is_absolute():
        mrp = root / mrp
    out_dir = root / args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    blob = member_blob(mrp, "gamelist.ext")
    if not blob:
        # try cfunction alias
        blob = member_blob(mrp, ".ext")
    if not blob:
        raise SystemExit(f"no gamelist.ext in {mrp}")

    helper = int(args.helper, 0)
    helper_pc = helper & ~1
    # Prefer live-proven prologue: PUSH {r4-r7,lr}; MOVS r7,r3; MOVS r6,r2; MOVS r5,r1
    sig = bytes([0xFF, 0xB5, 0x1F, 0x1C, 0x16, 0x1C, 0x05, 0x1C])
    file_off = blob.find(sig)
    if file_off < 0:
        file_off = 0
        code_base = helper_pc & ~0xFFFF
    else:
        code_base = helper_pc - file_off
    if args.code_base:
        code_base = int(args.code_base, 0)
        file_off = helper_pc - code_base

    cmps = find_method_cmps(blob, file_off, code_base + file_off, 0x200)
    lines = [
        "# gamelist helper dispatch annotated (E10A-3.1d Lane B)",
        "",
        f"- mrp: `{mrp}`",
        f"- ext_size: {len(blob)}",
        f"- live_helper: 0x{helper:X}",
        f"- code_base: 0x{code_base:X}",
        f"- file_off: 0x{file_off:X}",
        f"- entry_va: 0x{(code_base + file_off):X}",
        "",
        "## mythroad_mini natural ABI (DOCUMENTED)",
        "- code6: input=filebuf, input_len=MR_VERSION(2011) → store version",
        "- code8: input=&mrc_appInfo_st, input_len=16",
        "- code0: input=filebuf, input_len=MR_VERSION → mrc_init(); refresh",
        "- FAST_REAL currently passes code0 input=0 (not filebuf) — confirm via CMP/use of r2",
        "",
        "## method compare hits (CMP r1,#imm)",
    ]
    for c in cmps:
        lines.append(f"- pc=0x{c['pc']:X} imm={c['imm']} insn={c['insn']}")
    if any(c["imm"] == 0 for c in cmps):
        lines.append("")
        lines.append("Verdict: GAMELIST_METHOD0_TARGET_IDENTIFIED (CMP r1,#0 present)")
    lines.append("")
    lines.append("## disassembly window")
    lines.append("```")
    dasm = disasm_window(blob, file_off, code_base + file_off, 0x180)
    lines.extend(dasm)
    lines.append("```")

    bl_targets = []
    for ln in dasm:
        if "BL ->" in ln or "BLX ->" in ln:
            try:
                t = int(ln.split("->")[1].split()[0], 16)
                bl_targets.append(t)
            except Exception:
                pass
    if bl_targets:
        lines.append("")
        lines.append("## first BL/BLX targets from dispatcher window")
        for t in bl_targets[:12]:
            lines.append(f"- 0x{t:X}")
        lines.append("")
        lines.append(
            "Verdict: GAMELIST_MRC_INIT_TARGET_IDENTIFIED "
            f"(first_bl=0x{bl_targets[0]:X} — candidate, confirm via method0_trace)"
        )

    ann = out_dir / "gamelist_helper_dispatch_annotated.txt"
    ann.write_text("\n".join(lines) + "\n", encoding="utf-8")
    write_cfg_dot(out_dir / "gamelist_method0_cfg.dot", helper_pc, cmps, bl_targets)
    print(f"wrote {ann}")
    print(f"wrote {out_dir / 'gamelist_method0_cfg.dot'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
