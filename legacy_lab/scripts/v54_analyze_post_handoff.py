#!/usr/bin/env python3
"""Analyze v54 post-handoff natural game-self logs (no FORCE ui_mode)."""
from __future__ import annotations

import argparse
import collections
import re
import sys
from pathlib import Path


def yn(v: bool) -> str:
    return "是" if v else "否"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--markdown", default="")
    args = ap.parse_args()
    path = Path(args.log)
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    tags = collections.Counter(re.findall(r"\[JJFB_[A-Z0-9_]+\]", text))
    guests = collections.Counter(re.findall(r'guest="([^"]+)"', text))
    misses = [l for l in lines if "FILEOPEN_MISS" in l]
    force = [l for l in lines if "FORCE state/ui_mode" in l]
    natural = [l for l in lines if "NO FORCE ui_mode" in l or "natural_mode=1" in l]
    ui_modes = collections.Counter(re.findall(r"ui_mode=(0x[0-9A-Fa-f]+)", text))
    states = collections.Counter(re.findall(r"state=(\d+)\(0x[0-9A-Fa-f]+\)", text))
    methods = [int(x) for x in re.findall(r"mr_get_method\((\d+)\)", text)]
    net = [l for l in lines if any(k in l for k in ("JJFB_NET", "socket", "connect", "http", "vdload"))]
    strcom = [l for l in lines if "JJFB_STRCOM" in l or "_strCom" in l]
    game_self = [l for l in lines if "[JJFB_GAME_SELF]" in l]

    handoff_ok = (
        "action=run_host_801_recovery" in text
        and "host mrc_init(0) ret=0" in text
        and "RUNNING=1" in text
    )
    alias_ok = "method=ext_base_0xD4" in text and "[JJFB_ROBOTOL_LOAD]" in text
    no_force = bool(natural) and not force
    natural_045 = "0x45" in ui_modes and no_force

    if not handoff_ok:
        verdict = "回退：start handoff 未完成；先确认 v53 别名+801 仍在。"
        blocker = "start handoff incomplete"
        next_task = "重新跑通 v53 handoff，再开 v54 natural。"
    elif force:
        verdict = "失败：仍出现 FORCE ui_mode=0x45；GWY natural 门未生效。"
        blocker = "FORCE ui_mode still active"
        next_task = "确认 bridge.c GWY skip_force 与 JJFB_FORCE_UI_MODE=0。"
    elif not no_force:
        verdict = "未观察到 NO FORCE / natural_mode 日志；可能未跑到 tick=10。"
        blocker = "natural_mode log missing"
        next_task = "加长运行秒数或检查 timer dispatch。"
    elif natural_045:
        verdict = "成功：无 FORCE，且游戏自行进入 ui_mode=0x45；继续观察网络/资源。"
        blocker = "none for FORCE; observe game-self net/resources"
        next_task = "审计自然 FILEOPEN/_strCom/网络请求，不回 UI gate。"
    elif ui_modes and set(ui_modes) <= {"0x0"}:
        verdict = "成功关闭 FORCE；ui_mode 仍停在 0；下一 blocker 是自然推进事件/平台回调。"
        blocker = "ui_mode stuck at 0 without FORCE"
        next_task = "定位谁本应写 ERW state/ui_mode（事件、_strCom、网络），禁止 FORCE。"
    else:
        verdict = "FORCE 已关；记录自然状态分布后继续事件/网络审计。"
        blocker = "classify natural state progression"
        next_task = "扩展 JJFB_GAME_SELF / FILEOPEN / NET 证据。"

    key_lines = []
    for pat in (
        r"mr_get_method\(\d+\)",
        r"\[JJFB_MRP_ALIAS\].*",
        r"\[JJFB_ROBOTOL_LOAD\].*",
        r"bridge_dsm_mr_start_dsm.*",
        r"\[JJFB_START_HANDOFF\].*",
        r"\[JJFB_801_GUARD\].*",
        r"\[JJFB_801\] host .*",
        r"\[JJFB_SEND\] ARM robotol timer.*",
        r"\[JJFB_GAME_SELF\].*",
        r"\[JJFB_FIRST_SCREEN\].*",
    ):
        for m in re.finditer(pat, text):
            key_lines.append(m.group(0))
            if len(key_lines) >= 40:
                break
        if len(key_lines) >= 40:
            break

    report = []
    report.append("# v54 Natural Game-Self 运行结果")
    report.append("")
    report.append(f"- 日志：`{path}`")
    report.append(f"- 总行数：{len(lines)}")
    report.append(f"- FILEOPEN_MISS：{len(misses)}")
    report.append("")
    report.append("## 1. 目标")
    report.append("")
    report.append("- 保留 v53：别名 hook + MR_IGNORE 后置 host 801。")
    report.append("- 关闭默认 FORCE ui_mode=0x45，观察原始游戏自然推进。")
    report.append("")
    report.append("## 2. Handoff / Natural 门")
    report.append("")
    report.append(f"- alias+robotol：{yn(alias_ok)}")
    report.append(f"- handoff 801+timer：{yn(handoff_ok)}")
    report.append(f"- NO FORCE / natural_mode：{yn(no_force)}")
    report.append(f"- FORCE 0x45 出现：{yn(bool(force))}")
    report.append(f"- `mr_get_method`：`{methods}`")
    report.append(f"- ui_mode 分布：`{dict(ui_modes)}`")
    report.append(f"- state 分布：`{dict(states)}`")
    report.append("")
    report.append("## 3. GAME_SELF 日志")
    report.append("")
    if game_self:
        report.append("```text")
        report.extend(game_self[:20])
        report.append("```")
    else:
        report.append("- (无)")
    report.append("")
    report.append("## 4. FILEOPEN guests (top 20)")
    report.append("")
    for k, v in guests.most_common(20):
        report.append(f"- {v} `{k}`")
    if not guests:
        report.append("- (无 guest= 记录)")
    report.append("")
    report.append("## 5. 网络 / _strCom")
    report.append("")
    if net:
        report.append("```text")
        report.extend(net[:20])
        report.append("```")
    else:
        report.append("- 网络相关：无")
    if strcom:
        report.append("```text")
        report.extend(strcom[:20])
        report.append("```")
    else:
        report.append("- _strCom：无")
    report.append("")
    report.append("## 6. Tag counts (top 20)")
    report.append("")
    for k, v in tags.most_common(20):
        report.append(f"- {v} `{k}`")
    report.append("")
    report.append("## 7. 关键日志")
    report.append("")
    report.append("```text")
    report.extend(key_lines[:40] if key_lines else ["(none)"])
    report.append("```")
    report.append("")
    report.append("## 8. 结论")
    report.append("")
    report.append(f"- {verdict}")
    report.append("")
    report.append("## 9. 当前 blocker")
    report.append("")
    report.append(f"- {blocker}")
    report.append("")
    report.append("## 10. 下一步最小任务")
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
    sys.exit(main())
