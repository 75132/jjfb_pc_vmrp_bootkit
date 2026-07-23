#!/usr/bin/env python3
"""Static recover of robotol list path: 0x30D2F9 / 0x30D24C / 0x2E4D6C / 0x312A60 / B54 init."""
from __future__ import annotations

import json
import struct
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
BLOB_PATH = ROOT / "out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext"
CODE_BASE = 0x2D8DF4
OUT_DIR = ROOT / "out/product_event"
REP = ROOT / "reports"

SITES = {
    "enq_trampoline": 0x30D2F8,
    "enq_body": 0x30D24C,
    "path_a_publish": 0x2E4D6C,
    "list_push": 0x312A60,
    "list_ctor": 0x312AA4,
    "b54_init": 0x2FE82C,
    "case2_init": 0x30CBBC,
}


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def bl_target(pc: int, h0: int, h1: int) -> Optional[int]:
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
    return (pc + 4 + imm32) & ~1


def decode(blob: bytes, pc: int) -> Tuple[int, str, Dict[str, Any]]:
    off = pc - CODE_BASE
    if off < 0 or off + 1 >= len(blob):
        return 2, "???", {}
    h0 = u16(blob, off)
    meta: Dict[str, Any] = {"h0": f"0x{h0:04X}"}
    if (h0 & 0xF800) == 0xF000 and off + 3 < len(blob):
        h1 = u16(blob, off + 2)
        meta["h1"] = f"0x{h1:04X}"
        t = bl_target(pc, h0, h1)
        if t is not None:
            meta["bl"] = f"0x{t:X}"
            return 4, f"BL 0x{t:X}", meta
        if (h0 & 0xFFF0) == 0xF8D0:
            rn = h0 & 0xF
            rt = (h1 >> 12) & 0xF
            imm = h1 & 0xFFF
            return 4, f"LDR.W r{rt},[r{rn},#0x{imm:X}]", meta
        if (h0 & 0xFFF0) == 0xF8C0:
            rn = h0 & 0xF
            rt = (h1 >> 12) & 0xF
            imm = h1 & 0xFFF
            return 4, f"STR.W r{rt},[r{rn},#0x{imm:X}]", meta
        return 4, f"THUMB32 0x{h0:04X} 0x{h1:04X}", meta
    if (h0 & 0xFF00) == 0xB500:
        return 2, f"PUSH {{..,lr}}", meta
    if (h0 & 0xFF00) == 0xBD00:
        return 2, f"POP {{..,pc}}", meta
    if (h0 & 0xF800) == 0x6800:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        return 2, f"LDR r{rt},[r{rn},#0x{imm:X}]", meta
    if (h0 & 0xF800) == 0x6000:
        rt = h0 & 7
        rn = (h0 >> 3) & 7
        imm = ((h0 >> 6) & 0x1F) << 2
        return 2, f"STR r{rt},[r{rn},#0x{imm:X}]", meta
    if (h0 & 0xFFC0) == 0x4440:
        dn = (h0 >> 7) & 1
        rd = (h0 & 7) | (dn << 3)
        rm = (h0 >> 3) & 0xF
        return 2, f"ADD r{rd},r{rm}", meta
    if (h0 & 0xF800) == 0x4800:
        rt = (h0 >> 8) & 7
        imm = (h0 & 0xFF) << 2
        lit = ((pc + 4) & ~3) + imm
        val = u32(blob, lit - CODE_BASE) if 0 <= lit - CODE_BASE < len(blob) - 3 else 0
        meta["lit"] = f"0x{val:X}"
        return 2, f"LDR r{rt},[pc,#0x{imm:X}] ; =0x{val:X}", meta
    if (h0 & 0xFF00) == 0x2000:
        return 2, f"MOVS r0,#0x{h0 & 0xFF:X}", meta
    return 2, f".hword 0x{h0:04X}", meta


def annotate(blob: bytes, start: int, nbytes: int = 0x80) -> List[str]:
    lines: List[str] = []
    pc = start & ~1
    end = pc + nbytes
    while pc < end:
        n, text, meta = decode(blob, pc)
        note = ""
        if "0xB54" in text or meta.get("lit") == "0xB54":
            note = "  ; *** B54 list head slot ***"
        if "0x312A60" in text:
            note = "  ; list_push"
        if "0x312AA4" in text:
            note = "  ; list_ctor size=8"
        if "0x2FE82C" in text:
            note = "  ; B54 initializer"
        lines.append(f"0x{pc:08X}: {text}{note}")
        pc += n
    return lines


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    REP.mkdir(parents=True, exist_ok=True)
    blob = BLOB_PATH.read_bytes()

    ann_lines: List[str] = []
    for name, addr in SITES.items():
        ann_lines.append(f"===== {name} @ 0x{addr:X} =====")
        ann_lines.extend(annotate(blob, addr, 0xA0 if name != "enq_trampoline" else 0x20))
        ann_lines.append("")

    # Highlight Path-A B54 load
    ann_lines.append("===== path_a B54 load @ 0x2E4EE8 =====")
    ann_lines.extend(annotate(blob, 0x2E4EE0, 0x30))

    (OUT_DIR / "312a60_list_path_annotated.txt").write_text("\n".join(ann_lines) + "\n", encoding="utf-8")

    # CFG
    edges = [
        ("30D2F9", "30D24C", "BL"),
        ("30D24C", "101AB", "plat_buffer_fill"),
        ("30D24C", "2E4D6C", "PathA_publish"),
        ("2E4D6C", "312A60", "list_push via R9+B54"),
        ("30E15E", "30CBBC", "case_app_2"),
        ("30CBBC", "2FE82C", "BL_init"),
        ("2FE82C", "312AA4", "list_ctor"),
        ("312AA4", "B54", "STR_list_ptr"),
        ("B54", "312A60", "required_nonzero"),
    ]
    dot = ["digraph list_path {", '  rankdir=LR;', '  node [shape=box];']
    for a, b, lab in edges:
        dot.append(f'  "{a}" -> "{b}" [label="{lab}"];')
    dot.append("}")
    (OUT_DIR / "312a60_list_path_cfg.dot").write_text("\n".join(dot) + "\n", encoding="utf-8")

    contract = {
        "base_source": "ER_RW+B54",
        "role": "list_control_object",
        "fields": [
            {"offset": "0x0", "size": 4, "access": "read|write", "role": "head_pointer",
             "evidence_pc": "0x312A7E"},
            {"offset": "0x4", "size": 4, "access": "read|write", "role": "count_or_tail_aux",
             "evidence_pc": "0x312A78"},
        ],
        "initializer": {
            "function": "0x312AA4",
            "caller": "0x2FE82C",
            "trigger": "0x30CBBC via 0x30E15E (family case app=2)",
        },
        "verdicts": [
            "EVENT_LIST_OBJECT_ROLE_IDENTIFIED",
            "EVENT_LIST_INITIALIZER_IDENTIFIED",
            "EVENT_LIST_SENTINEL_CONTRACT_IDENTIFIED",
            "EVENT_LIST_FAULT_IS_MISSING_INITIALIZATION",
        ],
    }
    (REP / "product_event_list_contract.json").write_text(
        json.dumps(contract, indent=2) + "\n", encoding="utf-8"
    )
    print("wrote", OUT_DIR / "312a60_list_path_annotated.txt")
    print("wrote", OUT_DIR / "312a60_list_path_cfg.dot")
    print("wrote", REP / "product_event_list_contract.json")


if __name__ == "__main__":
    main()
