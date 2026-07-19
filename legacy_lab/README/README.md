# JJFB PC vmrp Bootkit

PC 端用 vmrp 跑 **机甲风暴**（GWY Launcher Shim → `gwy/jjfb.mrp` → `mrc_loader.ext` → `robotol.ext`）。

## 当前跑法

```powershell
# 需 MSYS2 mingw32（i686）在 PATH
powershell -ExecutionPolicy Bypass -File .\RUN_JJFB.ps1
```

实现脚本：`RUN_V71_PRESENT_COALESCE.ps1`（`RUN_JJFB.ps1` 只做转发）。

主线：`JJFB_GWY_LAUNCHER_MODE=1`，cfg index=36，资源根 `game_files/mythroad/320x480`，guest LCD 240×320，窗口 320×480 stretch。不改 `jjfb.mrp` / UI。

日志：`logs/jjfb_gwy_clean_stdout.txt`

## 先读

1. [`docs/HANDOFF.md`](../docs/HANDOFF.md) — 交接入口  
2. [`docs/LOCKED_FACTS.md`](../docs/LOCKED_FACTS.md) — 勿重开事实  
3. [`docs/INDEX.md`](../docs/INDEX.md) — 文档索引  
4. [`.cursor/rules/Rules.mdc`](../.cursor/rules/Rules.mdc) — 开发硬规则  

## 根目录（整理后）

| 路径 | 说明 |
|------|------|
| `RUN_JJFB.ps1` | **当前一键入口** |
| `RUN_V71_PRESENT_COALESCE.ps1` | 入口实现（编译+跑） |
| `CONFIG.json` | 路径 / cfg36 / 端口 |
| `runtime/` | vmrp 源码与 Win32 运行时 |
| `game_files/` | mythroad 游戏文件 |
| `scripts/` | 工具；`scripts/runners/` 为更早历史 runner |
| `logs/` | 最近一次 clean 跑日志 |
| `reports/` | 按需生成（默认空） |
| `archive/` | 旧 runner / 日志 / 报告 / packages |
| `docs/` `README/` | 交接与说明 |

## 历史材料

- 旧 root runner（V63–V70、OTHER_GAME）：`archive/runners/2026-07-17_jjfb_clean/`
- 旧日志 / 报告：`archive/logs|reports/2026-07-17_jjfb_clean/`
- V27–V62 runner：`scripts/runners/`
- 旧 Cursor brief：`docs/history/`
