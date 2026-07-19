#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def yn(value: bool) -> str:
    return "是" if value else "否"


def last_int(pattern: str, text: str, base: int = 10) -> int | None:
    vals = re.findall(pattern, text, re.I)
    return int(vals[-1], base) if vals else None


def main() -> int:
    ap = argparse.ArgumentParser(description="Analyze v53 MR_IGNORE-to-host-801 handoff")
    ap.add_argument("log", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--manifest", type=Path)
    ap.add_argument("--audit-json", type=Path)
    args = ap.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8-sig")) if args.manifest and args.manifest.exists() else {}
    audit = json.loads(args.audit_json.read_text(encoding="utf-8-sig")) if args.audit_json and args.audit_json.exists() else {}

    methods = [int(x) for x in re.findall(r"mr_get_method\((\d+)\)", text)]
    alias_ext_base = "method=ext_base_0xD4" in text
    alias_patched = "[JJFB_MRP_ALIAS] patched" in text
    alias_miss = "[JJFB_MRP_ALIAS] patch_miss" in text
    robotol_loaded = "[JJFB_ROBOTOL_LOAD]" in text
    err_3006 = bool(re.search(r"(?:err,?\s*code=|err code=)3006", text, re.I))
    key_error = "cann`t find sdk key!" in text or "can't find sdk key!" in text

    raw_rets = re.findall(r"bridge_dsm_mr_start_dsm(?:\s+ret=|\([^\n]*\):\s*)(0x[0-9a-f]+)", text, re.I)
    raw_ret = int(raw_rets[-1], 16) if raw_rets else None
    recover = "[JJFB_START_HANDOFF]" in text and "action=run_host_801_recovery" in text
    stop_before = "[JJFB_START_HANDOFF]" in text and "action=stop_before_host_801" in text
    guard_run = "[JJFB_801_GUARD] robotol_loaded=1" in text and "action=run_host_801" in text
    guard_skip = "[JJFB_801_GUARD] robotol_loaded=0" in text and "action=skip_host_801" in text

    version_ret = last_int(r"\[JJFB_801\] host version\(6\) ret=(-?\d+)", text)
    appinfo_ret = last_int(r"\[JJFB_801\] host appInfo\(8\) ret=(-?\d+)", text)
    init_ret = last_int(r"\[JJFB_801\] host mrc_init\(0\) ret=(-?\d+)", text)
    timer_running = "ARM robotol timer" in text and "RUNNING=1" in text
    timer_code2 = bool(re.search(r"ext_call code=2", text))
    miss_count = text.count("[JJFB_FILEOPEN_MISS]")

    start_ok = 1514 in methods
    loader_ok = 219 in methods
    robotol_blob = 161178 in methods
    postconditions = alias_patched and robotol_blob and robotol_loaded

    if key_error or not loader_ok:
        verdict = "回退到 SDK/第一阶段；先核对 canonical 48 字节 key。"
    elif not alias_patched or alias_miss:
        verdict = "v53 br_log 别名 hook 未完成；检查 ext_base+0xD4 日志和编译覆盖。"
    elif not robotol_blob or not robotol_loaded:
        verdict = "别名后仍未完成 robotol 解包/注册；尚未到 start handoff 阶段。"
    elif raw_ret == 1 and not recover:
        verdict = "已确认 MR_IGNORE(1)，但恢复门未放行；检查环境变量与三项后置条件。"
    elif recover and not guard_run:
        verdict = "MR_IGNORE 已被识别为可恢复交接，但 801 guard 未进入 run 分支。"
    elif init_ret is None:
        verdict = "host 6→8→0 未完整执行；查看同步 guest helper/P/ER_RW 的日志。"
    elif init_ret != 0:
        verdict = "别名和 start handoff 已完成；下一 blocker 位于 robotol host 801 输入、extChunk 或初始化内部。"
    elif init_ret == 0 and timer_running:
        verdict = "v53 目标完成：原始 start 返回 MR_IGNORE 后，host 6→8→0 恢复成功并启动 robotol timer。"
    elif init_ret == 0:
        verdict = "robotol mrc_init=0；只剩 timer/event 驱动是否持续运行需要确认。"
    elif raw_ret == 0 and postconditions:
        verdict = "start_dsm 已自然返回 0；host 正常交接路径可继续，MR_IGNORE 恢复分支未必需要。"
    else:
        verdict = "证据不完整，保留完整日志继续定位。"

    out = [
        "# v53 Start Handoff Recovery 运行结果",
        "",
        f"- 日志：`{args.log}`",
        f"- 总行数：{len(lines)}",
        f"- FILEOPEN_MISS：{miss_count}",
        f"- 原始 MRP SHA256：`{audit.get('mrp_sha256', '未提供')}`",
        f"- SDK key SHA256：`{manifest.get('key_sha256', '未提供')}`",
        "",
        "## 1. Loader 与别名",
        "",
        f"- SDK 报错：{yn(key_error)}",
        f"- `start.mr` 1514：{yn(start_ok)}",
        f"- `mrc_loader.ext` 219：{yn(loader_ok)}",
        f"- alias `ext_base+0xD4`：{yn(alias_ext_base)}",
        f"- alias patched：{yn(alias_patched)}",
        f"- `err code=3006`：{yn(err_3006)}",
        f"- `robotol.ext` 161178：{yn(robotol_blob)}",
        f"- robotol helper：{yn(robotol_loaded)}",
        "",
        "## 2. Start handoff / 801",
        "",
        f"- `start_dsm` 原始返回：{('0x%X' % raw_ret) if raw_ret is not None else '未捕获'}",
        f"- MR_IGNORE 恢复门：{yn(recover)}",
        f"- stop_before_host_801：{yn(stop_before)}",
        f"- 801 guard 放行：{yn(guard_run)}",
        f"- 801 guard 阻止误判：{yn(guard_skip)}",
        f"- host version(6)：{version_ret if version_ret is not None else '未调用/未捕获'}",
        f"- host appInfo(8)：{appinfo_ret if appinfo_ret is not None else '未调用/未捕获'}",
        f"- host mrc_init(0)：{init_ret if init_ret is not None else '未调用/未捕获'}",
        f"- robotol timer RUNNING：{yn(timer_running)}",
        f"- timer code=2：{yn(timer_code2)}",
        "",
        "## 3. 顺序",
        "",
        f"- `mr_get_method`：`{methods}`",
        "",
        "## 4. 自动结论",
        "",
        f"- {verdict}",
        "",
        "## 5. 关键日志",
        "",
        "```text",
    ]
    pats = (
        "JJFB_GUEST_EXT", "JJFB_MRP_ALIAS", "JJFB_ROBOTOL_LOAD",
        "JJFB_START_HANDOFF", "JJFB_801_GUARD", "JJFB_801",
        "mr_get_method(", "_mr_c_function_new(", "bridge_dsm_mr_start_dsm",
        "3006", "robotol timer", "FILEOPEN_MISS", "mr_exit",
    )
    selected = [line for line in lines if any(p.lower() in line.lower() for p in pats)]
    out.extend(selected[:320] or ["（无）"])
    out += ["```", ""]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(out), encoding="utf-8")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
