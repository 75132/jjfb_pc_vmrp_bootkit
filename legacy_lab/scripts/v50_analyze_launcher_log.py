#!/usr/bin/env python3
"""Summarize a v50 GWY Launcher Mode runtime log into Markdown."""
from __future__ import annotations

import argparse
import re
from collections import Counter
from pathlib import Path

OPEN_RE = re.compile(r'\[JJFB_FILEOPEN\] guest="([^"]*)" host="([^"]*)" ok=1 handle=(-?\d+) mode=(\d+) pc=0x([0-9A-Fa-f]+) lr=0x([0-9A-Fa-f]+)')
MISS_RE = re.compile(r'\[JJFB_FILEOPEN_MISS\] guest="([^"]*)" tried=\["([^"]*)","([^"]*)"\] exists_before=(-?\d+) mode=(\d+) pc=0x([0-9A-Fa-f]+) lr=0x([0-9A-Fa-f]+)')


def grep(lines: list[str], token: str) -> list[str]:
    return [x for x in lines if token in x]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--output", type=Path)
    args = ap.parse_args()
    text = args.log.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    opens = [m.groups() for line in lines if (m := OPEN_RE.search(line))]
    misses = [m.groups() for line in lines if (m := MISS_RE.search(line))]
    miss_counts = Counter(m[0] for m in misses)
    launch = grep(lines, "[JJFB_GWY_LAUNCH]")
    roots = grep(lines, "[JJFB_GWY_ROOT]")
    cfg = grep(lines, "[JJFB_CFG36]")
    loader = [x for x in lines if any(k in x for k in ("[JJFB_LOADER]", "[JJFB_801]", "mrc_loader", "robotol"))]
    network = [x for x in lines if any(k in x for k in ("[JJFB_NET]", "connect", "socket", "21002", "21003"))]

    target_ok = any(g.replace("\\", "/").endswith("gwy/jjfb.mrp") for g, *_ in opens)
    init0 = any("host mrc_init(0) ret=0" in x for x in lines)
    robotol_seen = any("robotol" in x.lower() for x in lines)

    out = [
        "# v50 GWY Launcher Mode 运行结果", "",
        f"- 日志：`{args.log}`",
        f"- 总行数：{len(lines)}",
        f"- FILEOPEN 成功：{len(opens)}",
        f"- FILEOPEN_MISS：{len(misses)}（唯一 guest：{len(miss_counts)}）",
        f"- `gwy/jjfb.mrp` 主机打开成功：{'是' if target_ok else '否/未捕获'}",
        f"- `mrc_init(0)` 返回 0：{'是' if init0 else '否/未捕获'}",
        f"- robotol 相关日志出现：{'是' if robotol_seen else '否'}",
        "",
        "## 1. 启动契约日志", "",
        "```text", *(launch + roots + cfg), "```",
        "",
        "## 2. 路径打开统计", "",
        "| guest | 成功次数 | miss 次数 | 最近 host/尝试路径 |",
        "|---|---:|---:|---|",
    ]
    ok_counts = Counter(g for g, *_ in opens)
    last_host: dict[str, str] = {}
    for g, h, *_ in opens:
        last_host[g] = h
    for g, h, *_ in misses:
        last_host[g] = h
    for g in sorted(set(ok_counts) | set(miss_counts)):
        out.append(f"| `{g}` | {ok_counts[g]} | {miss_counts[g]} | `{last_host.get(g, '')}` |")

    out += ["", "## 3. 首批 miss（最多 80 条）", "", "```text"]
    out += [line for line in lines if "[JJFB_FILEOPEN_MISS]" in line][:80] or ["（无）"]
    out += ["```", "", "## 4. loader / robotol 关键日志（最多 120 条）", "", "```text"]
    out += loader[:120] or ["（无）"]
    out += ["```", "", "## 5. 网络相关日志（最多 80 条）", "", "```text"]
    out += network[:80] or ["（无）"]
    out += ["```", "", "## 6. 自动判定", ""]
    if not target_ok:
        out.append("- **当前首要 blocker：入口文件没有通过 canonical `gwy/jjfb.mrp` 成功打开。先修 root/path，不看 UI。**")
    elif misses:
        first = misses[0][0]
        out.append(f"- 入口已打开；第一个资源 miss 是 `{first}`。下一轮应只修该 guest path 的映射或缺失资源来源。")
    elif not init0:
        out.append("- 文件路径未见 miss，但 loader 初始化未确认成功；下一轮审计 `_strCom 601/800/801` 的实际顺序和返回值。")
    else:
        out.append("- canonical 入口、资源路径和 loader 初始化均已越过；下一 blocker 应由后续原生日志（网络/游戏自身检查）确定，不再回退 UI 伪造路线。")

    dest = args.output or args.log.with_name("v50_gwy_launcher_run_result.md")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(dest)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
