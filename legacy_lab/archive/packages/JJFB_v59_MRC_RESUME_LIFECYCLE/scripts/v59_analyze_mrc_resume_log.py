#!/usr/bin/env python3
"""Analyze v59 mrc_resume lifecycle + upstream hooks."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def count(txt: str, pat: str) -> int:
    return len(re.findall(pat, txt, re.M))


def max_n(txt: str, pat: str) -> int:
    ns = [int(x) for x in re.findall(pat, txt)]
    return max(ns) if ns else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--markdown", type=Path, required=True)
    a = ap.parse_args()
    t = a.log.read_text(encoding="utf-8", errors="replace")

    vals = {
        "lifecycle": count(t, r"\[JJFB_V59_LIFECYCLE\] mrc_resume\(5\) after init"),
        "lifecycle_skip": count(t, r"mrc_resume skipped"),
        "bl_3053b8": count(t, r"\[JJFB_V58_FN\] BL_3053B8"),
        "fn_3053b8": count(t, r"\[JJFB_V58_FN\] fn_3053B8_wrap"),
        "prep": count(t, r"\[JJFB_V57_SRC\] call_2F5390_prep"),
        "bl_reg": count(t, r"\[JJFB_V57_SRC\] BL_3054A4"),
        "cb_reg": count(t, r"\[JJFB_V56_CALLBACK\] register"),
        "cb_entry": count(t, r"entry_2F5404"),
        "movs": count(t, r"\[JJFB_V57_SRC\] movs_imm_C0"),
        "family_c0": count(t, r"TARGETS_2FEBBC"),
        "family": max_n(t, r"\[JJFB_V56_FAMILY\] dispatch #(\d+)"),
        "gate": count(t, r"gate_init_2DADC4"),
        "writer": count(t, r"uimode_writer ENTER"),
        "force": count(t, r"FORCE state/ui_mode"),
        "natural": count(t, r"natural_mode=1"),
        "ext5": count(t, r"ext_call code=5"),
    }

    if vals["force"]:
        verdict = "出现 FORCE；本轮作废。"
        next_task = "确认 JJFB_FORCE_UI_MODE=0。"
    elif not vals["lifecycle"] and not vals["ext5"]:
        verdict = "未看到 mrc_resume(5)；检查构建/开关。"
        next_task = "确认 JJFB_MRC_RESUME_AFTER_INIT=1 且 vmrp.c 含 V59_LIFECYCLE。"
    elif vals["writer"] or vals["gate"]:
        verdict = "resume 后已到达 2DADC4/writer；上游任务突破。"
        next_task = "单步 B70/B58/DB0 与 splash 自然绘制。"
    elif vals["cb_entry"] or vals["bl_reg"] or vals["prep"] or vals["bl_3053b8"]:
        verdict = (
            "resume 已打通注册路径（3053B8/2F5390/3054A4 有命中）；"
            "继续观察 callback 调度与 C0/writer。"
        )
        next_task = "查 2F5404 是否被 timer 回调；以及 family C0 是否随后出现。"
    elif vals["lifecycle"] or vals["ext5"]:
        verdict = "已发 code=5，但未进入 304B5A/2F5390；查 helper 跳表或 input。"
        next_task = "对照 ext_call code=5 的 r1 与 304AEC 跳表。"
    else:
        verdict = "未归类；对照日志。"
        next_task = "人工读 JJFB_V59/V58/V57 行。"

    keys = (
        "JJFB_V59_LIFECYCLE",
        "JJFB_V58_FN",
        "JJFB_V57_SRC",
        "JJFB_V56_CALLBACK",
        "JJFB_V56_FAMILY",
        "JJFB_V56_PERIODIC",
        "natural_mode",
        "FORCE state",
        "uimode_writer",
        "gate_init",
        "ext_call code=5",
    )
    samples = [l for l in t.splitlines() if any(k in l for k in keys)][:100]

    lines = [
        "# v59 mrc_resume Lifecycle 运行结果",
        "",
        f"- 日志：`{a.log}`",
        "",
        "## 1. 目标",
        "",
        "- 平台契约：`mrc_init` 后单次 `mrc_resume`（ext code=5）。",
        "- 观察是否进入 `304B5A → 2F5390 → 3054A4` 注册链。",
        "- 禁止 FORCE ui_mode / 注入 C0 / host 画 UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
        f"| V59 mrc_resume log | {vals['lifecycle']} |",
        f"| ext_call code=5 | {vals['ext5']} |",
        f"| BL_3053B8 | {vals['bl_3053b8']} |",
        f"| fn_3053B8 | {vals['fn_3053b8']} |",
        f"| call_2F5390_prep | {vals['prep']} |",
        f"| BL_3054A4 | {vals['bl_reg']} |",
        f"| CALLBACK register | {vals['cb_reg']} |",
        f"| entry_2F5404 | {vals['cb_entry']} |",
        f"| MOVS #0xC0 | {vals['movs']} |",
        f"| family app=0xC0 | {vals['family_c0']} |",
        f"| family max # | {vals['family']} |",
        f"| 2DADC4 / writer | {vals['gate']} / {vals['writer']} |",
        f"| FORCE | {vals['force']} |",
        f"| natural_mode | {vals['natural']} |",
        "",
        "## 3. 关键日志",
        "",
        "```text",
    ]
    lines.extend(samples if samples else ["(none)"])
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
    a.markdown.write_text("\n".join(lines), encoding="utf-8")
    print(verdict)
    print(f"wrote {a.markdown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
