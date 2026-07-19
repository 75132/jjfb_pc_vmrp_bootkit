# MASTER_CONTEXT_HANDOFF_v2 — JJFB / 冒泡网游 GWY Launcher Shim 项目

## 1. 项目目标

目标是让老 MRP 网游《机甲风暴》`gwy/jjfb.mrp` 在 PC 版 vmrp / Mythroad 环境中启动运行。原冒泡网游 `gwy` 外壳存在强制更新/登录/更新阻断，导致无法正常从游戏列表进入机甲风暴。

当前最终方向不是“重做机甲风暴 UI”，而是：

```text
仿冒泡网游/gwy 平台启动机甲风暴的方式，绕过 gwy 外壳强制更新，补齐 startGame/runapp 对 jjfb 的启动契约，让原始 jjfb.mrp 自己跑。
```

核心原则：

```text
- 游戏部分 jjfb.mrp 不改、不伪造、不 host overlay；
- 不把目标变成开发完整模拟器；
- 平台兼容只补“启动 jjfb 所必需”的缺口；
- v43-v48 的 UI/动画分析是诊断，不是最终主路线；
- 最新主路线以 v49：GWY Launcher Shim 为准。
```

## 2. 已知关键文件/组件

```text
gwy/cfg.bin                 游戏列表配置，index=36 是机甲风暴
gwy/jjfb.mrp                目标游戏文件
gwy/gbrwcore.mrp/.ext       冒泡网游核心/启动相关
gwy/gbrwshell.ext           外壳相关
gwy/gamelist.ext            游戏列表和更新 UI/强制更新字符串
gwy/vdload.ext              下载/更新链，含 spd.skymobiapp.com:6009
mrc_loader.ext              EXT loader
robotol.ext                 目标游戏后续 native/runtime 模块
start.mr                    启动脚本
```

## 3. cfg index=36：机甲风暴启动参数

从 `gwy/cfg.bin` 得到的机甲风暴记录：

```text
title    = 机甲风暴(火爆公测)
icon     = ng_jjfb.gif
target   = gwy/jjfb.mrp
napptype = 12
nextid   = 482
ncode    = 512
narg     = 0
narg1    = 1
nmrpname = gwy/jjfb.mrp
```

参数字符串格式：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

v49 要求以这个为启动契约，模拟 `gwy` 外壳完成更新检查后的 `startGame/runapp` 行为。

## 4. 旧路线结果概览

### Android/抓包路线

- 通过 `gwy.mrp` 可以进入冒泡网游外壳，但点击游戏会弹强制更新。
- 曾观察端口/域：`spd.skymobiapp.com:6009`，路径 `/simpleDownload`、`/continueDownload`、`/dl_confirm`。
- mock `dyd.apk` 返回 200 无效，说明当前阻断不只是 APK 文件。
- 用户强调只希望劫持/模拟必要链路，不希望提前乱劫持 20000；曾聚焦 21002/21003。

### PC vmrp 路线

- PC vmrp 默认固定启动 `mythroad/dsm_gm.mrp`，CLI args 原本无效。
- 已验证参数注入可以到达：
  `napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink`
- 曾修过 `sdk_key` 分支，`start.mr:143 cann\`t find sdk key!` 相关跳过/修补。
- 直接把 `mrc_loader.ext` 当脚本会失败：`invalid control char`，因为 `.ext` 是 ARM/Thumb native，不是脚本。
- 需要支持 `_mr_c_load` / `_strCom(601/800/801)`。

## 5. 已完成的重要兼容能力

```text
_strCom(601,"mrc_loader.ext")
_strCom(800) 加载 mrc_loader.ext
_strCom(801) 返回 robotol.ext buffer
第二次 800 加载 robotol.ext
mrc_init(0) ret=0
timer/event/main loop 部分运行
mr_registerAPP 最小实现
sendAppEvent 5 参数语义部分实现
0x10134 资源构造链明确，必须返回 pixel ptr
RGB565 0xF81F colorkey 识别与跳过
240×320 轴修正：2F9968=240, 2F995C=320
obj=0 的 2EC6B0 skip/no dirty
```

## 6. v35-v48 UI/状态机诊断结果

这些分析解释了为什么之前“强推 UI”不是最终路线：

- `ui_mode=0x45` 被认为是 splash/loading/check UI 状态。
- 强推 `ui_mode=0x45` 可以进入 `0x306344 -> 0x30662C -> 0x2EF86C`，但只是局部分支空转。
- `loadingbar/bar/textbar` 对象可以构造，`loadingbar` 可画，`bar` 在 `progress_count > 0` 时也能画。
- `progress_count = ERW + 0xBA0 + 0x2C = 0x2B241C`。
- `AC8 = ERW + 0xAC8`，AC8>0 常进入 slogo 分支，AC8=0 常进入 loadingbar 分支。
- guest 自然 0 次写 AC8 / progress_count / ui_mode；手动 progress driver 可让 bar 从 0→12 画出，但仍不触发“检查更新/网络”字符串读取。
- `2EC6B0` 返回值 R0=0/1/obj/pixels 均无法推进状态。
- 结论：强推/修 `jjfb` 内部 UI 状态不是正确主路线。

## 7. 最新方向：v49 GWY Launcher Shim

v49 的方向纠偏：

```text
不要继续主线 force ui_mode/AC8/progress。
回到 gbrwcore/gamelist/cfg.bin/startGame/runapp。
做 GWY Launcher Shim：模拟冒泡网游外壳已完成更新检查后的启动契约，直接启动原始 gwy/jjfb.mrp。
```

需要审计/补齐：

```text
- cwd / root / 文件打开路径规则；
- nmrpname=gwy/jjfb.mrp；
- nextid=482 / ncode=512 / narg1=1；
- _gwyblink 标志；
- startGame/runapp 等价入口；
- mrc_loader/robotol 加载链；
- gbrwcore 在启动 jjfb 前设置的全局状态；
- 是否缺 mr_registerAPP / sendAppEvent / platform ID / sdk/user info。
```

## 8. 下一步给 Cursor 的任务

1. 生成 `reports/v49_gwy_launch_chain.md`：静态审计 `gbrwcore/gamelist/cfg.bin` 如何读取 index=36 并调用 startGame/runapp。
2. 生成 `reports/v49_launch_contract.md`：列出 argv、cwd、root、mrp path、loader ext、cfg fields、start event sequence、平台调用与缺口。
3. 新建 `RUN_V49_GWY_LAUNCHER_MODE.ps1`：不 force `ui_mode=0x45`，而是设置 gwy launcher context，注入 cfg index=36 参数，调用 startGame/runapp 等价入口。
4. 运行后监控：`mrc_loader.ext loaded? robotol.ext loaded? jjfb.mrp opened? cfg param parsed? game resources requested? self update/check strings read? socket/network API called?`
5. 如果卡住，优先查缺的 gwy 启动上下文，而不是回去推 jjfb 内部 UI。

## 9. 对新对话/新助手的提醒

不要把这个项目理解成“生成/模仿游戏 UI”。所有 UI/动画文档只是曾经的诊断记录。用户当前要的是：绕开冒泡网游外壳强制更新，模拟其启动游戏的方式，让原游戏自己运行。
