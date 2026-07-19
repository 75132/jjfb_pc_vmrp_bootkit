#!/usr/bin/env python3
"""Stage E8C: resolve robotol idle-loop flag literals at 0x3066AC..0x3066DC (observe-only)."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Optional, Tuple


def u16(b: bytes, off: int) -> int:
    return struct.unpack_from("<H", b, off)[0]


def u32(b: bytes, off: int) -> int:
    return struct.unpack_from("<I", b, off)[0]


def sign_extend(val: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (val & (sign - 1)) - (val & sign)


def ldr_literal(va: int, h: int) -> Optional[Tuple[int, int]]:
    """Thumb LDR Rd, [pc, #imm8*4] -> (rd, literal_va)."""
    if (h & 0xF800) != 0x4800:
        return None
    rd = (h >> 8) & 7
    imm = (h & 0xFF) << 2
    lit = ((va + 4) & ~2) + imm
    return rd, lit


def bcond_target(va: int, h: int) -> int | None:
    if (h & 0xF000) != 0xD000 or ((h >> 8) & 0xF) == 0xF:
        return None
    imm = sign_extend(h & 0xFF, 8) << 1
    return (va + 4 + imm) | 1


def decode_site(blob: bytes, code_base: int, ldr_va: int) -> dict:
    off = ldr_va - code_base
    h0 = u16(blob, off)
    lit = ldr_literal(ldr_va, h0)
    if not lit:
        return {"ldr_va": hex(ldr_va), "error": f"not LDR lit h=0x{h0:04X}"}
    rd, lit_va = lit
    lit_off = lit_va - code_base
    er_off = u32(blob, lit_off)

    # Walk following insns: optional MOVS, ADD Rd,R9, LDRSB/LDR, CMP, Bcond
    i = off + 2
    load_width = "unknown"
    cmp_imm = None
    branch_false = None
    seq: list[str] = [f"LDR r{rd}, [pc] -> lit=0x{lit_va:X} val=0x{er_off:X}"]
    pc = ldr_va + 2
    steps = 0
    while i + 1 < len(blob) and steps < 8:
        h = u16(blob, i)
        if h == 0x2300:
            seq.append("MOVS r3, #0")
        elif (h & 0xFF00) == 0x4400 and ((h >> 3) & 0xF) == 9:
            # ADD Rd, R9 (high-reg form 01000100 Rd Rm)
            seq.append(f"ADD r{h & 7}, r9")
        elif h == 0x56C0:
            load_width = "s8"
            seq.append("LDRSB r0, [r0, r3]")
        elif (h & 0xF800) == 0x6800 and (h & 0x7) == 0:
            load_width = "u32"
            seq.append(f"LDR r{((h >> 8) & 7)}, [r{((h >> 3) & 7)}]")
        elif (h & 0xFF00) == 0x2800:
            cmp_imm = h & 0xFF
            seq.append(f"CMP r0, #{cmp_imm}")
        elif (h & 0xF000) == 0xD000:
            branch_false = bcond_target(pc, h)
            cond = (h >> 8) & 0xF
            seq.append(f"Bcond cond={cond} -> 0x{branch_false:X}" if branch_false else "Bcond")
            break
        else:
            # stop on unexpected
            if load_width != "unknown" and cmp_imm is not None:
                break
            seq.append(f"h=0x{h:04X}")
        i += 2
        pc += 2
        steps += 1

    return {
        "ldr_va": f"0x{ldr_va:X}",
        "file_off": f"0x{off:X}",
        "literal_va": f"0x{lit_va:X}",
        "er_rw_offset": f"0x{er_off:X}",
        "er_rw_offset_u": er_off,
        "load_width": load_width,
        "cmp_imm": cmp_imm,
        "branch_if_cmp_fail": f"0x{branch_false:X}" if branch_false else None,
        "note": "flag_guest = R9 + er_rw_offset (R9 = robotol ER_RW during lifecycle)",
        "seq": seq,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext", required=True)
    ap.add_argument("--code-base", type=lambda x: int(x, 0), default=0x2D8DF4)
    ap.add_argument(
        "--sites",
        default="0x3066AC,0x3066BC,0x3066C8,0x3066D2,0x3066DC",
        help="comma LDR site VAs",
    )
    ap.add_argument("-o", "--out-dir", default="out/e8c_tmp")
    args = ap.parse_args()

    blob = Path(args.ext).read_bytes()
    sites = [int(x.strip(), 0) for x in args.sites.split(",") if x.strip()]
    flags = [decode_site(blob, args.code_base, va) for va in sites]
    offsets = [f["er_rw_offset"] for f in flags if "er_rw_offset" in f]

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    payload = {
        "code_base": f"0x{args.code_base:X}",
        "ext": str(args.ext),
        "flags": flags,
        "watch_offsets": offsets,
        "watch_offsets_csv": ",".join(offsets),
        "evidence": "TARGET_OBSERVED",
    }
    (out_dir / "flag_map.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")

    lines = [
        "# E8C idle flag resolve",
        f"ext={args.ext}",
        f"code_base=0x{args.code_base:X}",
        f"watch_offsets={payload['watch_offsets_csv']}",
        "",
    ]
    for f in flags:
        lines.append(f"## {f.get('ldr_va')}")
        for k, v in f.items():
            if k == "seq":
                lines.append("seq:")
                for s in v:
                    lines.append(f"  - {s}")
            else:
                lines.append(f"  {k}: {v}")
        lines.append("")
    (out_dir / "flag_resolve.txt").write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out_dir / 'flag_map.json'}")
    print(f"watch_offsets={payload['watch_offsets_csv']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
