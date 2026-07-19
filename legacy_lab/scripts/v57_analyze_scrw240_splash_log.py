#!/usr/bin/env python3
"""Analyze v57 SCRW=240 / 10134 size-map / splash present logs."""
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
    force = "FORCE state/ui_mode" in text
    bmp = [l for l in lines if "JJFB_10134_BMP" in l]
    size_map = [l for l in lines if "size-map" in l]
    load = [l for l in lines if "JJFB_BMP_LOAD" in l]
    remap = [l for l in lines if "axis_remap" in l]
    splash = [l for l in lines if "JJFB_GWY_SPLASH] presented" in l or "GWY_SPLASH" in l]
    present = "presented" in text and ("original assets" in text or "GWY_SPLASH" in text)
    mode45 = "final ui_mode=0x45" in text or "ui_mode 0x3 -> 0x45" in text
    offscreen = bool(re.search(r"DrawRect #\d+ 2[4-9]\d,", text))  # x>=240 without remap log nearby

    if force:
        verdict = "仍出现 FORCE；偏离 v57 契约。"
        blocker = "FORCE still active"
        next_task = "确认 JJFB_FORCE_UI_MODE=0。"
    elif not bmp:
        verdict = "10134 仍无 BMP 构造（size-map 未生效或未请求 splash 尺寸）。"
        blocker = "no 10134_BMP"
        next_task = "核对 size-map 与 app=0x2D8A/0x240/0x1C20 日志。"
    elif not present:
        verdict = "10134 已加载资源，但未 present splash assets。"
        blocker = "splash present missing"
        next_task = "检查 JJFB_GWY_SPLASH_BLIT 与 guest_pixels。"
    elif mode45 and present:
        verdict = "docx 路径成立：SCRW240 + 原版 splash 资源已 load/present；无 FORCE。"
        blocker = "observe natural 2EC6B0 / 2DADC4 callers next"
        next_task = "确认画面可见；并行追 2DADC4 自然 caller。"
    else:
        verdict = "部分证据；对照日志。"
        blocker = "partial"
        next_task = "加长运行核对。"

    samples = [l for l in lines if any(k in l for k in (
        "size-map", "10134_BMP", "BMP_LOAD", "axis_remap", "GWY_SPLASH",
        "FORCE state", "final ui_mode", "host mrc_init", "natural_mode"
    ))][:60]

    report = []
    report.append("# v57 SCRW240 / 10134 / splash 运行结果")
    report.append("")
    report.append(f"- 日志：`{args.log}`")
    report.append(f"- 行数：{len(lines)}")
    report.append("")
    report.append("## 1. 目标")
    report.append("")
    report.append("- 结合 `docx`：SCRW=240、绘图后 refresh。")
    report.append("- 修复 10134 对 splash 尺寸 no-match；DrawRect 320 布局 remap；present 原版资源。")
    report.append("- 不做 host FORCE ui_mode。")
    report.append("")
    report.append("## 2. 检查项")
    report.append("")
    report.append(f"- handoff+timer：{yn(handoff)}")
    report.append(f"- FORCE mem-write：{yn(force)}")
    report.append(f"- size-map 次数：{len(size_map)}")
    report.append(f"- 10134_BMP 次数：{len(bmp)}")
    report.append(f"- BMP_LOAD 次数：{len(load)}")
    report.append(f"- axis_remap 次数：{len(remap)}")
    report.append(f"- splash present：{yn(present)}")
    report.append(f"- ui_mode 0x45：{yn(mode45)}")
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
