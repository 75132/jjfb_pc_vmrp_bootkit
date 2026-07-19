#!/usr/bin/env python3
"""Analyze v60 family app=2 timerCreate + resume registration."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def count(txt: str, pat: str) -> int:
    return len(re.findall(pat, txt, re.M))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--markdown", type=Path, required=True)
    a = ap.parse_args()
    t = a.log.read_text(encoding="utf-8", errors="replace")

    vals = {
        "app2": count(t, r"\[JJFB_V60_LIFECYCLE\] family app=2"),
        "timer_ok": count(t, r"ERW\+0x8C4 timer=0x[1-9a-fA-F][0-9a-fA-F]* OK"),
        "timer_null": count(t, r"STILL_NULL"),
        "tcreate": count(t, r"\[JJFB_V60_TIMER\] mrc_timerCreate"),
        "fn_30cbbc": count(t, r"\[JJFB_V60_TIMER\] fn_30CBBC_init"),
        "resume": count(t, r"\[JJFB_V59_LIFECYCLE\] mrc_resume\(5\) after init"),
        "timer_err": count(t, r"timer err:1000"),
        "erw_ok_reg": count(t, r"ERW8C4=0x[1-9a-fA-F]"),
        "bl_reg": count(t, r"\[JJFB_V57_SRC\] BL_3054A4"),
        "cb_target": count(t, r"CALLBACK_2F5404"),
        "cb_entry": count(t, r"entry_2F5404"),
        "gate": count(t, r"gate_init_2DADC4"),
        "writer": count(t, r"uimode_writer ENTER"),
        "c0": count(t, r"TARGETS_2FEBBC"),
        "force": count(t, r"FORCE state/ui_mode"),
    }

    if vals["force"]:
        verdict = "出现 FORCE；本轮作废。"
        next_task = "确认 JJFB_FORCE_UI_MODE=0。"
    elif not vals["app2"]:
        verdict = "未发 family app=2；检查 JJFB_FAMILY_APP2_AFTER_INIT。"
        next_task = "确认开关与构建。"
    elif vals["timer_err"]:
        verdict = "仍有 timer err:1000；app=2 未把有效 timer 写入 ERW+0x8C4。"
        next_task = "查 30CBBC/timerCreate 是否命中及返回值。"
    elif vals["cb_entry"] and not (vals["writer"] or vals["gate"]):
        verdict = (
            "entry_2F5404 已命中且无 timer err:1000；"
            "尚未到 2DADC4/writer — 回调调度已开但上游未跑完。"
        )
        next_task = "追 2F5404 → 305EB8 → 2DADC4；查为何只调度 1 次。"
    elif vals["cb_entry"] or vals["writer"] or vals["gate"]:
        verdict = "回调已调度或已到 writer；黑屏上游突破。"
        next_task = "继续追 splash 自然绘制 / B70 门控。"
    elif vals["cb_target"] and not vals["timer_err"] and vals["timer_ok"]:
        verdict = (
            "app=2 已创建 timer，resume 注册不再报 1000；"
            "仍无 entry_2F5404 — 下一步查 timer 调度契约。"
        )
        next_task = "对照 mrc_timerStartEx / mr_timerStart 与 host SDL timer。"
    elif vals["tcreate"] or vals["fn_30cbbc"]:
        verdict = "进入了 timerCreate 路径；对照 ERW+0x8C4 与 resume 结果。"
        next_task = "核对 register 行的 ERW8C4 与 timer err。"
    else:
        verdict = "部分命中；人工读 V60/V59 日志。"
        next_task = "对照关键行。"

    keys = (
        "JJFB_V60_",
        "JJFB_V59_LIFECYCLE",
        "timer err",
        "ERW8C4",
        "CALLBACK",
        "entry_2F5404",
        "uimode_writer",
        "gate_init",
        "TARGETS_2FEBBC",
        "natural_mode",
        "FORCE state",
    )
    samples = [l for l in t.splitlines() if any(k in l for k in keys)][:100]

    lines = [
        "# v60 Family app=2 Timer + Resume 运行结果",
        "",
        f"- 日志：`{a.log}`",
        "",
        "## 1. 目标",
        "",
        "- 参照 `JJFB_v57_LIFECYCLE_SOURCE_COVERAGE_COMPLETE`：",
        "  method5/resume 注册前必须先有 timer 对象。",
        "- 平台契约：`family app=2` → `30CBBC` → `mrc_timerCreate` → `ERW+0x8C4`，",
        "  再 `mrc_resume(5)` 注册 `2F5405`。",
        "- 禁止 FORCE ui_mode / 注入 C0 / host 画 UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
        f"| family app=2 | {vals['app2']} |",
        f"| fn_30CBBC / timerCreate | {vals['fn_30cbbc']} / {vals['tcreate']} |",
        f"| ERW+0x8C4 OK | {vals['timer_ok']} |",
        f"| ERW+0x8C4 NULL | {vals['timer_null']} |",
        f"| mrc_resume | {vals['resume']} |",
        f"| timer err:1000 | {vals['timer_err']} |",
        f"| BL_3054A4 / CALLBACK | {vals['bl_reg']} / {vals['cb_target']} |",
        f"| entry_2F5404 | {vals['cb_entry']} |",
        f"| C0 / gate / writer | {vals['c0']} / {vals['gate']} / {vals['writer']} |",
        f"| FORCE | {vals['force']} |",
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
