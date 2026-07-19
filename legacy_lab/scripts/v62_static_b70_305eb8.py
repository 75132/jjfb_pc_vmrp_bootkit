#!/usr/bin/env python3
"""v62: static map of 305EB8 gates and ERW+0x15D / 0xB71 / 0xB70 writers."""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

EXT = 0x2D8DF4
ROOT = Path(r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit")
MRP = ROOT / r"runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320\gwy\jjfb.mrp"
OUT = ROOT / "reports" / "v62_b70_305eb8_static_map.md"


def load_robotol() -> bytes:
    d = MRP.read_bytes()
    _, hend, _, idx = struct.unpack_from("<4I", d, 0)
    pos, end = idx, hend + 8
    e: dict[str, tuple[int, int, int]] = {}
    while pos < end:
        n = struct.unpack_from("<I", d, pos)[0]
        pos += 4
        name = d[pos : pos + n].rstrip(b"\0").decode("latin1")
        pos += n
        off, clen, flags = struct.unpack_from("<3I", d, pos)
        pos += 12
        e[name] = (off, clen, flags)
    return zlib.decompress(d[e["robotol.ext"][0] : e["robotol.ext"][0] + e["robotol.ext"][1]], 31)


def dump(rob: bytes, va: int, nbytes: int = 200, stop_pop: bool = True) -> list[str]:
    lines: list[str] = []
    off = va - EXT
    i = off
    end_i = min(len(rob), off + nbytes)
    cond_names = {
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
        14: "AL",
    }
    while i < end_i:
        hw = rob[i] | (rob[i + 1] << 8)
        va2 = EXT + i
        extra = ""
        if i + 4 <= len(rob) and (hw & 0xF800) == 0xF000:
            hw2 = rob[i + 2] | (rob[i + 3] << 8)
            if (hw2 & 0xF800) == 0xF800:
                s = (hw >> 10) & 1
                imm10 = hw & 0x3FF
                j1 = (hw2 >> 13) & 1
                j2 = (hw2 >> 11) & 1
                imm11 = hw2 & 0x7FF
                i1 = j1 ^ s ^ 1
                i2 = j2 ^ s ^ 1
                imm = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
                if s:
                    imm |= ~((1 << 25) - 1)
                to = (va2 + 4 + imm) & 0xFFFFFFFF
                lines.append(f"0x{va2:08X}: BL 0x{to:X}")
                i += 4
                continue
        if (hw & 0xFF00) == 0xB500:
            extra = " PUSH"
        elif (hw & 0xFF00) == 0xBD00:
            extra = " POP"
        elif (hw & 0xF800) == 0x2800:
            extra = f" cmp r{(hw >> 8) & 7}, #{hw & 0xFF}"
        elif (hw & 0xF800) == 0x2000:
            extra = f" movs r{(hw >> 8) & 7}, #{hw & 0xFF}"
        elif (hw & 0xF800) == 0x4800:
            imm = (hw & 0xFF) * 4
            base = (va2 + 4) & ~2
            lit = base + imm
            if 0 <= lit - EXT + 3 < len(rob):
                val = struct.unpack_from("<I", rob, lit - EXT)[0]
                extra = f" ldr r{(hw >> 8) & 7}, =0x{val:X}"
        elif (hw & 0xF800) == 0x6800:
            extra = f" ldr r{hw & 7},[r{(hw >> 3) & 7},#{((hw >> 6) & 0x1F) * 4}]"
        elif (hw & 0xF800) == 0x6000:
            extra = f" str r{hw & 7},[r{(hw >> 3) & 7},#{((hw >> 6) & 0x1F) * 4}]"
        elif (hw & 0xF800) == 0x7800:
            extra = f" ldrb r{hw & 7},[r{(hw >> 3) & 7},#{(hw >> 6) & 0x1F}]"
        elif (hw & 0xF800) == 0x7000:
            extra = f" strb r{hw & 7},[r{(hw >> 3) & 7},#{(hw >> 6) & 0x1F}]"
        elif (hw & 0xFE00) == 0x5600:
            rm = (hw >> 6) & 7
            rn = (hw >> 3) & 7
            rd = hw & 7
            extra = f" ldrsb r{rd},[r{rn},r{rm}]"
        elif (hw & 0xF000) == 0xD000:
            cond = (hw >> 8) & 0xF
            imm = hw & 0xFF
            if imm & 0x80:
                imm -= 256
            to = va2 + 4 + imm * 2
            extra = f" B{cond_names.get(cond, '?')} -> 0x{to:X}"
        elif (hw & 0xF800) == 0xE000:
            imm = hw & 0x7FF
            if imm & 0x400:
                imm -= 0x800
            to = va2 + 4 + imm * 2
            extra = f" B -> 0x{to:X}"
        elif hw == 0x4770:
            extra = " bx lr"
        elif (hw & 0xFF87) == 0x4780:
            extra = f" blx r{(hw >> 3) & 0xF}"
        elif (hw & 0xFF00) == 0x4400:
            extra = " ADD high (ERW+)"
        lines.append(f"0x{va2:08X}: {hw:04X}{extra}")
        if stop_pop and (hw & 0xFF00) == 0xBD00 and va2 > va:
            break
        i += 2
    return lines


def find_lits(rob: bytes, val: int) -> list[int]:
    return [i for i in range(0, len(rob) - 3) if struct.unpack_from("<I", rob, i)[0] == val]


def find_ldr_users(rob: bytes, lit_off: int) -> list[int]:
    """Return VAs of LDR rX, [pc,#imm] that resolve to lit_off."""
    lit_va = EXT + lit_off
    users = []
    for i in range(0, len(rob) - 1, 2):
        hw = rob[i] | (rob[i + 1] << 8)
        if (hw & 0xF800) != 0x4800:
            continue
        va = EXT + i
        imm = (hw & 0xFF) * 4
        base = (va + 4) & ~2
        if base + imm == lit_va:
            users.append(va)
    return users


def main() -> int:
    rob = load_robotol()
    lines: list[str] = []
    lines.append("# v62 305EB8 / B70-B71 / 15D Static Map")
    lines.append("")
    lines.append(f"- robotol decompressed: {len(rob)}")
    lines.append(f"- ext_base: `0x{EXT:X}`")
    lines.append("")
    lines.append("## 305EB8 branch conditions (Path C periodic)")
    lines.append("")
    lines.append("```text")
    lines.extend(dump(rob, 0x305EB8, 100))
    lines.append("```")
    lines.append("")
    lines.append("Decoded gates to reach `BL 0x2DADC4` at `0x305EF4`:")
    lines.append("")
    lines.append("1. `LDRSB` from `ERW+0x15C+1` (=`ERW+0x15D`) must equal **1**")
    lines.append("2. `LDRSB` from `ERW+0xB71` must be **nonzero**")
    lines.append("3. `LDRSB` from `ERW+0x134D` must be **0** (then may clear B71)")
    lines.append("4. then `BL 0x2DADC4`")
    lines.append("")
    lines.append("Note: prior probes logged `ERW+0xB70`, but **305EB8 checks `0xB71`**.")
    lines.append("`0xB70` is the later gate inside `0x2DADC4`/`0x2DAE00`.")
    lines.append("")

    for title, va, n in [
        ("305E08 (flag helper near writers)", 0x305E08, 220),
        ("2DAE00 (B70 gate inside 2DADC4)", 0x2DAE00, 80),
        ("2FE940 (candidate B71/15C writer)", 0x2FE940, 80),
        ("30EDC0 (alt 30EE50 path)", 0x30EDC0, 120),
        ("2E4EE0 (15C-related)", 0x2E4EE0, 100),
        ("2DC560 (15C/B71/134D pool user)", 0x2DC560, 80),
    ]:
        lines.append(f"## {title}")
        lines.append("")
        lines.append("```text")
        lines.extend(dump(rob, va, n))
        lines.append("```")
        lines.append("")

    lines.append("## Literal pool xref summary")
    lines.append("")
    lines.append("| offset | meaning | literal VAs | LDR users (sample) |")
    lines.append("|--------|---------|-------------|--------------------|")
    for val, meaning in [
        (0x15C, "base for 15D load"),
        (0x15D, "gate1 must==1"),
        (0xB70, "2DADC4 gate"),
        (0xB71, "305EB8 gate2 nonzero"),
        (0x134D, "305EB8 gate3 must==0"),
    ]:
        lits = find_lits(rob, val)
        users = []
        for lo in lits:
            users.extend(find_ldr_users(rob, lo))
        lit_s = ", ".join(f"`0x{EXT+x:X}`" for x in lits[:6]) or "-"
        usr_s = ", ".join(f"`0x{u:X}`" for u in users[:8]) or "-"
        lines.append(f"| `0x{val:X}` | {meaning} | {lit_s} | {usr_s} |")
    lines.append("")

    lines.append("## Writer candidates (static)")
    lines.append("")
    lines.append("- `0x2FE978`: `movs r0,#1; strb [r1,#0]` near literals `0xB71`/`0x15C`")
    lines.append("- `0x30EE08`: `movs r0,#1; strb [r1,#0]` near `0xB71`/`0x15C` (after `BL 0x30EE50`)")
    lines.append("- `0x2E4F2C`: `movs r0,#1; strb [r1,#0]` near `0x15C`")
    lines.append("- `0x305E08`: helper called from several sites before flag stores")
    lines.append("")
    lines.append("## Dynamic next step")
    lines.append("")
    lines.append("Install mem-write hooks on `ERW+0x15D` / `ERW+0xB71` / `ERW+0xB70`")
    lines.append("and enrich `entry_305EB8` log with `15D`/`B71` values.")
    lines.append("No FORCE ui_mode / C0 inject / host UI.")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT}")
    print("\n".join(dump(rob, 0x305EB8, 100)))
    print("---")
    print("\n".join(dump(rob, 0x305E08, 220)))
    print("---")
    print("\n".join(dump(rob, 0x2FE940, 80)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
