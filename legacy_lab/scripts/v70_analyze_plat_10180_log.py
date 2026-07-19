#!/usr/bin/env python3
"""Analyze v70: plat 0x10180/0x10130 + B71/B70 progress after Path A."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def count(t: str, pat: str) -> int:
    return len(re.findall(pat, t))


def main() -> int:
    log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "logs" / "v70_plat_10180_stdout.txt"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "reports" / "v70_plat_10180_run_result.md"
    t = log.read_text(encoding="utf-8", errors="replace")

    c = {
        "v70_banner": count(t, r"\[JJFB_V70_PLAT\]"),
        "h10180": count(t, r"\[JJFB_V70_10180\]"),
        "h10130": count(t, r"\[JJFB_V70_10130\]"),
        "un_10180": count(t, r"unhandled plat/msg 0x10180"),
        "un_10130": count(t, r"unhandled plat/msg 0x10130"),
        "leave_2fc26c": count(t, r"leave_2FC26C"),
        "after_ret": count(t, r"after_2E2520.*EXPECT_DEQUEUE"),
        "b71_set": count(t, r"strb_B71=1"),
        "b70_set": count(t, r"strb_B70_in_2FEBBC"),
        "entry_2febbc": count(t, r"entry_2FEBBC"),
        "fail_b71": count(t, r"fail_B71_eq_0"),
        "pass_305eb8": count(t, r"entry_305EB8(?!.*fail_)"),
        "bl_2fc03c": count(t, r"BL_2FC03C_site|init_2FC03C"),
        # Real force only (not "NO FORCE" / "no_FORCE" contract banners)
        "force_ui": count(t, r"\[JJFB_FORCE\].*ui_mode|FORCE state=0x45|JJFB_FORCE_UI_MODE=[1-9]"),
    }

    if c["force_ui"]:
        verdict = "检测到 FORCE ui_mode（违规）。"
        next_task = "关掉 FORCE；回到平台 ABI。"
    elif c["un_10180"] or c["un_10130"]:
        verdict = "10180/10130 仍 unhandled（未编进本次 binary？）。"
        next_task = "确认 bridge.c 已编译进 main.exe。"
    elif c["b70_set"] or c["entry_2febbc"] or c["bl_2fc03c"]:
        verdict = "已到 2FEBBC/B70/2FC03C。"
        next_task = "跟自然 splash / 游戏自跑；收敛 PROBE。"
    elif c["b71_set"] and c["fail_b71"] == 0:
        verdict = "B71 已置位且 305EB8 不再 fail_B71。"
        next_task = "观察后续 2DADC4 / B70。"
    elif c["b71_set"]:
        verdict = "见 B71=1 写者，但仍有 fail_B71（可能被 2FE82C 清掉）。"
        next_task = "跟 2FE82C 清零时序 vs 305EB8。"
    elif c["h10180"] and c["leave_2fc26c"] and c["fail_b71"]:
        verdict = "10180 已生效且 Path A 完成，但 B71 仍恒 0。"
        next_task = "B58 仍空 → 30ED2C 未跑；下一刀：第二包填 B58 或自然网络包，勿 FORCE B71。"
    elif c["h10180"]:
        verdict = "10180 已返回 blob；Path A 进度需核对。"
        next_task = "确认 leave_2FC26C / dequeue。"
    else:
        verdict = "未见 V70_10180 日志。"
        next_task = "确认 RUN 未 SkipBuild 且 overlay 在。"

    lines = [
        "# v70 plat 0x10180 / 0x10130 run result",
        "",
        "## 1. goal",
        "",
        "- 补齐 `0x10180`（2F65BC userinfo blob）与 `0x10130`（大块 alloc notify）",
        "- 避免 unhandled ret=1 污染 ERW；观察是否推进 B71/B70",
        "",
        "## 2. counts",
        "",
    ]
    for k, v in c.items():
        lines.append(f"- `{k}`: {v}")
    lines += [
        "",
        "## 3. conclusion",
        "",
        f"- {verdict}",
        "",
        "## 4. next",
        "",
        f"- {next_task}",
        "",
        "## 5. notes",
        "",
        "- MOUSE_UP→B71 已证伪（会置 134D=2）",
        "- Path A 空 body → 2FC26C；B58 空 → 不进 30ED2C",
        "- 禁止 FORCE B70/B71 / C0 / ui_mode",
        "",
    ]
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines), encoding="utf-8")
    print(verdict)
    print("next:", next_task)
    print("wrote", out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
