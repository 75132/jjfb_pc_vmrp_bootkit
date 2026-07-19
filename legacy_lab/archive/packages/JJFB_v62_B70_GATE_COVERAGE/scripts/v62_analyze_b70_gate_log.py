#!/usr/bin/env python3
"""Analyze v62 15D/B71/B70 gate coverage."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def count(t: str, pat: str) -> int:
    return len(re.findall(pat, t))


def main() -> int:
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "logs" / "v62_b70_gate_stdout.txt"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "reports" / "v62_b70_gate_run_result.md"
    t = log.read_text(encoding="utf-8", errors="replace")

    c = {
        "v62_cov": count(t, r"\[JJFB_V62_FLAG\] coverage installed"),
        "mem_watch": count(t, r"mem watch ERW\+0x15D"),
        "writer": count(t, r"\[JJFB_V62_FLAG\] writer"),
        "mem_write": count(t, r"\[JJFB_V62_FLAG\] mem_write"),
        "after_app2": count(t, r"after family app=2 15D="),
        "entry_305eb8": count(t, r"entry_305EB8"),
        "fail_15d": count(t, r"fail_15D_ne_1"),
        "fail_b71": count(t, r"fail_B71_eq_0"),
        "fail_134d": count(t, r"fail_134D_ne_0"),
        "ok_gate": count(t, r"ok_to_2DADC4"),
        "caller_305ef4": count(t, r"caller_305EF4"),
        "gate_2dadc4": count(t, r"gate_init_2DADC4"),
        "writer_ui": count(t, r"uimode_writer ENTER"),
        "force": count(t, r"FORCE state|FORCE ui_mode"),
        "w_15d_30cbbc": count(t, r"strb_15D=1_in_30CBBC"),
        "w_b71_clear": count(t, r"strb_B71=0_in_2FE82C"),
        "w_b71_set_30ed": count(t, r"strb_B71=1_in_30ED2C"),
        "w_b71_set_2dc": count(t, r"strb_B71=1_in_2DC4D8"),
        "w_15d_2dc": count(t, r"strb_15D=1_in_2DC4D8"),
        "w_b70_reset": count(t, r"strb_B70_in_2FEBBC"),
    }

    after = re.findall(
        r"after family app=2 15D=(\d+) B71=(\d+) B70=(\d+) 134D=(\d+)", t
    )
    entries = re.findall(
        r"entry_305EB8 #(\d+).*?15D=(\d+) B71=(\d+) f134d=(\d+) B70=(\d+).*?(\w+)",
        t,
    )

    if c["ok_gate"] or c["gate_2dadc4"]:
        verdict = "已穿过 305EB8 门控进入 2DADC4；下一轮追 B70→writer。"
        next_task = "观察 B70 置位与 2FC03C/uimode_writer。"
    elif c["fail_b71"] and not c["fail_15d"]:
        verdict = "15D 已满足，blocker 是 B71 恒 0（无人置位）。"
        next_task = "追 2E2520→2DC4D8 / 30ED2C 谁应写 B71=1（事件或平台契约）。"
    elif c["fail_15d"]:
        verdict = "305EB8 卡在 15D!=1；检查 30CBBC 是否执行到 30CCF4。"
        next_task = "确认 family app=2 / 30CCF4 写者命中。"
    else:
        verdict = "门控日志不足；检查 v62 探针是否装上。"
        next_task = "确认 RUN 脚本与 bridge 覆盖。"

    lines = [
        "# v62 B70/B71/15D Gate 运行结果",
        "",
        f"- 日志：`{log}`",
        "",
        "## 1. 目标",
        "",
        "- 静态确认：`305EB8` 门控是 `15D==1` / `B71!=0` / `134D==0`（不是旧探针的 B70）。",
        "- 动态观察谁写这些标志；禁止 FORCE ui_mode / C0 inject / host UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
    ]
    for k, label in [
        ("v62_cov", "V62 coverage"),
        ("mem_watch", "mem watch"),
        ("writer", "flag writer sites"),
        ("mem_write", "flag mem_write"),
        ("w_15d_30cbbc", "30CCF4 15D=1"),
        ("w_b71_clear", "2FE854 B71=0"),
        ("w_b71_set_30ed", "30ED7A B71=1"),
        ("w_b71_set_2dc", "2DC572 B71=1"),
        ("w_15d_2dc", "2DC576 15D=1"),
        ("w_b70_reset", "2FEC9A B70"),
        ("after_app2", "after app=2 dump"),
        ("entry_305eb8", "entry_305EB8"),
        ("fail_15d", "fail_15D"),
        ("fail_b71", "fail_B71"),
        ("fail_134d", "fail_134D"),
        ("ok_gate", "ok_to_2DADC4"),
        ("caller_305ef4", "caller_305EF4"),
        ("gate_2dadc4", "gate_init_2DADC4"),
        ("writer_ui", "uimode_writer"),
        ("force", "FORCE"),
    ]:
        lines.append(f"| {label} | {c[k]} |")

    lines += ["", "## 3. 关键日志", "", "```text"]
    if after:
        a = after[0]
        lines.append(f"after family app=2 15D={a[0]} B71={a[1]} B70={a[2]} 134D={a[3]}")
    for e in entries[:6]:
        lines.append(
            f"entry_305EB8 #{e[0]} 15D={e[1]} B71={e[2]} f134d={e[3]} B70={e[4]} {e[5]}"
        )
    for m in re.findall(r"\[JJFB_V62_FLAG\] writer.*", t)[:8]:
        lines.append(m)
    for m in re.findall(r"\[JJFB_V62_FLAG\] mem_write.*", t)[:8]:
        lines.append(m)
    lines += ["```", "", "## 4. 结论", "", f"- {verdict}", "", "## 5. blocker / 下一步", "", f"- next: {next_task}", ""]
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
