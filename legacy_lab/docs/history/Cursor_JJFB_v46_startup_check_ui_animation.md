# Cursor 开发说明 v46：目标改为“原版启动检查/更新 UI 与动画完整跑完”

> 用户最新澄清：不是跳过启动页，也不是只追 login/network。  
> 当前目标是让《机甲风暴》**正常跑完启动时的检查网络/检查更新 UI 和动画效果**。  
> 这个阶段跑顺了，后续主 UI/登录/网络加载才更可靠。

---

## 0. 目标重新定义

之前 v45 偏向“尽快离开 splash 进入 login/network”，这个方向过急。  
现在目标改为：

```text
完整跑通原版启动检查流程：
slogo / loadingbar / bar / textbar / progress / 检查网络 / 检查更新 UI / 动画
```

不是：

```text
跳过 splash
强推 ui_mode 离开 0x45
只追 initNetwork/socket
只让画面大概出现
```

所以当前不是要绕开启动页，而是要让启动页这个状态机自然跑完。

---

## 1. 当前真正卡点

v44 已确认：

```text
ui_mode=0x45 下能进入 0x2EF86C；
loadingbar/bar/textbar 对象已构造；
loadingbar 能画；
但 AC8 / progress_count 没有自然写入；
bar/textbar/progress 动画没有继续；
启动检查流程没有跑完。
```

所以 v46 要追的不是“怎么离开 0x45”，而是：

```text
0x45 内部启动检查状态机为什么不动？
progress 为什么不增长？
bar/textbar 为什么不动画？
检查网络/检查更新的文字和状态为什么不出现？
```

---

## 2. v46 主线

### P0：完整还原 0x45 启动检查流程

重点函数：

```text
0x2EF86C   splash/check/update UI 入口
0x2EF9A7   slogo 请求
0x2EF9DF   slogo blit
0x2EFA33   loadingbar 请求
0x2EFA43   bar 请求
0x2EFA53   textbar 请求
0x2EFA97   loadingbar blit
0x2EFAA0   progress_count 读取
0x2EFADE   progress/bar 循环疑似点
0x306344 / 0x30662C / 0x304619  dispatch
```

目标不是只画 loadingbar，而是让这套流程自然出现：

```text
slogo
-> loadingbar
-> bar/textbar progress animation
-> 检查网络/检查更新状态文字
-> 更新完成/进入后续界面
```

---

## 3. AC8 不应简单固定，必须当作启动阶段 gate 分析

当前：

```text
AC8=1 -> slogo 分支
AC8=0 -> loadingbar/bar/textbar 分支
```

但这不代表应该手动固定。  
它可能是：

```text
logo阶段完成标志
splash前半/后半 gate
启动检查步骤标志
动画状态标志
```

v46 要做：

```text
1. watch AC8 所有 read/write；
2. 找谁应该写 AC8；
3. 如果没有自然写，查是否缺事件/回调导致；
4. 临时 nudge 时只用于验证，不作为最终方案。
```

建议实验：

```text
AC8 natural
AC8 force_slogo_once_then_release
AC8 pulse: 1 持续 1/3/5 tick 后放回 guest
AC8 forced transition 1->0
```

观察：

```text
是否出现 slogo -> loadingbar 自然衔接；
是否 progress_count 开始变化；
是否 bar/textbar 出现；
是否出现检查网络/更新文字。
```

---

## 4. progress_count 是动画核心，不能只用来跳过

地址：

```text
progress_count = ERW + 0xBA0 + 0x2C = 0x2B241C
```

当前是 0，导致：

```text
bar idx 全 SKIP
动画不跑
```

v46 要查：

```text
谁应该写 progress_count？
是 timer 每 tick 递增？
是资源加载回调递增？
是网络/更新检查步骤递增？
是 2EC6B0/draw 完成后递增？
是 sendAppEvent 某个 0x101xx 返回后递增？
```

### 关键：不要直接用 progress nudge 跳过

可以做 nudge，但目的是理解动画：

```text
progress=1/6/12 能否画出 bar/textbar？
progress 增长到多少会进入下一阶段？
progress 是否对应检查步骤？
```

不是为了直接跳到下一界面。

日志：

```text
[JJFB_PROGRESS_READ] value=...
[JJFB_PROGRESS_WRITE] old->new pc lr tick
[JJFB_PROGRESS_DRAW] idx=... slot=bar/textbar x/y/w/h
[JJFB_PROGRESS_PHASE] value=... text=...
```

---

## 5. UI 与动画现在是主线，不是外观细节

用户明确说：这里加载没问题，后面 UI 估计也差不多。  
所以现在必须把启动 UI/动画当成“平台图形兼容性验收”。

保留和修正：

```text
240×320 固定；
2F9968=240；
2F995C=320；
RGB565 0xF81F colorkey；
obj=0 不 dirty；
resource blit 必须来自原版资源。
```

需要继续修：

```text
slogo 透明色；
loadingbar 正确位置；
bar/textbar progress 动画；
textbar 上的状态文字；
紫块/青块来源；
所有资源 blit 的 dirty rect。
```

但不做：

```text
host overlay；
自画假文字；
伪造机甲风暴 logo；
把 DEBUG_PRESENT 说成 native refresh。
```

---

## 6. 检查网络/检查更新 UI 是本阶段核心

需要主动搜索/监控字符串：

```text
联网
网络
检查
更新
正在
加载
失败
成功
版本
连接
服务器
login
server
update
network
```

在 robotol / gamelist / jjfb / ext 里做静态与动态：

```text
string -> VA
xref -> function
runtime read watch
对应 ui_mode/state/progress
```

目标：

```text
确认检查网络/更新 UI 是哪个函数/哪个 mode 画出来的；
确认当前为何还没到这个阶段。
```

不要等 initNetwork 出现才处理。  
启动检查 UI 可能在真正 socket 前就应该显示。

---

## 7. event/timer 仍然要查，但服务于启动动画

v45 的事件矩阵不废弃，但目标改成：

```text
哪个事件让 progress_count 动起来？
哪个事件让 AC8 从 slogo 到 loadingbar？
哪个事件让检查网络/更新 UI 出现？
```

不是直接找哪个事件跳到 login/network。

事件矩阵输出改成：

```text
seq
AC8变化
progress变化
bar/textbar是否画
slogo->loadingbar是否衔接
检查网络/更新字符串是否出现
ui_mode是否仍在0x45
```

---

## 8. 2EC6B0 返回值 A/B 继续，但验收改成动画是否推进

继续测试：

```text
R0=0
R0=1
R0=obj
R0=pixels
```

但观察指标改成：

```text
progress_count 是否增长；
bar/textbar 是否开始画；
AC8 是否自然变化；
检查网络/更新 UI 是否出现；
ui_mode 0x45 内部阶段是否推进。
```

不是只看是否离开 0x45。

---

## 9. v46 推荐运行矩阵

### Run A：自然启动检查流程

```powershell
$env:JJFB_FORCE_UI_MODE="45"
$env:JJFB_AC8_MODE="natural"
$env:JJFB_PROGRESS_NUDGE="0"
.\RUN_V46_STARTUP_CHECK_UI.ps1
```

### Run B：slogo 一次后释放

```powershell
$env:JJFB_FORCE_UI_MODE="45"
$env:JJFB_AC8_MODE="force_slogo_once_then_release"
.\RUN_V46_STARTUP_CHECK_UI.ps1
```

### Run C：progress 动画扫描

```powershell
$env:JJFB_FORCE_UI_MODE="45"
$env:JJFB_PROGRESS_SCAN="1"
.\RUN_V46_STARTUP_CHECK_UI.ps1
```

### Run D：事件矩阵看启动动画

```powershell
$env:JJFB_EVENT_MATRIX="1"
$env:JJFB_EVENT_MATRIX_TARGET="startup_ui"
.\RUN_V46_STARTUP_EVENT_MATRIX.ps1
```

---

## 10. v46 必须输出报告

```text
reports/v46_startup_check_ui_flow.md
```

必须回答：

```text
1. 0x45 内部有哪些阶段？
2. AC8 表示什么？
3. progress_count 表示什么？
4. slogo -> loadingbar 是否能自然衔接？
5. bar/textbar 为什么不画？
6. 检查网络/检查更新 UI 字符串是否存在、是否被读？
7. 哪个事件/返回值/状态写入能让启动动画推进？
8. 当前最小阻塞点是什么？
```

---

## 11. 成功标准

### 最低成功

```text
slogo 和 loadingbar 能在同一次运行里按原版资源连续出现；
紫色透明正确；
坐标大体符合 240×320。
```

### 中级成功

```text
bar/textbar progress 动画开始；
progress_count > 0；
检查网络/检查更新相关文字或状态出现。
```

### 高级成功

```text
启动检查 UI 跑完，进入下一层 login/server/update/network 流程。
```

---

## 12. 给 Cursor 的一句话

**目标调整：不是跳过 splash，也不是只追网络，而是让机甲风暴原版“启动检查网络/检查更新 UI 和动画”正常跑完。v46 请把 0x45 内部启动状态机作为主线：追 AC8、BA0+0x2C progress、bar/textbar 动画、检查网络/更新字符串和事件/timer 回调；保留 240×320、0xF81F 透明和真实资源 blit，但不要 host overlay、不要假 UI、不要为了离开 0x45 而跳过启动动画。**
