#!/usr/bin/env python3
"""Analyze v64 0x10165 enqueue → B54 → 2E2520 Path A probe."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def count(t: str, pat: str) -> int:
    return len(re.findall(pat, t))


def main() -> int:
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "logs" / "v64_enqueue_stdout.txt"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "reports" / "v64_enqueue_run_result.md"
    t = log.read_text(encoding="utf-8", errors="replace")

    c = {
        "reg_10165": count(t, r"\[JJFB_V64_ENQ\] register 0x10165"),
        "probe": count(t, r"\[JJFB_V64_ENQ\] PROBE once"),
        "probe_skip": count(t, r"\[JJFB_V64_ENQ\] PROBE skipped"),
        "probe_ret": count(t, r"\[JJFB_V64_ENQ\] PROBE ret="),
        "site_30d2f8": count(t, r"\[JJFB_V64_ENQ\] site=0x30D2F8"),
        "site_30d24c": count(t, r"\[JJFB_V64_ENQ\] site=0x30D24C"),
        "site_2e4d6c": count(t, r"\[JJFB_V64_ENQ\] site=0x2E4D6C"),
        "drain": count(t, r"drain_2DC80C ENTER"),
        "event_dispatch": count(t, r"\[JJFB_V56_EVENT\]"),
        "path_a_hit": count(t, r"TARGETS_2DADC4|event code=5|event code=12"),
        "caller_2e4066": count(t, r"caller_2E4066"),
        "gate_2dadc4": count(t, r"gate_init_2DADC4"),
        "writer": count(t, r"uimode_writer ENTER"),
        "force_ui": count(t, r"FORCE state=0x45|force_ui_mode state"),
        "mrc_noop_note": count(t, r"mrc_event 1\.\.5 is no-op"),
    }

    if c["gate_2dadc4"] and c["writer"]:
        verdict = "10165 入队已打通 Path A 到 writer。"
        next_task = "把 10165 调用收敛为正式生命周期（非单次 PROBE）。"
    elif c["gate_2dadc4"]:
        verdict = "已进 2DADC4；下一 blocker 在 B70/writer。"
        next_task = "看 gate_B70 / uimode_writer。"
    elif c["event_dispatch"] or c["path_a_hit"]:
        verdict = "B54 已有事件进 2E2520；未到 gate_init。"
        next_task = "查 2E4040 前置（B58 等）。"
    elif c["site_2e4d6c"] and not c["event_dispatch"]:
        verdict = "2E4D6C 入队已跑，但后续 drain 未把事件送到 2E2520。"
        next_task = "查 2E4D6C 返回值 / B54 头 / 2DC80C 过滤分支。"
    elif c["probe"] and not c["site_2e4d6c"]:
        verdict = "已调用 10165 handler，但未进 2E4D6C（参数/门控失败）。"
        next_task = "对照 30D24C 对 r0/r1/15C 的要求。"
    elif c["reg_10165"] and not c["probe"]:
        verdict = "已注册 10165，但 PROBE 未触发。"
        next_task = "确认 JJFB_V64_ENQUEUE_ONCE=1。"
    else:
        verdict = "未看到 10165 注册/PROBE。"
        next_task = "确认 GWY bringup 是否走到 robotol mrc_init。"

    key = []
    for pat in (
        r"\[JJFB_V64_ENQ\].*",
        r"\[JJFB_V56_EVENT\].*",
        r"gate_init_2DADC4.*",
        r"caller_2E4066.*",
        r"uimode_writer.*",
    ):
        key.extend(re.findall(pat, t)[:8])

    lines = [
        "# v64 10165 Enqueue 运行结果",
        "",
        f"- 日志：`{log}`",
        "",
        "## 1. 目标",
        "",
        "- 证伪：helper `mrc_event(5)` 不入 B54（v63）。",
        "- 补齐：host 保存并调用 `0x10165` 注册的 `0x30D2F9`（→`30D24C`→`2E4D6C`→B54）。",
        "- 期望：timer drain `2DC80C` → `2E2520` → Path A `2DADC4`。",
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
        ("probe_skip", "PROBE skipped"),
        ("probe_ret", "PROBE ret"),
        ("site_30d2f8", "site 30D2F8"),
        ("site_30d24c", "site 30D24C"),
        ("site_2e4d6c", "site 2E4D6C"),
        ("drain", "drain 2DC80C"),
        ("event_dispatch", "2E2520 EVENT"),
        ("path_a_hit", "Path A code 5/12"),
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
