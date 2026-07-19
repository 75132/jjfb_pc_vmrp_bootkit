#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def yes(v: bool) -> str:
    return "是" if v else "否"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--manifest", type=Path)
    args = ap.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    manifest = {}
    if args.manifest and args.manifest.exists():
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))

    method_sizes = [int(x) for x in re.findall(r"mr_get_method\((\d+)\)", text)]
    opens = []
    open_re = re.compile(r'\[JJFB_FILEOPEN\] guest="([^"]*)" host="([^"]*)" ok=1')
    for line in lines:
        m = open_re.search(line)
        if m:
            opens.append((m.group(1), m.group(2)))
    sdk_hosts = [host for guest, host in opens if guest.endswith("sdk_key.dat")]
    mrp_hosts = [host for guest, host in opens if guest.endswith("jjfb.mrp")]

    key_error = "cann`t find sdk key!" in text
    canonical_start = 1514 in method_sizes
    # Original jjfb.mrp mrc_loader.ext unpack size is 219; patched dsm_gm used 217.
    loader_sizes = [s for s in method_sizes if s in (217, 219)]
    loader_blob = bool(loader_sizes)
    loader_size = loader_sizes[0] if loader_sizes else None
    robotol_blob = 161178 in method_sizes
    cfunction_miss = (
        'read file  "cfunction.ext" err' in text
        or ('"cfunction.ext"' in text and "code=3006" in text)
    )
    robotol_line = any(
        "_mr_c_function_new(" in line and "mr_c_function_P:" in line and
        not re.search(r"_mr_c_function_new\(002AEB", line)
        for line in lines
    )
    bridge_ok = bool(re.search(r"bridge_dsm_mr_start_dsm ret=0x0", text))
    init_match = re.findall(r"\[JJFB_801\] host mrc_init\(0\) ret=(-?\d+)", text)
    init_ret = int(init_match[-1]) if init_match else None
    timer_running = "ARM robotol timer" in text and "RUNNING=1" in text
    miss_count = text.count("[JJFB_FILEOPEN_MISS]")

    if key_error or not loader_blob:
        verdict = "sdk key 仍未通过；优先核对实际 host 路径和 48 字节内容。"
    elif loader_blob and not robotol_blob and cfunction_miss:
        verdict = (
            "sdk key 已通过，mrc_loader.ext 已装载；二阶段在请求 "
            "`cfunction.ext` 时失败（MRP 内实际名为 `robotol.ext`）。"
        )
    elif loader_blob and not robotol_blob:
        verdict = "sdk key 已通过，blocker 已推进到 mrc_loader.ext 的二阶段装载。"
    elif robotol_blob and init_ret != 0:
        verdict = "robotol.ext 已装载；下一 blocker 是 robotol mrc_init/host 801 契约。"
    elif robotol_blob and init_ret == 0:
        verdict = "原始 start.mr → mrc_loader.ext → robotol.ext 链已自然恢复。"
    else:
        verdict = "证据不足，保留原始日志继续定位。"

    key_hex = manifest.get("key_hex", "（manifest 不可用）")
    key_sha = manifest.get("key_sha256", "（manifest 不可用）")
    key_len = manifest.get("key_len", "（manifest 不可用）")

    out = []
    out.append("# v51 Valid SDK Key 运行结果")
    out.append("")
    out.append(f"- 日志：`{args.log}`")
    out.append(f"- 总行数：{len(lines)}")
    out.append(f"- FILEOPEN_MISS：{miss_count}")
    out.append(f"- 部署 key：{key_len} 字节，SHA256 `{key_sha}`")
    out.append("")
    out.append("## 1. SDK key 证据")
    out.append("")
    out.append(f"- `cann`t find sdk key!`：{yes(key_error)}")
    out.append(f"- canonical `start.mr` 压缩长度 1514：{yes(canonical_start)}")
    out.append(
        f"- `mrc_loader.ext` 解包 217/219：{yes(loader_blob)}"
        + (f"（实际={loader_size}）" if loader_size is not None else "")
    )
    out.append(f"- `robotol.ext` 解包 161178：{yes(robotol_blob)}")
    out.append(f"- `cfunction.ext` 读取失败 code=3006：{yes(cfunction_miss)}")
    out.append(f"- robotol helper 日志：{yes(robotol_line)}")
    out.append(f"- `bridge_dsm_mr_start_dsm ret=0x0`：{yes(bridge_ok)}")
    out.append(f"- host `mrc_init(0)`：{init_ret if init_ret is not None else '未捕获'}")
    out.append(f"- robotol timer RUNNING：{yes(timer_running)}")
    out.append("")
    out.append("实际打开的 sdk_key.dat host：")
    out.append("")
    out.append("```text")
    out.extend(sdk_hosts or ["（未捕获）"])
    out.append("```")
    out.append("")
    out.append("部署 key 十六进制：")
    out.append("")
    out.append("```text")
    out.append(key_hex)
    out.append("```")
    out.append("")
    out.append("## 2. MRP 打开与解包顺序")
    out.append("")
    out.append(f"`mr_get_method`：`{method_sizes}`")
    out.append("")
    out.append("MRP host 路径：")
    out.append("")
    out.append("```text")
    out.extend(mrp_hosts or ["（未捕获）"])
    out.append("```")
    out.append("")
    out.append("## 3. 自动结论")
    out.append("")
    out.append(f"- {verdict}")
    out.append("")
    out.append("## 4. 关键日志")
    out.append("")
    out.append("```text")
    pats = (
        "JJFB_SDK_KEY", "sdk key", "mr_get_method(", "_mr_c_function_new(",
        "cfunction.ext", "bridge_dsm_mr_start_dsm ret", "JJFB_801",
        "robotol timer", "FILEOPEN_MISS", "mr_exit"
    )
    selected = [line for line in lines if any(p in line for p in pats)]
    out.extend(selected[:220] or ["（无）"])
    out.append("```")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
