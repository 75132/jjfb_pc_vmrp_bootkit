#!/usr/bin/env python3
"""Static Thumb disasm for C0 reset/start 0x2FEBBC..0x2FECAA (Task 13).

Focus: 0x2FEC20..0x2FEC50 including LDRSH @0x2FEC3C (fault site).
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROB = ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext"
OUT = ROOT / "out/c0_reset_start_disasm.txt"
CODE_BASE = 0x2D8DF4
RANGE_LO = 0x2FEBBC
RANGE_HI = 0x2FECAA
FOCUS_LO = 0x2FEC20
FOCUS_HI = 0x2FEC50
FAULT_PC = 0x2FEC3C


def reg(n: int) -> str:
    return f"r{n}" if n < 13 else ("sp" if n == 13 else "lr")


def decode_half(h: int, pc: int) -> tuple[str, str, list[str], int]:
    ann: list[str] = []
    if (h & 0xFE00) == 0xB400:
        lo, hi = h & 0xFF, (h >> 8) & 0xFF
        regs = [f"r{i}" for i in range(8) if lo & (1 << i)]
        if hi:
            regs.append("lr")
        return "PUSH", "{" + ",".join(regs) + "}", ann, 2
    if (h & 0xFE00) == 0xBC00:
        lo, hi = h & 0xFF, (h >> 8) & 0xFF
        regs = [f"r{i}" for i in range(8) if lo & (1 << i)]
        if hi:
            regs.append("pc")
        return "POP", "{" + ",".join(regs) + "}", ann, 2
    if (h & 0xF000) == 0xD000 and (h & 0x0F00) != 0x0F00:
        conds = [
            "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
            "HI", "LS", "GE", "LT", "GT", "LE",
        ]
        c = (h >> 8) & 0xF
        imm = struct.unpack("<b", bytes([h & 0xFF]))[0]
        tgt = (pc + 4 + imm * 2) & 0xFFFFFFFF
        return f"B{conds[c]}", f"0x{tgt:X}", ann, 2
    if (h & 0xF800) == 0xE000:
        imm = h & 0x7FF
        if imm & 0x400:
            imm -= 0x800
        tgt = (pc + 4 + imm * 2) & 0xFFFFFFFF
        return "B", f"0x{tgt:X}", ann, 2
    if (h & 0xF800) == 0x2000:
        rd, imm = (h >> 8) & 7, h & 0xFF
        return "MOVS", f"{reg(rd)}, #0x{imm:X}", ann, 2
    if (h & 0xF800) == 0x3000:
        rd, imm = (h >> 8) & 7, h & 0xFF
        return "ADDS", f"{reg(rd)}, #{imm}", ann, 2
    if (h & 0xFF00) == 0x4400:
        rd = ((h >> 4) & 0x8) | (h & 7)
        rm = (h >> 3) & 0xF
        return "ADD", f"{reg(rd)}, {reg(rm)}", ann, 2
    if (h & 0xFF87) == 0x4487:  # ADD rX, PC variants covered above
        pass
    if (h & 0xF800) == 0x4800:
        rd, imm = (h >> 8) & 7, h & 0xFF
        lit = ((pc + 4) & ~2) + imm * 4
        ann.append(f"literal_pool@0x{lit:X}")
        return "LDR", f"{reg(rd)}, [pc, #0x{imm * 4:X}]", ann, 2
    if (h & 0xF800) == 0x6800:
        rt, rn, imm = h & 7, (h >> 3) & 7, ((h >> 6) & 0x1F) * 4
        return "LDR", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    if (h & 0xF800) == 0x6000:
        rt, rn, imm = h & 7, (h >> 3) & 7, ((h >> 6) & 0x1F) * 4
        return "STR", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    if (h & 0xF800) == 0x7800:
        rt, rn, imm = h & 7, (h >> 3) & 7, (h >> 6) & 0x1F
        return "LDRB", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    if (h & 0xF800) == 0x7000:
        rt, rn, imm = h & 7, (h >> 3) & 7, (h >> 6) & 0x1F
        return "STRB", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    if (h & 0xF800) == 0x8800:
        rt, rn, imm = h & 7, (h >> 3) & 7, ((h >> 6) & 0x1F) * 2
        return "LDRH", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    if (h & 0xF800) == 0x8000:
        rt, rn, imm = h & 7, (h >> 3) & 7, ((h >> 6) & 0x1F) * 2
        return "STRH", f"{reg(rt)}, [{reg(rn)}, #0x{imm:X}]", ann, 2
    # LDRSH reg, [reg, reg]  — 0x5E00..  (A5.2.9)
    if (h & 0xFFC0) == 0x5E00:
        rt, rn, rm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        ann.append("signed_halfword_load")
        if pc == FAULT_PC:
            ann.append("FAULT_SITE")
        return "LDRSH", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
    if (h & 0xFFC0) == 0x5A00:
        rt, rn, rm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        return "LDRH", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
    if (h & 0xFFC0) == 0x5800:
        rt, rn, rm = h & 7, (h >> 3) & 7, (h >> 6) & 7
        return "LDR", f"{reg(rt)}, [{reg(rn)}, {reg(rm)}]", ann, 2
    if (h & 0xFFC0) == 0x4080:
        rdn, rm = h & 7, (h >> 3) & 7
        return "LSLS", f"{reg(rdn)}, {reg(rm)}", ann, 2
    if (h & 0xFFC0) == 0x0000 and (h & 0x07C0) != 0:
        rd, rm, imm = h & 7, (h >> 3) & 7, (h >> 6) & 0x1F
        return "LSLS", f"{reg(rd)}, {reg(rm)}, #{imm}", ann, 2
    if (h & 0xF800) == 0x0000:
        rd, rm, imm = h & 7, (h >> 3) & 7, (h >> 6) & 0x1F
        return "LSLS", f"{reg(rd)}, {reg(rm)}, #{imm}", ann, 2
    if (h & 0xFF00) == 0x4600:
        rd = ((h >> 4) & 0x8) | (h & 7)
        rm = (h >> 3) & 0xF
        return "MOV", f"{reg(rd)}, {reg(rm)}", ann, 2
    if (h & 0xF800) == 0xF000:
        return "BL_HI", f"imm11=0x{h & 0x7FF:X}", ["bl_prefix"], 2
    if (h & 0xF800) == 0xF800:
        return "BL_LO", f"imm11=0x{h & 0x7FF:X}", ["bl_suffix"], 2
    if (h & 0xFF00) == 0x4700:
        rm = (h >> 3) & 0xF
        return "BX" if (h & 0x80) == 0 else "BLX", reg(rm), ann, 2
    if (h & 0xFFC0) == 0x4280:
        rn, rm = (h >> 3) & 7, h & 7
        return "CMP", f"{reg(rn)}, {reg(rm)}", ann, 2
    if (h & 0xF800) == 0x2800:
        rn, imm = (h >> 8) & 7, h & 0xFF
        return "CMP", f"{reg(rn)}, #0x{imm:X}", ann, 2
    return f".hword", f"0x{h:04X}", ["undecoded"], 2


def decode_bl(hi: int, lo: int, pc: int) -> tuple[str, str, list[str], int] | None:
    if (hi & 0xF800) != 0xF000 or (lo & 0xF800) != 0xF800:
        return None
    imm11 = hi & 0x7FF
    imm11_lo = lo & 0x7FF
    s = (imm11 >> 10) & 1
    j1 = (imm11_lo >> 10) & 1  # wrong — Thumb BL encoding uses lo bits differently
    # Standard Thumb BL: H=F000|imm11, L=F800|imm11; offset = sign_extend(imm22)<<1
    imm = ((hi & 0x7FF) << 11) | (lo & 0x7FF)
    if imm & 0x200000:
        imm -= 0x400000
    tgt = (pc + 4 + (imm << 1)) & 0xFFFFFFFF
    return "BL", f"0x{tgt:X}", ["call"], 4


def disasm_range(blob: bytes, lo: int, hi: int) -> list[str]:
    lines: list[str] = []
    pc = lo
    while pc < hi:
        off = pc - CODE_BASE
        if off < 0 or off + 2 > len(blob):
            lines.append(f"0x{pc:08X}: <out of robotol image>")
            break
        h = struct.unpack_from("<H", blob, off)[0]
        if off + 4 <= len(blob):
            h2 = struct.unpack_from("<H", blob, off + 2)[0]
            bl = decode_bl(h, h2, pc)
            if bl:
                mnem, ops, ann, sz = bl
                mark = "  << FOCUS" if FOCUS_LO <= pc < FOCUS_HI else ""
                fault = "  *** FAULT_PC" if pc == FAULT_PC else ""
                raw = f"{h:04X} {h2:04X}"
                a = (" ; " + ",".join(ann)) if ann else ""
                lines.append(f"0x{pc:08X}: {raw}  {mnem:8} {ops}{a}{mark}{fault}")
                pc += sz
                continue
        mnem, ops, ann, sz = decode_half(h, pc)
        mark = "  << FOCUS" if FOCUS_LO <= pc < FOCUS_HI else ""
        fault = "  *** FAULT_PC" if pc == FAULT_PC else ""
        raw = f"{h:04X}"
        a = (" ; " + ",".join(ann)) if ann else ""
        lines.append(f"0x{pc:08X}: {raw}      {mnem:8} {ops}{a}{mark}{fault}")
        pc += sz
    return lines


def main() -> None:
    if not ROB.exists():
        raise SystemExit(f"missing {ROB}")
    blob = ROB.read_bytes()
    lines = [
        f"# C0 reset/start disasm robotol CODE_BASE=0x{CODE_BASE:X}",
        f"# range 0x{RANGE_LO:X}..0x{RANGE_HI:X}  focus 0x{FOCUS_LO:X}..0x{FOCUS_HI:X}",
        f"# FAULT_PC=0x{FAULT_PC:X} expected LDRSH r1,[r1,r0] raw=0x5E09 (Case A: base NULL)",
        "",
        "## full range",
    ]
    lines.extend(disasm_range(blob, RANGE_LO, RANGE_HI + 2))
    lines.append("")
    lines.append("## focus window")
    lines.extend(disasm_range(blob, FOCUS_LO, FOCUS_HI + 2))
    # Annotate fault raw
    off = FAULT_PC - CODE_BASE
    raw = struct.unpack_from("<H", blob, off)[0]
    lines.append("")
    lines.append(f"## fault site detail")
    lines.append(f"PC=0x{FAULT_PC:X} mode=Thumb raw=0x{raw:04X}")
    mnem, ops, ann, _ = decode_half(raw, FAULT_PC)
    lines.append(f"mnemonic={mnem} ops={ops} ann={ann}")
    if (raw & 0xFFC0) == 0x5E00:
        rt, rn, rm = raw & 7, (raw >> 3) & 7, (raw >> 6) & 7
        lines.append(
            f"dest={reg(rt)} base={reg(rn)} index={reg(rm)} "
            f"effective=[{reg(rn)}+{reg(rm)}] width=signed_halfword"
        )
        lines.append(
            "runtime Case A: if base(r1)==0 after r1=*(R9+0xE6C), nested table NULL"
        )
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT} ({len(lines)} lines)")
    for L in lines:
        if "FAULT" in L or "FOCUS" in L or L.startswith("##") or L.startswith("PC=") or L.startswith("mnemonic") or L.startswith("dest="):
            print(L)


if __name__ == "__main__":
    main()
