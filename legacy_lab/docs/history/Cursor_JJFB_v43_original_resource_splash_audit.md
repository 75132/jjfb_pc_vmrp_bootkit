# Cursor 开发说明 v43：按“跑机甲风暴”重新收敛，仔细核对原游戏资源、状态和显示链

> 用户再次强调：目标不是开发/完善模拟器，而是**把机甲风暴网游跑起来**。  
> 平台兼容只是遇到阻塞时的最小补丁。  
> 当前重点不是 chrome 像素级还原，而是让原版 splash/loading/login 流程真实推进。

---

## 0. 先纠正当前认知

现在看到的 UI 不是完全原版，用户反馈是对的。  
问题不是“差一点美化”，而是至少有三类硬错误：

```text
1. 紫色色块没有透明掉；
2. 240×320 布局轴仍不完整，纵向 y 明显不对；
3. slogo 和 loadingbar 被 AC8 / SLOGO_NUDGE 拆成两个互斥人工分支，原版启动状态机没有自然衔接。
```

因此 v43 不要继续说“已经启动成功”。  
正确说法是：

```text
原版资源已经能被请求和构造，但原版启动流程还没自然跑通。
```

---

## 1. 从 v41 日志得出的关键事实

### 1.1 slogo 字符串地址已经修正

之前误把 `0x314EF4` 当 slogo，实际那是 speedlight 相关。  
当前正确 slogo：

```text
slogo!157!58.bmp @ 0x3142DC
```

v41 mode45 已经能读到它：

```text
[JJFB_BMP_REQ] name=slogo!157!58.bmp w=157 h=58 bytes=0x4724 lr=0x2EF9A7
[JJFB_2EC6B0_BLIT] name=slogo!157!58.bmp x=41 y=82 w=157 h=58
```

这里 x=41 是合理的：

```text
(240 - 157) / 2 = 41
```

---

### 1.2 SLOGO_NUDGE=1 会显示 slogo，但会卡在 slogo 分支

v41 mode45 中：

```text
[JJFB_GAME_STATE] SLOGO_NUDGE ac8 0 -> 1
[JJFB_FIRST_SCREEN] SPLASH_ENTER #280+ ui_mode=0x45 ac8=1
[JJFB_2EC6B0_BLIT] slogo!157!58.bmp x=41 y=82 ...
```

后面一直反复：

```text
SPLASH_ENTER
0x12340
2EC6B0 slogo
```

没有进入 loadingbar/bar/textbar 的自然后续。

结论：

```text
AC8=1 不是完整启动流程；
它只是强行进入/保持 slogo 分支。
长期固定 AC8=1 会卡住。
```

---

### 1.3 SLOGO_NUDGE=0 会请求 loadingbar/bar/textbar，但不显示 slogo

v41 noslogo 中：

```text
loadingbar!201!29.bmp
bar!16!18.bmp
textbar!120!30.bmp
```

并且对象绑定很完整：

```text
loadingbar -> slot@0x2B2418  pc=0x2EFA34
bar        -> slot@0x2B2410  pc=0x2EFA44
textbar    -> slot@0x2B2414  pc=0x2EFA52
```

然后真正调用：

```text
[JJFB_2EC6B0] lr=0x2EFA97 obj=loadingbar x=19 y=140 w=201 h=29
```

结论：

```text
AC8=0 是 loadingbar/progress 分支；
但此分支没有 slogo。
```

所以当前启动页不是自然流程，而是被人为分裂成：

```text
AC8=1 -> slogo
AC8=0 -> loadingbar/bar/textbar
```

v43 必须追 AC8 的自然状态机，不能只用两个手动模式拼图。

---

## 2. 资源/透明色必须仔细核对原游戏文件

用户指出“原版应该会去掉素材紫色色块”，这个判断是对的。

### 2.1 强证据：对象字段里已经出现 0xF81F

v41 noslogo 中 loadingbar 对象：

```text
[JJFB_BMP_OBJ] loadingbar
+1C = 0xF81FF81F
```

`0xF81F` 是 RGB565 经典品红/紫色透明色：

```text
R=31, G=0, B=31
```

这不是普通像素，很可能是透明 colorkey。

因此不要再把所有像素全拷贝。  
必须做 colorkey skip。

---

### 2.2 Cursor 必须输出资源审计图，而不是凭感觉

请从原游戏资源和 cache 里导出资源预览：

```text
slogo!157!58.bmp
loadingbar!201!29.bmp
bar!16!18.bmp
textbar!120!30.bmp
wy_jiao*
wy_xian*
jiantou*
```

输出两版 PNG：

```text
raw preview：不透明处理
keyed preview：跳过 0xF81F / 自动识别透明色
```

并写报告：

```text
resource name
w/h
RGB565 first pixels
corner colors
edge dominant color
是否检测到 0xF81F
透明像素数量
```

建议输出：

```text
reports/resource_audit_v43.md
reports/resource_contactsheet_raw.png
reports/resource_contactsheet_keyed.png
```

这一步是“核对原游戏文件”，不是美化。

---

## 3. 透明色实现要求

### 3.1 在所有 host-side game resource blit 中做 colorkey

涉及：

```text
2EC6B0 host blit
310BB4 host blit
EAGER/DEBUG resource blit
```

透明色策略：

```text
1. 优先使用对象字段 / 资源元信息中的 0xF81F；
2. 如果字段不可用，检查四角/边缘主色；
3. RGB565 0xF81F 跳过；
4. 注意端序，不要把 0xF81F 搞成 0x1FF8；
5. 只跳过透明像素，不改变非透明像素。
```

日志：

```text
[JJFB_COLORKEY] name=... key=0xF81F source=obj+1C/corner
[JJFB_BLIT_KEYED] name=... drawn=... skipped=...
```

### 3.2 不要一刀切所有资源

有些资源可能没有透明色。  
所以：

```text
slogo / wy_* / jiantou 优先启用；
loadingbar / textbar 按实际审计决定；
如果四角不是透明色，不强行跳。
```

---

## 4. 240×320 布局轴必须修完整

当前只修了一半。

### 4.1 当前证据

v41 noslogo：

```text
[JJFB_AXIS] 2F9968 natural=320 -> ret 240
[JJFB_AXIS] 2F995C natural=240 (pass-through)
[JJFB_2EC6B0] loadingbar x=19 y=140
```

x 正确，y 不对。

公式等价：

```text
x = (2F9968() - 201) / 2 = (240 - 201) / 2 = 19
y = 2F995C() - 100 = 240 - 100 = 140
```

对 240×320 竖屏，纵向高度应该是 320：

```text
y = 320 - 100 = 220
```

### 4.2 v43 要求

让：

```text
2F9968 -> layoutWidth  = 240
2F995C -> layoutHeight = 320
```

但不要全局乱改字段。优先只在这两个函数返回层做适配。

必须输出日志：

```text
[JJFB_DIM] 2F9968=240 2F995C=320 screen=240x320
[JJFB_LAYOUT] loadingbar x=(240-201)/2=19 y=320-100=220
```

成功标准：

```text
loadingbar y 从 140 移到约 220；
不裁剪；
资源尺寸仍然是 201×29，不拉伸。
```

---

## 5. AC8 / splash 状态机要重新分析，不能固定 SLOGO_NUDGE

当前最重要的不是再画更多资源，而是弄明白：

```text
AC8 到底是“是否显示 slogo”、
“启动阶段计数器”、
“splash 前半/后半 gate”、
还是“是否已显示 logo”的标志？
```

v41 证明：

```text
AC8=1 -> 只 slogo，且反复；
AC8=0 -> loadingbar/bar/textbar。
```

这不像完整原版流程。

---

### 5.1 必须跟踪 AC8 自然读写

加 watch：

```text
ERW+0xAC8
相关父结构
```

日志：

```text
[JJFB_AC8_READ]  value=... pc=... lr=... tick=...
[JJFB_AC8_WRITE] old->new pc=... lr=... tick=...
[JJFB_SPLASH_PHASE] ac8=... phase=slogo/loading/progress/unknown pc=...
```

重点找：

```text
谁自然写 AC8？
谁判断 AC8？
为什么 AC8=1 不进入 loadingbar？
是否缺 timer/progress 使 AC8 没有回到 0 或进入下一阶段？
```

---

### 5.2 做“短时 slogo 然后自然 loading”的测试，但不能冒充最终方案

为了跑游戏，可以做最小推进实验：

```text
AC8=1 只持续 N tick 或直到 slogo 画出一次；
然后释放/恢复 AC8 自然值；
观察是否进入 loadingbar/bar/textbar；
再看 ui_mode 是否离开 0x45。
```

环境变量建议：

```text
JJFB_SPLASH_AC8_MODE=natural
JJFB_SPLASH_AC8_MODE=force_slogo_once
JJFB_SPLASH_AC8_MODE=force_loading
```

不要长期固定 `SLOGO_NUDGE=1`。

---

## 6. bar / textbar / progress 要追，不要只显示 loadingbar

noslogo 中，bar/textbar 已经构造：

```text
bar        -> slot@0x2B2410
textbar    -> slot@0x2B2414
loadingbar -> slot@0x2B2418
```

但只看到 loadingbar 的 `2EC6B0`：

```text
lr=0x2EFA97 loadingbar
```

后面出现：

```text
pc=0x2EFA96 / 0x2EFA9E / 0x2EFADE
r0 = 0x0, 0xC, 0x18, 0x24...
r6 = 0,1,2,3...
```

这很像 progress/bar 循环，但没有正确绘制 bar/textbar。

v43 要求：

```text
1. 反汇编/trace 0x2EFA96 -> 0x2EFADE；
2. 标出它是否使用 bar slot@0x2B2410 或 textbar slot@0x2B2414；
3. 记录每次循环计算出的 x/y/w/h；
4. 确认是否因 2EC6B0 guard 返回值不对而跳过后续绘制。
```

---

## 7. 2EC6B0 guard 的返回值/副作用不能忽略

当前：

```text
ERW+0x150C = 0x270F
2EC6B0 host blit 后直接 ret lr
```

这能显示图，但可能没有给 guest 原平台绘制函数应有的返回值/副作用。

必须 A/B 测试：

```text
2EC6B0 host blit 后：
R0=0
R0=1
R0=obj
R0=pixels
保留/恢复 flags
```

观察：

```text
bar/textbar 是否消费
AC8 是否变化
ui_mode 是否离开 0x45
progress 是否推进
login/server 是否出现
```

不要只看画面，要看状态是否推进。

---

## 8. 原游戏文件分析的最低输出

请 Cursor 生成一个明确报告，而不是只说日志：

```text
reports/v43_original_resource_and_splash_analysis.md
```

报告必须包含：

```text
1. 原资源清单：slogo/loadingbar/bar/textbar/wy/jiantou 的路径、尺寸、是否存在；
2. 透明色判断：每张资源的 key、跳过像素比例；
3. splash 函数图：
   0x2EF86C 入口
   0x2EF9A7 slogo 请求
   0x2EF9DF slogo blit
   0x2EFA33 loadingbar 请求
   0x2EFA43 bar 请求
   0x2EFA53 textbar 请求
   0x2EFA97 loadingbar blit
   0x2EFADE progress loop 疑似点
4. AC8 状态机：
   AC8=0 路径
   AC8=1 路径
   谁读/谁写/是否自然变化
5. 240×320 布局：
   2F9968/2F995C 返回值
   loadingbar 坐标公式
6. 当前阻塞：
   为什么 slogo->loadingbar 不能同轮自然衔接；
   为什么 bar/textbar 没显示；
   为什么 ui_mode 卡 0x45。
```

---

## 9. v43 推荐运行矩阵

### Run A：自然 AC8，不强推

```powershell
$env:JJFB_SCREEN_W="240"
$env:JJFB_SCREEN_H="320"
$env:JJFB_DIM_MAP="width_height"
$env:JJFB_COLORKEY="auto"
$env:JJFB_SPLASH_AC8_MODE="natural"
.\RUN_V43_GAME_SPLASH_AUDIT.ps1 -Mode 45
```

### Run B：slogo once

```powershell
$env:JJFB_SPLASH_AC8_MODE="force_slogo_once"
.\RUN_V43_GAME_SPLASH_AUDIT.ps1 -Mode 45
```

### Run C：loading branch

```powershell
$env:JJFB_SPLASH_AC8_MODE="force_loading"
.\RUN_V43_GAME_SPLASH_AUDIT.ps1 -Mode 45
```

对比：

```text
紫色是否消失；
loadingbar y 是否正确；
slogo 和 loadingbar 是否能同一运行衔接；
bar/textbar 是否显示；
ui_mode 是否离开 0x45；
login/server/network 是否出现。
```

---

## 10. 成功标准

### 近期成功

```text
1. 紫色透明色正确去除；
2. loadingbar 坐标按 240×320 正确；
3. slogo 和 loadingbar 不再只能靠两个互斥模式分别显示。
```

### 中期成功

```text
bar/textbar/progress 显示；
ui_mode 离开 0x45；
进入 login/server/update 分支。
```

### 游戏目标成功

```text
出现 initNetwork/socket/connect；
开始处理网游登录/服务器流程。
```

---

## 11. 给 Cursor 的一句话

**请仔细核对原游戏资源和 v41 日志：现在不是单纯 UI 不好看，而是透明色、布局轴、AC8 状态机三处导致“看起来不像原版”。v43 先输出原资源审计和 splash 函数图；实现 RGB565 0xF81F 透明色；让 2F9968=240、2F995C=320；停止长期 SLOGO_NUDGE，改为追 AC8 自然读写和 slogo->loadingbar->progress 状态衔接；同时分析 0x2EFA96-0x2EFADE 为什么 bar/textbar 没显示。所有改动只服务于让机甲风暴继续跑，不服务于完整模拟器。**
