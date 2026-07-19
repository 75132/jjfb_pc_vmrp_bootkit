#!/usr/bin/env python3
"""Analyze v56 GWY bring-up / white-screen fix logs."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def yn(v: bool) -> str:
    return "是" if v else "否"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--markdown", default="")
    args = ap.parse_args()
    text = Path(args.log).read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    handoff = "host mrc_init(0) ret=0" in text and "RUNNING=1" in text
    natural = "natural_mode=1" in text
    force = "FORCE state/ui_mode" in text
    bringup = "[JJFB_GWY_BRINGUP]" in text
    presented = "presented dark baseline" in text
    writer_enter = "uimode_writer ENTER" in text
    called_2fc418 = "call 0x2FC418" in text
    mode_2fc03c = re.findall(r"ui_mode 0x[0-9A-Fa-f]+ -> (0x[0-9A-Fa-f]+) after 2FC03C", text)
    mode_2fc418 = re.findall(r"ui_mode 0x[0-9A-Fa-f]+ -> (0x[0-9A-Fa-f]+) after 2FC418", text)
    mode_final = re.findall(r"final ui_mode=(0x[0-9A-Fa-f]+) C44=(\d+)", text)
    c44 = re.findall(r"C44=(\d+) after 2FC8B8", text)
    if mode_final:
        c44_final = mode_final[-1][1]
        mode_after = [mode_final[-1][0]]
    elif mode_2fc418:
        c44_final = c44[-1] if c44 else "-"
        mode_after = mode_2fc418
    else:
        c44_final = c44[-1] if c44 else "-"
        mode_after = mode_2fc03c
    splash = "SPLASH_ENTER" in text
    disp = len(re.findall(r"\[JJFB_DRAW\] DispUpEx", text))
    draw = len(re.findall(r"\[JJFB_DRAW\]", text))
    host_blit = "2EC6B0_BLIT" in text or "DEBUG_PRESENT" in text

    mode_ok = bool(mode_after) and mode_after[-1].lower() in ("0x45", "0X45")
    c44_ok = c44_final != "-" and c44_final != "0"

    if force:
        verdict = "仍出现 FORCE mem-write；bring-up 路径被旧 FORCE 覆盖。"
        blocker = "FORCE still active"
        next_task = "确认 JJFB_FORCE_UI_MODE=0 与 GWY skip_force。"
    elif not bringup:
        verdict = "未看到 GWY_BRINGUP 日志。"
        blocker = "bringup not reached"
        next_task = "确认编译含 jjfb_gwy_bringup_first_screen 且 tick=10 执行。"
    elif not presented:
        verdict = "bring-up 未完成 present。"
        blocker = "present missing"
        next_task = "检查 guiDrawBitmap / SDL window。"
    elif mode_ok and c44_ok:
        verdict = "bring-up 成功：guest writer 写到 0x45，C44 打开，已 present 非白底。"
        blocker = "splash content may still be sparse (progress/AC8)" if not splash else "observe splash/net next"
        next_task = "若仍接近空白，追 splash host-blit / 自然 progress；并行追 2DADC4 自然 caller。"
    elif presented and not mode_ok:
        verdict = "已 present 暗底，但 guest writer 未把 ui_mode 写成 0x45。"
        blocker = f"ui_mode final={mode_after[-1] if mode_after else 'unknown'}"
        next_task = "单步 2FC418 内部；检查 BA0/前置条件。"
    else:
        verdict = "部分 bring-up 证据；需对照日志。"
        blocker = "partial"
        next_task = "加长运行并核对 C44/ui_mode。"

    samples = [l for l in lines if any(k in l for k in (
        "JJFB_GWY_BRINGUP", "uimode_writer", "FORCE state", "SPLASH_ENTER",
        "DispUpEx", "natural_mode", "host mrc_init", "robotol timer"
    ))][:50]

    report = []
    report.append("# v56 GWY Bring-up 运行结果（白屏）")
    report.append("")
    report.append(f"- 日志：`{args.log}`")
    report.append(f"- 行数：{len(lines)}")
    report.append("")
    report.append("## 1. 目标")
    report.append("")
    report.append("- 解决白屏：平台 dims/C44 + guest `0x2FC03C`→必要时 `0x2FC418` + DispUpEx。")
    report.append("- 不做 host `uc_mem_write` FORCE ui_mode。")
    report.append("")
    report.append("## 2. 检查项")
    report.append("")
    report.append(f"- handoff+timer：{yn(handoff)}")
    report.append(f"- natural_mode（无 FORCE 写）：{yn(natural and not force)}")
    report.append(f"- FORCE mem-write 出现：{yn(force)}")
    report.append(f"- GWY_BRINGUP：{yn(bringup)}")
    report.append(f"- presented dark baseline：{yn(presented)}")
    report.append(f"- uimode_writer ENTER：{yn(writer_enter)}")
    report.append(f"- call 0x2FC418：{yn(called_2fc418)}")
    report.append(f"- ui_mode after 2FC03C：`{mode_2fc03c[-1] if mode_2fc03c else '-'}`")
    report.append(f"- ui_mode after 2FC418：`{mode_2fc418[-1] if mode_2fc418 else '-'}`")
    report.append(f"- ui_mode final：`{mode_after[-1] if mode_after else '-'}`")
    report.append(f"- C44 final：`{c44_final}`")
    report.append(f"- SPLASH_ENTER：{yn(splash)}")
    report.append(f"- DispUpEx 次数：{disp}")
    report.append(f"- JJFB_DRAW 次数：{draw}")
    report.append(f"- host blit/present：{yn(host_blit)}")
    report.append("")
    report.append("## 3. 关键日志")
    report.append("")
    report.append("```text")
    report.extend(samples if samples else ["(none)"])
    report.append("```")
    report.append("")
    report.append("## 4. 结论")
    report.append("")
    report.append(f"- {verdict}")
    report.append("")
    report.append("## 5. blocker / 下一步")
    report.append("")
    report.append(f"- blocker: {blocker}")
    report.append(f"- next: {next_task}")
    report.append("")

    out = "\n".join(report)
    if args.markdown:
        Path(args.markdown).write_text(out, encoding="utf-8")
        print(f"wrote {args.markdown}")
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
