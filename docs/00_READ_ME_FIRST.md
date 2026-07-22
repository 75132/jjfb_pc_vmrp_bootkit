# JJFB / GWY 独立启动器重构包：先读这里

## 一句话纠偏

本项目的目标不是继续推进《机甲风暴》内部变量、UI 状态或固定地址，而是从干净的 vmrp/Mythroad 基线开发一个**独立的 GWY/MRP 外壳启动器**：读取 `gwy` 资源树与 `cfg.bin`，重建冒泡网游外壳在“允许启动游戏”时提供的启动契约和平台服务，避开已经失效的 GWY 强制联网/更新界面，让原始 `gwy/jjfb.mrp` 自己运行。

## 这次为什么必须重构，而不是继续在现有 bridge.c 上修

本次包内实测项目中：

- 当前 `bridge.c` 约 377KB，相比干净基线增加约 8773 行；
- 已混入 v43–v86 大量 UI、固定地址、ERW 偏移、事件注入和诊断探针；
- 当前日常 runner 仍开启 `MRC_RESUME_AFTER_INIT`、`FAMILY_APP2_AFTER_INIT`、`V64_ENQUEUE_ONCE`、`FAMILY_C0_AFTER_B71` 等 JJFB 专用推进；
- 这些代码虽然帮助认识了平台 ABI，但已经把“外壳启动器”与“游戏内部状态补丁”混在了一起。

因此最正确的工程动作是：

```text
冻结当前 bootkit 为 legacy_lab（证据库）
        ↓
从原始 vmrp-master 建 clean_launcher 分支/新目录
        ↓
只迁移可证明为通用平台能力的代码
        ↓
所有 JJFB 差异放入 declarative compatibility profile
        ↓
核心代码禁止出现 JJFB 固定地址和 ERW 偏移
```

## 推荐阅读顺序

1. `docs/01_ROUTE_CORRECTION.md`：项目边界和正确方向。
2. `docs/02_CURRENT_STATE_EVIDENCE.md`：本次上传包的实际证据。
3. `docs/03_TARGET_ARCHITECTURE.md`：新启动器分层架构。
4. `docs/05_LAUNCH_CONTRACT.md`：启动参数、路径、loader、生命周期契约。
5. `docs/07_REUSE_AND_MIGRATION.md`：现有代码哪些保留、哪些隔离、哪些重写。
6. `docs/08_REDEVELOPMENT_ROADMAP.md`：Cursor 分阶段开发任务。
7. `docs/cursor/CURSOR_MASTER_PROMPT.md`：新对话直接交给 Cursor。
8. `cursor/.cursor/rules/JJFB_LAUNCHER_REBUILD.mdc`：复制到新仓库。
9. `cursor/.cursor/skills/jjfb-gwy-launcher-rebuild/SKILL.md`：Cursor Skill。
10. `docs/15_FIRST_30_TASKS.md`：前 30 个可直接执行的任务。
11. `docs/16_CLEAN_REPO_LAYOUT_AND_API_CONTRACTS.md`：接口级设计。
12. `docs/17_TWO_TRACK_RESEARCH_METHOD.md`：产品轨/研究轨隔离。
13. `docs/20_REFERENCE_IMPLEMENTATION_PLAN.md`：接近代码级的实现计划。
14. `docs/21_FILE_BY_FILE_MIGRATION_GUIDE.md`：旧代码逐文件迁移。
15. `docs/22_LAUNCHER_PRODUCT_SPEC.md`：最终产品规格。

## 最终产品应是什么

不是一个“JJFB 状态修复器”，而是一个具有以下能力的独立程序：

```text
gwy_launcher.exe --root <mythroad-root> --cfg gwy/cfg.bin --index 36
```

它应当：

1. 扫描并验证完整 `mythroad/<resolution>/gwy` 资源树；
2. 解析 `cfg.bin`，得到目标 MRP 与启动字段；
3. 构造统一 `LaunchDescriptor`；
4. 初始化干净 Mythroad/vmrp 平台；
5. 建立 guest VFS、SDK identity、显示、内存、timer/event、network 服务；
6. 启动原始 `start.mr`；
7. 通过通用 MRP 成员解析加载 EXT；
8. 根据平台注册表调度生命周期和回调；
9. 不直接写 JJFB 的 `ui_mode`、B70/B71/AC8/progress；
10. 让游戏自然请求资源和网络。

## 包内自动证据

- `evidence/key_mrp_manifest.md`：关键 MRP 的成员、大小、APPID/版本和哈希。
- `evidence/cfg_index36_record.md`：cfg index 36 原始字段。
- `evidence/current_source_delta.md`：当前源码相对基线的膨胀规模。
- `evidence/current_launch_key_timeline.txt`：当前启动链关键日志。
- `evidence/observed_platform_calls.csv`：robotol 已观察到的平台调用。
- `tools/`：只读 MRP/cfg 分析器和新核心反跑偏审计器。

## 关键原则

> 外壳启动器可以补平台能力、路径语义、加载器语义和生命周期调度；不可以把某个游戏的内存状态人工变成“看起来运行了”。
