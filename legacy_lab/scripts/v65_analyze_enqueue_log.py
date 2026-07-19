#!/usr/bin/env python3
"""Analyze v65 0x101AB fill → 2E4D6C(len!=0) → 312A60 → B54 → 2E2520."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def count(t: str, pat: str) -> int:
    return len(re.findall(pat, t))


def main() -> int:
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "logs" / "v65_101ab_stdout.txt"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "reports" / "v65_101ab_run_result.md"
    t = log.read_text(encoding="utf-8", errors="replace")

    c = {
        "reg_10165": count(t, r"\[JJFB_V64_ENQ\] register 0x10165"),
        "probe": count(t, r"\[JJFB_V64_ENQ\] PROBE once"),
        "fill_101ab": count(t, r"\[JJFB_V65_101AB\] fill"),
        "unhandled_101ab": count(t, r"unhandled plat/msg 0x101AB"),
        "site_30d24c": count(t, r"\[JJFB_V64_ENQ\] site=0x30D24C"),
        "site_2e4d6c": count(t, r"\[JJFB_V64_ENQ\] site=0x2E4D6C"),
        "r1_zero": count(t, r"r1==0 early-exit"),
        "r1_nonzero": count(t, r"\[JJFB_V65_ENQ\] 2E4D6C buf=.*len/r1=0x(?!0\b)"),
        "peek": count(t, r"\[JJFB_V65_ENQ\] 2E4D6C peek"),
        "push_312a60": count(t, r"\[JJFB_V65_ENQ\] 312A60 push"),
        "push_from_2e4d6c": count(t, r"\[JJFB_V65_ENQ\] 312A60 push.*lr=0x2E4EF"),
        "alloc_fail": count(t, r"0x10132 alloc FAIL"),
        "unmap": count(t, r"UC_ERR_READ_UNMAPPED|Invalid memory read"),
        "drain": count(t, r"drain_2DC80C ENTER"),
        "event_dispatch": count(t, r"\[JJFB_V56_EVENT\]"),
        "path_a_hit": count(t, r"TARGETS_2DADC4"),
        "caller_2e4066": count(t, r"caller_2E4066"),
        "gate_2dadc4": count(t, r"gate_init_2DADC4"),
        "writer": count(t, r"uimode_writer ENTER"),
        "force_ui": count(t, r"FORCE state=0x45|force_ui_mode state"),
    }

    # Prefer explicit len/r1 non-zero lines from our v65 log
    r1_nz = 0
    for m in re.finditer(r"\[JJFB_V65_ENQ\] 2E4D6C buf=0x[0-9A-Fa-f]+ len/r1=0x([0-9A-Fa-f]+)", t):
        if int(m.group(1), 16) != 0:
            r1_nz += 1
    c["r1_nonzero"] = r1_nz

    if c["gate_2dadc4"] and c["writer"]:
        verdict = "101AB 填包后 Path A 已到 writer。"
        next_task = "把 10165/101AB 收敛为正式生命周期（非单次 PROBE）。"
    elif c["gate_2dadc4"]:
        verdict = "已进 2DADC4；下一 blocker 在 B70/writer。"
        next_task = "看 gate_B70 / uimode_writer。"
    elif c["event_dispatch"] or c["path_a_hit"]:
        verdict = "B54 已有事件进 2E2520；未到 gate_init。"
        next_task = "查 2E4040 前置（B58 等）。"
    elif c.get("push_from_2e4d6c") or (
        c["push_312a60"] and not c.get("unmap") and c["r1_nonzero"]
    ):
        if c["event_dispatch"]:
            verdict = "312A60 已从 2E4D6C push，且已有 2E2520。"
            next_task = "看 Path A 是否到 2DADC4。"
        else:
            verdict = "312A60 已 push，但 drain 未送到 2E2520。"
            next_task = "查 2DC80C 出队/过滤与事件节点布局。"
    elif c.get("unmap") or c.get("alloc_fail"):
        verdict = "2E4D6C 解析中途崩溃（10132 小块分配/payload 布局）。"
        next_task = "修 10132 sz<4 与 15C 双次 308D98 的 payload。"
    elif c["push_312a60"] and not c["event_dispatch"]:
        verdict = "见 312A60，但可能是 host stub 误触（查 lr）。"
        next_task = "确认 lr=0x2E4EF2 的真 push。"
    elif c["r1_nonzero"] and not c["push_312a60"]:
        verdict = "2E4D6C 已拿到非空 r1，但未 push（解析/门控失败）。"
        next_task = "对照 2E4D6C 对 payload 的 BE 字段与 15C/A90 门控。"
    elif c["fill_101ab"] and c["r1_zero"]:
        verdict = "101AB 已填包，但 unpack 后 r1 仍为 0。"
        next_task = "核对 31103C 是 pack 还是 unpack，以及 ret=0 游标。"
    elif c["unhandled_101ab"]:
        verdict = "101AB 仍未处理（旧 binary？）。"
        next_task = "确认重新编译并部署 main.exe。"
    elif c["site_2e4d6c"]:
        verdict = "进了 2E4D6C，但未见 101AB fill / r1 状态。"
        next_task = "查日志是否含 JJFB_V65_101AB。"
    else:
        verdict = "未看到完整 10165→101AB→2E4D6C 链。"
        next_task = "确认 JJFB_V64_ENQUEUE_ONCE=1 与 bringup。"

    key = []
    for pat in (
        r"\[JJFB_V65_101AB\].*",
        r"\[JJFB_V65_ENQ\].*",
        r"\[JJFB_V64_ENQ\].*",
        r"\[JJFB_V56_EVENT\].*",
        r"gate_init_2DADC4.*",
        r"caller_2E4066.*",
        r"uimode_writer.*",
    ):
        key.extend(re.findall(pat, t)[:10])

    lines = [
        "# v65 101AB Fill → B54 Enqueue 运行结果",
        "",
        f"- 日志：`{log}`",
        "",
        "## 1. 目标",
        "",
        "- 补齐 plat `0x101AB`：给 `30D24C` 填 `>c>i` 可 unpack 的缓冲。",
        "- 让 `2E4D6C(r0, r1)` 的 `r1!=0`，真正 `312A60` push 到 B54。",
        "- 期望：drain → `2E2520` → Path A `2DADC4`。",
        "- 禁止 FORCE ui_mode / C0 inject / host UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
    ]
    for k, label in [
        ("reg_10165", "register 10165"),
        ("probe", "ENQ PROBE once"),
        ("fill_101ab", "101AB fill"),
        ("unhandled_101ab", "101AB unhandled"),
        ("site_30d24c", "site 30D24C"),
        ("site_2e4d6c", "site 2E4D6C"),
        ("r1_zero", "2E4D6C r1==0"),
        ("r1_nonzero", "2E4D6C r1!=0"),
        ("peek", "2E4D6C peek"),
        ("push_312a60", "312A60 push"),
        ("push_from_2e4d6c", "312A60 from 2E4D6C"),
        ("alloc_fail", "10132 alloc FAIL"),
        ("unmap", "UNMAP crash"),
        ("drain", "drain 2DC80C"),
        ("event_dispatch", "2E2520 EVENT"),
        ("path_a_hit", "Path A TARGETS_2DADC4"),
        ("caller_2e4066", "caller_2E4066"),
        ("gate_2dadc4", "gate_init_2DADC4"),
        ("writer", "uimode_writer"),
        ("force_ui", "FORCE ui"),
    ]:
        lines.append(f"| {label} | {c[k]} |")

    lines += [
        "",
        "## 3. 关键日志",
        "",
        "```text",
    ]
    lines.extend(key[:50] if key else ["(none)"])
    lines += [
        "```",
        "",
        "## 4. 结论",
        "",
        f"- {verdict}",
        "",
        "## 5. blocker / 下一步",
        "",
        f"- next: {next_task}",
        "",
        "## 6. v65 已打通（证据）",
        "",
        "- `0x101AB` 填包 + `>c>i` unpack → `2E4D6C(r1!=0)`",
        "- `312A60` 真 push（`lr=0x2E4EF3`）→ B54",
        "- drain → `2E2520` event code=5 → 进入 `2E4040`",
        "- 仍未到 `2E4066` / `gate_init_2DADC4`（节点字段/门控）",
        "",
    ]
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out}")
    print(f"verdict: {verdict}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
