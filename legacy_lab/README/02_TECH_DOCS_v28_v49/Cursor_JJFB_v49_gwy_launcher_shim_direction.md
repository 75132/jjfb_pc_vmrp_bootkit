# Cursor 开发说明 v49：方向纠偏 —— 不是还原 jjfb UI，而是仿冒泡网游启动链来启动 jjfb

> 用户再次明确：  
> 我们不是为了“重做/模仿机甲风暴 UI、动画”。  
> 游戏部分 `jjfb.mrp` 不能改、不能伪造、不能 host overlay。  
> 目标是：**仿冒泡网游/斯凯平台启动机甲风暴的方式，绕开 gwy 冒泡网游外壳的强制更新阻断，让 jjfb 作为原游戏正常被加载运行。**

---

## 0. 目标重新定义

### 正确目标

```text
模拟/补齐“冒泡网游平台启动机甲风暴”的启动协议和运行环境；
让原始 jjfb.mrp / mrc_loader.ext / robotol.ext 自己跑。
```

也就是：

```text
gwy/gbrwcore/gbrwshell 原本会做：
  游戏列表 -> 检查更新 -> no update / update ok -> startGame/runapp -> 加载 jjfb

现在 gwy 强制更新卡住；
我们绕开 gwy 外壳强制更新；
但要补齐它本来传给 jjfb 的启动上下文、参数、事件、平台函数。
```

### 错误目标

```text
❌ 重做机甲风暴启动 UI
❌ 手动仿 slogo/loadingbar/progress
❌ 改 jjfb 内部状态变量让动画动
❌ 用 host overlay 画假 UI
❌ 为了画面去 force ui_mode/progress/AC8
```

v43-v48 里对 UI/动画的追踪只保留为诊断信息，不能作为主路线。

---

## 1. 立即停止的做法

这些后续不要再作为主线：

```text
FORCE ui_mode=0x45
手动写 AC8
手动写 progress_count
progress driver
eventcode scan 只为了让 splash UI 动
slogo/loadingbar 像素级还原
chrome/310BB4 完整还原
```

原因：

```text
这些是在 jjfb 内部状态上“推 UI”；
不是仿冒泡网游启动方式；
会把路线带偏。
```

可以保留作为临时诊断，但不能当正式运行方案。

---

## 2. 新主线：做“GWY Launcher Shim / 冒泡网游启动器仿真层”

### 2.1 不改游戏，只补平台外壳

新目标模块命名建议：

```text
JJFB_GWY_LAUNCHER_MODE=1
```

含义：

```text
不通过 gwy UI；
不进入 gamelist 强制更新；
直接模拟 gwy 已完成检查更新后的 startGame/runapp 行为；
把 jjfb 原样作为目标游戏启动。
```

### 2.2 固定入口参数

cfg index=36 记录已知：

```text
title    = 机甲风暴(火爆公测)
target   = gwy/jjfb.mrp
napptype = 12
nextid   = 482
ncode    = 512
narg     = 0
narg1    = 1
nmrpname = gwy/jjfb.mrp
```

启动参数格式：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

v49 必须保证这个参数链是主入口，而不是直接从某个 UI mode 开始。

---

## 3. 重新审计冒泡网游原始启动链

必须回到 `gwy` 平台文件，而不是继续只看 `jjfb` splash：

```text
gwy/gbrwcore.mrp
gwy/gbrwcore.ext
gwy/gbrwshell.ext
gwy/gamelist.ext
gwy/vdload.ext
gwy/cfg.bin
gwy/jjfb.mrp
mrc_loader.ext
robotol.ext
```

### 3.1 输出启动链报告

生成：

```text
reports/v49_gwy_launch_chain.md
```

必须包含：

```text
1. cfg index=36 如何被读取；
2. startGame/runapp 调用点；
3. 参数字符串如何拼接；
4. 更新检查成功/无更新后跳到哪里；
5. runapp/startGame 最终调用了哪个平台 API；
6. _strCom(601/800/801) 在启动 jjfb 中的顺序；
7. 启动 jjfb 前 gwy 会设置哪些全局状态/文件路径/环境变量；
8. 当前我们 shim 缺了哪些。
```

---

## 4. 重点不是“绕过 jjfb 更新”，而是“绕过 gwy 强制更新”

区分两层：

```text
gwy 外壳更新：
  gamelist/vdload/gbrwcore 自己的强制更新；
  这是目前卡住的外壳。
  我们绕过/模拟其 no-update 后状态。

jjfb 游戏自身检查网络/更新：
  这是游戏运行后自己的流程；
  原则上不改、不跳；
  等它自然执行。
```

v49 不要把这两层混在一起。

---

## 5. 最小可行路线

### Route A：直接仿 post-update startGame

实现：

```text
1. 不启动 gwy UI；
2. 直接构造 cfg index=36 的 startGame 参数；
3. 调用和 gbrwcore startGame/runapp 等价的内部加载流程；
4. 让 mrc_loader.ext -> robotol.ext -> jjfb 自然接管。
```

重点要补：

```text
当前目录/资源根目录 = gwy/
nmrpname = gwy/jjfb.mrp
nextid = 482
napptype = 12
ncode = 512
_gwyblink 标志
MRP 文件打开路径规则
loader/ext 返回链
```

### Route B：运行 gbrwcore 但 patch 更新结果为 no-update

只作为备选，因为容易再次被强制更新卡住。

要做的是：

```text
只 patch gwy 外壳更新返回“无更新/已完成”；
不 patch jjfb 游戏内部逻辑。
```

---

## 6. 当前 v48 之前工作如何保留

保留这些“平台能力”：

```text
_mr_c_load / _strCom 601/800/801
mrc_loader.ext 加载
robotol.ext 加载
mrc_init ret=0
timer/event 基础分发
RGB565 0xF81F colorkey
240×320
obj=0 skip
真实资源 blit
```

但这些只作为平台兼容层，不作为 UI 还原目标。

废弃/降级：

```text
FORCE ui_mode=0x45
AC8/progress 手动 driver
针对 0x2EF86C 的 UI 人工推进
```

---

## 7. v49 具体任务

### P0：建立启动契约

请实现/输出：

```text
reports/v49_launch_contract.md
```

包含：

```text
argv/param string
working dir
resource root
mrp path
loader ext path
cfg fields
start event sequence
required platform calls
currently implemented?
missing?
```

### P1：新增 `RUN_V49_GWY_LAUNCHER_MODE.ps1`

运行方式：

```powershell
.\RUN_V49_GWY_LAUNCHER_MODE.ps1
```

内部不要 force `ui_mode=0x45`。  
而是：

```text
1. 初始化 vmrp；
2. 设置 gwy launcher context；
3. 注入 cfg index=36 参数；
4. 调用 startGame/runapp 等价入口；
5. 让 jjfb 自然进入自己的流程。
```

日志第一屏必须显示：

```text
[JJFB_GWY_LAUNCH] cfg_index=36 target=gwy/jjfb.mrp
[JJFB_GWY_LAUNCH] param=napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
[JJFB_GWY_LAUNCH] cwd=gwy root=...
[JJFB_GWY_LAUNCH] startGame/runapp equivalent called
```

### P2：监控是否进入真实游戏流程

不要以 splash 画面对错为第一指标。  
监控：

```text
mrc_loader.ext loaded?
robotol.ext loaded?
jjfb.mrp opened?
cfg param parsed?
game resources requested?
game self check/update strings read?
socket/network API called?
```

输出：

```text
reports/v49_gwy_launcher_run_result.md
```

### P3：如果仍然卡住，再查缺的“gwy 启动上下文”

例如：

```text
是否缺 nextid/ncode/narg1；
是否缺 _gwyblink；
是否路径 cwd 不对；
是否 file open root 不对；
是否 mrc_loader 需要来自 gbrwcore 的某个全局；
是否 runapp 前应有 mr_registerAPP / sendAppEvent；
是否缺 sky sdk_key/用户信息/平台 ID。
```

这才是正确的缺口方向。

---

## 8. 成功标准

### 最低成功

```text
不通过 gwy 强制更新；
不 force jjfb ui_mode；
由 GWY launcher shim 直接启动 gwy/jjfb.mrp；
mrc_loader/robotol/jjfb 自然运行。
```

### 中级成功

```text
jjfb 自己请求启动资源、启动检查 UI、检查网络/更新文字；
这些由游戏自然触发，不靠 host 写 progress/AC8。
```

### 高级成功

```text
进入游戏自身 login/server/network 流程。
```

---

## 9. 给 Cursor 的一句话

**方向纠偏：我们不是在还原机甲风暴 UI，也不是改 jjfb 内部状态；目标是仿冒泡网游 gwy 的启动游戏方式，绕过 gwy 外壳强制更新，补齐 startGame/runapp 对 jjfb 的启动契约。v49 请停止 force ui_mode/AC8/progress，回到 gbrwcore/gamelist/cfg.bin 的启动链，建立 GWY Launcher Shim：使用 cfg index=36 的参数 `napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink` 直接启动原始 `gwy/jjfb.mrp`，并审计缺失的 cwd/root/nextid/ncode/_gwyblink/loader 运行上下文。**
