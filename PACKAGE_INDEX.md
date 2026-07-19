# 完整包索引

## 立即使用

| 文件 | 用途 |
|---|---|
| `START_CURSOR_HERE.md` | 最短接管步骤 |
| `CURSOR_MASTER_PROMPT.md` | 完整复制给 Cursor 的主提示词 |
| `00_READ_ME_FIRST.md` | 项目方向、边界、阅读顺序 |
| `cursor/.cursor/rules/JJFB_LAUNCHER_REBUILD.mdc` | Cursor 强制规则 |
| `cursor/.cursor/skills/jjfb-gwy-launcher-rebuild/SKILL.md` | 分析/实现/调试 Skill |

## 核心路线文档

| 编号 | 文档 | 内容 |
|---:|---|---|
| 01 | `docs/01_ROUTE_CORRECTION.md` | 为什么旧路线偏离、如何纠偏 |
| 02 | `docs/02_CURRENT_STATE_EVIDENCE.md` | 对上传包的事实核验 |
| 03 | `docs/03_TARGET_ARCHITECTURE.md` | 最终分层架构 |
| 04 | `docs/04_MRP_GWY_MODULE_ANALYSIS.md` | GWY 目录和关键 MRP 模块分析 |
| 05 | `docs/05_LAUNCH_CONTRACT.md` | cfg→descriptor→start.mr→EXT 契约 |
| 06 | `docs/06_PLATFORM_ABI_AND_SCHEDULER.md` | 平台 ABI、注册表、非重入 scheduler |
| 07 | `docs/07_REUSE_AND_MIGRATION.md` | 现有成果迁移矩阵 |
| 08 | `docs/08_REDEVELOPMENT_ROADMAP.md` | Phase 0–10 路线图 |
| 09 | `docs/09_TEST_AND_ACCEPTANCE.md` | 测试与验收标准 |
| 10 | `docs/10_LOGGING_AND_DIAGNOSTICS.md` | 结构化日志和诊断 |
| 11 | `docs/11_OPEN_RESEARCH_QUESTIONS.md` | 未锁定问题与实验方法 |
| 12 | `docs/12_CURSOR_EXECUTION_PLAYBOOK.md` | Cursor 每轮工作法 |
| 13 | `docs/13_LEGAL_AND_SAFETY_SCOPE.md` | 本地研究边界 |
| 14 | `docs/14_GLOSSARY.md` | 术语表 |
| 15 | `docs/15_FIRST_30_TASKS.md` | 前 30 个可直接执行任务 |
| 16 | `docs/16_CLEAN_REPO_LAYOUT_AND_API_CONTRACTS.md` | 文件结构与 C API 契约 |
| 17 | `docs/17_TWO_TRACK_RESEARCH_METHOD.md` | 产品轨与研究轨分离 |
| 18 | `docs/18_BUILD_AND_DEBUG_RUNBOOK_WINDOWS.md` | Windows 构建调试手册 |
| 19 | `docs/19_RISK_REGISTER_AND_DECISIONS.md` | 风险、止损、决策 |
| 20 | `docs/20_REFERENCE_IMPLEMENTATION_PLAN.md` | 接近代码级实现流程 |
| 21 | `docs/21_FILE_BY_FILE_MIGRATION_GUIDE.md` | 旧 bootkit 逐文件迁移说明 |
| 22 | `docs/22_LAUNCHER_PRODUCT_SPEC.md` | CLI/GUI 产品规格和 MVP 边界 |

## 自动解析证据

- `evidence/key_mrp_manifest.*`：关键 shell/game MRP 成员清单。
- `evidence/gwy_resource_inventory*`：GWY 资源树统计。
- `evidence/cfg_index36_record.*`：修正后的 cfg record 36 字节审计。
- `evidence/current_source_delta.*`：修改版与 clean 基线差异。
- `evidence/observed_platform_calls.csv`：已观察平台调用，不等于已文档化语义。
- `evidence/current_launch_key_timeline.txt`：可复用 loader milestones。
- `evidence/tool_validation_report.md`：工具、骨架和审计自测。

## 可执行工具

```text
tools/mrp_inspect.py
tools/gwy_cfg_inspect.py
tools/audit_launcher_core.py
```

## 可复制模板

- Phase task / blocker / ADR / test report；
- `templates/clean_repo_skeleton/`：已通过 CMake + CTest 的最小 clean 骨架；
- `schemas/launcher_profile.schema.json`；
- `profiles/jjfb.example.json`。

## 架构决策

```text
decisions/ADR-0001-DIRECT-DESCRIPTOR-LAUNCH.md
decisions/ADR-0002-CLEAN-UPSTREAM-BASELINE.md
decisions/ADR-0003-DECLARATIVE-COMPATIBILITY.md
```
