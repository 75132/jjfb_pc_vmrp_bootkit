#!/usr/bin/env python3
"""v63: map event codes → 2DC4D8 / 2DADC4; clarify B71 vs Path A bootstrap."""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

EXT = 0x2D8DF4
ROOT = Path(r"C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit")
MRP = ROOT / r"runtime\vmrp_win32\vmrp_win32_20220102\mythroad\240x320\gwy\jjfb.mrp"
OUT = ROOT / "reports" / "v63_b71_event_path_static_map.md"

NAMES = {
    0: "MR_KEY_PRESS",
    1: "MR_KEY_RELEASE",
    2: "MR_MOUSE_DOWN",
    3: "MR_MOUSE_UP",
    4: "MR_MENU_SELECT",
    5: "MR_MENU_RETURN",
    6: "MR_DIALOG_EVENT",
    7: "MR_SMS_INDICATION",
    8: "MR_EXIT_EVENT",
    9: "MR_SMS_RESULT",
    10: "MR_LOCALUI_EVENT",
    11: "MR_OSD_EVENT",
    12: "MR_MOUSE_MOVE",
    13: "MR_ERROR_EVENT",
}


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


def dump(rob: bytes, va: int, nbytes: int = 180) -> list[str]:
    lines: list[str] = []
    off = va - EXT
    i = off
    end_i = min(len(rob), off + nbytes)
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
        elif (hw & 0xF800) == 0x7000:
            extra = f" strb r{hw & 7},[r{(hw >> 3) & 7},#{(hw >> 6) & 0x1F}]"
        elif (hw & 0xFE00) == 0x5600:
            extra = f" ldrsb r{hw & 7},[r{(hw >> 3) & 7},r{(hw >> 6) & 7}]"
        elif (hw & 0xFF00) == 0x4400:
            extra = " ADD ERW"
        elif (hw & 0xF000) == 0xD000:
            imm = hw & 0xFF
            if imm & 0x80:
                imm -= 256
            extra = f" Bcond{(hw >> 8) & 0xF} -> 0x{va2 + 4 + imm * 2:X}"
        elif (hw & 0xF800) == 0xE000:
            imm = hw & 0x7FF
            if imm & 0x400:
                imm -= 0x800
            extra = f" B -> 0x{va2 + 4 + imm * 2:X}"
        lines.append(f"0x{va2:08X}: {hw:04X}{extra}")
        if (hw & 0xFF00) == 0xBD00 and va2 > va:
            break
        i += 2
    return lines


def main() -> int:
    rob = load_robotol()
    lines: list[str] = []
    lines.append("# v63 B71 / Event Path Static Map")
    lines.append("")
    lines.append("## Jump table `0x2E2520` (index = event_code - 3)")
    lines.append("")
    lines.append("| event | name | dest | note |")
    lines.append("|------:|------|------|------|")
    table = 0x2E2544
    pc_base = 0x2E2540
    for idx in range(0, 20):
        off = table - EXT + idx * 2
        hw = struct.unpack_from("<H", rob, off)[0]
        dest = pc_base + hw * 2
        ev = idx + 3
        note = ""
        if abs(dest - 0x2E37A0) < 8:
            note = "→ BL 2DC4D8 (B71/15D)"
        if abs(dest - 0x2E4066) < 8:
            note = "→ BL 2DADC4 (Path A gate)"
        # also detect nearby BL targets by scanning a few instructions
        lines.append(f"| {ev} | `{NAMES.get(ev, '?')}` | `0x{dest:X}` | {note} |")

    lines.append("")
    lines.append("## Key correction vs v62 candidate")
    lines.append("")
    lines.append("- `MR_MOUSE_UP(3)` → `2DC4D8`.")
    lines.append("- When `15D==1`, that path sets **`134D=2` and `B71=1`** together.")
    lines.append("- `305EB8` requires `134D==0`, so this path **cannot** open Path C by itself.")
    lines.append("- When `15D!=1`, it only sets `15D=1` (no B71).")
    lines.append("- Therefore MOUSE_UP is **not** the bootstrap for `305EB8→2DADC4`.")
    lines.append("")
    lines.append("## Path A (bypass 305EB8 gates)")
    lines.append("")
    lines.append("- `MR_MENU_RETURN(5)` / `MR_MOUSE_MOVE(12)` → `2E4066` → `2DADC4`.")
    lines.append("- This enters gate_init **without** needing B71.")
    lines.append("- After `2DADC4`, tail `2DAE72` calls `30ED2C(r1=1)` which can set B71 for later Path C ticks.")
    lines.append("")
    lines.append("## 2DC4D8")
    lines.append("")
    lines.append("```text")
    lines.extend(dump(rob, 0x2DC4D8, 140))
    lines.append("```")
    lines.append("")
    lines.append("## 2E4066")
    lines.append("")
    lines.append("```text")
    lines.extend(dump(rob, 0x2E4060, 48))
    lines.append("```")
    lines.append("")
    lines.append("## Dynamic next step")
    lines.append("")
    lines.append("Observe whether host ever delivers event 5/12 into robotol queue,")
    lines.append("and whether a single controlled Path-A probe (opt-in env) reaches 2DADC4.")
    lines.append("No FORCE ui_mode / C0 inject / host UI / event-code blind scan.")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT}")
    for idx in range(0, 14):
        off = table - EXT + idx * 2
        hw = struct.unpack_from("<H", rob, off)[0]
        dest = pc_base + hw * 2
        ev = idx + 3
        mark = ""
        if abs(dest - 0x2E37A0) < 8:
            mark = " 2DC4D8"
        if 0x2E4050 <= dest <= 0x2E4080:
            mark = " near2E4066"
        print(f"ev={ev:2d} {NAMES.get(ev,'?'):20s} dest=0x{dest:X}{mark}")
    print("--- 2DC4D8 ---")
    print("\n".join(dump(rob, 0x2DC4D8, 140)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
