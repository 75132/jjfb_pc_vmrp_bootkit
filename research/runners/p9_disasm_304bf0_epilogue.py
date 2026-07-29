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

Stack-balance contract (required):
    add_sp_imm + pop_bytes == push_bytes + sub_sp_imm

Resume target (required):
    PC = ADD SP address | 1  (Thumb)  == 0x304C4B for this binary
"""
from __future__ import annotations
from pathlib import Path

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

ROBOTOL = Path("out/research_p9_extract/robotol.ext")
BASE = 0x2D8DF4  # guest load base of robotol.ext
ENTRY = 0x304BF0  # target function entry
WINDOW = 0x5000  # bytes to disassemble forward (function is large)
EXPECTED_RESUME_PC = 0x304C4B  # ADD SP @ 0x304C4A with Thumb bit


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
            lo, hi = part.split("-", 1)
            a, b = reg_name_to_idx(lo), reg_name_to_idx(hi)
            if a >= 0 and b >= 0:
                regs.update(range(a, b + 1))
        else:
            idx = reg_name_to_idx(part)
            if idx >= 0:
                regs.add(idx)
    return regs


def parse_sp_imm(op_str: str) -> int:
    """Extract immediate from 'sp, #imm' / 'sp, sp, #imm' / 'sp, #0xcc'."""
    try:
        if "#" not in op_str:
            return 0
        return int(op_str.split("#")[-1].split(",")[0].strip(), 0)
    except Exception:
        return 0


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
    for i, insn in enumerate(insns[1:], start=1):
        if insn.mnemonic == "push" and 14 in parse_reglist(insn.op_str):
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
        for insn in insns[:8]:
            if insn.mnemonic == "sub" and "sp" in insn.op_str.lower():
                prologue_sub = parse_sp_imm(insn.op_str)
                break

    push_bytes = len(prologue_push) * 4
    prologue_total = push_bytes + prologue_sub
    print("## Prologue")
    print(f"- PUSH regs: {sorted(prologue_push)} ({len(prologue_push)} regs, {push_bytes} bytes)")
    print(f"- SUB SP frame: {prologue_sub} bytes (0x{prologue_sub:X})")
    print(f"- prologue_total = PUSH + SUB = {prologue_total}")
    print(f"- Expected epilogue: ADD SP + POP must also total {prologue_total}\n")

    # --- find candidate epilogues: POP {..., pc} ---
    # Balance equation (ONLY):
    #     add_sp_imm + pop_bytes == push_bytes + sub_sp_imm
    print("## Candidate epilogues (ADD SP / POP {pc})")
    found = []
    for i, insn in enumerate(insns):
        is_pop_pc = insn.mnemonic == "pop" and 15 in parse_reglist(insn.op_str)
        is_bx_lr = insn.mnemonic == "bx" and "lr" in insn.op_str.lower()
        if not (is_pop_pc or is_bx_lr):
            continue
        prev = insns[i - 1] if i > 0 else None
        prev2 = insns[i - 2] if i > 1 else None
        add_imm = 0
        add_sp_addr = None
        if prev and prev.mnemonic == "add" and "sp" in prev.op_str.lower():
            add_imm = parse_sp_imm(prev.op_str)
            add_sp_addr = prev.address
        pop_regs = parse_reglist(insn.op_str) if is_pop_pc else set()
        pop_bytes = len(pop_regs) * 4
        epilogue_total = add_imm + pop_bytes
        balanced = bool(
            is_pop_pc
            and (15 in pop_regs)
            and add_sp_addr is not None
            and (epilogue_total == prologue_total)
        )
        tag = "BALANCED" if balanced else "partial"
        print(f"- 0x{insn.address:X}: {insn.mnemonic} {insn.op_str}  [{tag}]")
        if prev:
            extra = f" (ADD SP +{add_imm})" if add_imm else ""
            print(f"    prev: 0x{prev.address:X}: {prev.mnemonic} {prev.op_str}{extra}")
        if prev2:
            print(f"    prev2:0x{prev2.address:X}: {prev2.mnemonic} {prev2.op_str}")
        if is_pop_pc:
            print(f"    balance: add({add_imm})+pop({pop_bytes})={epilogue_total} "
                  f"vs push({push_bytes})+sub({prologue_sub})={prologue_total}")
        found.append({
            "pop_addr": insn.address,
            "mnemonic": insn.mnemonic,
            "op_str": insn.op_str,
            "balanced": balanced,
            "add_imm": add_imm,
            "pop_bytes": pop_bytes,
            "epilogue_total": epilogue_total,
            "add_sp_addr": add_sp_addr,
        })

    # --- recommend resume-to-epilogue target ---
    # Host must jump to the ADD SP instruction (not the POP), so SP advances
    # by +imm BEFORE the POP reads the saved registers from memory.
    print("\n## Recommendation")
    balanced = [f for f in found if f["balanced"]]
    if not balanced:
        print("- No balanced POP{pc} found in window; extend WINDOW or check module base.")
        return 1

    # Prefer the unique epilogue whose ADD cancels the SUB frame with compressed POP.
    best = None
    for f in balanced:
        if f["add_sp_addr"] is not None:
            best = f
            break
    if best is None:
        best = balanced[0]

    add_sp_addr = best["add_sp_addr"]
    pop_addr = best["pop_addr"]
    # Thumb bit set for emulator resume (0x304C4A | 1 == 0x304C4B).
    resume_pc = add_sp_addr | 1
    print(f"- Balanced epilogue: ADD SP at 0x{add_sp_addr:X} (+{best['add_imm']}), "
          f"POP at 0x{pop_addr:X} ({best['mnemonic']} {best['op_str']}, "
          f"{best['pop_bytes'] // 4} regs)")
    print(f"- Resume-to-epilogue target PC = 0x{resume_pc:X} (Thumb; lands on ADD SP)")
    print("- Required host setup before resume:")
    print(f"    SP = entry_SP - {prologue_total}  (post-PUSH, post-SUB)")
    print(f"    PUSH save area reconstructed at entry_SP - {push_bytes}")
    print("    R0 = status (A/B: 0 vs handle_guest); POP does NOT restore r0")
    print(f"    PC = 0x{resume_pc:X} (Thumb; emulator resumes at ADD SP)")

    # P10 required machine-readable lines
    print(f"prologue_total={prologue_total}")
    print(f"epilogue_total={best['epilogue_total']}")
    print(f"resume_pc=0x{resume_pc:X}")

    if prologue_total != 224 or best["epilogue_total"] != 224:
        print(f"[FAIL] expected totals 224/224, got {prologue_total}/{best['epilogue_total']}")
        return 1
    if resume_pc != EXPECTED_RESUME_PC:
        print(f"[FAIL] expected resume_pc=0x{EXPECTED_RESUME_PC:X}, got 0x{resume_pc:X}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
