# Cursor 重新开发路线图

## Phase 0：冻结旧实验，建立干净仓库

### 任务

- 把当前 bootkit 完整保留为 `legacy_lab`；
- 从原始 `vmrp-master` 建 clean source；
- 配置 32 位 MinGW 构建；
- 加入 `audit_launcher_core.py`；
- 建立最小 CI/本地检查。

### 完成标准

- clean `bridge` 与 upstream 差异极小；
- 能启动 upstream 示例 MRP；
- core 扫描不存在固定 JJFB 地址/offset；
- 当前原始资源不被复制进 Git（用本地路径配置）。

## Phase 1：只读格式工具

### 文件

```text
src/formats/mrp_archive.*
src/formats/gwy_cfg.*
src/formats/reg_ext.*
tools/mrp_inspect
tools/gwy_cfg_inspect
```

### 任务

- 解析 MRPG header/index；
- gzip/raw member；
- hash；
- parse index 36；
- 提取 reg.ext 模块清单；
- 比较外部 cfg 与 gamelist 内 cfg。

### 完成标准

- 对六个关键 MRP 输出与本包 evidence 一致；
- 能读出 JJFB `1514/219/161178`；
- 解析错误时 fail closed。

## Phase 2：LaunchDescriptor 与 profile

### 任务

- JSON schema validation；
- cfg/profile 交叉验证；
- parameter serializer；
- MRP APPID/APPVER/hash；
- immutable launch context。

### 完成标准

- JJFB descriptor 输出稳定 JSON；
- 参数字符串完全一致；
- 改错 hash/index/target 时拒绝启动。

## Phase 3：VFS 与 writable overlay

### 任务

- guest path normalization；
- canonical read-only root；
- save/cache overlay；
- `mythroad/`、`gwy/` 前缀；
- detailed trace；
- path traversal tests。

### 完成标准

- `gwy/jjfb.mrp`、`mythroad/gwy/jjfb.mrp` 均准确命中；
- sdk key canonical 命中；
- 游戏写文件不修改 source tree；
- 0 unexpected FILEOPEN_MISS in loader stage。

## Phase 4：干净 VM + platform table

### 任务

- Unicorn memory map；
- bridge stubs；
- pointer validation；
- memory/file/time/identity；
- structured trace。

### 完成标准

- 平台 stub 不含 target-specific code；
- example MRP 可以 init/exit；
- 越界 guest pointer 被拒绝。

## Phase 5：通用 EXT Loader/Resolver

### 任务

- 执行 `start.mr`；
- SDK key；
- `_mr_c_load`/`_strCom`；
- exact member lookup；
- reg.ext primary module；
- profile alias fallback；
- EXT registration object。

### 完成标准

- 原始 JJFB 不改；
- 不 patch guest literal；
- 加载 `mrc_loader.ext` 和 `robotol.ext`；
- init=0；
- helper/ER_RW 只作为 runtime metadata。

## Phase 6：PlatformRegistry 与 Scheduler

### 任务

- service table；
- registration calls；
- non-reentrant queue；
- timer deadlines；
- input events；
- pause/resume/foreground；
- deterministic replay。

### 完成标准

- callback 只从 registry 获取；
- core 无固定 guest callback 地址；
- event 产生不检查 JJFB ERW；
- 在测试 fixture 中完成 register→timer→callback。

## Phase 7：显示与输入

### 任务

- RGB565 framebuffer；
- guest 240×320；
- host 320×480 stretch；
- DrawBitmap/Rect/Char/DispUpEx；
- dirty present；
- keyboard/mouse mapping。

### 完成标准

- 不识别游戏资源名；
- 不 host 代画 splash；
- image/pitch/colorkey 单元测试通过；
- 无闪烁修复依赖 game PC。

## Phase 8：网络平台

### 任务

- socket/DNS/connect/send/recv；
- async completion events；
- trace endpoint；
- optional deny/offline mode；
- 不伪造成功登录。

### 完成标准

- 游戏自然调用网络时能看到 endpoint 与 API 顺序；
- 关闭网络返回一致错误；
- 启用网络不阻塞 emulator thread。

## Phase 9：JJFB integration profile

### 允许的 target-specific 内容

- cfg index/fields；
- target hash；
- display profile；
- SDK identity test vector；
- `cfunction.ext → robotol.ext` resolver alias；
- entry return 1 的严格 accepted condition。

### 禁止

- 任何 JJFB code address；
- 任何 ERW offset；
- FORCE/injection；
- host UI。

### 完成标准

- 运行日志显示原始 MRP hash；
- loader 完整；
- registry 收到平台注册；
- scheduler 运行；
- `audit_launcher_core.py` 通过。

## Phase 10：研究真实 GWY shell（可并行）

目标不是把 shell 重新作为依赖，而是补文档：

- cfg 读取方式；
- 游戏选择到 launch descriptor 的映射；
- lifecycle 顺序；
- platform service code 语义；
- 更新完成后的跳转。

方法：

- 静态 MRP/EXT 分析；
- 与其他 `gwy/*.mrp` 横向对照；
- 只读 trace；
- 不连接未知旧服务端；
- 不把实验代码合入 core。

## 每个 Phase 的 Cursor 输出格式

```text
1. 本阶段目标
2. 修改文件
3. 新增测试
4. 运行命令
5. 实际证据
6. 未知项
7. 是否满足完成标准
8. 下一阶段，不跨阶段乱做
```
