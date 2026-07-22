#!/usr/bin/env python3
"""E10A-3.1h prep: decode gamelist 0x2E3180 field-read pointer chain."""
from __future__ import annotations

from pathlib import Path

from phase6j_common import member_blob


def u16(data: bytes, i: int) -> int:
    return data[i] | (data[i + 1] << 8)


def u32(data: bytes, i: int) -> int:
    return int.from_bytes(data[i : i + 4], "little")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    out = root / "out" / "e10a31h"
    out.mkdir(parents=True, exist_ok=True)
    blob = member_blob(root / "game_files/mythroad/320x480/gwy/gamelist.mrp", "gamelist.ext")
    assert blob
    gbase = 0x2D4354
    start = 0x2E3180
    data = blob[start - gbase : start - gbase + 0x40]

    lines = [
        "# E10A-3.1h: gamelist field-read 0x2E3180 pointer chain",
        "",
        "## Entry ABI (live)",
        "",
        "- r0 = field_off = `0x349`",
        "- r1 = dst = `sp+0x30`",
        "- r2 = len = `3`",
        "- r9 = gamelist ERW `0x682B6C` at call; switches to cfunction for memcpy",
        "",
        "## Thumb body (annotated)",
        "",
    ]

    # Manual known-good decode from bytes
    annotations = [
        (0x2E3180, "ADDS r3, r0, #0     ; r3 = field_off"),
        (0x2E3182, "ADDS r0, r1, #0     ; r0 = dst"),
        (0x2E3184, "MOVS r1, #0x87"),
        (0x2E3186, "LSLS r1, r1, #5     ; r1 = 0x10E0 range_limit"),
        (0x2E3188, "CMP  r3, r1"),
        (0x2E318A, "PUSH {r4, lr}"),
        (0x2E318C, "BGE  fail_neg1      ; off >= limit"),
        (0x2E318E, "CMP  r3, #0"),
        (0x2E3190, "BLT  fail_neg1"),
        (0x2E3192, "ADDS r4, r3, r2     ; end = off+len"),
        (0x2E3194, "CMP  r4, r1"),
        (0x2E3196, "BLT  ok_path"),
        (0x2E3198, "MOVS r0, #0"),
        (0x2E319A, "MVNS r0, r0         ; return -1"),
        (0x2E319C, "POP  {r4, pc}"),
        (0x2E319E, "LDR  r1, [pc, #0x14]; lit -> addend"),
        (0x2E31A0, "ADD  r1, pc         ; r1 = &global_obj_slot"),
        (0x2E31A2, "LDR  r4, [r1, #0x38]; r4 = *(global+0x38)"),
        (0x2E31A4, "MOVS r1, #0xFF"),
        (0x2E31A6, "ADDS r1, #0xC1      ; r1 = 0x1C0"),
        (0x2E31A8, "LDR  r1, [r4, r1]   ; EXPECT: r1 = *(obj+0x1C0) buffer_base"),
        (0x2E31AA, "ADDS r1, r1, r3     ; r1 = buffer_base + field_off"),
        (0x2E31AC, "LDR  r3, [r4, #0xC] ; r3 = memcpy-like fn from obj+0xC"),
        (0x2E31AE, "BLX  r3             ; memcpy(dst, src, len)  [r0 still dst, r2 len]"),
        (0x2E31B0, "MOVS r0, #0"),
        (0x2E31B2, "POP  {r4, pc}"),
    ]
    for pc, note in annotations:
        off = pc - start
        if 0 <= off + 1 < len(data):
            hx = f"{data[off]:02X}{data[off+1]:02X}"
        else:
            hx = "????"
        lines.append(f"  0x{pc:X}: {hx}  {note}")

    lit_off = 0x2E31B4 - gbase
    lit = int.from_bytes(blob[lit_off : lit_off + 4], "little", signed=True)
    pc_add = 0x2E31A0 + 4
    glob = (lit + pc_add) & 0xFFFFFFFF
    lines += [
        "",
        "## Materialized addresses",
        "",
        f"- literal @ 0x2E31B4 = {lit} (0x{lit & 0xFFFFFFFF:X})",
        f"- global_obj_slot = lit + (0x2E31A0+4) = **0x{glob:X}**",
        f"- object = *(global_obj_slot + 0x38)",
        f"- buffer_base = *(object + 0x1C0)",
        f"- live memcpy src = 0x28101D ⇒ buffer_base = **0x280CD4**",
        f"- 0x280CD4 - cfunction_ERW(0x280400) = **0x8D4**",
        "",
        "## Hypotheses",
        "",
        "1. `*(object+0x1C0)` should be a file/work buffer; live value equals cfunction ERW+0x8D4.",
        "2. Historical ERW writes at off=0x8D4 were **status words** (e.g. 0xFFFFFF9D), not file bytes —",
        "   so using ERW+0x8D4 as a content buffer is suspicious / wrong binding.",
        "3. mem_get allocated `base=0x282A54 len=4MB` in the same run — a plausible intended filebuf,",
        "   but GPT field read did **not** use it (used 0x280CD4 instead).",
        "4. Offset 0x349 is a packed-field index into that buffer; empty ⇒ tag mismatch vs `\"GPT\"`.",
        "",
        "## Next probes",
        "",
        "- Live dump: `*(0x2D431C+0x38)`, `*(object+0x1C0)`, 16 bytes at base and base+0x349",
        "- Find writers of `object+0x1C0` (buffer_base publish)",
        "- Correlate with method0 input/filebuf ABI (still open, now primary-adjacent)",
        "",
    ]
    path = out / "gamelist_2e3180_ptrchain.txt"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {path}")
    print(f"global_obj_slot=0x{glob:X} buffer_base_live=0x280CD4 erw_delta=0x8D4")

    # Verify LDR encoding at 0x2E31A8
    h = u16(data, 0x2E31A8 - start)
    rm = (h >> 6) & 7
    rn = (h >> 3) & 7
    rd = h & 7
    print(f"half@0x2E31A8=0x{h:04X} LDR r{rd}, [r{rn}, r{rm}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
