#!/usr/bin/env python3
"""Analyze v61 nest-fix: continuous timer + 2F5404 → 305EB8 path."""
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
        "nest_cov": count(t, r"\[JJFB_V61_NEST\] defer_1E209_during_ext_call"),
        "defer": count(t, r"\[JJFB_V61_NEST\] defer 1E209"),
        "flush_def": count(t, r"\[JJFB_V61_NEST\] flush deferred 1E200"),
        "app2": count(t, r"\[JJFB_V60_LIFECYCLE\] family app=2"),
        "timer_ok": count(t, r"ERW\+0x8C4 timer=0x[1-9a-fA-F][0-9a-fA-F]* OK"),
        "resume": count(t, r"\[JJFB_V59_LIFECYCLE\] mrc_resume\(5\) after init"),
        "timer_err": count(t, r"timer err:1000"),
        "code2": count(t, r"ext_call code=2"),
        "code2_ret": count(t, r"ext_call code=2 ret="),
        "cb_entry": count(t, r"entry_2F5404"),
        "cb_tail": count(t, r"tail_call_305EB8"),
        "periodic": count(t, r"entry_305EB8"),
        "gate": count(t, r"gate_init_2DADC4"),
        "writer": count(t, r"uimode_writer ENTER"),
        "c0": count(t, r"TARGETS_2FEBBC"),
        "force": count(t, r"FORCE state/ui_mode"),
        "fam7": count(t, r"family.*app=0x7|fam app=7|app=7 "),
    }

    if vals["force"]:
        verdict = "出现 FORCE；本轮作废。"
        next_task = "关掉 FORCE 重跑。"
    elif vals["timer_err"]:
        verdict = "又出现 timer err:1000；app2 timer 契约回退。"
        next_task = "确认 FAMILY_APP2_AFTER_INIT 仍在 resume 前执行。"
    elif vals["code2"] <= 1 and vals["cb_entry"] <= 1 and not vals["code2_ret"]:
        verdict = "仍像卡在首次 code=2（无 ret）；嵌套挂死可能未修好。"
        next_task = "查 flush/defer 是否生效，或 app=7 自身仍死循环。"
    elif vals["cb_entry"] >= 2 or vals["code2_ret"] >= 2:
        if vals["writer"] or vals["gate"]:
            verdict = "定时回调持续；已到 gate/writer。"
            next_task = "追 B70/ui_mode 自然绘制。"
        elif vals["cb_tail"] or vals["periodic"]:
            verdict = "2F5404 已走到 2F5734/305EB8；尚未 gate/writer。"
            next_task = "追 305EB8 → 305EF4 → 2DADC4 条件。"
        else:
            verdict = "定时持续且 entry_2F5404 多次，但未到 2F5734。"
            next_task = "静态/动态看 2F5404 内为何不落到 2F5734。"
    elif vals["cb_entry"] == 1 and vals["code2_ret"] >= 1:
        verdict = "首次 code=2 已返回（嵌套修复生效），但回调未持续。"
        next_task = "查 guest timer re-arm / mrc_timerStart 是否持续。"
    else:
        verdict = "未形成清晰突破；对照 defer/flush 日志。"
        next_task = "确认 v61 二进制已部署并重跑。"

    lines = [
        "# v61 Callback Nest-Fix 运行结果",
        "",
        f"- 日志：`{a.log}`",
        "",
        "## 1. 目标",
        "",
        "- v60：`entry_2F5404` 出现 1 次后卡在 `family app=7`（嵌套 emu）。",
        "- v61：`ext_call` 期间 defer `1E209`，返回后再 flush。",
        "- 禁止 FORCE ui_mode / 注入 C0 / host 画 UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
    ]
    labels = [
        ("nest_cov", "V61 nest coverage"),
        ("defer", "defer 1E209"),
        ("flush_def", "flush deferred"),
        ("app2", "family app=2"),
        ("timer_ok", "ERW+0x8C4 OK"),
        ("resume", "mrc_resume"),
        ("timer_err", "timer err:1000"),
        ("code2", "ext_call code=2"),
        ("code2_ret", "ext_call code=2 ret"),
        ("cb_entry", "entry_2F5404"),
        ("cb_tail", "tail_call_305EB8"),
        ("periodic", "entry_305EB8"),
        ("gate", "gate_init_2DADC4"),
        ("writer", "uimode_writer"),
        ("c0", "family C0"),
        ("force", "FORCE"),
    ]
    for k, lab in labels:
        lines.append(f"| {lab} | {vals[k]} |")

    key = []
    for pat in (
        r"\[JJFB_V61_NEST\].*",
        r"entry_2F5404.*",
        r"tail_call_305EB8.*",
        r"entry_305EB8.*",
        r"ext_call code=2.*",
        r"gate_init_2DADC4.*",
        r"uimode_writer ENTER.*",
    ):
        key.extend(re.findall(pat, t)[:6])
    lines += ["", "## 3. 关键日志", "", "```text"]
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
    a.markdown.write_text("\n".join(lines), encoding="utf-8")
    print(verdict)
    print("wrote", a.markdown)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
