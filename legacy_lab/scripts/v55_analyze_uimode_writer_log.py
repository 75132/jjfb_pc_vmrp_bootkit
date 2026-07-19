#!/usr/bin/env python3
"""Analyze v55 natural ui_mode writer coverage logs."""
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

    handoff = "action=run_host_801_recovery" in text and "host mrc_init(0) ret=0" in text
    natural = "natural_mode=1" in text and "FORCE state/ui_mode" not in text
    cov = "uimode_writer coverage installed" in text
    writer_enter = [l for l in lines if "uimode_writer ENTER" in l]
    writer_store = [l for l in lines if "uimode_writer STORE" in l]
    gate_init = [l for l in lines if "gate_init_2DADC4" in l]
    gate_b70 = [l for l in lines if "gate_B70_check" in l]
    bl_2fc03c = [l for l in lines if "BL_2FC03C_site" in l]
    init_2fc03c = [l for l in lines if "init_2FC03C" in l]
    callers = [l for l in lines if "caller_2FECA2" in l or "caller_2E4066" in l or "caller_305EF4" in l]
    alt = [l for l in lines if "alt_30EE50" in l]
    game_self = [l for l in lines if "[JJFB_GAME_SELF]" in l]

    if writer_store:
        verdict = "自然 writer 已执行并写入 ui_mode=0x45；下一轮跟 splash/网络推进。"
        blocker = "none for writer reachability"
        next_task = "观察 writer 后 AC8/progress/connecting 是否自然出现。"
    elif writer_enter:
        verdict = "进入 0x2FC418 但未确认 STORE；检查是否提前返回。"
        blocker = "writer entered without confirmed store"
        next_task = "单步 0x2FC418 内部条件。"
    elif bl_2fc03c or init_2fc03c:
        verdict = "到达 2FC03C 路径但未进 writer；少见。"
        blocker = "2FC03C without writer"
        next_task = "审计 2FC03C 尾部 BL。"
    elif gate_b70:
        b70_vals = re.findall(r"B70=(\d+)", "\n".join(gate_b70))
        verdict = f"命中 B70 门（值样本 {b70_vals[:8]}），但未 BL 2FC03C。"
        blocker = "ERW+0xB70 gate keeps path from writer"
        next_task = "找出谁应写 ERW+0xB70（nonzero）以及何时写。"
    elif gate_init:
        verdict = "命中 2DADC4 门控 init，但未到 B70→2FC03C 分支。"
        blocker = "2DADC4 internal branch miss"
        next_task = "对照 B58/DB0 条件反汇编 2DADC4。"
    elif callers:
        verdict = "上游 caller 触发，但 2DADC4 未记到（钩子时序？）。"
        blocker = "caller without 2DADC4"
        next_task = "确认 hook 在 mrc_init 前安装成功。"
    elif cov and natural and handoff:
        verdict = "handoff+natural+coverage OK，但 writer 链全程未执行。"
        blocker = "no caller of 2DADC4/2FC418 in 25s natural run"
        next_task = "追谁应调用 2DADC4（2FECA2/2E4066/305EF4），及其前置平台事件。"
    else:
        verdict = "证据不完整。"
        blocker = "incomplete"
        next_task = "重跑并确认编译含 v55 hooks。"

    report = []
    report.append("# v55 ui_mode Writer Coverage 运行结果")
    report.append("")
    report.append(f"- 日志：`{args.log}`")
    report.append(f"- 行数：{len(lines)}")
    report.append("")
    report.append("## 1. 目标")
    report.append("")
    report.append("- 定位自然写 `ERW+0x8D0=0x45` 的 guest 函数，不用 FORCE。")
    report.append("- 覆盖：`0x2FC418` writer ← `0x2FC03C` ← `0x2DADC4`(B70) ← callers。")
    report.append("")
    report.append("## 2. 门禁")
    report.append("")
    report.append(f"- handoff 801+timer：{yn(handoff)}")
    report.append(f"- NO FORCE / natural_mode：{yn(natural)}")
    report.append(f"- writer coverage installed：{yn(cov)}")
    report.append(f"- writer ENTER：{len(writer_enter)}")
    report.append(f"- writer STORE：{len(writer_store)}")
    report.append(f"- 2DADC4：{len(gate_init)}")
    report.append(f"- B70 check：{len(gate_b70)}")
    report.append(f"- BL 2FC03C：{len(bl_2fc03c)}")
    report.append(f"- init 2FC03C：{len(init_2fc03c)}")
    report.append(f"- callers：{len(callers)}")
    report.append(f"- alt 30EE50：{len(alt)}")
    report.append("")
    report.append("## 3. GAME_SELF 样本")
    report.append("")
    report.append("```text")
    report.extend((game_self[:40] if game_self else ["(none)"]))
    report.append("```")
    report.append("")
    report.append("## 4. 结论")
    report.append("")
    report.append(f"- {verdict}")
    report.append("")
    report.append("## 5. 当前 blocker")
    report.append("")
    report.append(f"- {blocker}")
    report.append("")
    report.append("## 6. 下一步")
    report.append("")
    report.append(f"- {next_task}")
    report.append("")

    out = "\n".join(report)
    if args.markdown:
        Path(args.markdown).write_text(out, encoding="utf-8")
        print(f"wrote {args.markdown}")
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
