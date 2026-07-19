# Cursor 开发说明 v44：重新按“跑机甲风暴”定位 —— 停止在 forced splash 里修外观，追上游状态/进度写入

> 用户反馈：当前画面“不是原版”，位置、紫块、资源、流程都对不上。  
> 结论：这个反馈是对的。v43 虽然把 loadingbar 画对了一部分，但仍然是在 **强行进入 ui_mode=0x45 的局部 splash 分支**，不是完整原版启动流程自然跑通。

---

## 0. 当前最重要判断

当前窗口里看到的画面不能算“机甲风暴原版启动页完整加载出来”。

现在真正状态是：

```text
真实游戏资源能被请求；
loadingbar 能按 240×320 坐标画出；
但上游状态机没有自然推进：
  AC8 无自然写入；
  BA0+0x2C progress count 恒为 0；
  ui_mode 仍卡在 0x45；
  slogo/loadingbar/progress 不是自然衔接；
  原生网络仍没进。
```

所以 v44 的重心必须从：

```text
继续修 splash 画面像不像
```

改成：

```text
追为什么游戏状态没有继续往后跑。
```

---

## 1. 先纠正一个日志误读：0x2B241C 不是 loadingbar slot，而是 progress count

已知：

```text
ERW = 0x2B1850
BA0 = ERW + 0xBA0 = 0x2B23F0
```

结构：

```text
BA0+0x20 = 0x2B2410 = bar object
BA0+0x24 = 0x2B2414 = textbar object
BA0+0x28 = 0x2B2418 = loadingbar object
BA0+0x2C = 0x2B241C = progress count
```

但当前日志里有误导性输出：

```text
[JJFB_BMP_OBJ_READ] name=loadingbar which=parent_slot addr=0x2B241C pc=0x2EFAA0 ...
```

这个不能再标成 loadingbar parent_slot。  
`0x2B241C` 应改名为：

```text
progress_count_addr
```

`pc=0x2EFAA0` 读的是 progress count，不是 bitmap slot。

### 要改日志标签

把：

```text
addr=0x2B241C name=loadingbar parent_slot
```

改成：

```text
[JJFB_PROGRESS_COUNT_READ]
addr=0x2B241C value=... pc=0x2EFAA0 idx=... r0=...
```

否则后续判断会继续跑偏。

---

## 2. 当前画面为什么“不像原版”

### 2.1 loadingbar 是真实资源，但只是局部流程

当前自然模式日志：

```text
FORCE state/ui_mode 0x0 -> 0x45
SPLASH_ENTER #1...#158
AC8=0
loadingbar 请求
bar/textbar 请求
loadingbar blit x=19 y=220
progress count=0 -> bar 全部 SKIP
```

这说明：

```text
ui_mode=0x45 是被 host 强推进入的；
并不是游戏自然状态机推进到完整 splash/loading。
```

这也是为什么它反复进 SPLASH_ENTER，但不离开 0x45。

---

### 2.2 slogo 和 loadingbar 仍然不是自然衔接

当前：

```text
AC8=1 -> slogo
AC8=0 -> loadingbar/bar/textbar
```

但没有证据显示 guest 自己写 AC8。  
`force_slogo_once` 只是人工让 slogo 出来，释放后进入 chrome/wy_*，没有自然接 loadingbar。

所以不要继续用两个模式拼画面。  
现在要追：

```text
谁本来应该写 AC8？
为什么现在没有写？
是我们跳过了上游初始化？
还是某个平台回调没有触发？
```

---

### 2.3 紫色色块仍可能来自两类问题

当前 loadingbar 已经：

```text
[JJFB_BLIT_KEYED] skipped=202
```

但截图里仍能看到左侧若干紫色块，说明透明处理还没有覆盖所有路径。

可能来源：

```text
A. wy_jiao / wy_xian / chrome 小图没有 colorkey；
B. obj=0 的 2EC6B0 调用被错误处理，留下脏像素；
C. DEBUG_PRESENT 显示的是上一帧/未清屏 buffer；
D. 310BB4/2EC6B0 某条旁路没有走 keyed blit。
```

### v44 要求

所有画到 guest screen buffer 的资源路径都必须打印：

```text
[JJFB_BLIT_KEYED] name=... drawn=... skipped=... key=...
```

所有 `obj=0` 的 2EC6B0 必须：

```text
skip，不写 screen buffer，不 dirty present
```

如果仍有紫块，必须能从日志定位是哪张资源画的。

---

### 2.4 右上角青色块/异常块不是原版内容

截图右上角的青色块很可疑，不应把它当游戏 UI。  
重点查：

```text
谁写了 screen 区域 x≈190,y≈5 或右上角区域？
是否来自 obj=0 2EC6B0？
是否来自未初始化 buffer？
是否来自 chrome guard 的错误绘制？
是否来自上一次 frame 没清？
```

加脏矩形日志：

```text
[JJFB_DIRTY_RECT] tag=... name=... x=... y=... w=... h=...
```

只有明确来自原游戏资源的 dirty rect 才可信。

---

## 3. 不要再只在 0x2EF86C 里面绕圈

现在一直在：

```text
0x304619 / 0x306631 -> 0x2EF86C
```

每 tick 重复 SPLASH_ENTER。  
这说明上游事件/状态没有推进。

v44 应该向上追调用链：

```text
0x304619 是什么？
0x306631 是什么？
谁根据 ui_mode=0x45 调用 0x2EF86C？
ui_mode/state 存在哪里？
谁应该改变 ui_mode？
```

### 需要新增日志

```text
[JJFB_DISPATCH]
pc/lr
state
ui_mode
event_code
timer_tick
r0-r7
target_func
```

特别是每次调用 `0x2EF86C` 前，打印调用者和分发表：

```text
[JJFB_UI_DISPATCH] ui_mode=0x45 target=0x2EF86C caller=0x306631
```

并且 watch：

```text
ui_mode storage address
state storage address
```

不是只打印“ui_mode=0x45”，要知道它存在什么地址、谁写它。

---

## 4. 现在最高优先级：追 AC8 和 BA0+0x2C 的上游写入

### P0-A：AC8

```text
AC8 addr = ERW+0xAC8 = 0x2B2318
```

当前只有 read，没有 guest write。  
必须确认：

```text
1. AC8 是否应该由 guest 写；
2. 是否本来由平台/host callback 写；
3. 是否因为某个 ext_call / sendAppEvent 返回值错导致没写；
4. 是否因为强行 ui_mode=0x45 跳过了写 AC8 的初始化阶段。
```

### P0-B：progress count

```text
progress count addr = ERW+0xBA0+0x2C = 0x2B241C
```

当前恒为 0，导致：

```text
bar idx=0..11 全 SKIP
```

必须加：

```text
[JJFB_PROGRESS_COUNT_READ]
[JJFB_PROGRESS_COUNT_WRITE]
```

如果没有 write，就向上追：

```text
谁应该加载资源完成后增加 count？
是否 0x10134 / 2d92dc / 2EC6B0 需要回调或返回值？
是否 timer/event 应该推进 count？
是否网络/更新检查前置导致 count 不走？
```

---

## 5. 2EC6B0 guard 返回值必须做游戏推进 A/B，而不是只看画面

当前：

```text
fp150C = 0x270F
host blit 后 ret lr
r0=1
```

这可能让画面可见，但也可能破坏原平台函数副作用，导致 progress 不增加、ui_mode 不离开 0x45。

v44 必须做 A/B：

```text
JJFB_2EC6B0_RET=0
JJFB_2EC6B0_RET=1
JJFB_2EC6B0_RET=obj
JJFB_2EC6B0_RET=pixels
```

观察指标不是画面，而是：

```text
BA0+0x2C 是否变化；
AC8 是否变化；
ui_mode 是否离开 0x45；
bar/textbar 是否绘制；
是否出现 login/server/initNetwork。
```

如果某个返回值能推进状态，以推进游戏为准。

---

## 6. 不要把修图形细节当主任务

以下都降优先级：

```text
chrome 边框细节；
wy_jiao / wy_xian 完整绘制；
310BB4 完整 ABI；
原生 refresh 是否完美；
slogo 是否像素级正确。
```

现在唯一主目标：

```text
让游戏状态继续跑，离开 0x45，进入 loading/progress/login/network。
```

画面只做到：

```text
原版资源能大致显示，且不影响状态推进。
```

---

## 7. v44 运行矩阵

### Run A：不强推 ui_mode，看自然入口

```powershell
$env:JJFB_FORCE_UI_MODE="0"
$env:JJFB_COLORKEY="auto"
$env:JJFB_DIM_MAP="width_height"
.\RUN_V44_GAME_STATE_TRACE.ps1
```

目的：

```text
确认如果不强推 0x45，游戏自然停在哪；
谁应该写 ui_mode；
为什么自然不进入 splash。
```

### Run B：强推 0x45，只追上游/返回值

```powershell
$env:JJFB_FORCE_UI_MODE="45"
$env:JJFB_2EC6B0_RET="0"
.\RUN_V44_GAME_STATE_TRACE.ps1
```

重复测试：

```powershell
JJFB_2EC6B0_RET=1
JJFB_2EC6B0_RET=obj
JJFB_2EC6B0_RET=pixels
```

对比：

```text
AC8
BA0+0x2C
ui_mode
login/server/network
```

### Run C：短时强推，随后释放

```powershell
$env:JJFB_FORCE_UI_MODE="45_once"
.\RUN_V44_GAME_STATE_TRACE.ps1
```

目的：

```text
看进入 splash 初始化一次后，guest 是否能自己维持/推进；
如果不能，说明强推只是在局部函数里绕圈。
```

---

## 8. v44 必须输出的报告

```text
reports/v44_game_state_blocker.md
```

必须回答：

```text
1. ui_mode/state 存储地址是什么？
2. 谁写 ui_mode=0x45？如果是 host force，guest 自然值是什么？
3. 0x304619 / 0x306631 / 0x2EF86C 的调用关系；
4. AC8 是否有自然写入？没有的话，哪些函数读取它？
5. BA0+0x2C 是否有自然写入？没有的话，哪些函数读取它？
6. 2EC6B0 ret=0/1/obj/pixels 对游戏状态有无影响；
7. 当前卡在 0x45 的最小根因；
8. 下一步是补哪个“游戏推进必要”的平台回调/返回值，而不是补 UI。
```

---

## 9. 成功标准

### 近期成功

```text
确认当前不是 UI 坐标问题，而是状态推进问题；
找到 ui_mode / AC8 / BA0+0x2C 的真实上游写入缺口。
```

### 中期成功

```text
BA0+0x2C progress > 0；
bar 开始画；
ui_mode 离开 0x45；
出现 login/server/update 相关字符串或资源。
```

### 游戏目标成功

```text
出现 initNetwork / socket / connect；
开始进入网游登录或服务器流程。
```

---

## 10. 给 Cursor 的一句话

**v43 不能再继续按“画面像不像”修了。截图说明当前只是 forced ui_mode=0x45 下局部 splash 被 host blit 出来，仍不是真正原版启动流程。请 v44 追上游状态机：先找 ui_mode/state 存储地址和写入者，确认 0x304619/0x306631 如何 dispatch 到 0x2EF86C；把 0x2B241C 明确标为 BA0+0x2C progress count，不要再误标为 loadingbar slot；做 2EC6B0 返回值 A/B，观察 AC8、progress、ui_mode、login/network 是否推进。所有工作以“让机甲风暴离开 0x45 继续跑”为目标，而不是继续完善 UI/chrome。**
