#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def yn(v: bool) -> str:
    return "是" if v else "否"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--manifest", type=Path)
    ap.add_argument("--audit-json", type=Path)
    args = ap.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    manifest = {}
    audit = {}
    if args.manifest and args.manifest.exists():
        manifest = json.loads(args.manifest.read_text(encoding="utf-8-sig"))
    if args.audit_json and args.audit_json.exists():
        audit = json.loads(args.audit_json.read_text(encoding="utf-8-sig"))

    methods = [int(x) for x in re.findall(r"mr_get_method\((\d+)\)", text)]
    helper_rows = re.findall(r"\[JJFB_LOADER\] _mr_c_function_new #(\d+) helper=(?:0x)?([0-9A-Fa-f]+|\(nil\)) len=(\d+)", text)
    alias_enabled = "[JJFB_MRP_ALIAS] enabled=1" in text
    alias_patched = "[JJFB_MRP_ALIAS] patched" in text
    alias_patch_miss = "[JJFB_MRP_ALIAS] patch_miss" in text
    robotol_registered = "[JJFB_ROBOTOL_LOAD]" in text
    guard_run = "[JJFB_801_GUARD] robotol_loaded=1" in text and "action=run_host_801" in text
    guard_skip = "[JJFB_801_GUARD] robotol_loaded=0" in text and "action=skip_host_801" in text
    err_3006 = bool(re.search(r"(?:err,?\s*code=|err code=)3006", text, re.I))
    mr_exit_after_loader = "mr_exit" in text.lower()
    key_error = "cann`t find sdk key!" in text or "can't find sdk key!" in text
    init_values = [int(x) for x in re.findall(r"\[JJFB_801\] host mrc_init\(0\) ret=(-?\d+)", text)]
    init_ret = init_values[-1] if init_values else None
    timer_running = "ARM robotol timer" in text and "RUNNING=1" in text
    miss_count = text.count("[JJFB_FILEOPEN_MISS]")

    start_ok = 1514 in methods
    loader_ok = 219 in methods
    robotol_blob = 161178 in methods
    start_dsm_rets = re.findall(r"bridge_dsm_mr_start_dsm ret=(0x[0-9A-Fa-f]+|\d+)", text)
    start_dsm_ret = start_dsm_rets[-1] if start_dsm_rets else None
    start_dsm_ok = start_dsm_ret in ("0x0", "0")

    if key_error or not loader_ok:
        verdict = "SDK/第一阶段发生回退；先核对 v51 key 与 canonical 路径。"
    elif not alias_enabled:
        verdict = "v52 别名环境变量未启用。"
    elif alias_patch_miss:
        verdict = "已进入 mrc_loader，但 host 未在预期/邻近位置找到 cfunction.ext 字面量。"
    elif not alias_patched:
        verdict = "别名补丁没有留下应用证据；检查是否编译并运行了 v52 main.exe。"
    elif not robotol_blob:
        verdict = "别名已写入，但尚未解出 161178；继续看 3006、mr_exit 和 loader 调用顺序。"
    elif not robotol_registered:
        verdict = "robotol 数据已解包，但没有新的 _mr_c_function_new；blocker 在 robotol EXT 注册入口。"
    elif robotol_registered and not start_dsm_ok:
        verdict = (
            f"别名阶段完成（161178 + ROBOTOL_LOAD），但 start_dsm ret={start_dsm_ret}；"
            "下一轮审计 robotol `_strCom 800/801` 返回值（host 801 因 start_dsm 失败未执行）。"
        )
    elif not guard_run:
        verdict = "robotol helper 已注册，但 801 guard 未放行；检查状态同步。"
    elif init_ret != 0:
        verdict = "别名与 robotol 装载已成功；下一 blocker 是 robotol 的 _strCom 800/801 契约。"
    elif init_ret == 0 and timer_running:
        verdict = "start.mr → mrc_loader(219) → alias → robotol(161178) → mrc_init=0 链已恢复。"
    elif init_ret == 0:
        verdict = "robotol mrc_init=0，但 timer 未确认 RUNNING；下一步只查 timer/event 驱动。"
    else:
        verdict = "证据不完整，保留日志继续定位。"

    out = [
        "# v52 MRP Member Alias 运行结果",
        "",
        f"- 日志：`{args.log}`",
        f"- 总行数：{len(lines)}",
        f"- FILEOPEN_MISS：{miss_count}",
        f"- 原始 MRP SHA256：`{audit.get('mrp_sha256', '未提供')}`",
        f"- SDK key SHA256：`{manifest.get('key_sha256', '未提供')}`",
        "",
        "## 1. 链路判据",
        "",
        f"- SDK 报错：{yn(key_error)}",
        f"- `start.mr` 1514：{yn(start_ok)}",
        f"- `mrc_loader.ext` 219：{yn(loader_ok)}",
        f"- alias enabled：{yn(alias_enabled)}",
        f"- alias patched：{yn(alias_patched)}",
        f"- alias patch miss：{yn(alias_patch_miss)}",
        f"- `err code=3006`：{yn(err_3006)}",
        f"- `robotol.ext` 161178：{yn(robotol_blob)}",
        f"- 新 robotol helper 注册：{yn(robotol_registered)}",
        f"- `bridge_dsm_mr_start_dsm` ret：{start_dsm_ret if start_dsm_ret is not None else '未捕获'}",
        f"- 801 guard 放行：{yn(guard_run)}",
        f"- 801 guard 阻止 loader 误判：{yn(guard_skip)}",
        f"- host `mrc_init(0)`：{init_ret if init_ret is not None else '未调用/未捕获'}",
        f"- robotol timer RUNNING：{yn(timer_running)}",
        "",
        "## 2. 解包与 helper 顺序",
        "",
        f"- `mr_get_method`：`{methods}`",
        f"- `_mr_c_function_new`：`{helper_rows}`",
        "",
        "## 3. 自动结论",
        "",
        f"- {verdict}",
        "",
        "## 4. 关键日志",
        "",
        "```text",
    ]
    pats = (
        "JJFB_MRP_ALIAS", "JJFB_ROBOTOL_LOAD", "JJFB_801_GUARD",
        "mr_get_method(", "_mr_c_function_new #", "read file", "3006",
        "JJFB_801", "robotol timer", "mr_exit", "FILEOPEN_MISS",
        "bridge_dsm_mr_start_dsm ret", "--- r9:",
    )
    selected = [line for line in lines if any(p.lower() in line.lower() for p in pats)]
    out.extend(selected[:260] or ["（无）"])
    out += ["```", ""]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(out), encoding="utf-8")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
