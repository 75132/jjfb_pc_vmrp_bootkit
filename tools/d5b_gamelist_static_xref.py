#!/usr/bin/env python3
"""D5b static xref: cmd dispatcher + cfg-gate + handler reachability (gamelist.ext)."""
from __future__ import annotations

import struct
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXT = ROOT / "out" / "tmp_gamelist_disasm" / "gamelist.ext"
BASE = 0x2D4354
OUT = ROOT / "reports"

# Known from Full Boot / D4/D5 (TARGET_OBSERVED)
HANDLERS = [
    (0x10600, 0x2E74AD),
    (0x10601, 0x2E03E1),
    (0x10602, 0x2E0421),
    (0x10603, 0x2E0359),
    (0x10604, 0x2E0445),
    (0x10605, 0x2E0361),
    (0x10606, 0x2DFC61),
    (0x1060A, 0x2DFC59),
    (0x10608, 0x2DF699),
    (0x10609, 0x2E0405),
]
TIMER_HANDLER = 0x2E7754
CMD_DISP = 0x2E3A18
CFG_SITES = {
    0x2D5E4C: "cfg_open_a",
    0x2D5E5C: "cfg_open_b",
    0x2D5E6C: "cfg_open_c",
    0x2D5F14: "cfg_open_d",
    0x2D7C80: "cfg_gate",
    0x2D7CE4: "cfg_gate_mid",
    0x2E0F5C: "cfg_parent_a",
    0x2E1520: "cfg_parent_b",
    0x2E11DE: "cfg_callsite_a",
    0x2D829A: "cfg_callsite_b",
    0x2DAB90: "cfg_callsite_c",
    0x2D9654: "cfg_callsite_d",
    0x2E39C4: "cfg_wrap",
    0x2D9CBC: "gate_cmp_r0_0xC",
    CMD_DISP: "cmd_disp",
}


def bl_targets(data: bytes, base: int) -> list[tuple[int, int]]:
    outs: list[tuple[int, int]] = []
    i = 0
    while i + 3 < len(data):
        h = data[i] | (data[i + 1] << 8)
        h2 = data[i + 2] | (data[i + 3] << 8)
        if (h & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xD000:
            s = (h >> 10) & 1
            imm10 = h & 0x3FF
            j1 = (h2 >> 13) & 1
            j2 = (h2 >> 11) & 1
            imm11 = h2 & 0x7FF
            I1 = 1 - (j1 ^ s)
            I2 = 1 - (j2 ^ s)
            imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm -= 1 << 25
            outs.append((base + i, (base + i + 4 + imm) & ~1))
            i += 4
            continue
        i += 2
    return outs


def branch_to(data: bytes, base: int, want: int) -> list[tuple[int, str]]:
    hits: list[tuple[int, str]] = []
    i = 0
    while i + 1 < len(data):
        addr = base + i
        h = data[i] | (data[i + 1] << 8)
        if (h & 0xF000) == 0xD000 and (h & 0x0F00) != 0x0F00:
            imm = (h & 0xFF) << 1
            if imm & 0x100:
                imm -= 0x200
            t = addr + 4 + imm
            if (t & ~1) == (want & ~1):
                hits.append((addr, "bcond"))
        if (h & 0xF800) == 0xE000:
            imm = (h & 0x7FF) << 1
            if imm & 0x800:
                imm -= 0x1000
            t = addr + 4 + imm
            if (t & ~1) == (want & ~1):
                hits.append((addr, "b"))
        if i + 3 < len(data):
            h2 = data[i + 2] | (data[i + 3] << 8)
            if (h & 0xF800) == 0xF000 and (h2 & 0xD000) == 0xD000:
                s = (h >> 10) & 1
                imm10 = h & 0x3FF
                j1 = (h2 >> 13) & 1
                j2 = (h2 >> 11) & 1
                imm11 = h2 & 0x7FF
                I1 = 1 - (j1 ^ s)
                I2 = 1 - (j2 ^ s)
                imm = (s << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
                if s:
                    imm -= 1 << 25
                t = (addr + 4 + imm) & ~1
                if t == (want & ~1):
                    hits.append((addr, "bl"))
                i += 4
                continue
        i += 2
    return hits


def lit_refs(data: bytes, base: int, want: int) -> list[int]:
    pat = struct.pack("<I", want & ~1) + struct.pack("<I", want | 1)
    # search both even/odd thumb
    hits = []
    for w in (want & ~1, want | 1):
        p = struct.pack("<I", w)
        start = 0
        while True:
            j = data.find(p, start)
            if j < 0:
                break
            hits.append(base + j)
            start = j + 1
    return hits


def nearest_string(data: bytes, base: int, va: int, radius: int = 0x80) -> str:
    off = va - base
    lo = max(0, off - radius)
    hi = min(len(data), off + radius)
    best = ""
    i = lo
    while i < hi:
        if 32 <= data[i] < 127:
            j = i
            while j < hi and 32 <= data[j] < 127:
                j += 1
            if j - i >= 4:
                s = data[i:j].decode("ascii", errors="ignore")
                if len(s) > len(best):
                    best = s
            i = j
        else:
            i += 1
    return best[:60]


def reaches(bls: list[tuple[int, int]], start: int, goals: set[int], depth: int = 6) -> bool:
    """Conservative: follow bl targets only, from start as call site window."""
    # Build adj: from any bl site in [fn, fn+0x400) to target — use targets as nodes
    by_tgt = defaultdict(list)
    for a, t in bls:
        by_tgt[t].append(a)
    # BFS from start following edges start->targets of bls whose site is near visited
    visited = set()
    q = [start & ~1]
    while q:
        n = q.pop()
        if n in visited:
            continue
        visited.add(n)
        if n in goals:
            return True
        if len(visited) > 800:
            break
        for a, t in bls:
            if n <= a < n + 0x280 and t not in visited:
                q.append(t)
    return bool(visited & goals)


def main() -> None:
    if not EXT.exists():
        raise SystemExit(f"missing {EXT}")
    data = EXT.read_bytes()
    bls = bl_targets(data, BASE)
    goals = {k & ~1 for k in CFG_SITES}
    goals.add(CMD_DISP & ~1)

    # handler map
    lines = [
        "# D5b — gamelist 0x10102 handler map (static)",
        "",
        "evidence: TARGET_OBSERVED (Full Boot slot registrations)",
        "",
        "| event_code | handler_va | module_offset | reaches_cmd_or_cfg | nearest_string |",
        "|---|---|---|---|---|",
    ]
    for code, hva in HANDLERS + [(0, TIMER_HANDLER)]:
        off = (hva & ~1) - BASE
        ok = reaches(bls, hva & ~1, goals)
        ns = nearest_string(data, BASE, hva & ~1)
        label = f"0x{code:X}" if code else "timer@FIRE"
        lines.append(
            f"| {label} | `0x{hva:X}` | `0x{off:X}` | {'yes' if ok else '**no**'} | `{ns}` |"
        )
    lines += [
        "",
        "## Notes",
        "",
        "- `reaches_cmd_or_cfg` is a loose BL-window heuristic (not proof of dynamic path).",
        "- Runtime enter/ret logs override this for trigger evidence.",
        "",
    ]
    (OUT / "d5b_gamelist_handler_map.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    # cfg gate xref
    cfg_lines = [
        "# D5b — cfg gate / cfg-open xref",
        "",
        "evidence: TARGET_OBSERVED static",
        "",
        "| site | role | bl callers | b/bcond | lit pool | nearest_string |",
        "|---|---|---|---|---|---|",
    ]
    for va, role in sorted(CFG_SITES.items()):
        blc = branch_to(data, BASE, va)
        bl_only = [hex(a) for a, k in blc if k == "bl"]
        other = [f"{hex(a)}:{k}" for a, k in blc if k != "bl"]
        lits = [hex(x) for x in lit_refs(data, BASE, va)[:6]]
        ns = nearest_string(data, BASE, va)
        cfg_lines.append(
            f"| `0x{va:X}` | {role} | {', '.join(bl_only) or '—'} | "
            f"{', '.join(other) or '—'} | {', '.join(lits) or '—'} | `{ns}` |"
        )
    cfg_lines += [
        "",
        "## Gate note",
        "",
        "- `0x2D9CBC` contains `cmp r0, #0xC` (12 = napptype) in surrounding function (D4).",
        "- `cfg_wrap` (`0x2E39C4`) had sole `bl` to `cfg_gate` in prior audit; if callers empty, may be dead or table-driven.",
        "",
    ]
    (OUT / "d5b_cfg_gate_xref.md").write_text("\n".join(cfg_lines) + "\n", encoding="utf-8")

    # cmd disp
    cmd_bl = branch_to(data, BASE, CMD_DISP)
    cmd_lit = lit_refs(data, BASE, CMD_DISP)
    cmd_lines = [
        "# D5b — command dispatcher xref",
        "",
        f"- cmd_dispatcher_va: `0x{CMD_DISP:X}` (off `0x{CMD_DISP - BASE:X}`)",
        f"- evidence: TARGET_OBSERVED (D5 probe off=0xF6C4)",
        "",
        "## Direct branches",
        "",
    ]
    if not cmd_bl:
        cmd_lines.append("(none)")
    else:
        for a, k in cmd_bl:
            cmd_lines.append(f"- `{k}` from `0x{a:X}`")
    cmd_lines += ["", "## Literal pools", ""]
    if not cmd_lit:
        cmd_lines.append("(none)")
    else:
        for a in cmd_lit[:20]:
            cmd_lines.append(f"- `0x{a:X}`")
    # callers of sites that bl to cmd
    callers_of_callers = []
    for a, k in cmd_bl:
        if k != "bl":
            continue
        for a2, t2 in bls:
            if t2 == (a & ~1) or (a - 0x40 <= t2 <= a):
                callers_of_callers.append((a2, a))
    cmd_lines += [
        "",
        "## Char comparisons near dispatcher (manual TARGET_OBSERVED)",
        "",
        "```text",
        "B / H / I / P / S / c / s / p",
        "```",
        "",
        "Looks like a protocol/command stream parser, not keepalive timer.",
        "",
        "## Handler reachability (heuristic)",
        "",
    ]
    for code, hva in HANDLERS:
        ok = reaches(bls, hva & ~1, {CMD_DISP & ~1})
        cmd_lines.append(f"- event `0x{code:X}` handler `0x{hva:X}` → cmd_disp: {'yes' if ok else 'no'}")
    ok_t = reaches(bls, TIMER_HANDLER & ~1, {CMD_DISP & ~1})
    cmd_lines.append(f"- timer `0x{TIMER_HANDLER:X}` → cmd_disp: {'yes' if ok_t else 'no'}")
    cmd_lines.append("")
    (OUT / "d5b_command_source_audit.md").write_text("\n".join(cmd_lines) + "\n", encoding="utf-8")

    # mr_entry placeholder static
    needles = [b"napptype", b"nmrpname", b"gwyblink", b"mr_entry", b"_mr_entry", b"gwy/cfg.bin"]
    mr_lines = [
        "# D5b — mr_entry / launch param consumption (static + runtime pending)",
        "",
        "## Static strings in gamelist.ext",
        "",
    ]
    for n in needles:
        offs = []
        start = 0
        while True:
            j = data.find(n, start)
            if j < 0:
                break
            offs.append(BASE + j)
            start = j + 1
        mr_lines.append(f"- `{n.decode()}` count={len(offs)} va={', '.join(hex(x) for x in offs[:6])}")
    mr_lines += [
        "",
        "## DOCUMENTED (mythroad)",
        "",
        "- `mr_start_dsm` copies entry → `mr_entry[]`",
        "- Lua global `_mr_entry`",
        "- `_mr_c_function_table[144]` → `mr_entry` pointer (mythroad.c)",
        "",
        "## Runtime (filled after quiet boot)",
        "",
        "See `d5b_mr_entry_param_consumption.md` runtime section / boot markers:",
        "`[JJFB_PARAM_MAP]` / `[JJFB_PARAM_READ]` / `[JJFB_MR_ENTRY_*]`.",
        "",
    ]
    (OUT / "d5b_mr_entry_param_consumption.md").write_text("\n".join(mr_lines) + "\n", encoding="utf-8")

    print("wrote", OUT / "d5b_gamelist_handler_map.md")
    print("wrote", OUT / "d5b_cfg_gate_xref.md")
    print("wrote", OUT / "d5b_command_source_audit.md")
    print("wrote", OUT / "d5b_mr_entry_param_consumption.md")


if __name__ == "__main__":
    main()
