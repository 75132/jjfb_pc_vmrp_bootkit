#!/usr/bin/env python3
"""Analyze v66 Path A: 2E4040 → 2F68E4(BE0) → 2E4066 → 2DADC4."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def count(t: str, pat: str) -> int:
    return len(re.findall(pat, t))


def main() -> int:
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "logs" / "v66_path_a_stdout.txt"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "reports" / "v66_path_a_run_result.md"
    t = log.read_text(encoding="utf-8", errors="replace")

    c = {
        "fill_101ab": count(t, r"\[JJFB_V66_101AB\] fill"),
        "push_2e4d6c": count(t, r"312A60 push.*lr=0x2E4EF"),
        "event5": count(t, r"code=5\(0x5\).*TARGETS_2DADC4"),
        "enter_4040": count(t, r"enter_2E4040"),
        "enter_68e4": count(t, r"enter_2F68E4"),
        "leave_68e4": count(t, r"leave_2F68E4"),
        "caller_2e4066": count(t, r"caller_2E4066"),
        "gate_2dadc4": count(t, r"gate_init_2DADC4"),
        "writer": count(t, r"uimode_writer ENTER"),
        "b58_spam": count(t, r"312A60 push.*lr=0x2F694F"),
        "force_ui": count(t, r"FORCE state=0x45|force_ui_mode state"),
    }

    if c["gate_2dadc4"] and c["writer"]:
        verdict = "Path A 已到 uimode writer。"
        next_task = "观察自然 splash/ui_mode；收敛 PROBE 为生命周期。"
    elif c["gate_2dadc4"]:
        verdict = "已进 gate_init_2DADC4；下一 blocker 在 B70/writer。"
        next_task = "看 gate_B70 / uimode_writer。"
    elif c["caller_2e4066"]:
        verdict = "已 BL 2DADC4（caller_2E4066），但 gate 日志未到。"
        next_task = "查 2DADC4 入口 hook / 是否立即返回。"
    elif c["leave_68e4"] and not c["caller_2e4066"]:
        verdict = "2F68E4 已返回，但未到 2E4066。"
        next_task = "查 2E4062 之后是否被打断。"
    elif c["enter_68e4"] and not c["leave_68e4"]:
        verdict = "2F68E4 仍未退出（body 终止符无效？）。"
        next_task = "核对 B5C 首 BE u32 是否为 0。"
    elif c["enter_4040"] and not c["enter_68e4"]:
        verdict = "进了 2E4040 但未进 2F68E4。"
        next_task = "查 2E405E BL 是否执行。"
    elif c["event5"]:
        verdict = "有 code=5 事件，但未进 2E4040 日志。"
        next_task = "确认 v66 hooks 已编译。"
    else:
        verdict = "未看到 Path A code=5。"
        next_task = "确认 JJFB_V64_ENQUEUE_ONCE + V66_101AB。"

    key = []
    for pat in (
        r"\[JJFB_V66_101AB\].*",
        r"\[JJFB_V66_PATH_A\].*",
        r"\[JJFB_V56_EVENT\].*TARGETS_2DADC4.*",
        r"caller_2E4066.*",
        r"gate_init_2DADC4.*",
        r"uimode_writer.*",
    ):
        key.extend(re.findall(pat, t)[:8])

    lines = [
        "# v66 Path A 2E4040→2DADC4 运行结果",
        "",
        f"- 日志：`{log}`",
        "",
        "## 1. 目标",
        "",
        "- 证伪：body 以非 0 BE u32 开头会让 `2F68E4` 死循环。",
        "- 补齐：body = BE `0` 终止 → `2F68E4` 立即返回 → `2E4066` → `2DADC4`。",
        "- 禁止 FORCE ui_mode / C0 inject / host UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
    ]
    for k, label in [
        ("fill_101ab", "V66 101AB fill"),
        ("push_2e4d6c", "312A60 from 2E4D6C"),
        ("event5", "EVENT code=5"),
        ("enter_4040", "enter 2E4040"),
        ("enter_68e4", "enter 2F68E4"),
        ("leave_68e4", "leave 2F68E4"),
        ("caller_2e4066", "caller_2E4066"),
        ("gate_2dadc4", "gate_init_2DADC4"),
        ("writer", "uimode_writer"),
        ("b58_spam", "B58 spam from 2F694F"),
        ("force_ui", "FORCE ui"),
    ]:
        lines.append(f"| {label} | {c[k]} |")

    lines += [
        "",
        "## 3. 关键日志",
        "",
        "```text",
    ]
    lines.extend(key[:40] if key else ["(none)"])
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
    ]
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out}")
    print(f"verdict: {verdict}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
