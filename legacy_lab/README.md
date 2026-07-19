# legacy_lab — 冻结的旧 bootkit 证据库

本目录是大转向前的完整旧工程，**只读证据 / 研究轨**，不参与 clean `gwy_launcher` 构建。

## 主要内容

| 子目录/文件 | 说明 |
|---|---|
| `runtime/vmrp_src/` | 干净上游快照（与根目录 `third_party/vmrp_upstream` 同源） |
| `runtime/vmrp_src_build_v27/` | 高度修改运行版（`bridge.c` ~385KB）— **禁止复制进 core** |
| `runtime/vmrp_src_audit_v25/` | 早期审计快照 |
| `scripts/` | v43–v86 探针与分析脚本 |
| `docs/` `README/` `reports/` `archive/` | 旧文档与交接 |
| `tools/` | 旧资源/UI 辅助工具 |
| `runners/` | 旧 `RUN_*.ps1` / `CONFIG.json` / `HOW_TO_START.md` |
| `JJFB_GWY_LAUNCHER_REBUILD_GUIDE_COMPLETE/` | 重构指南原包（内容已提升到仓库根） |
| `logs/` `packages/` `patches/` `mock/` `sdk_key/` `docx/` | 旧运行产物与杂项 |

## 使用规则

1. 需要借鉴平台 ABI 观察时，只读查阅本目录。
2. 可迁移的是**思路与证据**，不是把修改版 bridge 整文件搬回 clean。
3. 固定地址 / ERW / UI 强制实验永远留在这里。
4. 反跑偏：`tools/audit_launcher_core.py`（仓库根）扫到 core 出现本目录那类地址必须失败。
