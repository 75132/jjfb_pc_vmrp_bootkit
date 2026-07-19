# 旧 bootkit 文件级迁移指南：哪些借鉴，哪些重写，哪些永久隔离

## 1. 三个源码快照的角色

上传包中至少有三类 vmrp 源码：

```text
runtime/vmrp_src/vmrp-master
  → 约 51KB bridge.c，作为 clean/upstream 起点

runtime/vmrp_src_audit_v25/vmrp-master
  → 早期审计快照，bridge 与 clean 基本一致

runtime/vmrp_src_build_v27/vmrp-master
  → 当前高度修改运行版，bridge.c 约 385KB
```

新项目起点必须是第一类。第三类只读迁移思想和证据，不能复制覆盖。

## 2. `bridge.c`

### 可以借鉴的知识

- Unicorn bridge 调用约定；
- guest register/stack 读写方法；
- EXT helper、P、ER_RW 的运行期关系；
- 已观察平台服务代码和参数日志；
- guest call depth/延迟调用的必要性；
- RGB565、bitmap/pitch/colorkey 的问题样本；
- loader 的 `version(6) → appInfo(8) → init(0)` 证据。

### 不可直接迁移的代码

修改版存在大量：

```text
jjfb_hook_*
jjfb_install_*_hooks
jjfb_progress_*
jjfb_ac8_*
jjfb_uimode_*
jjfb_vXX_*
br_jjfb_sendAppEvent
```

以及大量固定 PC、ERW offset、画面特判、环境变量。全部放 `legacy_lab`。

### 新实现方式

| 旧混合职责 | 新模块 |
|---|---|
| bridge service switch | `platform/platform_table.c` |
| guest pointer 读取 | `runtime/guest_memory.c` |
| EXT load/register | `runtime/ext_loader.c` |
| callback 保存 | `platform/platform_registry.c` |
| timer/event 推进 | `platform/platform_scheduler.c` |
| 图形 | `platform/platform_display.c` |
| trace | `trace/trace_jsonl.c` |

## 3. `fileLib.c`

修改版里的 `my_resolve_path()` 证明 canonical resource root 很重要，但正式代码要重写。

### 可迁移概念

- `mythroad/` 与 `gwy/` guest 前缀；
- canonical resolution root；
- 记录 guest→host 映射；
- 写文件前创建父目录。

### 旧实现风险

- 字符串拼接和 truncation 语义不够严格；
- 进程 cwd fallback 会掩盖错误；
- source root 与 save/cache 写路径混合；
- Windows absolute/UNC/大小写/路径逃逸处理不完整；
- 目标特定环境变量命名。

### 正式迁移

不要复制 `my_resolve_path()`。按 `GuestVfs` API 重写，并以表格测试覆盖。

## 4. `vmrp.c`

### 可借鉴

- start 参数传入点；
- VM 初始化/退出边界；
- current MRP name/root 的概念；
- loader 返回值观测。

### 重写要求

- CLI/profile 生成 descriptor；
- `vmrp.c` 不知道 cfg index 36；
- 不在 runtime 全局变量中散落 JJFB 参数；
- 状态进入 `LaunchContext`；
- entry result 结构化，不用日志字符串反解析。

## 5. `main.c`

### 可借鉴

- SDL 窗口创建；
- 逻辑分辨率到 host 窗口缩放；
- input polling 的基本循环。

### 禁止迁移

- host 代画游戏 UI；
- splash-specific present；
- 画面冻结/去闪烁基于目标状态；
- 通过屏幕像素判断生命周期。

### 新位置

```text
platform_display_sdl.c
platform_input_sdl.c
app/launcher_main.c
```

## 6. `network.c`

### 可借鉴

- socket bridge 基础；
- non-blocking/错误码映射需求；
- endpoint trace。

### 必须隔离

- hosts 劫持；
- mock 旧更新 URL；
- 针对旧端口的成功伪造；
- 登录/更新业务协议。

新 `platform_network` 只实现 socket/DNS 语义。业务服务器模拟是另一个仓库/阶段。

## 7. `memory.c`

迁移 guest allocation/map 经验，但所有 API 都要加入：

- overflow 检查；
- executable/read/write 权限模型；
- owner/generation；
- cleanup；
- debug poison；
- 测试中的小地址空间和失败注入。

## 8. 可直接复用的 Python 资产

### 高价值，整理后复用

- SDK key 生成算法与 golden vector；
- MRP member/header 静态审计逻辑；
- 日志里 loader milestone 的提取思路；
- hash manifest 生成。

### 仅 legacy

- 固定地址 disassembly/map；
- ui_mode/B70/B71/AC8/progress 分析器；
- 修改 MRP/start.mr 的脚本；
- event/family 注入 runner；
- host fake UI 资源工具。

## 9. runner 迁移

旧 `RUN_V71_PRESENT_COALESCE.ps1` 等 runner 不进入新仓库。新 runner 只做：

```text
resolve paths
→ preflight tools
→ configure/build
→ unit/integration tests
→ launch profile
→ collect manifest/trace/summary
```

不能设置任何游戏状态推进环境变量。

## 10. 迁移完成的判断

当新项目可以删除 `legacy_lab` 链接仍然构建和跑单测，说明架构没有暗中依赖旧实验。只有 integration test 在本地通过 profile 访问原始资源。
