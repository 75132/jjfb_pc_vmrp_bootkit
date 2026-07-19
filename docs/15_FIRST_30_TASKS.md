# Cursor 前 30 个任务：逐项执行，不跨阶段

本文把路线拆到可以直接创建 Issue/Task 的粒度。编号不是版本号，不要继续使用 v87/v88 命名。

## Phase 0：仓库与基线

### Task 0.1 建立 clean repository

**输入**：upstream `vmrp-master`、本重构包。  
**动作**：创建新仓库结构；upstream 固定为 `third_party/vmrp_upstream` 或 Git submodule；旧 bootkit 只读挂载为 `legacy_lab_external`。  
**禁止**：复制修改版 bridge。  
**验收**：目录、README、LICENSE/来源记录齐全；clean build 不包含 legacy。

### Task 0.2 固定工具链

- Windows 32-bit MinGW/MSYS2；
- C11；
- CMake 或保留 Makefile 的同时增加 CMake wrapper；
- 锁定 SDL2/Unicorn 版本和来源；
- 输出 `build-info.json`。

**验收**：空改 upstream 可重现构建，二进制架构为 PE32。

### Task 0.3 建立测试运行器

- 单测可在不启动 GUI 的情况下运行；
- fixture 路径由环境变量提供；
- 测试不复制大型资源进 Git；
- Windows `RUN_TESTS.ps1`。

### Task 0.4 接入反跑偏审计

把 `tools/audit_launcher_core.py` 放入 CI/本地 preflight。  
**验收**：人工加入 `0x2DADC4` 到 core 时测试必须失败；放到 `legacy_lab` 时通过。

## Phase 1：只读格式

### Task 1.1 MRP header parser

实现：magic、总长、header/index 边界、APPID/APPVER、溢出检测。  
**测试**：JJFB golden + 截断/错误 magic/超长字段。

### Task 1.2 MRP member index parser

实现成员名长度、NUL、offset、stored length、reserved。  
**验收**：输出与 `evidence/key_mrp_members.csv` 一致。

### Task 1.3 MRP member decoder

支持 raw/gzip；限制 decoded size；校验 CRC/解压错误。  
**验收**：JJFB 关键 decoded 长度 3787/232/253420/2472。

### Task 1.4 `mrp_inspect` 原生命令

输出 text/JSON；支持 list、extract-one、hash；默认只读。  
**禁止**：archive repack/patch 进入 launcher 主流程。

### Task 1.5 cfg record framing

先把 record size/header/index bounds 写成显式模型；未知字段保留 raw。  
**验收**：index 36 边界稳定，解析错误 fail closed。

### Task 1.6 cfg known-field decoder

读取标题、图标、napptype、nextid、ncode、target；每个字段带 offset/source/confidence。  
**验收**：与 `cfg_index36_record.json` 一致。

### Task 1.7 reg.ext exploratory parser

先实现 strings/module-name heuristic，输出 `confidence=heuristic`；不要谎称结构已完全逆出。  
**验收**：JJFB 能列出 robotol 和模块名，gbrwcore 能列出 gbrwcore.ext。

## Phase 2：描述符和 profile

### Task 2.1 profile schema validator

实现 JSON schema 或等价强类型校验；拒绝未知危险字段。  
**验收**：profile 中加入 `guest_address` 或 `erw_offset` 必须失败。

### Task 2.2 LaunchDescriptor builder

从 cfg + profile + MRP 构造不可变对象；profile 不覆盖冲突事实，只能声明预期。  
**验收**：冲突 target/hash/appid 立即停止。

### Task 2.3 launch parameter serializer

字段顺序显式、长度受限、UTF-8/ASCII 规则明确。  
**golden**：JJFB 参数与证据完全一致。

### Task 2.4 launch manifest

每次启动前输出 descriptor JSON、resource hash、binary build info、profile hash；便于复现。

## Phase 3：VFS

### Task 3.1 guest path canonicalizer

统一 `/`、剥离允许前缀、拒绝 `..`/绝对盘符/NUL、大小写策略可配置。  
**测试**：至少 30 个路径表格用例。

### Task 3.2 read-only canonical root

支持：

```text
gwy/jjfb.mrp
mythroad/gwy/jjfb.mrp
mythroad/sdk_key.dat
```

映射到 resolution root；所有命中记录 resolution reason。

### Task 3.3 writable overlay

`save/cache/temp` 写到用户数据目录；读取采用 overlay-first 或 profile 指定策略；source tree 永不写。

### Task 3.4 VFS trace and miss report

每个 open 输出 guest、normalized、backend、host、mode、result；按 unique guest 汇总 miss。

## Phase 4：VM 与基础 ABI

### Task 4.1 VmRuntime wrapper

封装 Unicorn init/map/run/stop；所有 guest call 通过一个入口；记录 call depth。

### Task 4.2 GuestMemory API

`guest_read/write/span/cstring`；加法溢出和映射边界检查；服务层不能直接裸 `uc_mem_read`。

### Task 4.3 基础 platform table

按 upstream 表结构提供 memory/file/time/log 等最低服务；服务注册为 typed table，不用巨型 switch。

### Task 4.4 identity + SDK key

把已验证 key 算法写成独立模块；输入 identity 可配置；生成 48-byte binary；golden SHA 测试。

## Phase 5：EXT loader

### Task 5.1 StartMrRunner

以 descriptor 指定 archive/entry/param 进入原始 start.mr；返回结构化 `EntryResult`。

### Task 5.2 ExtResolver exact lookup

先 exact member lookup；找不到时返回明确 miss，不在 guest 字面量上 patch。

### Task 5.3 reg.ext primary resolver

在解析可信时用 reg.ext；不可信时不猜。

### Task 5.4 profile alias fallback

仅在 exact miss 后执行声明式 alias；记录 requested/resolved/rule/profile/hash。  
**JJFB golden**：`cfunction.ext` → `robotol.ext`。

### Task 5.5 ExtLoader mapping/register

解压、分配、权限/对齐、helper/ER_RW runtime object、注册；不含游戏状态逻辑。

### Task 5.6 entry-return policy

`MR_SUCCESS/MR_IGNORE/MR_FAILED` 分开；profile 接受 `1` 必须要求 EXT 已成功解析、映射、注册等后置条件。

## Phase 6：平台注册和调度

### Task 6.1 PlatformRegistry

保存 guest 注册的 family/periodic/enqueue/callback/ext context；注册项带 owner generation 和有效期。

### Task 6.2 Scheduler queue

单线程、非重入、FIFO/priority 明确；guest call depth > 0 时只入队。

### Task 6.3 deterministic timer

monotonic clock + fake clock；due/loop/cancel；pause/resume 语义测试。

### Task 6.4 lifecycle state machine

只实现已文档化/跨目标确认的 `start/foreground/pause/resume/exit`；未知映射保持 trace-only。

### Task 6.5 replay harness

从 JSONL trace 重放注册和队列行为，用于不依赖 JJFB 的 scheduler 测试。

## Phase 7 以后首项

### Task 7.1 generic framebuffer

RGB565 logical framebuffer、pitch、dirty rect、host scale；不得识别 JJFB 资源名或固定地址。

完成 Task 7.1 前，不要开发 shell GUI、网络协议 mock 或游戏内部状态。
