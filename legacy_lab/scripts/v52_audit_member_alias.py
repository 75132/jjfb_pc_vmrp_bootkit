#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path

MAGIC = 0x4750524D  # MRPG little-endian


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
            raise ValueError(f"invalid filename length {name_len} at 0x{pos-4:x}")
        raw_name = data[pos : pos + name_len]
        pos += name_len
        file_offset, file_len, flags = struct.unpack_from("<3I", data, pos)
        pos += 12
        name = raw_name.rstrip(b"\0").decode("latin1")
        if file_offset + file_len > len(data):
            raise ValueError(f"entry outside file: {name}")
        entries[name] = {
            "offset": file_offset,
            "compressed_len": file_len,
            "flags": flags,
        }
    return data, entries


def gunzip_member(data: bytes, entry: dict[str, int]) -> bytes:
    raw = data[entry["offset"] : entry["offset"] + entry["compressed_len"]]
    return zlib.decompress(raw, 31)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mrp", type=Path)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--json", dest="json_out", type=Path)
    ap.add_argument("--strict", action="store_true")
    args = ap.parse_args()

    data, entries = parse_mrp(args.mrp)
    required = ("start.mr", "mrc_loader.ext", "robotol.ext")
    missing = [name for name in required if name not in entries]
    loader = gunzip_member(data, entries["mrc_loader.ext"]) if not missing else b""
    literal = b"cfunction.ext\0"
    literal_offset = loader.find(literal)
    entry_offset = 8
    helper_delta = literal_offset - entry_offset if literal_offset >= 0 else None

    result = {
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
        "loader_request_literal": "cfunction.ext",
        "loader_literal_offset": literal_offset,
        "loader_entry_offset": entry_offset,
        "literal_from_helper_delta": helper_delta,
        "expected_delta_hex": f"0x{helper_delta:X}" if helper_delta is not None else None,
    }

    checks = {
        "required_members": not missing,
        "cfunction_absent": "cfunction.ext" not in entries,
        "canonical_lengths": (
            result["start_compressed_len"] == 1514
            and result["mrc_loader_compressed_len"] == 219
            and result["robotol_compressed_len"] == 161178
        ),
        "loader_decompressed_232": len(loader) == 232,
        "literal_at_0xD4": literal_offset == 0xD4,
        "helper_delta_0xCC": helper_delta == 0xCC,
    }
    result["checks"] = checks
    result["strict_ok"] = all(checks.values())

    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = [
        "# v52 MRP 内成员别名静态审计",
        "",
        f"- MRP：`{args.mrp}`",
        f"- SHA-256：`{result['mrp_sha256']}`",
        f"- 文件大小：{len(data)} 字节",
        f"- 索引成员：{len(entries)}",
        "",
        "## 1. 原始成员事实",
        "",
        f"- `start.mr` 压缩长度：{result['start_compressed_len']}",
        f"- `mrc_loader.ext` 压缩长度：{result['mrc_loader_compressed_len']}",
        f"- `mrc_loader.ext` 解压长度：{len(loader)}",
        f"- `robotol.ext`：offset={result['robotol_offset']}，压缩长度={result['robotol_compressed_len']}",
        f"- `cfunction.ext` 成员存在：{'是' if result['cfunction_member_present'] else '否'}",
        "",
        "## 2. v52 别名落点",
        "",
        f"- loader 中 `cfunction.ext\\0` 偏移：`0x{literal_offset:X}`" if literal_offset >= 0 else "- 未找到 loader 请求字面量",
        f"- `mr_c_function_load` 入口偏移：`0x{entry_offset:X}`",
        f"- helper 到请求字面量的增量：`0x{helper_delta:X}`" if helper_delta is not None else "- helper 增量不可计算",
        "",
        "因此 host 在第二阶段 `_mr_c_function_new` 回调中，仅把 guest 内存中的请求字面量：",
        "",
        "```text",
        "cfunction.ext -> robotol.ext",
        "```",
        "",
        "原始 `jjfb.mrp` 文件和索引均不修改。",
        "",
        "## 3. 严格检查",
        "",
    ]
    for name, ok in checks.items():
        lines.append(f"- {name}: {'通过' if ok else '失败'}")
    lines += ["", f"**总判定：{'通过' if result['strict_ok'] else '失败'}**", ""]

    text = "\n".join(lines)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    print(text)
    return 0 if (result["strict_ok"] or not args.strict) else 2


if __name__ == "__main__":
    raise SystemExit(main())
