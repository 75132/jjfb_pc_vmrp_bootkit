#!/usr/bin/env python3
"""E8X: decode 0x2F2854 thin wrapper → 0x2EA188 worker + F74/0x2F99D0 ABI."""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from e8w_f6c_object_xref import CODE_BASE, decode, find_bl_callers, u16, u32

EXT = Path("out/JJFB_E8A_delivery/02_mrp_extracted/jjfb/robotol.ext")


def find_end(blob: bytes, fn: int, maxlen: int = 0x800) -> int:
    pc = fn + 2
    while pc < fn + maxlen:
        h = u16(blob, pc - CODE_BASE)
        if (h & 0xFF00) == 0xB500 and pc > fn + 0x30:
            return pc
        pc += 2
    return fn + maxlen


def dump(blob: bytes, start: int, end: int):
    lines = []
    bls = []
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
        lines.append(f"0x{pc:X}: {text}{mark}")
        pc += sz
    return lines, bls


def main() -> int:
    blob = EXT.read_bytes()
    out = Path("out/e8x_tmp")
    out.mkdir(parents=True, exist_ok=True)

    # Thin wrapper ends quickly at POP.
    wrap_lines, wrap_bls = dump(blob, 0x2F2854, 0x2F287C)
    (out / "2f2854_wrapper.txt").write_text("\n".join(wrap_lines) + "\n", encoding="utf-8")

    fn = 0x2EA188
    end = find_end(blob, fn)
    lines, bls = dump(blob, fn, end)
    (out / "2ea188_disasm.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    # Early predicates in 2EA188
    early = []
    for L in lines[:120]:
        if any(k in L for k in ("CMP", "BEQ", "BNE", "BGT", "BLE", "BL ", "lit=", "BX", "POP")):
            early.append(L)

    # 2F9970 / 2F9964 helpers
    for name, va in [("2f9970", 0x2F9970), ("2f9964", 0x2F9964), ("2f99d0", 0x2F99D0)]:
        e = find_end(blob, va, 0x400 if va == 0x2F99D0 else 0x100)
        ls, bs = dump(blob, va, e)
        (out / f"{name}_disasm.txt").write_text("\n".join(ls) + "\n", encoding="utf-8")

    caller, _ = dump(blob, 0x2E8908, 0x2E89C0)
    (out / "e88cc_2f2854_caller.txt").write_text("\n".join(caller) + "\n", encoding="utf-8")

    report = {
        "wrapper_2F2854": {
            "role": "thin wrapper: r4=r0 r5=r1 r6=r2; BL 0x2EA188; return",
            "always_r0_from_caller": "MOVS r0,#0 before both BL sites — r0 is ALWAYS 0",
            "bls": [{"pc": f"0x{a:X}", "tgt": f"0x{b:X}"} for a, b in wrap_bls],
            "lines": wrap_lines,
        },
        "worker_2EA188": {
            "end": f"0x{end:X}",
            "size": end - fn,
            "bls": [{"pc": f"0x{a:X}", "tgt": f"0x{b:X}"} for a, b in bls],
            "callers": [f"0x{c:X}" for c in find_bl_callers(blob, 0x2EA188)[:20]],
            "early_predicates": early,
        },
        "caller_abi": {
            "0x2E8980": "r0=0, r1=layout_delta_or_0, r2=ret_2F9970, r3=height_like",
            "0x2E89A8": "r0=0, r1=layout_delta_or_0, r2=ret_2F9970, r3=width_like; E8W hit lr=0x2E89AD",
            "note": "F74 is NOT passed in R0-R3 to 0x2F2854; layout math uses F74 via 0x2F5B38 earlier",
        },
        "f74_producer_2F99D0": {
            "callsite": "0x2E891A with r0 from prior path, r1=r6-0x1E",
            "writes_F74_at": "0x2E8920 STR r0,[r5,#8] after return",
            "starts_with": "BL 0x312AA4 then string/path ops via 0x2D9648 / 0x305E78",
        },
        "hypothesis": [
            "2F2854_RETURNS_NOOP_DUE_ZERO_ARGS if 2EA188 early-outs on r1/r2==0",
            "Real draw may be in 2EA188 BL targets (graphics/resource)",
            "F74 scratch opens gate but does not feed R0-R3; need nonzero r1/r2 from layout math",
        ],
    }
    (out / "e8x_decode_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("wrapper bls:", report["wrapper_2F2854"]["bls"])
    print("2EA188 size", report["worker_2EA188"]["size"], "bls", len(bls))
    print("--- 2EA188 early ---")
    for L in early[:60]:
        print(L)
    print("--- wrapper ---")
    print("\n".join(wrap_lines))
    print("--- caller F74/2F2854 ---")
    for L in caller:
        if any(k in L for k in ("2F2854", "2F99D0", "2F5B38", "2F9970", "MOVS r0", "F6C", "BGE", "BEQ")):
            print(L)

    # 0x2F449C — real work called from 0x2EA188
    fn449 = 0x2F449C
    end449 = find_end(blob, fn449, 0x800)
    l449, b449 = dump(blob, fn449, end449)
    (out / "2f449c_disasm.txt").write_text("\n".join(l449) + "\n", encoding="utf-8")
    print("\n=== 2F449C size", end449 - fn449, "bls", len(b449), "===")
    for a, b in b449[:25]:
        print(f"  0x{a:X} -> 0x{b:X}")
    print("\n".join(l449[:70]))

    # writers / readers of R9+0x830 (2F9970 return)
    hits_830 = []
    for i in range(0, len(blob) - 1, 2):
        h = u16(blob, i)
        if (h & 0xF800) != 0x4800:
            continue
        pc = CODE_BASE + i
        lit = ((pc + 4) & ~2) + ((h & 0xFF) << 2)
        if not (0 <= lit - CODE_BASE + 3 < len(blob)):
            continue
        if u32(blob, lit - CODE_BASE) != 0x830:
            continue
        rd = (h >> 8) & 7
        j = i + 2
        base = None
        for _ in range(36):
            sz, text, meta = decode(blob, CODE_BASE + j)
            if meta.get("add_reg") and meta.get("rm") == 9 and meta.get("rd") == rd:
                base = rd
            if base is not None and meta.get("str") and meta.get("rn") == base:
                hits_830.append(
                    {
                        "kind": "STR",
                        "pc": f"0x{CODE_BASE + j:X}",
                        "insn": text,
                        "lit_pc": f"0x{pc:X}",
                    }
                )
            if base is not None and meta.get("ldr") and meta.get("rn") == base:
                hits_830.append(
                    {
                        "kind": "LDR",
                        "pc": f"0x{CODE_BASE + j:X}",
                        "insn": text,
                        "lit_pc": f"0x{pc:X}",
                    }
                )
            j += sz
            if meta.get("pop_pc") or text == "BX lr":
                break
    (out / "r9_830_xrefs.json").write_text(json.dumps(hits_830, indent=2), encoding="utf-8")
    print("\n=== R9+0x830 xrefs", len(hits_830), "===")
    for h830 in hits_830[:30]:
        print(h830)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
