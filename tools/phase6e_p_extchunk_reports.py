#!/usr/bin/env python3
"""Phase 6E offline reports from live extchunk audit stdout (observe-only)."""
from __future__ import annotations

import re
import sys
from pathlib import Path


def thumb_disasm_half(h: int) -> str:
    """Minimal Thumb decoder for report annotation (not a full disassembler)."""
    if (h & 0xFF00) == 0xB500:
        return "PUSH {...,lr}"
    if (h & 0xF800) == 0x6800:
        rt = h & 7
        rn = (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        return f"LDR r{rt},[r{rn},#0x{imm:X}]"
    if (h & 0xF800) == 0x6000:
        rt = h & 7
        rn = (h >> 3) & 7
        imm = ((h >> 6) & 0x1F) * 4
        return f"STR r{rt},[r{rn},#0x{imm:X}]"
    if (h & 0xFF00) == 0x2000:
        return f"MOVS r0,#0x{h & 0xFF:X}"
    if (h & 0xFE00) == 0x1C00:
        rd = h & 7
        rn = (h >> 3) & 7
        imm = (h >> 6) & 7
        return f"ADDS r{rd},r{rn},#{imm}"
    if (h & 0xF800) == 0x4800:
        rd = (h >> 8) & 7
        imm = (h & 0xFF) * 4
        return f"LDR r{rd},[PC,#0x{imm:X}]"
    if (h & 0xFF00) == 0x4700:
        return f"BX/BLX-ish 0x{h:04X}"
    return f".hword 0x{h:04X}"


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: phase6e_p_extchunk_reports.py <stdout> <reports_dir> [txt_report]", file=sys.stderr)
        return 2
    stdout_path = Path(sys.argv[1])
    reports_dir = Path(sys.argv[2])
    reports_dir.mkdir(parents=True, exist_ok=True)
    text = stdout_path.read_text(encoding="utf-8", errors="replace") if stdout_path.exists() else ""

    def m1(pat: str, default: str = "") -> str:
        m = re.search(pat, text)
        return m.group(1) if m else default

    klass = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*extchunk_class=(\S+)", "UNKNOWN")
    writes = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*writes_seen=(\d+)", "0")
    reads = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*reads_seen=(\d+)", "0")
    nxt = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*next_allowed_fix=(\S+)", "NONE")
    p_base = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*P=(0x[0-9A-Fa-f]+)", "0x0")
    func = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*function_start=(0x[0-9A-Fa-f]+)", "0x0")
    mempc = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*memory_access_pc=(0x[0-9A-Fa-f]+)", "0x0")
    null_at = m1(r"\[JJFB_EXTCHUNK_SUMMARY\][^\n]*null_at_use=(\S+)", "unknown")

    phases = re.findall(
        r"\[JJFB_EXTCHUNK_PHASE\] phase=(\S+) P=(0x[0-9A-Fa-f]+) P\+0xC=(0x[0-9A-Fa-f]+)",
        text,
    )
    reads_log = re.findall(
        r"\[JJFB_EXTCHUNK_READ\] value=(0x[0-9A-Fa-f]+) pc=(0x[0-9A-Fa-f]+)",
        text,
    )
    writes_log = re.findall(
        r"\[JJFB_EXTCHUNK_WRITE\] old=(0x[0-9A-Fa-f]+) new=(0x[0-9A-Fa-f]+) pc=(0x[0-9A-Fa-f]+)",
        text,
    )
    p_reads = re.findall(
        r"\[JJFB_P_READ\] off=(0x[0-9A-Fa-f]+) val=(0x[0-9A-Fa-f]+) pc=(0x[0-9A-Fa-f]+) lr=(0x[0-9A-Fa-f]+)",
        text,
    )
    p_writes = re.findall(
        r"\[JJFB_P_WRITE\] off=(0x[0-9A-Fa-f]+) old=(0x[0-9A-Fa-f]+) new=(0x[0-9A-Fa-f]+) pc=(0x[0-9A-Fa-f]+) lr=(0x[0-9A-Fa-f]+)",
        text,
    )
    owner = m1(r"\[JJFB_P_OWNER\] p=(0x[0-9A-Fa-f]+) source_reg=(\S+) pc=(0x[0-9A-Fa-f]+)")
    owner_line = ""
    om = re.search(
        r"\[JJFB_P_OWNER\] p=(0x[0-9A-Fa-f]+) source_reg=(\S+) pc=(0x[0-9A-Fa-f]+)", text
    )
    if om:
        owner_line = f"p={om.group(1)} reg={om.group(2)} pc={om.group(3)}"

    bytes_m = re.search(
        r"\[JJFB_P_FUNC_BYTES\] start=(0x[0-9A-Fa-f]+) end=(0x[0-9A-Fa-f]+) bytes=([0-9A-Fa-f:]+)",
        text,
    )

    # --- audit md ---
    audit_lines = [
        "# Phase 6E — P.mrc_extChunk Provider Audit",
        "",
        "## Summary",
        "",
        f"- extchunk_class: `{klass}`",
        f"- writes_seen: `{writes}`",
        f"- reads_seen: `{reads}`",
        f"- null_at_use: `{null_at}`",
        f"- P: `{p_base}`",
        f"- function_start: `{func}`",
        f"- memory_access_pc: `{mempc}`",
        f"- next_allowed_fix: `{nxt}`",
        f"- P owner: `{owner_line or 'not observed'}`",
        "",
        "## DOCUMENTED layout (mr_helper.h)",
        "",
        "```",
        "mr_c_function_st:",
        "  +0x00 start_of_ER_RW",
        "  +0x04 ER_RW_Length",
        "  +0x08 ext_type",
        "  +0x0C mrc_extChunk*",
        "  +0x10 stack",
        "mrc_extChunk_st:",
        "  +0x28 sendAppEvent",
        "  +0x2c extMrTable",
        "```",
        "",
        "Evidence: **DOCUMENTED**. Live NULL / fault: **TARGET_OBSERVED** (6D-B/6E).",
        "",
        "## Phase timeline (P+0xC)",
        "",
        "| phase | P | P+0xC |",
        "|---|---|---|",
    ]
    for ph, p, c in phases:
        audit_lines.append(f"| `{ph}` | `{p}` | `{c}` |")
    if not phases:
        audit_lines.append("| _(none)_ | | |")

    audit_lines += [
        "",
        "## P+0xC reads/writes",
        "",
        f"- EXTCHUNK_READ count (logged): {len(reads_log)}",
        f"- EXTCHUNK_WRITE count (logged): {len(writes_log)}",
        "",
    ]
    for v, pc in reads_log[:32]:
        audit_lines.append(f"- READ value={v} pc={pc}")
    for old, new, pc in writes_log[:32]:
        audit_lines.append(f"- WRITE {old}->{new} pc={pc}")
    if not reads_log and not writes_log:
        audit_lines.append("- No dedicated EXTCHUNK_READ/WRITE tags (may still have JJFB_P_READ).")

    audit_lines += [
        "",
        "## P.mrc_extChunk 与 GWY launcher context 的关系假设",
        "",
        "Evidence level: **HYPOTHESIS** until a discriminating startGame/runapp experiment.",
        "",
        "- Direct `gwy/jjfb.mrp` launch with `_gwyblink` may skip shell steps that",
        "  plugin/`mrc_extLoad` / startGame/runapp use to publish `mrc_extChunk`.",
        "- Legacy CROSS_TARGET comment: Mythroad 800 load never sets mrc_extChunk;",
        "  only plugin load paths do — not proven for this clean launcher.",
        "- Fault `LDR [r0,#0x28]` with r0 from `P+0xC` matches DOCUMENTED `sendAppEvent`",
        "  offset; NULL chunk alone explains the fault without R9 promotion.",
        "- cfg36 / resource root / napptype: not mutated in Phase 6E; observe-only.",
        "",
        "## Candidate next fix (NOT implemented)",
        "",
        f"- Selected next_allowed_fix: `{nxt}`",
        "- Candidate A: restore missing GWY startGame/runapp context that initializes P.mrc_extChunk",
        "- Candidate B: call missing platform extChunk publication routine before continuation",
        "- Candidate C: fix file/path/launcher root so registry publication occurs naturally",
        "",
        "Forbidden: fake P+0xC, force ER_RW, skip fault, R9 promotion.",
        "",
    ]
    (reports_dir / "phase6e_p_extchunk_audit.md").write_text(
        "\n".join(audit_lines) + "\n", encoding="utf-8"
    )

    # --- disasm md ---
    dis_lines = [
        "# Phase 6E fault-function disasm (live-discovered)",
        "",
        f"function_start=`{func}` memory_access_pc=`{mempc}`",
        "",
        "Addresses come from live `[JJFB_EXTCHUNK_SUMMARY]` / `[JJFB_P_FUNC_BYTES]` only.",
        "Core C does not embed fixed JJFB VAs.",
        "",
    ]
    if bytes_m:
        start = int(bytes_m.group(1), 16)
        end = int(bytes_m.group(2), 16)
        parts = bytes_m.group(3).split(":")
        dis_lines.append(f"Window: `0x{start:X}` .. `0x{end:X}` ({len(parts)} halfwords)")
        dis_lines.append("")
        dis_lines.append("| pc | halfword | decode |")
        dis_lines.append("|---|---|---|")
        pc = start
        for part in parts:
            try:
                h = int(part, 16)
            except ValueError:
                continue
            note = ""
            if pc == int(mempc, 16) if mempc.startswith("0x") else -1:
                note = "  **fault site**"
            dis_lines.append(f"| `0x{pc:X}` | `0x{h:04X}` | {thumb_disasm_half(h)}{note} |")
            pc += 2
        # annotate expected pattern
        dis_lines += [
            "",
            "## Expected pattern (TARGET_OBSERVED + DOCUMENTED)",
            "",
            "- `LDR r0,[r3,#0xC]` → P.mrc_extChunk",
            "- `LDR r7,[r0,#0x28]` → mrc_extChunk.sendAppEvent (DOCUMENTED)",
            "",
        ]
    else:
        dis_lines.append("_No `[JJFB_P_FUNC_BYTES]` in stdout; fault window not captured._")
        dis_lines.append("")
    (reports_dir / "phase6e_304558_disasm.md").write_text(
        "\n".join(dis_lines) + "\n", encoding="utf-8"
    )

    # --- xref md ---
    xref_lines = [
        "# Phase 6E P+0xC field xref (live + heuristic)",
        "",
        "Static guest image scan is limited without a dumped code image in this phase.",
        "This report lists **executed** P-field accesses from live hooks.",
        "",
        "## Live P reads",
        "",
        "| off | val | pc | lr | executed |",
        "|---|---|---|---|---|",
    ]
    for off, val, pc, lr in p_reads[:64]:
        xref_lines.append(f"| `{off}` | `{val}` | `{pc}` | `{lr}` | yes |")
    if not p_reads:
        xref_lines.append("| _(none)_ | | | | |")

    xref_lines += [
        "",
        "## Live P writes",
        "",
        "| off | old | new | pc | lr | executed |",
        "|---|---|---|---|---|---|",
    ]
    for off, old, new, pc, lr in p_writes[:64]:
        xref_lines.append(f"| `{off}` | `{old}` | `{new}` | `{pc}` | `{lr}` | yes |")
    if not p_writes:
        xref_lines.append("| _(none)_ | | | | | |")

    xref_lines += [
        "",
        "## P+0xC specific",
        "",
        f"- writes_seen (summary): {writes}",
        f"- reads_seen (summary): {reads}",
        "",
        "If writes_seen=0 and a read of P+0xC returned 0 before `LDR [r0,#0x28]`,",
        "the provider path never published mrc_extChunk on this launch.",
        "",
    ]
    (reports_dir / "phase6e_p_field_xref.md").write_text(
        "\n".join(xref_lines) + "\n", encoding="utf-8"
    )

    print(f"wrote {reports_dir / 'phase6e_p_extchunk_audit.md'}")
    print(f"wrote {reports_dir / 'phase6e_304558_disasm.md'}")
    print(f"wrote {reports_dir / 'phase6e_p_field_xref.md'}")
    print(f"class={klass} writes={writes} reads={reads} next={nxt}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
