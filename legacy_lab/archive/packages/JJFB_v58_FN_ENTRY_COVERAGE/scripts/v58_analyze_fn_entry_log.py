#!/usr/bin/env python3
"""Analyze v58 fn-entry / BL-site coverage (no inject)."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def count(txt: str, pat: str) -> int:
    return len(re.findall(pat, txt, re.M))


def max_family_n(txt: str) -> int:
    ns = [int(x) for x in re.findall(r"\[JJFB_V56_FAMILY\] dispatch #(\d+)", txt)]
    return max(ns) if ns else 0


def tag_counts(txt: str) -> dict[str, int]:
    out: dict[str, int] = {}
    for m in re.finditer(r"\[JJFB_V58_FN\] (\S+) #(\d+)", txt):
        tag, n = m.group(1), int(m.group(2))
        out[tag] = max(out.get(tag, 0), n)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--markdown", type=Path, required=True)
    a = ap.parse_args()
    t = a.log.read_text(encoding="utf-8", errors="replace")
    tags = tag_counts(t)

    vals = {
        "v58_installed": count(t, r"\[JJFB_V58_FN\] coverage installed"),
        "v58_hits": count(t, r"\[JJFB_V58_FN\] (?!coverage)"),
        "prep": count(t, r"\[JJFB_V57_SRC\] call_2F5390_prep"),
        "bl_reg": count(t, r"\[JJFB_V57_SRC\] BL_3054A4"),
        "movs": count(t, r"\[JJFB_V57_SRC\] movs_imm_C0"),
        "cmp": count(t, r"\[JJFB_V57_SRC\] cmp_r0_C0"),
        "family": max_family_n(t),
        "family_c0": count(t, r"\[JJFB_V56_FAMILY\].*TARGETS_2FEBBC"),
        "writer": count(t, r"uimode_writer ENTER"),
        "force": count(t, r"FORCE state/ui_mode"),
        "natural": count(t, r"natural_mode=1"),
    }

    hit_fns = [k for k, v in tags.items() if v > 0 and k.startswith("fn_")]
    hit_bls = [k for k, v in tags.items() if v > 0 and k.startswith("BL_")]

    if vals["force"]:
        verdict = "出现 FORCE；本轮作废。"
        next_task = "确认 JJFB_FORCE_UI_MODE=0。"
    elif vals["prep"] or vals["bl_reg"] or vals["movs"]:
        verdict = "已进入 v57 生产者；继续追到 2DADC4/writer。"
        next_task = "单步从命中 fn 到 2F5390/C0。"
    elif hit_fns or hit_bls:
        verdict = (
            f"进入了上层 fn/BL（{', '.join(sorted(tags.keys()))}），"
            "但未到达 v57 的 2F5390/C0 站点；查分支条件。"
        )
        next_task = "对命中函数做条件分支静态/动态对照。"
    elif vals["v58_installed"]:
        verdict = "v58 上层 fn/BL 全部未进入；注册/C0 链的入口函数从未被调用。"
        next_task = (
            "查谁应以跳表/间接调用进入 0x304AEC / 0x2E8C00 / 0x2DB044 / 0x2EB770；"
            "对照 GWY startGame 后缺哪次 plat/event。"
        )
    else:
        verdict = "V58 coverage 未安装。"
        next_task = "检查 bridge 构建。"

    samples = [
        l
        for l in t.splitlines()
        if any(
            k in l
            for k in (
                "JJFB_V58_FN",
                "JJFB_V57_SRC",
                "JJFB_V56_FAMILY",
                "natural_mode",
                "FORCE state",
                "uimode_writer",
            )
        )
    ][:80]

    tag_rows = "\n".join(f"| `{k}` | {v} |" for k, v in sorted(tags.items())) or "| (none) | 0 |"

    lines = [
        "# v58 Fn-Entry Coverage 运行结果",
        "",
        f"- 日志：`{a.log}`",
        "",
        "## 1. 目标",
        "",
        "- 观察注册/C0 链的上层 fn entry 与 BL site 是否被进入。",
        "- 禁止 FORCE / 注入 C0 / 注入 event 5·12 / host 画 UI。",
        "",
        "## 2. 计数",
        "",
        "| 探针 | 次数 |",
        "|---|---:|",
        f"| V58_FN installed | {vals['v58_installed']} |",
        f"| V58_FN hit lines | {vals['v58_hits']} |",
        f"| call 2F5390 prep | {vals['prep']} |",
        f"| BL 3054A4 | {vals['bl_reg']} |",
        f"| MOVS #0xC0 | {vals['movs']} |",
        f"| CMP #0xC0 | {vals['cmp']} |",
        f"| family (max #) | {vals['family']} |",
        f"| family app=0xC0 | {vals['family_c0']} |",
        f"| writer | {vals['writer']} |",
        f"| FORCE | {vals['force']} |",
        f"| natural_mode | {vals['natural']} |",
        "",
        "## 3. 分 tag 最大序号",
        "",
        "| tag | max # |",
        "|---|---:|",
        tag_rows,
        "",
        "## 4. 关键日志",
        "",
        "```text",
    ]
    lines.extend(samples if samples else ["(none)"])
    lines += [
        "```",
        "",
        "## 5. 结论",
        "",
        f"- {verdict}",
        "",
        "## 6. blocker / 下一步",
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
