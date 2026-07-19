#!/usr/bin/env python3
"""Analyze v63 Path A (MENU_RETURN/MOUSE_MOVE) → 2DADC4 probe."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def count(t: str, pat: str) -> int:
    return len(re.findall(pat, t))


def main() -> int:
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "logs" / "v63_path_a_event_stdout.txt"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "reports" / "v63_path_a_event_run_result.md"
    t = log.read_text(encoding="utf-8", errors="replace")

    c = {
        "probe": count(t, r"\[JJFB_V63_PATH_A\] PROBE once"),
        "mrc_ret": count(t, r"\[JJFB_V63_PATH_A\] mrc_event\("),
        "path_a_hit": count(t, r"\[JJFB_V63_PATH_A\] event code="),
        "mouse_up_note": count(t, r"MR_MOUSE_UP.*NOT Path C"),
        "caller_2e4066": count(t, r"caller_2E4066"),
        "gate_2dadc4": count(t, r"gate_init_2DADC4"),
        "gate_b70": count(t, r"gate_B70_check"),
        "bl_2fc03c": count(t, r"BL_2FC03C_site"),
        "writer": count(t, r"uimode_writer ENTER"),
        "ok_305eb8": count(t, r"ok_to_2DADC4"),
        "fail_b71": count(t, r"fail_B71_eq_0"),
        "force_ui": count(t, r"FORCE state=0x45|force_ui_mode state"),
    }

    if c["gate_2dadc4"] and c["writer"]:
        verdict = "Path A 已进 2DADC4 且到达 writer；上游打通。"
        next_task = "收敛 Path A 为正式生命周期契约（非盲扫）。"
    elif c["gate_2dadc4"]:
        verdict = "Path A 已进 2DADC4；下一 blocker 在 B70/B58/DB0 或 writer。"
        next_task = "看 gate_B70 / init_2FC03C / uimode_writer。"
    elif c["caller_2e4066"] or c["path_a_hit"]:
        verdict = "Path A 事件已到达 dispatch，但未进 gate_init。"
        next_task = "查 2E4040 前置条件（B58 等）。"
    elif c["probe"]:
        verdict = "已发出 Path A PROBE，但未看到 2E2520/2E4066。"
        next_task = "查 mrc_event(code=1) 是否进入 robotol 事件队列。"
    else:
        verdict = "未触发 Path A PROBE；检查 JJFB_PATH_A_EVENT_ONCE。"
        next_task = "确认 RUN 脚本环境变量。"

    lines = [
        "# v63 Path A Event 运行结果",
        "",
        f"- 日志：`{log}`",
        "",
        "## 1. 目标",
        "",
        "- 证伪：`MR_MOUSE_UP` 不是 Path C 的 B71 启动器（会置 134D=2）。",
        "- 探针：单次 `MR_MENU_RETURN(5)` 或 `MR_MOUSE_MOVE(12)` → `2E4040` → `2DADC4`。",
        "- 禁止 FORCE ui_mode / C0 inject / host UI / 事件码盲扫。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
    ]
    for k, label in [
        ("probe", "PATH_A PROBE"),
        ("mrc_ret", "mrc_event ret"),
        ("path_a_hit", "Path A event at 2E2520"),
        ("mouse_up_note", "MOUSE_UP note"),
        ("caller_2e4066", "caller_2E4066"),
        ("gate_2dadc4", "gate_init_2DADC4"),
        ("gate_b70", "gate_B70_check"),
        ("bl_2fc03c", "BL_2FC03C"),
        ("writer", "uimode_writer"),
        ("ok_305eb8", "ok_to_2DADC4"),
        ("fail_b71", "fail_B71 (sample)"),
        ("force_ui", "FORCE ui"),
    ]:
        lines.append(f"| {label} | {c[k]} |")

    lines += ["", "## 3. 关键日志", "", "```text"]
    for pat in [
        r"\[JJFB_V63_PATH_A\].*",
        r"caller_2E4066.*",
        r"gate_init_2DADC4.*",
        r"gate_B70_check.*",
        r"uimode_writer ENTER.*",
        r"ok_to_2DADC4.*",
    ]:
        for m in re.findall(pat, t)[:6]:
            lines.append(m)
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
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out}")
    print("verdict:", verdict)
    print("next:", next_task)
    for k, v in c.items():
        if v:
            print(f"  {k}={v}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
