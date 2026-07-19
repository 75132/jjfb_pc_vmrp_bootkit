# 文档索引

**当前入口：根目录 [`RUN_JJFB.ps1`](../RUN_JJFB.ps1)**（实现：`RUN_V71_PRESENT_COALESCE.ps1`）  
**交接：[`HANDOFF.md`](HANDOFF.md)** · **事实：[`LOCKED_FACTS.md`](LOCKED_FACTS.md)**

| 路径 | 说明 |
|------|------|
| [`LOCKED_FACTS.md`](LOCKED_FACTS.md) | 勿重开的事实 + 常用 env |
| [`reports/`](reports/) | 按需实验结论（默认空；旧报告在 archive） |
| [`history/`](history/) | 旧版 Cursor brief / tip / handoff 原文 |
| 根目录 `README/README.md` | 仓库入口 |
| `scripts/runners/` | V27–V62 历史 runner |
| `archive/` | 旧 packs、旧日志、旧报告、V63–V70 root runners |

## 主线（不要跑偏）

```text
GWY Launcher Shim
→ cfg index=36
→ startGame/runapp
→ mythroad/320x480 + guest 240x320
→ gwy/jjfb.mrp → mrc_loader → robotol
```

禁止：假 UI / 改 jjfb.mrp / force ui_mode 当正式方案。

## 归档说明（2026-07-17）

根目录探针 runner（V63–V70、`RUN_OTHER_GAME`）、旧 `logs/`、旧 `reports/`、`packages/`  
已移到 `archive/{runners,logs,reports,packages}/2026-07-17_jjfb_clean/`。
