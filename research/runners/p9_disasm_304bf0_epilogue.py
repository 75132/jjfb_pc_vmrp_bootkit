#!/usr/bin/env python3
"""P9 research: locate the real epilogue of the guest function at 0x304BF0.

The function is the MRP resource lookup body inside robotol.ext (loaded at
guest base 0x2D8DF4). P8 concluded that resuming to the inner callsite
0x304BF4 is unsafe because 0x304BF0 is a function *entry* with a PUSH frame.
The documented "Next" step is to find the matching POP epilogue so a future
resume-to-epilogue mode can close the stack contract cleanly.

This script is read-only research. It disassembles the function, derives the
prologue frame (PUSH + SUB SP) and finds candidate epilogues (ADD SP + POP{pc})
that balance the frame. It does NOT modify any core source.
"""
from __future__ import annotations
from pathlib import Path

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

ROBOTOL = Path("out/research_p9_extract/robotol.ext")
BASE = 0x2D8DF4  # guest load base of robotol.ext
ENTRY = 0x304BF0  # target function entry
WINDOW = 0x5000  # bytes to disassemble forward (function is large)


def reg_name_to_idx(name: str) -> int:
    name = name.lower()
    if name == "pc":
        return 15
    if name == "lr":
        return 14
    if name == "sp":
        return 13
    if name.startswith("r") and name[1:].isdigit():
        return int(name[1:])
    return -1


def parse_reglist(op_str: str) -> set[int]:
    """Parse a capstone register list like '{r0-r7, lr}' or '{r4, r5, pc}'."""
    s = op_str.strip()
    if not (s.startswith("{") and s.endswith("}")):
        return set()
    inner = s[1:-1]
    regs: set[int] = set()
    for part in inner.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            lo, hi = part.split("-")
            a, b = reg_name_to_idx(lo), reg_name_to_idx(hi)
            if a >= 0 and b >= 0:
                regs.update(range(a, b + 1))
        else:
            idx = reg_name_to_idx(part)
            if idx >= 0:
                regs.add(idx)
    return regs


def main() -> int:
    code = ROBOTOL.read_bytes()
    off = ENTRY - BASE
    if not (0 <= off < len(code) - WINDOW):
        raise SystemExit(f"entry 0x{ENTRY:X} out of decoded robotol range")
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    md.detail = True
    insns = list(md.disasm(code[off:off + WINDOW], ENTRY))

    # Truncate at the next function boundary (a PUSH that saves lr/pc that is
    # not our entry prologue) so we only analyse the 0x304BF0 body.
    for i, ins in enumerate(insns[1:], start=1):
        if ins.mnemonic == "push" and 14 in parse_reglist(ins.op_str):
            insns = insns[:i]
            break
    if insns:
        end = insns[-1].address
        print(f"# Disassembly of 0x{ENTRY:X} (robotol@{BASE:#X}, file off {off:#X})")
        print(f"# Function spans 0x{ENTRY:X} .. 0x{end:X} "
              f"({end - ENTRY} bytes, {len(insns)} insns)\n")

    # --- prologue frame analysis ---
    prologue_push: set[int] = set()
    prologue_sub = 0
    if insns:
        first = insns[0]
        if first.mnemonic == "push":
            prologue_push = parse_reglist(first.op_str)
        # next SUB SP if present
        for ins in insns[:6]:
            if ins.mnemonic == "sub" and "sp" in ins.op_str.lower():
                # format: sub sp, sp, #imm
                try:
                    imm = int(ins.op_str.split("#")[-1].split(",")[-1].strip(), 0)
                except Exception:
                    imm = 0
                prologue_sub = imm
                break

    push_bytes = sum(4 for r in prologue_push)
    print(f"## Prologue")
    print(f"- PUSH regs: {sorted(prologue_push)} ({len(prologue_push)} regs, {push_bytes} bytes)")
    print(f"- SUB SP frame: {prologue_sub} bytes (0x{prologue_sub:X})")
    print(f"- Expected epilogue SP restore: +{prologue_sub} (ADD SP) then +{push_bytes} (POP)\n")

    # --- find candidate epilogues: POP {..., pc} ---
    print("## Candidate epilogues (ADD SP / POP {pc})")
    found = []
    for i, ins in enumerate(insns):
        is_pop_pc = ins.mnemonic == "pop" and 15 in parse_reglist(ins.op_str)
        is_bx_lr = ins.mnemonic == "bx" and "lr" in ins.op_str.lower()
        if is_pop_pc or is_bx_lr:
            prev = insns[i - 1] if i > 0 else None
            prev2 = insns[i - 2] if i > 1 else None
            add_imm = 0
            if prev and prev.mnemonic == "add" and "sp" in prev.op_str.lower():
                try:
                    add_imm = int(prev.op_str.split("#")[-1].split(",")[-1].strip(), 0)
                except Exception:
                    add_imm = 0
            pop_regs = parse_reglist(ins.op_str) if is_pop_pc else set()
            balanced = (len(pop_regs) * 4) == push_bytes and pop_regs.issuperset(
                r for r in prologue_push if r != 14
            ) and (15 in pop_regs)
            tag = "BALANCED" if balanced else "partial"
            print(f"- 0x{ins.address:X}: {ins.mnemonic} {ins.op_str}  [{tag}]")
            if prev:
                print(f"    prev: 0x{prev.address:X}: {prev.mnemonic} {prev.op_str}"
                      f"{(' (ADD SP +'+str(add_imm)+')') if add_imm else ''}")
            if prev2:
                print(f"    prev2:0x{prev2.address:X}: {prev2.mnemonic} {prev2.op_str}")
            found.append((ins.address, ins.mnemonic, ins.op_str, balanced, add_imm,
                          sorted(pop_regs)))

    # --- recommend resume-to-epilogue target ---
    print("\n## Recommendation")
    balanced = [f for f in found if f[3]]
    if balanced:
        # prefer the one preceded by an ADD SP that matches the SUB frame
        best = None
        for f in balanced:
            if f[4] == prologue_sub:
                best = f
                break
        if best is None:
            best = balanced[0]
        tgt = best[0]
        print(f"- Balanced epilogue at 0x{tgt:X}: POP {best[2]}")
        print(f"- Resume-to-epilogue target PC = 0x{tgt|X} | 1 = 0x{tgt+1:X}")
        print(f"- Required host setup before resume:")
        print(f"    SP = entry_SP         (post-PUSH, i.e. entry_SP0 - {push_bytes})")
        if prologue_sub:
            print(f"    (ADD SP #{prologue_sub} inside epilogue cancels the SUB SP frame)")
        print(f"    R0 = status/return (A/B: 0 vs handle_guest)")
        print(f"    PC = 0x{tgt+1:X} (Thumb)")
        print(f"    POP restores r0-r7,pc -> returns to LR (0x{g_entry_lr_note()})")
    else:
        print("- No balanced POP{pc} found in window; extend WINDOW or check module base.")
    return 0


def g_entry_lr_note() -> str:
    # informational only; real LR is captured at runtime by on_lookup_entry
    return "runtime LR (builder 0x2D93D1 per P8 direct_lr)"


if __name__ == "__main__":
    raise SystemExit(main())
