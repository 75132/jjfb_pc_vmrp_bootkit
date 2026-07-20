#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from e8w_f6c_object_xref import CODE_BASE, decode, find_fn_start, find_bl_callers

blob = (ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext").read_bytes()
OUT = Path(__file__).resolve().parent / "e9t_upstream.txt"


def dis(start: int, end: int, label: str) -> list[str]:
    lines = [f"===== {label} 0x{start:X}..0x{end:X} ====="]
    pc = start
    while pc < end:
        sz, text, meta = decode(blob, pc)
        m = ""
        bl = meta.get("bl")
        if bl is not None:
            m += f"  ; BL->0x{bl:X}"
        if "STR" in text:
            m += "  ; STORE"
        lines.append(f"0x{pc:08X}: {text}{m}")
        pc += sz
    return lines


lines: list[str] = []
lines += dis(0x2FC03C, 0x2FC070, "2FC03C")
lines += dis(0x2FC1E0, 0x2FC260, "near_STR_2C")
s = find_fn_start(blob, 0x2FED14)
lines += dis(s, s + 0x60, "2FED14")
s2 = find_fn_start(blob, 0x30EE50)
lines += dis(s2, s2 + 0x80, "30EE50")
lines.append(f"callers_2FC03C={[hex(c) for c in find_bl_callers(blob, 0x2FC03C)]}")
lines.append(f"callers_30EE50={[hex(c) for c in find_bl_callers(blob, 0x30EE50)]}")
OUT.write_text("\n".join(lines), encoding="utf-8")
print("\n".join(lines[:80]))
print("wrote", OUT)
