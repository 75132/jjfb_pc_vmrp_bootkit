#!/usr/bin/env python3
"""Analyze v57 C0 / callback source coverage (no inject)."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def count(txt: str, pat: str) -> int:
    return len(re.findall(pat, txt, re.M))


def max_family_n(txt: str) -> int:
    ns = [int(x) for x in re.findall(r"\[JJFB_V56_FAMILY\] dispatch #(\d+)", txt)]
    return max(ns) if ns else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--markdown", type=Path, required=True)
    a = ap.parse_args()
    t = a.log.read_text(encoding="utf-8", errors="replace")

    vals = {
        "v57_installed": count(t, r"\[JJFB_V57_SRC\] coverage installed"),
        # Match dynamic hit lines only — not the install banner that names the tags.
        "prep_2f5390": count(t, r"\[JJFB_V57_SRC\] call_2F5390_prep"),
        "bl_3054a4": count(t, r"\[JJFB_V57_SRC\] BL_3054A4"),
        "bl_3054a4_target": count(t, r"\[JJFB_V57_SRC\] BL_3054A4.*CALLBACK_2F5404"),
        "cmp_c0": count(t, r"\[JJFB_V57_SRC\] cmp_r0_C0"),
        "cmp_c0_eq": count(t, r"\[JJFB_V57_SRC\] cmp_r0_C0.*r0=0xC0"),
        "movs_c0": count(t, r"\[JJFB_V57_SRC\] movs_imm_C0"),
        "family_log_lines": count(t, r"\[JJFB_V56_FAMILY\]"),
        "family": max_family_n(t),
        "family_c0": count(t, r"\[JJFB_V56_FAMILY\].*TARGETS_2FEBBC"),
        "cb_register": count(t, r"\[JJFB_V56_CALLBACK\] register"),
        "cb_target": count(t, r"\[JJFB_V56_CALLBACK\].*CALLBACK_2F5404"),
        "cb_entry": count(t, r"entry_2F5404"),
        "gate": count(t, r"gate_init_2DADC4"),
        "writer": count(t, r"uimode_writer ENTER"),
        "force": count(t, r"FORCE state/ui_mode"),
        "natural": count(t, r"natural_mode=1"),
    }

    if vals["force"]:
        verdict = "出现 FORCE；本轮作废。"
        next_task = "确认 JJFB_FORCE_UI_MODE=0。"
    elif vals["writer"] or vals["gate"]:
        verdict = "已到达 2DADC4/writer；上游任务完成，下一轮查 B70/B58/DB0。"
        next_task = "单步门控 ERW。"
    elif vals["prep_2f5390"] == 0 and vals["bl_3054a4"] == 0:
        verdict = (
            "回调注册上游（call 2F5390 / BL 3054A4）整段未执行；"
            "不是“已注册未调度”，而是注册链从未启动。"
        )
        next_task = "静态追 0x304418 / 0x3053BA 的 caller（谁应启动注册）。"
    elif vals["bl_3054a4"] and not vals["cb_entry"]:
        verdict = "进入了 BL 3054A4，但 2F5404 未回调；查 host 调度契约。"
        next_task = "对照 register 参数与 host timer/callback 表。"
    elif vals["cmp_c0"] and not vals["family_c0"]:
        verdict = "见到 CMP #0xC0，但 family 从未以 app=0xC0 分派。"
        next_task = "查 cmp 点所在函数与 1E209 发送路径。"
    elif vals["family"] and not vals["family_c0"]:
        verdict = "family 只有 app=9；C0 生产者未运行。"
        next_task = "追 movs_imm_C0 / 304418 上游。"
    else:
        verdict = "部分命中；对照 JJFB_V57_SRC 日志。"
        next_task = "加长运行并核对计数。"

    samples = [
        l
        for l in t.splitlines()
        if any(
            k in l
            for k in (
                "JJFB_V57_SRC",
                "JJFB_V56_FAMILY",
                "JJFB_V56_CALLBACK",
                "natural_mode",
                "FORCE state",
                "gate_init_2DADC4",
                "uimode_writer",
            )
        )
    ][:60]

    lines = [
        "# v57 C0 / Callback Source Coverage 运行结果",
        "",
        f"- 日志：`{a.log}`",
        "",
        "## 1. 目标",
        "",
        "- 只观察谁应产生 family `app=0xC0`、谁应 BL `3054A4` 注册 `2F5405`。",
        "- 禁止 FORCE / 注入 C0 / 注入 event 5·12 / host 画 UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
        f"| V57_SRC installed | {vals['v57_installed']} |",
        f"| call 2F5390 prep | {vals['prep_2f5390']} |",
        f"| BL 3054A4 | {vals['bl_3054a4']} |",
        f"| BL 3054A4 + CALLBACK_2F5404 | {vals['bl_3054a4_target']} |",
        f"| CMP r0,#0xC0 | {vals['cmp_c0']} |",
        f"| CMP 时 r0==0xC0 | {vals['cmp_c0_eq']} |",
        f"| MOVS #0xC0 | {vals['movs_c0']} |",
        f"| family dispatch (max #) | {vals['family']} |",
        f"| family log lines | {vals['family_log_lines']} |",
        f"| family app=0xC0 | {vals['family_c0']} |",
        f"| callback register | {vals['cb_register']} |",
        f"| CALLBACK_2F5404 标记 | {vals['cb_target']} |",
        f"| 2F5404 entry | {vals['cb_entry']} |",
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
