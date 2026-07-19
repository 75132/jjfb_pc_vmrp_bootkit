# Cursor 开发说明 v47：启动检查动画不跳过 —— 锁定 progress/AC8 上游 writer 与检查更新阶段入口

> v46 已确认：0x45 内部不是“画面不对”那么简单，而是启动检查 UI 状态机没有自然推进。  
> 现在不要继续泛泛修 UI，也不要立刻跳到网络。v47 只做两件事：  
> **找谁该写 AC8 / progress_count；找检查网络/检查更新阶段为什么没进入。**

---

## 1. v46 已经确定的事实

### 1.1 0x45 是启动检查 UI 状态

调用链：

```text
0x306344 dispatch
  -> ui_mode==0x45
  -> 0x30662C
  -> 0x2EF86C
```

0x2EF86C 内部：

```text
AC8 > 0:
    slogo 请求 / blit

AC8 == 0:
    loadingbar / bar / textbar 请求
    loadingbar blit
    progress loop 0x2EFA9E..0x2EFADA
```

---

### 1.2 progress_count 是启动条动画进度

地址：

```text
progress_count = ERW + 0xBA0 + 0x2C = 0x2B241C
```

语义：

```text
for idx in 0..11:
    if progress_count > idx:
        draw bar segment
    else:
        skip
```

v46 证实：

```text
progress_count=0 -> bar 全部 SKIP
首次 0x2EF86C 入口前写 progress_count=1 -> idx=0 bar segment 能 DRAW
```

所以：

```text
progress_count 是动画 gate；
动画通路本身能跑；
问题是 guest 没有自然写 progress_count。
```

---

### 1.3 检查网络/检查更新字符串存在，但还没进入

已 watch：

```text
0x313B30 连接超时，请检查网络
0x313C48 连接失败，请重试
0x313C5C 连接中，请稍等
0x313C74 正在下载资源文件…
0x313CF4 检查更新列表
0x314204 正在登陆,请稍等
```

v46 结果：

```text
只看到 watching 安装；
没有 JJFB_STARTUP_STR 命中。
```

说明：

```text
检查网络/更新 UI 阶段还没有进入；
不是字符串缺失。
```

---

### 1.4 2EC6B0 返回值不是当前 blocker

v44/v46 已测：

```text
R0=0 / 1 / obj / pixels
```

都不能自然推进：

```text
AC8 不写
progress 不写
ui_mode 不变
检查更新字符串不读
```

所以 v47 不要继续在 2EC6B0 返回值上耗时间。

---

## 2. v47 的判断

当前最小卡点不是：

```text
贴图位置
透明色
chrome
310BB4
2EC6B0 返回值
```

而是：

```text
启动检查 UI 的上游驱动事件缺失。
```

换句话说，游戏代码已经到了能画 loadingbar 的地方，但缺少某个“进度/阶段推进”的输入：

```text
timer tick 回调？
资源加载完成回调？
更新检查进度回调？
app/resume/start 事件？
0x10140 handler 回调？
sendAppEvent 返回/副作用？
```

---

## 3. P0：静态找 progress_count 写点，别只等运行触发

v46 运行没有自然写，说明写点可能根本没执行。  
v47 必须静态找：

```text
谁可能写 ERW+0xBA0+0x2C？
谁可能写 ERW+0xAC8？
谁可能写 ERW+0x8D0？
```

### 3.1 必须输出 xref 报告

生成：

```text
reports/v47_state_xref.md
```

至少包含：

```text
target:
  ERW+0x8D0   ui_mode
  ERW+0xAC8   AC8
  ERW+0xBA0+0x2C progress_count

for each:
  read pc
  write pc
  function start guess
  lr/caller if dynamic
  nearby instructions
  是否已执行
  若未执行，谁调用它
```

### 3.2 静态扫描方式

不只搜索立即数 `0x8D0/0xAC8/0xBA0/0x2C`，还要考虑：

```text
基址寄存器 + 小偏移
多级结构：
  base = ERW
  ptr = ERW + 0xBA0
  count = ptr + 0x2C
```

建议对 robotol 代码区做粗扫描：

```text
所有 STR/STRH/STRB 到 ERW+0x800..0xC50 的动态写入 hook；
同时记录每个写入 pc 的反汇编。
```

动态 hook 已有，但 v47 要扩大和分组：

```text
[JJFB_ERW_WRITE] off=0x... old=... new=... pc=... lr=... tag=...
```

---

## 4. P1：针对 0x10140 / timer handler 做闭环

v46 仍显示 `0x10140` / timer 在转，但 progress 不动。  
这很可疑：

```text
timer 活着，但没有驱动 progress_count。
```

v47 要回答：

```text
0x10140 注册的 handler 是谁？
timer tick 调用的是哪个函数？
timer tick 是否真的回到 robotol 预期 handler？
是否只调用了 0x2EF86C，而没调用更新检查/progress handler？
```

### 4.1 记录 0x10140 注册表

日志：

```text
[JJFB_10140_REG]
app=...
code=...
param0=...
param1=...
handler=...
lr=...
```

维护表：

```text
handler_id -> function ptr -> owner/module -> times_called
```

### 4.2 每次 timer tick 输出调用目标

```text
[JJFB_TIMER_DISPATCH]
tick=...
handler=...
event_code=...
r0-r3/sp0
before ui_mode/ac8/progress
after  ui_mode/ac8/progress
```

如果 timer dispatch 前后这三个值都不变，说明 tick 没调用对函数或缺参数。

---

## 5. P2：检查更新/网络 UI 字符串的 xref，不要等它出现

既然字符串没被读，就做静态 xref：

```text
检查更新列表 @0x313CF4
连接中，请稍等 @0x313C5C
正在下载资源文件 @0x313C74
正在登陆,请稍等 @0x314204
```

输出：

```text
reports/v47_startup_string_xref.md
```

必须包含：

```text
string VA
referencing function
前置条件 / branch
需要的 ui_mode / state / progress / event
是否被调用过
```

目标：

```text
找到从 progress/loading UI 到 检查更新 UI 的跳转条件。
```

---

## 6. P3：受控 progress driver 用于验证动画链，不当最终方案

为了尽快验证启动动画，可以保留一个“调试 driver”，但必须标注为 probe。

### 6.1 目的

不是跳过启动检查，而是确认：

```text
progress 从 0 -> 12 时，bar/textbar 是否能完整动画；
progress=12 后，是否触发检查网络/更新字符串；
如果不能，说明还缺别的 gate。
```

### 6.2 实现

环境变量：

```text
JJFB_PROGRESS_DRIVER=off
JJFB_PROGRESS_DRIVER=linear
JJFB_PROGRESS_DRIVER=step
```

行为：

```text
只在 ui_mode=0x45 且 loadingbar/bar/textbar object 已绑定后启用；
每次进入 0x2EF86C 前写 progress_count；
值按 tick 增长：0,1,2,...12；
不要直接跳到 12；
不要改 ui_mode；
不要写检查更新字符串。
```

日志：

```text
[JJFB_PROGRESS_DRIVER] tick=... count old->new reason=probe
[JJFB_PROGRESS_DRAW] idx=... count=...
```

### 6.3 观察指标

```text
bar/textbar 动画是否完整；
progress=12 后是否读检查网络/更新字符串；
AC8 是否变化；
ui_mode 是否变化；
是否出现 update/network/login 资源或文字。
```

如果 progress driver 能引出检查更新字符串，则说明真正缺口是 progress writer。  
如果 progress=12 仍不引出，则继续追下一个 gate。

---

## 7. P4：AC8 不要再长期 force，做脉冲实验即可

AC8 已知：

```text
AC8>0 -> slogo
AC8=0 -> loadingbar
```

v47 允许：

```text
AC8 pulse 1~5 ticks
```

用于验证 slogo -> loadingbar 过渡。  
但不要长期固定 AC8。

目标：

```text
slogo 出现一次后，回到 AC8=0，让 loading/progress 继续。
```

如果 AC8 pulse 之后 progress driver 能让启动检查 UI继续，则可以先保留为 bring-up probe，同时继续找自然 writer。

---

## 8. P5：画面只保留最低正确性

保留：

```text
240×320
2F9968=240
2F995C=320
0xF81F colorkey
obj=0 skip
真实资源 blit
```

暂停：

```text
chrome 细节
310BB4 完整 ABI
slogo 精确位置
原生 refresh 完美化
```

现在画面修复只服务一个目标：

```text
启动检查 UI 动画能完整跑。
```

---

## 9. v47 推荐运行

### Run A：纯追写点

```powershell
$env:JJFB_PROGRESS_DRIVER="off"
$env:JJFB_AC8_MODE="natural"
.\RUN_V47_STATE_WRITER_TRACE.ps1
```

### Run B：progress 线性 driver

```powershell
$env:JJFB_PROGRESS_DRIVER="linear"
$env:JJFB_AC8_MODE="natural"
.\RUN_V47_STARTUP_PROGRESS_DRIVER.ps1
```

### Run C：slogo pulse + progress driver

```powershell
$env:JJFB_AC8_MODE="pulse"
$env:JJFB_AC8_PULSE_TICKS="3"
$env:JJFB_PROGRESS_DRIVER="linear"
.\RUN_V47_STARTUP_PROGRESS_DRIVER.ps1
```

---

## 10. v47 成功标准

### 最低成功

```text
找到 progress_count 或 AC8 的候选 writer / 上游函数；
或者确定确实没有运行到 writer，并找到未进入原因。
```

### 中级成功

```text
progress driver 让 bar/textbar 动画完整跑完；
progress=12 后触发检查网络/检查更新字符串读取。
```

### 高级成功

```text
找到自然事件/回调，使 progress 不靠 driver 也增长。
```

### 游戏目标成功

```text
启动检查 UI 完整跑完，进入后续 update/login/network 阶段。
```

---

## 11. 给 Cursor 的一句话

**v46 证明 0x45 内部启动 UI 的图形链可以工作，bar 动画也能在 progress_count>0 时画；真正缺口是 progress/AC8 的自然写入和检查更新阶段入口。v47 不要继续修 2EC6B0 或 UI 外观，请静态+动态找 ERW+0xBA0+0x2C 与 ERW+0xAC8 的 writer，闭环 0x10140/timer handler，同时用线性 progress driver 作为 probe 验证 progress 0→12 后是否触发检查网络/更新字符串。**
