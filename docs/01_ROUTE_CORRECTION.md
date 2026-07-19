# 方向纠偏：从“扶游戏状态”回到“复刻外壳启动方法”

## 1. 用户真正要解决的问题

原冒泡网游/GWY 外壳在进入游戏列表或点击游戏时要求联网、登录或强制更新。旧服务已经不可用，导致原本合法存在于本地的 MRP 游戏无法被外壳启动。

项目应解决的是：

```text
复刻外壳的启动职责，而不是复刻外壳 UI；
绕过已失效的外壳强制联网阻断，而不是修改游戏自身逻辑；
为目标 MRP 提供它原本从平台获得的启动参数和运行时服务。
```

## 2. 正确系统边界

### 2.1 启动器负责

- 发现 `mythroad` 资源根；
- 读取 GWY 游戏列表配置；
- 选择游戏记录；
- 构造 `startGame/runapp` 等价参数；
- 设置目标 MRP、工作目录、VFS 命名空间；
- 初始化 Mythroad 平台 ABI；
- 执行 MRP `start.mr`；
- 提供 EXT 加载、内存、文件、显示、时间、事件、timer、网络等平台服务；
- 根据“平台注册的 handler/callback”执行通用调度；
- 记录结构化日志和 blocker。

### 2.2 启动器不负责

- 写 `ERW+0x8D0` 让 JJFB 进入 `ui_mode=0x45`；
- 写 B70/B71/B6C/AC8/134D/progress；
- 跳转到 JJFB 的固定 PC；
- host 画 slogo/loadingbar 或“检查更新”界面；
- 修改 `jjfb.mrp`、`robotol.ext` 或模块 EXT 的游戏逻辑；
- 伪造“已登录/已连接游戏服务器”；
- 为了看到画面而绕过游戏自身状态机。

## 3. 两层“更新”必须永久分开

### GWY 外壳更新层

相关资产：

```text
gbrwcore.mrp
gbrwshell.mrp
gamelist.mrp
vdload.mrp
cfg.bin
```

它负责外壳游戏列表、模块更新与下载。`vdload.ext` 中可见：

```text
spd.skymobiapp.com:6009
/simpleDownload
/continueDownload
/dl_confirm
```

这一层是要绕开的失效阻断。最稳妥方式不是伪造旧服务器，而是**不启动这套 UI/更新状态机，直接执行它最终的本地游戏启动职责**。

### JJFB 游戏自身网络层

由 `jjfb.mrp → robotol.ext → game modules` 触发。它可能仍会访问游戏登录、更新或服务器。这属于游戏自己的运行过程。

启动器只应提供 socket/DNS/网络 ABI，并记录请求；是否继续做离线服务端兼容，是另一个独立项目阶段。

## 4. 为什么此前路线会偏

`start.mr`、`mrc_loader.ext`、`robotol.ext` 被加载后，当前平台没有完整复刻生命周期和回调调度，于是研究逐渐转向：

- 追踪某个 JJFB 固定地址；
- 找谁写某个 ERW 字节；
- 手工补 app=C0、事件 5/12、B70/B71；
- 修画面/进度/闪烁。

这些实验获得了有价值的 ABI 线索，但它们是“观察某个游戏的后果”，不是“复刻平台的原因”。

正确重构应把问题改写为：

```text
错误问法：怎样让 0x2DADC4 被调用？
正确问法：平台注册了哪些 handler，标准生命周期何时调用这些 handler？

错误问法：怎样让 B71 变成 1？
正确问法：哪个平台事件/回调在标准 Mythroad 生命周期中产生，注册和调度协议是什么？

错误问法：怎样画出 splash？
正确问法：显示 API、资源加载和 refresh/present 的平台语义是否正确？
```

## 5. 正确的技术策略

### 主路线：Direct Post-Update Launcher

```text
解析 cfg.bin
→ 选择 index 36
→ 生成 LaunchDescriptor
→ 初始化平台服务
→ start_mrp(gwy/jjfb.mrp, start.mr, params)
→ start.mr 自然加载主 EXT
→ 平台根据注册表调度 lifecycle/timer/event
```

优势：

- 不依赖失效 GWY 更新服务器；
- 不需要运行 gamelist UI；
- 能推广到 `gwy` 下其他 MRP；
- 核心可测试；
- 游戏差异可以 profile 化。

### 辅助路线：Shell Trace Laboratory

单独保留一个只读研究工具，用来启动/静态分析 `gbrwcore/gamelist`，采集：

- 配置读取；
- MRP/EXT 成员；
- 平台调用；
- `runapp/startGame` 参数；
- 更新分支。

它不能成为日常启动器，也不能和 clean core 混编。

## 6. 成功定义

### 启动器工程成功

- 能解析任意指定 GWY MRP 与 cfg 记录；
- 核心无 JJFB 固定地址；
- 核心无 ui_mode/AC8/B70/B71 注入；
- MRP/EXT 加载由通用解析器完成；
- 事件由注册式 scheduler 驱动；
- JJFB 只通过 profile 描述差异；
- 原始 MRP 哈希始终不变。

### JJFB 运行成功

- 原始 `jjfb.mrp` 打开；
- `start.mr` 运行；
- `mrc_loader.ext` 与 `robotol.ext` 加载；
- robotol 注册平台 handler/callback；
- 标准 lifecycle/timer/event 被调度；
- 游戏自然请求 `jjfbol`、BMP、模块或网络；
- 不依靠游戏内部状态 FORCE。
