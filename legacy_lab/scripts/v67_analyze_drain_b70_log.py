#!/usr/bin/env python3
"""Analyze v67: 2E2520 return / 312C0C dequeue / 2FC26C alt (B70==0)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def count(t: str, pat: str) -> int:
    return len(re.findall(pat, t))


def main() -> int:
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "logs" / "v67_drain_b70_stdout.txt"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "reports" / "v67_drain_b70_run_result.md"
    t = log.read_text(encoding="utf-8", errors="replace")

    c = {
        "fill_101ab": count(t, r"\[JJFB_V66_101AB\] fill"),
        "enter_4040": count(t, r"enter_2E4040"),
        "gate_2dadc4": count(t, r"gate_init_2DADC4"),
        "gate_b70": count(t, r"gate_B70_check"),
        "bl_2fc26c": count(t, r"BL_2FC26C_site"),
        "enter_2fc26c": count(t, r"enter_2FC26C"),
        "bl_2fc03c": count(t, r"BL_2FC03C_site|init_2FC03C"),
        "writer": count(t, r"uimode_writer ENTER"),
        "strb_b70": count(t, r"strb_B70_in_2FEBBC"),
        "after_ret": count(t, r"after_2E2520"),
        "keep": count(t, r"after_2E2520.*KEEP_IN_QUEUE"),
        "expect_dq": count(t, r"after_2E2520.*EXPECT_DEQUEUE"),
        "remove": count(t, r"312C0C remove"),
        "remove_from_drain": count(t, r"312C0C remove.*lr=0x2DC8E"),
        "drawfp_skip": count(t, r"\[JJFB_V67_DRAWFP\] bad fp150C"),
        "force_ui": count(t, r"FORCE state=0x45|force_ui_mode state"),
    }

    rets = re.findall(r"after_2E2520 #\d+ ret=0x([0-9A-Fa-f]+)", t)
    counts_after = re.findall(r"after_2E2520 #\d+ ret=0x[0-9A-Fa-f]+ \([^)]+\) B54=0x[0-9A-Fa-f]+ count=(\d+)", t)

    if c["writer"] or c["bl_2fc03c"]:
        verdict = "已到 2FC03C/writer。"
        next_task = "观察自然 ui_mode / splash；收敛 PROBE。"
    elif c["strb_b70"]:
        verdict = "自然写了 B70（2FEBBC），但尚未进 writer。"
        next_task = "跟 2FEBBC → 2DADC4 → 2FC03C。"
    elif c["expect_dq"] and c["remove_from_drain"] and counts_after and all(int(x) == 0 for x in counts_after[:3]):
        verdict = "2E2520 已返回并出队（B54 count→0）。drawFP 阻断已解除。"
        next_task = "跟 2FC26C 后的自然 B70/2FEBBC 或 0x1E201 契约；勿 FORCE。"
    elif c["expect_dq"] and c["remove_from_drain"]:
        verdict = "已 DEQUEUE；检查后续 count 是否回升。"
        next_task = "若 count 仍>0，查二次入队；否则跟 B70。"
    elif c["drawfp_skip"] and not c["after_ret"]:
        verdict = "已 skip 坏 drawFP，但仍未见 after_2E2520。"
        next_task = "确认 2EC6B0 入口总是命中；或在 2EC71A 再加 guard。"
    elif c["keep"] and not c["expect_dq"]:
        verdict = "2E2520 一直返回 1（KEEP_IN_QUEUE）→ B54 不出队。"
        next_task = "查谁把 Path A 返回值变成 1（应走 2E4194 MOVS r0,#0）。"
    elif c["expect_dq"] and not c["remove_from_drain"]:
        verdict = "2E2520 返回 0 但未见 drain 侧 312C0C。"
        next_task = "核对 2DC8D8 之后控制流 / hook。"
    elif c["enter_2fc26c"] and c["after_ret"] == 0:
        verdict = "2FC26C 进入后 2E2520 仍不返回（可能仍被坏 BLX/其它路径掐断）。"
        next_task = "查 V67_DRAWFP 是否命中；dump fp150C 实际值。"
    elif c["enter_2fc26c"] and c["expect_dq"]:
        verdict = "Path A → 2FC26C（B70==0 alt）且 2E2520 返回 0 出队。"
        next_task = "跟自然 B70/2FEBBC 或 2FC26C 后续契约。"
    elif c["gate_b70"]:
        verdict = "仍停在 gate_B70；缺返回值日志。"
        next_task = "确认 V67_DRAIN hooks 已编译。"
    else:
        verdict = "未到 Path A / gate。"
        next_task = "确认 JJFB_V64_ENQUEUE_ONCE=1。"

    key = []
    for pat in (
        r"\[JJFB_V67_DRAIN\].*",
        r"\[JJFB_V67_GATE\].*",
        r"\[JJFB_V67_DRAWFP\].*",
        r"gate_B70_check.*",
        r"strb_B70.*",
        r"uimode_writer.*",
        r"\[JJFB_V66_101AB\].*",
    ):
        key.extend(re.findall(pat, t)[:10])

    lines = [
        "# v67 drain / B70 运行结果",
        "",
        f"- 日志：`{log}`",
        "",
        "## 1. 目标",
        "",
        "- 解释 code=5 节点为何反复 drain。",
        "- 区分：`2E2520 ret==1`（故意留队）vs `312C0C` 出队失败。",
        "- 观察 B70==0 时的 `2FC26C` alt（非 FORCE / 非 C0）。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
    ]
    for k, label in [
        ("fill_101ab", "101AB fill"),
        ("enter_4040", "enter 2E4040"),
        ("gate_2dadc4", "gate_init_2DADC4"),
        ("gate_b70", "gate_B70_check"),
        ("bl_2fc26c", "BL 2FC26C site"),
        ("enter_2fc26c", "enter 2FC26C"),
        ("after_ret", "after_2E2520"),
        ("keep", "ret==1 KEEP"),
        ("expect_dq", "ret!=1 EXPECT_DEQUEUE"),
        ("remove", "312C0C remove"),
        ("remove_from_drain", "312C0C from drain"),
        ("drawfp_skip", "V67 bad drawFP skip"),
        ("strb_b70", "strb B70 in 2FEBBC"),
        ("bl_2fc03c", "2FC03C"),
        ("writer", "uimode_writer"),
        ("force_ui", "FORCE ui"),
    ]:
        lines.append(f"| {label} | {c[k]} |")

    lines += [
        "",
        "## 3. 返回值样本",
        "",
        "```text",
        f"rets(hex)={rets[:20]}",
        f"B54_count_after={counts_after[:20]}",
        "```",
        "",
        "## 4. 关键日志",
        "",
        "```text",
    ]
    lines.extend(key[:50] if key else ["(none)"])
    lines += [
        "```",
        "",
        "## 5. 结论",
        "",
        f"- {verdict}",
        "",
        "## 6. blocker / 下一步",
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
