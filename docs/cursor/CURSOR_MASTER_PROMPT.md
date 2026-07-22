# Cursor 主提示词：从零重建 GWY/MRP 独立启动器

把本文件完整复制到 Cursor 新对话。不要只贴一两段。

---

你现在负责重新开发一个 **GWY/MRP 独立外壳启动器**。项目用于本地研究和数字保存：读取用户合法持有的 Mythroad/GWY 资源，绕开已经失效的旧外壳强制联网/更新界面，按原平台的启动契约运行原始目标 MRP。

## 一、绝对目标

最终程序应类似：

```text
gwy_launcher.exe launch --profile profiles/jjfb.json
```

它必须完成：

1. 发现并验证 `mythroad/<resolution>/gwy` 资源树；
2. 解析 `cfg.bin`，从记录中得到目标 MRP、图标、标题和启动参数字段；
3. 读取 MRP header/index、APPID、APPVER、成员表和 hash；
4. 构造不可变的 `LaunchDescriptor`；
5. 初始化干净 vmrp/Mythroad runtime；
6. 提供通用 VFS、identity、SDK key、memory、file、display、timer、event、network ABI；
7. 执行原始 `start.mr`；
8. 通过通用 `ExtResolver` 加载 MRP 内 EXT；
9. 记录 guest 注册的平台 handler，并由非重入 Scheduler 调度生命周期、timer、input 和 network completion；
10. 保持原始 `gwy/jjfb.mrp` 哈希不变。

## 二、项目纠偏：禁止继续旧路线

本项目不是：

- 修改 JJFB 的 `ui_mode`；
- 写 ERW+B70/B71/B58/AC8/BA0 等偏移；
- hook 固定游戏地址；
- host 代画 splash/loading；
- 为了让截图变化而注入 C0、event 5/12 或 progress；
- 继续在 377KB 的旧 `bridge.c` 上叠 v87/v88 补丁；
- 模拟旧更新服务器或伪造登录成功。

出现上述需求时，停止并将实验放入 `legacy_lab`，不能合入 clean core。

## 三、工程起点

1. 从用户包中的原始/干净 `runtime/vmrp_src/vmrp-master` 建新仓库或 clean 分支；
2. 当前高度修改的 bootkit 全部冻结为 `legacy_lab`；
3. 不复制旧修改版 `bridge.c` 作为起点；
4. 先实现格式、描述符、VFS 和测试，再接入 VM；
5. 每次只推进本包 `docs/08_REDEVELOPMENT_ROADMAP.md` 的一个 Phase。

## 四、必须阅读的文件顺序

```text
00_READ_ME_FIRST.md
docs/01_ROUTE_CORRECTION.md
docs/02_CURRENT_STATE_EVIDENCE.md
docs/03_TARGET_ARCHITECTURE.md
docs/04_MRP_GWY_MODULE_ANALYSIS.md
docs/05_LAUNCH_CONTRACT.md
docs/06_PLATFORM_ABI_AND_SCHEDULER.md
docs/07_REUSE_AND_MIGRATION.md
docs/08_REDEVELOPMENT_ROADMAP.md
docs/15_FIRST_30_TASKS.md
docs/16_CLEAN_REPO_LAYOUT_AND_API_CONTRACTS.md
docs/20_REFERENCE_IMPLEMENTATION_PLAN.md
```

同时加载：

```text
cursor/.cursor/rules/JJFB_LAUNCHER_REBUILD.mdc
cursor/.cursor/skills/jjfb-gwy-launcher-rebuild/SKILL.md
```

## 五、已锁定的 JJFB 启动证据

这些是验证向量，不是让你写固定内存补丁：

```text
cfg index = 36
target    = gwy/jjfb.mrp
napptype  = 12
nextid    = 482
ncode     = 512
narg      = 0
narg1     = 1
flag      = gwyblink
APPID     = 400101
APPVER    = 12
MRP SHA256= 52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036
```

启动参数：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

原始目标 MRP 的关键成员：

```text
start.mr       stored=1514 decoded=3787
mrc_loader.ext stored=219  decoded=232
robotol.ext    stored=161178 decoded=253420
reg.ext        stored=1381 decoded=2472
```

已验证最低 loader 链：

```text
cfg/descriptor
→ original gwy/jjfb.mrp
→ start.mr
→ canonical sdk_key.dat
→ mrc_loader.ext
→ logical cfunction.ext request
→ profile/resolver alias robotol.ext
→ version(6)
→ appInfo(8)
→ init(0)=0
```

alias 必须在 archive resolver 层完成：

```text
cfunction.ext → robotol.ext
```

禁止再修改 guest `ext_base+0xD4` 字面量。

## 六、架构约束

核心必须拆成以下模块，禁止再造巨型 bridge：

```text
formats/mrp_archive
formats/gwy_cfg
formats/reg_ext
launcher/launch_descriptor
launcher/launch_state_machine
vfs/guest_vfs
runtime/vm_runtime
runtime/ext_loader
runtime/ext_resolver
platform/platform_registry
platform/platform_scheduler
platform/platform_memory
platform/platform_file
platform/platform_identity
platform/platform_display
platform/platform_timer
platform/platform_event
platform/platform_network
trace/trace_jsonl
profiles/profile_loader
```

核心模块不得包含：

- `jjfb` 固定地址；
- 固定 ERW offset；
- v55/v56 等实验版本名；
- FORCE 环境变量；
- 基于屏幕是否变化的状态推进；
- 网络业务成功伪造。

JJFB 差异只允许存在于：

```text
profiles/jjfb.json
integration tests
evidence/
legacy_lab/
```

## 七、开发工作法

每个任务必须按以下顺序：

1. 写明当前 Phase 和单一目标；
2. 先读对应 evidence/golden data；
3. 先补测试；
4. 实现最小通用模块；
5. 运行构建、单测、集成测试和反跑偏审计；
6. 输出实际命令和完整结果；
7. 明确未知项，不能把猜测写成事实；
8. 未满足完成标准时不得进入下一 Phase。

每轮回复固定格式：

```text
本轮阶段：Phase X / Task Y
目标：
修改文件：
实现说明：
测试命令：
测试结果：
证据：
未知/风险：
反跑偏审计：通过/失败
是否满足阶段完成标准：是/否
下一项：
```

## 八、第一轮只执行这项

不要立即碰 VM 或 JJFB 启动。先执行：

```text
Phase 0 / Task 0.1：建立 clean repository
```

要求：

- 导入 upstream vmrp 为固定快照；
- 创建本包建议目录；
- 加入 `tools/mrp_inspect.py`、`tools/gwy_cfg_inspect.py`、`tools/audit_launcher_core.py`；
- 加入 schema/profile；
- 把旧 bootkit 作为外部只读 `legacy_lab` 路径，不参与构建；
- 建立 32 位 Windows build script；
- 建立最小 test runner；
- 跑反跑偏审计；
- 不修改旧 bridge，不启动 JJFB。

完成后停下，按固定格式汇报，不要擅自跨到 Phase 1。
