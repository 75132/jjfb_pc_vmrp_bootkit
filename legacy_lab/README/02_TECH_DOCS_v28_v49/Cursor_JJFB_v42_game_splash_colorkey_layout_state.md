# Cursor 继续开发说明：v42 — 游戏优先：修透明色、真实布局轴、splash 状态推进

> 用户反馈非常关键：当前看到的 UI 不是原版效果；紫色色块仍显示、位置不对、部分资源没加载。  
> v42 不再按“完善模拟器”推进，而是按“让机甲风暴启动流程继续跑”推进。  
> 当前要解决的是 **游戏 splash/loading 原版显示和状态推进**，不是 chrome 像素级还原。

---

## 1. v41 日志实测结论

### 1.1 资源确实来自原版，但显示方式还不对

已经看到原版资源进入链路：

```text
slogo!157!58.bmp
loadingbar!201!29.bmp
bar!16!18.bmp
textbar!120!30.bmp
wy_jiao / wy_xian / jiantou...
```

但当前显示仍然不对，原因不是“缺资源”单一问题，而是：

```text
A. host blit 没处理素材透明色 / 色键；
B. 240×320 布局轴只修了一半；
C. slogo 和 loadingbar 被人为分成两个分支跑，未形成自然启动序列；
D. 2EC6B0/150C bad-fp guard 可能只显示了图，但没有给 guest 返回正确成功状态，导致状态不推进。
```

---

## 2. 紫色色块问题：必须先处理色键透明

用户看到的紫色色块，基本就是 BMP 素材里的透明背景色。  
原版手机平台绘制时应该跳过这个颜色；当前 host blit 是逐像素全拷贝，所以紫色也被画出来了。

### P0-1：不要再把紫色当正常像素画

在 host blit 路径加入透明色 / color key：

涉及路径：

```text
2EC6B0 host blit
310BB4 host blit
任何 DEBUG_PRESENT 前写 guest screen buffer 的 bitmap copy
```

### P0-2：不要硬编码一种颜色就完事

先做动态检测：

```text
1. 对每张 BMP/RGB565 资源统计四角像素；
2. 如果四角高度一致或边缘主色占比很高，则作为 colorkey 候选；
3. 优先识别 RGB565 紫/品红：
   0xF81F
   以及可能的 BGR565/端序变体；
4. 对 slogo / wy_* / jiantou / button 等资源启用 skip colorkey；
5. loadingbar/textbar 是否透明要按资源统计决定，不要一刀切。
```

日志：

```text
[JJFB_COLORKEY] name=slogo!157!58.bmp key=0xF81F method=corner count=...
[JJFB_BLIT_KEYED] name=... drawn=... skipped=...
```

### P0-3：这是“跑游戏”的必要修复，不是模拟器美化

透明色不处理，原版资源永远看起来不像原版。  
这是当前最直观的错误，优先级高。

---

## 3. 位置问题：240×320 布局轴仍未完整修正

v41 noslogo 日志：

```text
loadingbar x=19 y=140 w=201 h=29
```

x 已经正确：

```text
x = (240 - 201) / 2 = 19
```

但 y 仍然明显偏上。  
说明当前只修了 `2f9968`，没有把纵向高度源修完整。

### P1-1：2f9968 / 2f995c 应分别作为 layoutWidth / layoutHeight

当前应目标：

```text
2f9968() -> 240   // layout width，供水平居中
2f995c() -> 320   // layout height，供垂直位置
```

不要只修 2f9968。

如果公式是：

```text
y = 2f995c() - 100
```

则应该得到：

```text
y = 320 - 100 = 220
```

而不是：

```text
y = 240 - 100 = 140
```

### P1-2：做强日志确认

必须打印：

```text
[JJFB_DIM] 2f9968_ret=240 2f995c_ret=320 screen=240x320
[JJFB_LAYOUT] loadingbar formula x=(240-201)/2=19 y=320-100=220
[JJFB_2EC6B0_BLIT] loadingbar x=19 y=220 w=201 h=29 clip=201x29
```

如果 2f995c 不能 hook，查它读哪个字段，不要改全局分辨率。

---

## 4. slogo / loadingbar 被拆成两个模式，不是原版流程

v41 两组日志显示：

### SLOGO_NUDGE=1

```text
slogo 被请求并绘制：
slogo!157!58.bmp x=41 y=82

但 loadingbar/bar/textbar 不请求。
```

### SLOGO_NUDGE=0

```text
loadingbar/bar/textbar 被请求；
loadingbar 被绘制。

但 slogo 不请求。
```

这说明目前不是自然流程，而是人工 gate 把两个分支拆开了。

### P2-1：不要把 AC8 固定成 0 或 1 当最终方案

`AC8` 很可能是 splash 前半/后半、是否显示 logo、计数器、阶段标志之一。  
现在应该追它的自然变化，不要长期硬塞。

日志要记录：

```text
AC8 读写点
ui_mode
state
tick
0x2EF86C / 0x2EF9DA / 0x2EFA92 分支
```

重点：

```text
[JJFB_AC8_WRITE] old->new pc lr tick
[JJFB_SPLASH_PHASE] phase=slogo|loadingbar|progress ui_mode=... ac8=...
```

### P2-2：目标是 slogo -> loadingbar 的自然序列

不要用两个运行模式分别看一张图。  
目标是同一次运行里：

```text
slogo 请求并绘制
随后 loadingbar/bar/textbar 请求并绘制
随后 progress/登录/网络分支继续
```

---

## 5. 一些资源没有加载：重点追 progress/状态推进

v41 noslogo 中：

```text
loadingbar/bar/textbar 都请求了；
但只有 loadingbar 进入 2EC6B0 blit；
bar/textbar 没有自然绘制。
```

这不是资源缺失，而是后续状态/绘制分支没走到。

### P3-1：追 bar/textbar 为什么不消费

对 bar/textbar 的 object 建 watch：

```text
object
object+0x04 pixels
pixels
父结构 slot
```

日志：

```text
[JJFB_BAR_CONSUMER]
[JJFB_TEXTBAR_CONSUMER]
```

看它们是否被进度循环、文字绘制、或某个后半段分支消费。

### P3-2：追 splash 退出条件

现在 `SPLASH_ENTER` 重复很多次，说明卡在 ui_mode=0x45：

```text
SPLASH_ENTER #1 ... #100+
ui_mode=0x45
```

需要找：

```text
什么条件让 ui_mode 离开 0x45？
什么条件进入 login/server/update/network？
progress 计数在哪里？
资源加载完成 flag 在哪里？
```

日志：

```text
[JJFB_UI_MODE_WRITE] old->new pc lr tick
[JJFB_PROGRESS] value=... pc=... lr=...
[JJFB_SPLASH_EXIT_GATE] condition=... result=...
```

---

## 6. 2EC6B0 / ERW+0x150C 不能只“显示了就返回”

当前：

```text
ERW+0x150C = 0x270F
2EC6B0 guard 做 host blit 后 ret lr
```

这能显示图，但可能少了原平台绘图函数的返回值/副作用。  
如果返回值或副作用不对，游戏会以为绘制失败、资源没画完、状态不推进。

### P4-1：测试 2EC6B0 返回值

对 host blit 后的返回值做 A/B：

```text
R0 = 0
R0 = 1
R0 = obj
R0 = pixels
```

每次只试一个，看：

```text
是否 bar/textbar 消费
是否 ui_mode 离开 0x45
是否 progress 变化
是否 login/server 出现
```

### P4-2：继续查谁应该写 ERW+0x150C

但不要把它当“模拟器完整性”任务，而是因为它直接影响游戏绘制状态推进。

---

## 7. 当前不要做

```text
❌ 不要继续说“已经启动成功”；
❌ 不要把紫色色块当正常资源；
❌ 不要只显示 slogo 或 loadingbar 任一分支就算原版首屏；
❌ 不要用两个手动模式拼凑原版流程；
❌ 不要继续优先修 chrome 边框细节；
❌ 不要 host overlay；
❌ 不要 fake 游戏内容；
❌ 不要拉伸资源到 240×320。
```

---

## 8. v42 推荐运行矩阵

### Run A：透明色 + 维度修正 + no forced ac8

```powershell
$env:JJFB_SCREEN_W="240"
$env:JJFB_SCREEN_H="320"
$env:JJFB_DIM_MAP="width_height"
$env:JJFB_COLORKEY="auto"
$env:JJFB_SLOGO_NUDGE="auto"   # 不固定 0/1，记录自然读写
.\RUN_V42_GAME_SPLASH.ps1 -Mode 45
```

### Run B：对照 slogo branch

```powershell
$env:JJFB_COLORKEY="auto"
$env:JJFB_SLOGO_NUDGE="1"
.\RUN_V42_GAME_SPLASH.ps1 -Mode 45
```

### Run C：对照 loadingbar branch

```powershell
$env:JJFB_COLORKEY="auto"
$env:JJFB_SLOGO_NUDGE="0"
.\RUN_V42_GAME_SPLASH.ps1 -Mode 45
```

对比重点：

```text
紫色是否消失
loadingbar y 是否从 140 变到约 220
bar/textbar 是否被消费
slogo 与 loadingbar 是否能同一运行自然衔接
ui_mode 是否离开 0x45
```

---

## 9. 成功标准

### 近期成功

```text
1. slogo 紫色色块消失；
2. loadingbar 坐标不再偏上，按 240×320 布局；
3. slogo/loadingbar 不再靠两个互斥模式分别显示。
```

### 中期成功

```text
bar/textbar 被自然消费并显示；
splash progress 继续；
ui_mode 离开 0x45。
```

### 游戏目标成功

```text
进入 login/server/update/network 分支；
出现 initNetwork/socket/connect 相关调用。
```

---

## 10. 给 Cursor 的一句话

**v41 现在看起来不像原版，不是资源缺失一个问题，而是三件事叠加：host blit 没做紫色透明色键、240×320 纵向布局轴仍错、AC8/SLOGO_NUDGE 把 slogo 和 loadingbar 拆成了两个人工分支。v42 请先实现 RGB565 colorkey 透明、让 2f9968=240 且 2f995c=320，再追 AC8/ui_mode/progress 的自然状态机，让同一次运行里 slogo→loadingbar/bar/textbar→login/network 继续推进；不要继续把 chrome 或模拟器完整性作为主目标。**
