# archive

历史迭代产物，按类型归档。当前工作以根目录 `RUN_JJFB.ps1` 为准。

| 子目录 | 内容 |
|--------|------|
| runners/ | 各版本 PowerShell 启动脚本 |
| logs/ | 旧跑测日志 |
| reports/ | 旧实验报告 |
| packages/ | 旧交付包 |
| handoffs/ notes/ readmes/ openclaw_cmds/ zips/ | 早期材料 |

## 2026-07-17_jjfb_clean

整理目标：根目录只保留 JJFB GWY 主入口。

移入本 stamp：

- root `RUN_V63`…`RUN_V70`、`RUN_OTHER_GAME.ps1` → `archive/runners/2026-07-17_jjfb_clean/`
- 旧 `logs/*`（保留 `jjfb_gwy_clean_*` + v56 audit/key）→ `archive/logs/2026-07-17_jjfb_clean/`
- 旧 `reports/*` → `archive/reports/2026-07-17_jjfb_clean/`
- 根 `packages/*` → `archive/packages/2026-07-17_jjfb_clean/`

当前入口：`RUN_JJFB.ps1` → `RUN_V71_PRESENT_COALESCE.ps1`。
