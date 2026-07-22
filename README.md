# GWY / MRP Independent Launcher (clean)

独立 GWY/MRP 外壳启动器。目标是重建冒泡网游外壳的启动契约与平台服务，让原始 `gwy/jjfb.mrp` 自然运行——不是继续在旧 `bridge.c` 上堆 JJFB 状态补丁。

## 先读

1. [`docs/00_READ_ME_FIRST.md`](docs/00_READ_ME_FIRST.md)
2. [`docs/cursor/START_CURSOR_HERE.md`](docs/cursor/START_CURSOR_HERE.md)
3. [`docs/15_FIRST_30_TASKS.md`](docs/15_FIRST_30_TASKS.md)
4. [`docs/REPO_LAYOUT.md`](docs/REPO_LAYOUT.md) — 目录约定

路线口令：

> 解析 shell，重建 launch contract；模拟平台，不扶游戏状态。

## 根目录入口（仅产品）

| 脚本 | 用途 |
|------|------|
| `RUN_BUILD.ps1` | 构建 `launcher_core` + `gwy_launcher` |
| `RUN_BUILD_VMRP.ps1` | `Gwy`（产品 stubs）/ `GwyResearch` |
| `RUN_TESTS.ps1` | 产品审计 + 单测 + validate |
| `RUN_PRODUCT_DIRECT_JJFB.ps1` | 产品黄金链验收 |
| `RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1` | 产品 robotol/mrc_init 冒烟 |
| `RUN_GAMES.ps1` | 游戏列表 GUI |
| `RUN_VMRP_VISUAL.ps1` | 资源同步 / 可视化准备 |
| `RUN_RESEARCH_GWY_SHELL.ps1` | **显式**研究轨入口 |

研究阶段 runner（E5–E10A / E8–E9）在 [`research/runners/`](research/runners/)，不要塞回根目录。

## 目录角色

| 路径 | 角色 |
|---|---|
| `src/` `include/gwy_launcher/` | 产品核心 |
| `research/` | 研究轨（runners、e10a 产物、packs） |
| `third_party/vmrp_upstream/` | 干净 vmrp 基线 |
| `game_files/` | 原始游戏资源（只读） |
| `profiles/` `schemas/` | 声明式兼容 profile |
| `tools/` | 审计与工具 |
| `docs/` `evidence/` `decisions/` | 路线、证据、ADR |
| `packages/` | 参考包与清单 |
| `legacy_lab/` | 冻结旧 bootkit（不参与产品 build） |

## 禁止

- 把 `legacy_lab` 修改版 `bridge.c` 复制进 clean 核心
- 在核心写入 JJFB 固定地址 / ERW offset / ui_mode / progress 强制
- 修改原始 `gwy/jjfb.mrp` / `robotol.ext` / `mrc_loader.ext`
- 把新的阶段 runner / 分析 pack 放回仓库根目录

## 构建

```powershell
.\RUN_BUILD.ps1
.\RUN_TESTS.ps1
.\RUN_PRODUCT_DIRECT_JJFB.ps1 -Seconds 90
```
