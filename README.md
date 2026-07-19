# GWY / MRP Independent Launcher (clean)

独立 GWY/MRP 外壳启动器。目标是重建冒泡网游外壳的启动契约与平台服务，让原始 `gwy/jjfb.mrp` 自然运行——不是继续在旧 `bridge.c` 上堆 JJFB 状态补丁。

## 先读

1. [`00_READ_ME_FIRST.md`](00_READ_ME_FIRST.md)
2. [`START_CURSOR_HERE.md`](START_CURSOR_HERE.md)
3. [`docs/15_FIRST_30_TASKS.md`](docs/15_FIRST_30_TASKS.md) — 当前从 **Phase 0 / Task 0.1** 起

路线口令：

> 解析 shell，重建 launch contract；模拟平台，不扶游戏状态。

## 目录角色

| 路径 | 角色 |
|---|---|
| `src/` `include/gwy_launcher/` | 新产品核心（clean） |
| `third_party/vmrp_upstream/` | 干净 vmrp 基线（~51KB `bridge.c`） |
| `game_files/` | 原始游戏资源树（只读使用，不改 MRP） |
| `profiles/` `schemas/` | 声明式兼容 profile |
| `tools/` | 只读检查与反跑偏审计 |
| `docs/` `evidence/` `decisions/` | 路线、证据、ADR |
| `legacy_lab/` | 冻结的旧 bootkit / 探针 / 修改版 bridge（不参与 build） |
| `.cursor/` | 新规则与 skill |

## 禁止

- 把 `legacy_lab/runtime/vmrp_src_build_v27` 的修改版 `bridge.c` 复制进 clean 核心
- 在核心写入 JJFB 固定地址 / ERW offset / ui_mode / progress 强制
- 修改原始 `gwy/jjfb.mrp` / `robotol.ext` / `mrc_loader.ext`

## 构建骨架

```powershell
.\RUN_BUILD.ps1                  # PE32 gwy_launcher + build-info.json
.\RUN_BUILD.ps1 -UpstreamBaseline  # also rebuild clean third_party/vmrp_upstream
.\RUN_TESTS.ps1                  # audit + negative gate + ctest + jjfb hash smoke
```

## 现在就能测

### 1) 格式/契约（无窗口）
```powershell
.\RUN_TESTS.ps1
.\build-i686\gwy_launcher.exe validate --root .\game_files\mythroad\320x480
```

### 2) 网游列表 + 接入启动链
```powershell
.\RUN_GAMES.ps1
```
列表点「启动」会走：`LaunchDescriptor` → VFS 预检 → `launch_manifest.json` → 干净 vmrp。

也可命令行：
```powershell
.\build-i686\gwy_launcher.exe launch --root .\game_files\mythroad\320x480 --index 36 `
  --vmrp .\out\vmrp_run\main.exe --cwd .\out\vmrp_run
```
