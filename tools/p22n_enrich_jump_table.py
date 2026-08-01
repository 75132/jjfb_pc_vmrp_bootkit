#!/usr/bin/env python3
"""Enrich P22N opcode jump-table CSV with static tags from cfunction_runtime.bin."""
from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path


def branch_target(pc: int, insn: int) -> int | None:
    if (insn & 0x0E000000) != 0x0A000000:
        return None
    imm = insn & 0x00FFFFFF
    if imm & 0x00800000:
        imm |= ~0x00FFFFFF
    return (pc + 8 + (imm << 2)) & 0xFFFFFFFF


def scan_case(blob: bytes, base: int, case_off: int, limit: int = 0x200) -> dict:
    tags = []
    has_blx = False
    has_bl = False
    writes = False
    calls_helperish = False
    uiish = False
    end = min(len(blob), case_off + limit)
    pc = base + case_off
    off = case_off
    while off + 4 <= end:
        (w,) = struct.unpack_from("<I", blob, off)
        # BL
        if (w & 0x0F000000) == 0x0B000000:
            has_bl = True
            tgt = branch_target(pc, w)
            if tgt is not None:
                toff = (tgt - base) & 0xFFFFFFFF
                if toff in (0x174C8, 0x10740, 0x7B6C, 0x1D098):
                    calls_helperish = True
                    tags.append(f"BL_+0x{toff:X}")
        # BLX rn
        if (w & 0x0FFFFFF0) == 0x012FFF30:
            has_blx = True
        # STR/STM family (data-processing store-ish: 0x05x / 0x04x store)
        if (w & 0x0C100000) == 0x04000000:  # STR*
            writes = True
        if (w & 0x0E100000) == 0x08000000:  # STM*
            writes = True
        # LDMFD ...pc → case return
        if (w & 0x0FFF8000) == 0x08BD8000:
            break
        # B back to interpreter loop
        tgt = branch_target(pc, w)
        if tgt is not None and ((tgt - base) & 0xFFFFFFFF) in (0x1C408, 0x1C3A0, 0x1C3AC):
            break
        off += 4
        pc += 4
    if has_blx:
        tags.append("BLX_fp")
    if has_bl:
        tags.append("BL")
    if writes:
        tags.append("writes_state")
    if calls_helperish:
        tags.append("calls_list_or_ui_path")
    if case_off in (0x1CD40,):
        tags.append("OP14_CURSOR")
        uiish = False
    if case_off in (0x1D1CC,):
        tags.append("OP1B_LIST")
    if uiish:
        tags.append("possible_UI")
    return {
        "has_blx": has_blx,
        "has_bl": has_bl,
        "writes": writes,
        "tags": ";".join(tags) if tags else "case",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--base", default="0x80000")
    ap.add_argument("--jt-csv", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    base = int(args.base, 0)
    blob = Path(args.bin).read_bytes()
    rows = []
    with open(args.jt_csv, newline="", encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            rows.append(line)
    # re-parse as csv
    rdr = csv.DictReader(open(args.jt_csv, encoding="utf-8"))
    out_rows = []
    for r in rdr:
        try:
            op = int(r["opcode"], 0)
            case_off = int(r["case_off"], 0)
        except Exception:
            continue
        info = scan_case(blob, base, case_off) if case_off else {"tags": "fallthrough"}
        old = r.get("tags") or ""
        merged = old
        if info["tags"] and info["tags"] not in old:
            merged = (old + ";" + info["tags"]).strip(";")
        note = r.get("notes") or ""
        extra = []
        if info.get("has_blx"):
            extra.append("indirect_call")
        if info.get("writes"):
            extra.append("may_write_record_or_object")
        if op == 0x14:
            extra.append("OBSERVED_op14")
        if op == 0x1B:
            extra.append("OBSERVED_op1b")
        if extra:
            note = (note + ";" + ";".join(extra)).strip(";")
        out_rows.append(
            {
                "opcode": f"0x{op:02X}",
                "jt_slot_off": r.get("jt_slot_off"),
                "case_off": r.get("case_off"),
                "abs_case": r.get("abs_case"),
                "tags": merged,
                "notes": note,
            }
        )
    with open(args.out, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(
            f, fieldnames=["opcode", "jt_slot_off", "case_off", "abs_case", "tags", "notes"]
        )
        w.writeheader()
        w.writerows(out_rows)
        f.write("# NOTE: 0x9C41C BEQ +0x1C56C = NORMAL_OPCODE_DISPATCH (not a lock)\n")
        f.write("# default when opcode>0x26 falls through +0x1C584 -> +0x1C408\n")
    print(f"enriched {len(out_rows)} opcodes -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
