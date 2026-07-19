# Cursor 开发说明 v45：加速跑游戏 —— 放弃 forced splash 美化，追“启动事件/状态写入”缺口

> v44 已证明：卡点不是坐标、不是透明色、不是 2EC6B0 返回值，而是**游戏状态机没有自然启动**。  
> 用户现在明确要求：不是开发模拟器，而是尽快让《机甲风暴》跑起来。  
> v45 的目标：找到并补上“让 ui_mode 从 0 进入启动流程”的最小事件/回调缺口，推动游戏离开 0x45，进入 progress / login / network。

---

## 0. 当前最重要结论

不要再把主要时间花在 UI 外观上。

当前画面/动画不对的根因不是“画得不好”，而是：

```text
当前看到的 splash 是 host 强推 ui_mode=0x45 后进入的局部分支；
不是游戏自己从 ui_mode=0 自然启动出来的完整流程。
```

所以：

```text
强推 0x45 -> 只在 0x306344 -> 0x30662C -> 0x2EF86C 空转
AC8 无 guest write
BA0+0x2C progress 无 guest write
ui_mode 无 guest write
2EC6B0 ret=0/1/obj/pixels 都不能推进
```

这说明修 UI/动画必须先修状态机。  
**动画不对 = 状态没推进，不是继续修贴图。**

---

## 1. v45 优先级

### P0：找“谁应该写 ui_mode”

地址：

```text
ui_mode/state = ERW + 0x8D0 = 0x2B2120
```

v44 结论：

```text
noforce：ui_mode 一直 0
force45：唯一写 0x45 的是 host FORCE
guest 从不写 ui_mode
```

v45 必须回答：

```text
哪个函数/事件本应写 ERW+0x8D0？
它为什么没执行？
是缺 mrc_event？
是缺 timer callback？
是缺 sendAppEvent 返回？
是缺 app resume/start/focus 事件？
是缺网络/更新回调？
```

---

### P1：找“谁应该写 progress”

地址：

```text
progress_count = ERW + 0xBA0 + 0x2C = 0x2B241C
```

v44 结论：

```text
bar/textbar 已构造
loadingbar 已画
但 progress_count 永远 0
所以 bar idx=0..11 全 SKIP
```

v45 必须回答：

```text
progress_count 是由 timer 递增？
由资源加载完成回调递增？
由 2EC6B0/draw 完成回调递增？
由 0x10134 / 2d92dc 完成回调递增？
还是由另一个 app event 写？
```

---

### P2：追自然 mode=0 路径

不要只看 forced 0x45。  
自然模式下：

```text
ui_mode=0
0x306344 dispatch head 每 tick 执行
但不进 0x2EF86C
```

v45 必须追：

```text
ui_mode=0 时 0x306344 实际走哪个分支？
mode0 target 是谁？
mode0 target 做了什么？
它是不是在等某个 start/resume/timer/platform event？
```

日志不要再只写：

```text
target=dispatch_head
```

必须具体到：

```text
[JJFB_MODE0_FLOW] pc=... call=... ret=... r0-r7
```

---

## 2. 加速方案：事件矩阵自动跑，不要手动猜

目前最大问题是“不知道缺哪个启动事件”。  
v45 直接做一个事件/回调矩阵，自动跑，找哪个能写 ui_mode / progress / AC8。

### 2.1 基线

每轮运行都从：

```text
mrc_init ret=0
robotol loaded
timer running
ui_mode=0
```

开始，不强推 0x45。

### 2.2 自动测试事件序列

尝试不同 host-side ext event / mrc_event / sendAppEvent 组合。  
不要一次只手动试一个。

建议矩阵：

```text
SEQ_A: current baseline
SEQ_B: init -> resume
SEQ_C: init -> focus/active
SEQ_D: init -> timer only
SEQ_E: init -> start event -> timer
SEQ_F: init -> resume -> timer -> paint/refresh event
SEQ_G: init -> app event 6 -> 8 -> 0 -> 2
SEQ_H: init -> code 0/1/2/3/4/5/6/7/8/9 单步扫描
SEQ_I: timer period 33ms / 50ms / 100ms 对照
```

具体 code 名字可以先未知，但日志必须记录：

```text
[JJFB_EVENT_TEST] seq=... event=... param0=... param1=...
```

每个 seq 跑 5~10 秒，捕获：

```text
UIMODE_WRITE
AC8_WRITE
PROGRESS_COUNT_WRITE
SPLASH_ENTER
LOGIN/NETWORK
```

自动生成表：

```text
reports/v45_event_matrix.md
```

字段：

```text
seq
events
ui_mode_written?
ui_mode_value
AC8_written?
progress_written?
splash_enter?
login_string?
initNetwork?
socket/connect?
result
```

成功判断：

```text
只要某个序列能让 guest 写 ui_mode/progress/AC8，就锁定这条事件链。
```

---

## 3. 静态 + 动态结合：找写 ui_mode 的代码

仅靠运行 watch 不够，因为现在写点可能根本没执行。  
v45 需要做静态扫描。

### 3.1 动态 watch ERW 整段，而不是只 watch 三个地址

当前只看了：

```text
0x8D0
0xAC8
0xBA0+0x2C
```

v45 扩大到：

```text
ERW+0x800 ~ ERW+0xC00
```

记录所有写入：

```text
[JJFB_ERW_WRITE] off=0x... old=... new=... pc=... lr=...
```

目的：

```text
找状态结构附近是否还有其他 gate/counter 被写；
可能真正 mode 不是 0x8D0，而是相邻字段。
```

### 3.2 静态扫描疑似写 ERW+8D0 的函数

在 robotol/ext code 区做静态 xref：

```text
查找访问 ERW+0x8D0 / 0xAC8 / 0xBA0 / 0xBA0+0x2C 的指令附近函数
输出：
function_start
pc
instruction
target_offset
调用者
```

报告：

```text
reports/v45_state_write_xrefs.md
```

如果静态很难精确，至少把所有访问 `0x8D0 / 0xAC8 / 0xBA0 / 0x2C` 常量或附近偏移的函数列出来。

---

## 4. 追 sendAppEvent / 0x101xx 是否缺回调

v44 已经证明画图返回值不是 blocker。  
现在要看平台消息是否少了“完成回调”。

重点盯：

```text
0x10134 资源构造
0x10132 alloc
0x10140 handler/timer
0x10120 / 0x10162 / 0x10165
sendAppEvent extCode/app/code/param0/param1
```

每个平台消息都要记录：

```text
caller pc/lr
返回值
是否写 ERW 附近状态
是否紧接着读 ui_mode/progress
```

特别是资源完成链：

```text
2d92dc -> 0x10132 -> 0x10134 -> 2D958C object bind
```

需要查：

```text
对象绑定后是否应触发“load complete”事件？
是否需要 host 调用某个 callback？
是否 0x10134 除了返回 pixels，还应设置完成状态？
```

但不要回去改 0x10134 返回 object；v38 已确认必须返回 pixel ptr。

---

## 5. 快速实用策略：可临时 nudge 但必须验证能否继续跑

为了加快跑游戏，可以做受控 nudge，但不能把它当原版成果。

### 5.1 progress nudge 试验

既然 `progress_count=0` 导致 bar 全 SKIP，可以做一个试验：

```text
JJFB_PROGRESS_NUDGE=1
在 loadingbar/bar/textbar 三对象全部绑定后，把 BA0+0x2C 从 0 设成 1、6、12
```

观察：

```text
bar 是否画出
ui_mode 是否离开 0x45
是否进入 login/server/network
```

这不是最终修复，只是判断：

```text
progress_count 是否是当前后续流程的唯一 gate。
```

### 5.2 ui_mode nudge 试验

不要长期固定 0x45。  
可以扫描候选模式：

```text
0x01, 0x02, 0x10, 0x20, 0x40, 0x45, 0x46, 0x47, 0x50
```

但目的不是显示不同画面，而是看：

```text
哪个 mode 进入 login/server/network 或写 progress。
```

生成：

```text
reports/v45_uimode_scan.md
```

---

## 6. UI/画面修复只保留最低限度

当前截图中紫块、青块、位置不对都影响观感，但现在不是主线。

保留最低修复：

```text
1. 0xF81F colorkey 继续保留；
2. 2F9968=240 / 2F995C=320 保留；
3. obj=0 2EC6B0 skip、不 dirty 保留；
4. 240×320 固定。
```

暂停：

```text
1. chrome 完整还原；
2. wy_jiao/wy_xian 细节；
3. 310BB4 完整 ABI；
4. 原生 refresh 完美化；
5. slogo 像素级位置细调。
```

---

## 7. v45 运行矩阵

### Run A：自然 + 事件矩阵

```powershell
$env:JJFB_FORCE_UI_MODE="0"
$env:JJFB_EVENT_MATRIX="1"
.\RUN_V45_EVENT_MATRIX.ps1
```

输出：

```text
reports/v45_event_matrix.md
logs/v45_event_matrix_*.txt
```

### Run B：progress nudge

```powershell
$env:JJFB_FORCE_UI_MODE="45"
$env:JJFB_PROGRESS_NUDGE="1"
$env:JJFB_PROGRESS_VALUE="12"
.\RUN_V45_GAME_NUDGE.ps1
```

观察：

```text
bar/textbar 是否画
ui_mode 是否离开 0x45
login/network 是否出现
```

### Run C：ui_mode scan

```powershell
$env:JJFB_UIMODE_SCAN="1"
.\RUN_V45_UIMODE_SCAN.ps1
```

扫描候选 mode，目标不是看 UI，而是找可推进状态。

---

## 8. v45 成功标准

### 最低成功

```text
找出一个事件序列、mode、或 progress nudge 能让状态发生变化。
```

### 中级成功

```text
BA0+0x2C progress > 0；
bar/textbar 开始画；
ui_mode 离开 0x45。
```

### 高级成功

```text
出现 login/server/update 相关字符串或资源。
```

### 游戏目标成功

```text
出现 initNetwork / socket / connect；
开始处理网游登录或服务器流程。
```

---

## 9. 给 Cursor 的一句话

**v44 证明 2EC6B0 返回值不是 blocker，forced 0x45 只是局部 splash 空转。v45 请停止继续修 UI 外观，转为找“启动事件/状态写入”缺口：做事件矩阵自动跑、静态扫描 ui_mode/AC8/progress 写点、动态 watch ERW+0x800~0xC00、测试 progress/uimode nudge 是否能推进到 login/network。保留 colorkey、240×320 和 obj=0 skip，但不要再把 chrome/310BB4/UI 还原作为主线。**
