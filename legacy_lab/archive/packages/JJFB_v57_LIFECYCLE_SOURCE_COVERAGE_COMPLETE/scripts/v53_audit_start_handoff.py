#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path

MAGIC = 0x4750524D  # MRPG, little-endian


def parse_mrp(path: Path) -> tuple[bytes, dict[str, dict[str, int]]]:
    data = path.read_bytes()
    if len(data) < 240:
        raise ValueError("MRP too small")
    magic, header_end, total_size, index_offset = struct.unpack_from("<4I", data, 0)
    if magic != MAGIC:
        raise ValueError(f"bad MRP magic: 0x{magic:08x}")
    if total_size > len(data):
        raise ValueError(f"declared size {total_size} > file size {len(data)}")
    index_end = header_end + 8
    if not (16 <= index_offset < index_end <= len(data)):
        raise ValueError("invalid index bounds")

    pos = index_offset
    entries: dict[str, dict[str, int]] = {}
    while pos < index_end:
        if pos + 4 > index_end:
            raise ValueError("truncated filename length")
        name_len = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        if name_len < 1 or name_len >= 256 or pos + name_len + 12 > index_end:
            raise ValueError(f"invalid filename length {name_len} at 0x{pos - 4:x}")
        raw_name = data[pos : pos + name_len]
        pos += name_len
        file_offset, file_len, flags = struct.unpack_from("<3I", data, pos)
        pos += 12
        name = raw_name.rstrip(b"\0").decode("latin1")
        if file_offset + file_len > len(data):
            raise ValueError(f"entry outside file: {name}")
        entries[name] = {"offset": file_offset, "compressed_len": file_len, "flags": flags}
    return data, entries


def inflate(data: bytes, entry: dict[str, int]) -> bytes:
    raw = data[entry["offset"] : entry["offset"] + entry["compressed_len"]]
    return zlib.decompress(raw, 31)


def source_contains(path: Path, *needles: str) -> bool:
    if not path.exists():
        return False
    text = path.read_text(encoding="utf-8", errors="replace")
    return all(n in text for n in needles)


def main() -> int:
    ap = argparse.ArgumentParser(description="Audit v53 canonical MRP and host recovery source contract")
    ap.add_argument("mrp", type=Path)
    ap.add_argument("--source-root", type=Path)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--json", dest="json_out", type=Path)
    ap.add_argument("--strict", action="store_true")
    args = ap.parse_args()

    data, entries = parse_mrp(args.mrp)
    required = ("start.mr", "mrc_loader.ext", "robotol.ext")
    missing = [n for n in required if n not in entries]
    loader = inflate(data, entries["mrc_loader.ext"]) if not missing else b""
    literal = b"cfunction.ext\0"
    literal_offset = loader.find(literal)

    result: dict[str, object] = {
        "mrp": str(args.mrp),
        "mrp_size": len(data),
        "mrp_sha256": hashlib.sha256(data).hexdigest(),
        "entry_count": len(entries),
        "missing_required": missing,
        "cfunction_member_present": "cfunction.ext" in entries,
        "start_compressed_len": entries.get("start.mr", {}).get("compressed_len"),
        "mrc_loader_compressed_len": entries.get("mrc_loader.ext", {}).get("compressed_len"),
        "mrc_loader_decompressed_len": len(loader),
        "robotol_compressed_len": entries.get("robotol.ext", {}).get("compressed_len"),
        "robotol_offset": entries.get("robotol.ext", {}).get("offset"),
        "loader_literal_offset": literal_offset,
    }

    checks: dict[str, bool] = {
        "required_members": not missing,
        "cfunction_absent": "cfunction.ext" not in entries,
        "canonical_lengths_1514_219_161178": (
            result["start_compressed_len"] == 1514
            and result["mrc_loader_compressed_len"] == 219
            and result["robotol_compressed_len"] == 161178
        ),
        "loader_decompressed_232": len(loader) == 232,
        "loader_literal_at_ext_base_0xD4": literal_offset == 0xD4,
    }

    if args.source_root:
        bridge = args.source_root / "bridge.c"
        vmrp = args.source_root / "vmrp.c"
        header = args.source_root / "header" / "bridge.h"
        checks.update({
            "br_log_guest_hook_present": source_contains(
                bridge,
                "jjfb_v53_probe_guest_log",
                "method=ext_base_0xD4",
                "source=br_log",
            ),
            "robotol_postcondition_exports_present": source_contains(
                header,
                "jjfb_mrp_alias_applied",
                "jjfb_robotol_ext_loaded",
                "jjfb_robotol_ext_helper",
            ),
            "mr_ignore_recovery_is_gated": source_contains(
                vmrp,
                "JJFB_ACCEPT_START_IGNORE_AFTER_ROBOTOL",
                "ret == MR_IGNORE",
                "jjfb_mrp_alias_applied() && jjfb_robotol_ext_loaded()",
                "action=run_host_801_recovery",
            ),
            "host_contract_order_6_8_0": source_contains(
                vmrp,
                "bridge_dsm_ext_call(uc, 6",
                "bridge_dsm_ext_call(uc, 8",
                "bridge_dsm_ext_call(uc, 0",
            ),
        })
        result["source_root"] = str(args.source_root)

    result["checks"] = checks
    result["strict_ok"] = all(checks.values())

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# v53 Start Handoff Recovery 静态审计",
        "",
        f"- MRP：`{args.mrp}`",
        f"- SHA-256：`{result['mrp_sha256']}`",
        f"- 文件大小：{len(data)} 字节",
        f"- 索引成员：{len(entries)}",
        "",
        "## 1. Canonical MRP 事实",
        "",
        f"- `start.mr` 压缩长度：{result['start_compressed_len']}",
        f"- `mrc_loader.ext` 压缩/解压长度：{result['mrc_loader_compressed_len']} / {len(loader)}",
        f"- `robotol.ext` 压缩长度：{result['robotol_compressed_len']}",
        f"- `cfunction.ext` 成员存在：{'是' if result['cfunction_member_present'] else '否'}",
        f"- loader 请求字面量偏移：`0x{literal_offset:X}`" if literal_offset >= 0 else "- loader 请求字面量：未找到",
        "",
        "## 2. v53 Host 契约",
        "",
        "- 别名触发点：guest `br_log` 观察到 mrc_loader 的 `--- ext:` 与 `_mr_c_function_new(...)`。",
        "- 内存改写点：`ext_base + 0xD4`，仅把 `cfunction.ext` 改为 `robotol.ext`。",
        "- `start_dsm` 原始返回仍保留；只有 `MR_IGNORE(1) + alias_applied + robotol_loaded` 同时成立，才执行 host `6 → 8 → 0` 恢复。",
        "- 不修改 `jjfb.mrp` 文件或索引。",
        "",
        "## 3. 严格检查",
        "",
    ]
    lines.extend(f"- {name}: {'通过' if ok else '失败'}" for name, ok in checks.items())
    lines += ["", f"**总判定：{'通过' if result['strict_ok'] else '失败'}**", ""]
    text = "\n".join(lines)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    print(text)
    return 0 if (result["strict_ok"] or not args.strict) else 2


if __name__ == "__main__":
    raise SystemExit(main())
