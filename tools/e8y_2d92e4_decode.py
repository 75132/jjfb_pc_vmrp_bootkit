#!/usr/bin/env python3
"""E8Y: decode 0x2D92E4 resource init + xref R9+0xA64/A68/A6C writers."""
from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
from e8w_f6c_object_xref import CODE_BASE, decode, find_bl_callers, find_fn_start, u16, u32

EXT = Path("out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext")
OUT = Path("out/e8y_tmp")


def find_end(blob: bytes, fn: int, maxlen: int = 0x1000) -> int:
    pc = fn + 2
    while pc < fn + maxlen:
        h = u16(blob, pc - CODE_BASE)
        # POP {...,pc} / BX lr end markers after some body
        if (h & 0xFF00) == 0xBD00 and pc > fn + 0x20:
            return pc + 2
        if h == 0x4770 and pc > fn + 0x20:  # BX lr
            return pc + 2
        if (h & 0xFF00) == 0xB500 and pc > fn + 0x40:
            return pc
        pc += 2
    return fn + maxlen


def dump(blob: bytes, start: int, end: int):
    lines: List[str] = []
    bls: List[Tuple[int, int]] = []
    r9_ops: List[str] = []
    pc = start
    while pc < end:
        sz, text, meta = decode(blob, pc)
        mark = ""
        if meta.get("bl"):
            bls.append((pc, meta["bl"]))
            mark += " ;BL"
        lv = meta.get("lit_val")
        if lv is not None:
            mark += f" lit=0x{lv:X}"
            if lv in (0xA58, 0xA5C, 0xA60, 0xA64, 0xA68, 0xA6C, 0xA70):
                r9_ops.append(f"0x{pc:X}: {text} lit=0x{lv:X}")
                mark += " ;R9_Axx"
        lines.append(f"0x{pc:X}: {text}{mark}")
        pc += sz
    return lines, bls, r9_ops


def litpool_xrefs(blob: bytes, offs: List[int]) -> List[Dict[str, Any]]:
    """Find LDR lit → ADD rN,r9 → STR/LDR of given R9 offsets."""
    hits: List[Dict[str, Any]] = []
    seen = set()
    for i in range(0, len(blob) - 3, 2):
        pc = CODE_BASE + i
        h = u16(blob, i)
        # LDR Rt, [PC, #imm] thumb: 0x48xx / 0x49xx style via decode
        sz, text, meta = decode(blob, pc)
        lv = meta.get("lit_val")
        if lv is None or lv not in offs:
            continue
        # scan forward ~24 bytes for ADD rx,r9 and STR/LDR
        writers = []
        for j in range(i, min(i + 32, len(blob) - 1), 2):
            pc2 = CODE_BASE + j
            sz2, t2, m2 = decode(blob, pc2)
            writers.append(f"0x{pc2:X}: {t2}")
            if "STR" in t2.upper() or "STRB" in t2.upper() or "STRH" in t2.upper():
                key = (pc, lv, pc2)
                if key in seen:
                    break
                seen.add(key)
                fn = find_fn_start(blob, pc)
                callers = find_bl_callers(blob, fn)[:8] if fn else []
                hits.append(
                    {
                        "kind": "STR_via_lit" if "STR" in t2.upper() else "other",
                        "lit_pc": f"0x{pc:X}",
                        "offset": f"0x{lv:X}",
                        "str_pc": f"0x{pc2:X}",
                        "insn": t2,
                        "window": writers[:12],
                        "fn": f"0x{fn:X}" if fn else None,
                        "callers": [f"0x{c:X}" for c in callers],
                    }
                )
                break
            if sz2 == 4:
                j += 2
        if sz == 4:
            pass
    return hits


def main() -> int:
    blob = EXT.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)

    fn = 0x2D92E4
    end = find_end(blob, fn, 0x800)
    lines, bls, r9_ops = dump(blob, fn, end)
    (OUT / "2d92e4_disasm.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    # Also dump a bit past first POP for tail helpers
    lines2, bls2, _ = dump(blob, fn, min(fn + 0x200, end + 0x80))
    (OUT / "2d92e4_extended.txt").write_text("\n".join(lines2) + "\n", encoding="utf-8")

    callers = find_bl_callers(blob, fn)
    (OUT / "2d92e4_callers.txt").write_text(
        "\n".join(f"0x{c:X}" for c in callers) + "\n", encoding="utf-8"
    )

    # 0x2F449C region around A64 check / 2D92E4 calls
    draw_lines, draw_bls, draw_r9 = dump(blob, 0x2F449C, 0x2F4700)
    (OUT / "2f449c_a64_path.txt").write_text("\n".join(draw_lines) + "\n", encoding="utf-8")

    offs = [0xA58, 0xA5C, 0xA60, 0xA64, 0xA68, 0xA6C, 0xA70]
    xrefs = litpool_xrefs(blob, offs)
    (OUT / "a64_xrefs.json").write_text(json.dumps(xrefs, indent=2), encoding="utf-8")

    # Capstone fallback for 2D92E4 if custom decode is thin
    try:
        import capstone

        md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
        md.detail = True
        off = fn - CODE_BASE
        code = blob[off : off + (end - fn)]
        cs_lines = []
        for insn in md.disasm(code, fn):
            cs_lines.append(f"0x{insn.address:X}: {insn.mnemonic} {insn.op_str}")
        (OUT / "2d92e4_capstone.txt").write_text("\n".join(cs_lines) + "\n", encoding="utf-8")
    except Exception as e:
        (OUT / "2d92e4_capstone.txt").write_text(f"capstone_fail: {e}\n", encoding="utf-8")

    # SVC search inside 2D92E4 body
    svc_pcs = []
    pc = fn
    while pc < end:
        h = u16(blob, pc - CODE_BASE)
        if (h & 0xFF00) == 0xDF00:  # SVC #imm
            svc_pcs.append({"pc": f"0x{pc:X}", "imm": h & 0xFF})
        pc += 2

    report = {
        "code_base": f"0x{CODE_BASE:X}",
        "fn_2D92E4": {"start": f"0x{fn:X}", "end": f"0x{end:X}", "size": end - fn},
        "bls": [{"pc": f"0x{a:X}", "tgt": f"0x{b:X}"} for a, b in bls],
        "r9_ops_in_fn": r9_ops,
        "callers_n": len(callers),
        "callers_head": [f"0x{c:X}" for c in callers[:40]],
        "svc_in_fn": svc_pcs,
        "a64_xrefs_n": len(xrefs),
        "a64_str_xrefs": [x for x in xrefs if "STR" in x.get("kind", "")],
        "draw_path_r9": draw_r9,
        "hypothesis": [
            "2F449C with A64==0 calls 2D92E4 to resolve string tables into A64/A68/A6C",
            "E8Y next: survive 2D92E4 then resume toward 0x310BBC",
        ],
    }
    (OUT / "e8y_decode_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    print(f"2D92E4 size={end-fn} bls={len(bls)} callers={len(callers)} svc={svc_pcs}")
    print("--- early lines ---")
    for L in lines[:60]:
        print(L)
    print("--- BLs ---")
    for a, b in bls[:20]:
        print(f"  0x{a:X} -> 0x{b:X}")
    print(f"--- A64 STR xrefs {len(report['a64_str_xrefs'])} ---")
    for x in report["a64_str_xrefs"][:15]:
        print(x)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
