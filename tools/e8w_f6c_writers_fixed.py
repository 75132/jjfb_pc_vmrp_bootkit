#!/usr/bin/env python3
"""E8W: corrected F6C/F70/F74 writer scan + helper decode snippets."""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from e8w_f6c_object_xref import CODE_BASE, decode, find_bl_callers, find_fn_start, u16, u32

EXT = Path("out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext")


def find_writers_fixed(blob: bytes):
    out = []
    for i in range(0, len(blob) - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = CODE_BASE + i
        lit = ((pc + 4) & ~2) + ((h & 0xFF) << 2)
        if not (0 <= lit - CODE_BASE + 3 < len(blob)):
            continue
        L = u32(blob, lit - CODE_BASE)
        if L not in (0xF6C, 0xF70, 0xF74):
            continue
        rd = (h >> 8) & 7
        j = i + 2
        base_reg = None
        base_off = L
        for _ in range(64):
            if j + 1 >= len(blob):
                break
            pc2 = CODE_BASE + j
            sz, text, meta = decode(blob, pc2)
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                base_reg = rd
            if meta.get("str") and base_reg is not None and meta.get("rn") == base_reg:
                imm = int(meta.get("imm") or 0)
                abs_off = base_off + imm
                if abs_off in (0xF6C, 0xF70, 0xF74):
                    rt = meta.get("rt")
                    src = f"r{rt}"
                    for back in range(2, 48, 2):
                        p = pc2 - back
                        if p < CODE_BASE:
                            break
                        _, t2, m2 = decode(blob, p)
                        if m2.get("movs") is not None and m2.get("rd") == rt:
                            src = f"MOVS#{m2['movs']}"
                            break
                        if m2.get("bl") and back <= 12:
                            src = f"ret_BL_0x{m2['bl']:X}"
                            break
                    fn = find_fn_start(blob, pc2)
                    callers = find_bl_callers(blob, fn)
                    if src.startswith("MOVS#0"):
                        kind = "clear0"
                    elif src.startswith("MOVS#"):
                        kind = "nonzero_imm"
                    else:
                        kind = "variable"
                    out.append(
                        {
                            "abs_off": abs_off,
                            "field": f"R9+0x{abs_off:X}",
                            "store_pc": f"0x{pc2:X}",
                            "fn": f"0x{fn:X}",
                            "insn": text,
                            "src": src,
                            "kind": kind,
                            "callers_n": len(callers),
                            "callers": [f"0x{c:X}" for c in callers[:12]],
                        }
                    )
            if meta.get("ldr_pc") and meta.get("rd") == base_reg:
                base_reg = None
            if meta.get("ldr") and meta.get("rt") == base_reg and meta.get("rn") != base_reg:
                base_reg = None
            j += sz
            if meta.get("pop_pc") or text == "BX lr":
                break
    seen = set()
    uniq = []
    for w in out:
        k = (w["store_pc"], w["abs_off"])
        if k in seen:
            continue
        seen.add(k)
        uniq.append(w)
    return uniq


def disasm_fn(blob: bytes, start: int, size: int = 0x80) -> list[str]:
    lines = []
    pc = start
    end = start + size
    while pc < end:
        sz, text, _ = decode(blob, pc)
        lines.append(f"0x{pc:X}: {text}")
        pc += sz
    return lines


def main() -> int:
    blob = EXT.read_bytes()
    out_dir = Path("out/e8w_tmp")
    out_dir.mkdir(parents=True, exist_ok=True)
    writers = find_writers_fixed(blob)
    report = {
        "writers": writers,
        "helpers": {
            "0x2F5B38": disasm_fn(blob, 0x2F5B38, 0xA0),
            "0x2F99D0": disasm_fn(blob, 0x2F99D0, 0x80),
            "0x2F2854": disasm_fn(blob, 0x2F2854, 0x60),
            "0x2D9CBC": disasm_fn(blob, 0x2D9CBC, 0x60),
            "0x2F287C": disasm_fn(blob, 0x2F287C, 0x80),
            "0x2FBB6C": disasm_fn(blob, 0x2FBB6C, 0x80),
        },
        "layout": {
            "note": "Embedded struct at R9+0xF6C",
            "gate": "open if F74!=0 OR F70!=0",
            "F74_use": "passed to BL 0x2F5B38 as r0 (with r1=4)",
            "path_to_2F2854": "0x2E8980 / 0x2E89A8 after layout math; r0=0",
        },
    }
    (out_dir / "f6c_writers_fixed.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"writers={len(writers)}")
    for w in sorted(writers, key=lambda x: (x["abs_off"], x["store_pc"])):
        print(
            f"{w['store_pc']} {w['field']} fn={w['fn']} kind={w['kind']} "
            f"src={w['src']} n={w['callers_n']} {w['callers'][:6]}"
        )
    print("--- 0x2F5B38 ---")
    print("\n".join(report["helpers"]["0x2F5B38"][:40]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
