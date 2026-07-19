#!/usr/bin/env python3
"""Generate the v50 static GWY resource-tree and path-mapping reports."""
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path
from typing import Iterable

PURPOSES = {
    "gifs": "GWY 列表、图标和入口图片资源；包含机甲风暴入口图。",
    "jjfbol": "机甲风暴在线版外置资源包；地图、怪物、动作、配置等 companion MRP。",
    "save": "平台/游戏存档与缓存目录；当前包为空，但必须保留可写目录。",
    "sound": "平台或游戏 MIDI 声音资源。",
    "caclobby": "大厅资源。",
    "caclottery": "抽奖/活动资源。",
    "gkdxy": "其他 GWY 游戏资源目录。",
    "mhxx": "其他 GWY 游戏资源目录。",
    "tlbb": "其他 GWY 游戏资源目录。",
    "xyol": "其他 GWY 游戏资源目录。",
}

KEY_HOST = [
    "jjfb.mrp", "cfg.bin", "gbrwcore.mrp", "gbrwshell.mrp", "gamelist.mrp",
    "vdload.mrp", "gamelist.ext", "gbrwcore.ext", "mrc_loader.ext", "robotol.ext",
]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def files_under(path: Path) -> list[Path]:
    return sorted((p for p in path.rglob("*") if p.is_file()), key=lambda p: p.as_posix().lower())


def mrp_entries(path: Path) -> list[tuple[str, int, int]]:
    """Parse the MRP directory format used by this package."""
    data = path.read_bytes()
    if len(data) < 240:
        return []
    # Package header facts:
    #   LE32 @ 4  = directory end
    #   LE32 @ 12 = directory start
    # Entry: LE32 name_len, name bytes (NUL included), LE32 offset, LE32 length, LE32 reserved.
    list_end = struct.unpack_from("<I", data, 4)[0]
    pos = struct.unpack_from("<I", data, 12)[0]
    if not (16 <= pos < list_end <= len(data)):
        return []
    out: list[tuple[str, int, int]] = []
    while pos < list_end:
        if pos + 4 > list_end:
            break
        name_len = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        if not (1 <= name_len <= 512) or pos + name_len + 12 > len(data):
            break
        raw = data[pos:pos + name_len]
        pos += name_len
        raw = raw.split(b"\x00", 1)[0]
        try:
            name = raw.decode("ascii")
        except UnicodeDecodeError:
            break
        off, length, _reserved = struct.unpack_from("<III", data, pos)
        pos += 12
        if not name or off > len(data) or length > len(data) or off + length > len(data):
            break
        out.append((name, off, length))
    return out


def md_file_list(base: Path, paths: Iterable[Path]) -> str:
    rows = []
    for p in paths:
        rows.append(f"- `{p.relative_to(base).as_posix()}` — {p.stat().st_size} B")
    return "\n".join(rows) if rows else "- （空）"


def resolve_guest(guest: str, mythroad_root: Path, gwy_root: Path) -> Path:
    g = guest.replace("\\", "/")
    if g.startswith("/") and g[1:].startswith(("gwy/", "mythroad/", "240x320/")):
        g = g[1:]
    if g.startswith("mythroad/240x320/"):
        return mythroad_root / g[len("mythroad/240x320/"):]
    if g.startswith("240x320/"):
        return mythroad_root / g[len("240x320/"):]
    if g.startswith("mythroad/gwy/"):
        return mythroad_root / g[len("mythroad/"):]
    if g.startswith("gwy/"):
        return mythroad_root / g
    return gwy_root / g


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = ap.parse_args()
    root = args.project_root.resolve()
    mythroad = root / "game_files" / "mythroad" / "240x320"
    gwy = mythroad / "gwy"
    reports = root / "reports"
    reports.mkdir(parents=True, exist_ok=True)
    if not gwy.is_dir():
        raise SystemExit(f"missing GWY root: {gwy}")

    top_files = sorted((p for p in gwy.iterdir() if p.is_file()), key=lambda p: p.name.lower())
    top_dirs = sorted((p for p in gwy.iterdir() if p.is_dir()), key=lambda p: p.name.lower())
    all_files = files_under(gwy)

    jjfb = gwy / "jjfb.mrp"
    entries = mrp_entries(jjfb) if jjfb.exists() else []
    entry_map = {name.lower(): (name, off, length) for name, off, length in entries}

    lines = [
        "# v50 GWY 资源树静态审计",
        "",
        "> 生成方式：`python scripts/v50_gwy_resource_audit.py`。本报告只陈述静态文件事实；运行时请求需由 v50 runner 日志补充。",
        "",
        "## 1. 资源根",
        "",
        f"- Mythroad root：`{mythroad}`",
        f"- GWY root：`{gwy}`",
        f"- 文件总数：**{len(all_files)}**",
        f"- 总字节数：**{sum(p.stat().st_size for p in all_files)} B**",
        "",
        "## 2. 一级目录",
        "",
        "| 目录 | 文件数 | 字节数 | 当前用途判断 |",
        "|---|---:|---:|---|",
    ]
    for d in top_dirs:
        fs = files_under(d)
        purpose = PURPOSES.get(d.name, "GWY 内其他游戏/平台资源；与 jjfb 启动无直接证据，保持原结构。")
        lines.append(f"| `{d.name}/` | {len(fs)} | {sum(p.stat().st_size for p in fs)} | {purpose} |")

    lines += ["", "## 3. GWY 根目录文件", "", md_file_list(gwy, top_files)]

    lines += ["", "## 4. 关键文件存在性", "", "| 名称 | 主机顶层存在 | 说明 |", "|---|---|---|"]
    top_names = {p.name.lower(): p for p in top_files}
    for name in KEY_HOST:
        p = top_names.get(name.lower())
        if p:
            note = f"`{p.relative_to(root).as_posix()}`，{p.stat().st_size} B"
            if p == jjfb:
                note += f"，SHA-256 `{sha256(p)}`"
            lines.append(f"| `{name}` | 是 | {note} |")
        elif name.lower() in entry_map:
            en, off, length = entry_map[name.lower()]
            lines.append(f"| `{name}` | 否（非顶层） | 位于 `jjfb.mrp` 内部，offset={off}, length={length} |")
        else:
            alt = name.replace(".ext", ".mrp")
            if alt.lower() in top_names:
                p2 = top_names[alt.lower()]
                lines.append(f"| `{name}` | 否 | 顶层实际是 `{alt}`（{p2.stat().st_size} B）；不要凭文档扩展名臆造文件。 |")
            else:
                lines.append(f"| `{name}` | 否 | 当前静态包未发现。 |")

    lines += ["", "## 5. `jjfb.mrp` 内部启动项", ""]
    if entries:
        lines += ["| 内部项 | offset | length |", "|---|---:|---:|"]
        for name, off, length in entries:
            if name.lower() in {"start.mr", "mrc_loader.ext", "robotol.ext"}:
                lines.append(f"| `{name}` | {off} | {length} |")
        lines.append("")
        lines.append(f"共解析到 **{len(entries)}** 个内部条目。`mrc_loader.ext` 与 `robotol.ext` 是 MRP 内部项，不应复制为主机顶层文件。")
    else:
        lines.append("未能解析内部目录；需保留原文件，不做重打包。")

    for dirname in ("jjfbol", "gifs"):
        d = gwy / dirname
        lines += ["", f"## 6.{1 if dirname == 'jjfbol' else 2} `{dirname}/` 文件清单", "", md_file_list(gwy, files_under(d))]

    lines += [
        "", "## 7. 静态结论", "",
        "1. `gwy/jjfb.mrp` 不是孤立文件；启动契约必须保留 `gwy/` 整棵目录。",
        "2. `jjfbol/`、`gifs/`、`save/`、`sound/` 都应映射到同一个 GWY root。",
        "3. `mrc_loader.ext`、`robotol.ext` 位于 `jjfb.mrp` 内部，顶层缺失不等于资源缺失。",
        "4. 顶层实际文件是 `gbrwcore.mrp`、`gamelist.mrp` 等；文档里出现 `.ext` 时必须以真实包为准。",
        "5. 动态结论必须以 `[JJFB_FILEOPEN]` / `[JJFB_FILEOPEN_MISS]` 为证据，不能再由 UI 是否显示反推。",
    ]
    (reports / "v50_gwy_resource_tree.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    test_guests = [
        "gwy/jjfb.mrp", "mythroad/240x320/gwy/jjfb.mrp", "mythroad/gwy/jjfb.mrp",
        "/gwy/jjfb.mrp", "gwy/jjfbol/default.mrp", "jjfbol/default.mrp",
        "gwy/gifs/ng_jjfb.gif", "gifs/ng_jjfb.gif", "gwy/save", "save",
        "gwy/sound/msg.mid", "sound/msg.mid", "cfg.bin", "mrc_loader.ext", "robotol.ext",
    ]
    plines = [
        "# v50 文件路径映射表（静态预期）", "",
        "> 这是 runner 执行前的静态映射基线。运行后由 `scripts/v50_analyze_launcher_log.py` 生成实际打开结果。", "",
        "| guest path | host resolved path | exists? | 预期说明 |",
        "|---|---|---:|---|",
    ]
    for g in test_guests:
        p = resolve_guest(g, mythroad, gwy)
        note = "主机资源" if p.exists() else "若为 MRP 内部项，应由 MRP loader 读取，不应主机直开"
        plines.append(f"| `{g}` | `{p}` | {'是' if p.exists() else '否'} | {note} |")
    plines += [
        "", "## 映射优先级", "",
        "1. 开启 `JJFB_GWY_LAUNCHER_MODE=1` 后，`mythroad/240x320/*`、`240x320/*`、`mythroad/gwy/*`、`gwy/*` 优先映射到 canonical `mythroad/240x320`。",
        "2. 无前缀资源（如 `jjfbol/default.mrp`、`gifs/ng_jjfb.gif`、`save/*`）优先映射到 `gwy_root`。",
        "3. canonical 不存在时才回退进程当前目录，防止旧的扁平副本掩盖路径错误。",
        "4. 创建文件时，即使目标尚不存在，也返回 canonical 路径，以保证缓存/存档写回 `gwy/save` 等真实目录。",
        "", "## 运行时待填字段", "",
        "runner 日志需补齐：`opened by pc/lr`、`handle/ret`、`FILEOPEN_MISS` 次数和首个真正阻断点。",
    ]
    (reports / "v50_file_path_mapping.md").write_text("\n".join(plines) + "\n", encoding="utf-8")
    print(reports / "v50_gwy_resource_tree.md")
    print(reports / "v50_file_path_mapping.md")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
