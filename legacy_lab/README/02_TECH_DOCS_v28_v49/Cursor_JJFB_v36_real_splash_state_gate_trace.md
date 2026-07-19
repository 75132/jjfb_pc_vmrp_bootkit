# Cursor 继续开发说明：v36 — 停止“看得见但不是原版”，追真正 splash/login 入口

> 用户反馈：当前窗口里看到的不是想要的“原版机甲风暴启动画面”。  
> 这个反馈是对的。v35 虽然证明 guest buffer 可以被 DEBUG_PRESENT 显示，但**尚未进入真正的原版 splash/loading/logo 资源链**。  
> 下一步不要继续美化当前 DEBUG 文本/色块，而要追：为什么 `slogo/loadingbar/logo/login` 没有被请求，为什么 state/gate 没有进入真正首屏路径。

---

## 1. v35 的真实结论

### 已经做到

```text
loader / mrc_loader / robotol / mrc_init / timer / 0x11F00 / guest buffer DEBUG_PRESENT 已通。
```

### 但用户看到的不是原版游戏画面

日志证据：

```text
[JJFB_FIRST_SCREEN] cleared guest RGB565 to dark
[JJFB_DEBUG_PRESENT] from 11F00 dirty 0,0 240x320
[JJFB_FIRST_SCREEN] 11f00_ascii CYFmhdNmS1roRgroRa==
```

这说明现在显示的是：

```text
暗底 + 从 0x11F00 code 对象里抓出的 ASCII token / 占位字素
```

不是：

```text
机甲风暴 logo
loadingbar
登录页
服务器列表
原版标题
```

### 本轮没有出现主体首屏资源

`README_结论.md` 已确认：

```text
本轮未出现 slogo!157!58.bmp / loadingbar 的 open
真正 logo/loading 首屏资源尚未被请求
0x11F00 payload 仍是 base64 token，不是可读「机甲风暴」标题
原生 DispUpEx / mrc_refreshScreen = 0
```

所以 v35 是“显示管线诊断成功”，不是“原版首屏成功”。

---

## 2. 现在不要继续做什么

```text
❌ 不要继续美化 DEBUG_PRESENT 画面。
❌ 不要把 0x11F00 的 CYFmhdNmS1roRgroRa== 当作游戏标题继续画。
❌ 不要继续把目标说成“窗口不白”。
❌ 不要优先修 310BB4 chrome 边框。
❌ 不要只靠 0x10134 fail-open 混过去。
❌ 不要继续 FORCE state 后不验证是否跳过真实 splash。
```

用户要的是：

```text
原版 MRP 自己的启动/loading/logo/登录首屏
```

不是：

```text
host debug 显示、占位块、假文字、base64 token。
```

---

## 3. 当前最可疑点

### 3.1 FORCE state 0 -> 1 可能跳过了自然 splash

日志：

```text
[JJFB_SEND] FORCE state 0 -> 1 (splash nudge)
[JJFB_FIRST_SCREEN] state_change 0 -> 1
```

这个“splash nudge”可能有两种结果：

```text
A. 帮助走过卡点；
B. 直接跳过真正 state0 splash 初始化，导致 slogo/loadingbar 没被请求。
```

必须做 A/B 测试：

```text
Run A: 禁用 FORCE state 0 -> 1
Run B: 启用 FORCE state 0 -> 1
对比：
- 是否请求 slogo/loadingbar
- state/gate 变化
- 2d92dc 名字链
- 0x11F00 code 对象变化
```

新增环境变量：

```text
JJFB_FORCE_SPLASH_NUDGE=0/1
```

默认建议：

```text
先跑 0，即不强推 state。
```

---

### 3.2 0x10134 fail-open 可能吞掉真实资源构造

v35 日志：

```text
[JJFB_10134] fail-open app=0xF2 code=0x0 p0=0x0 p1=0x0
[JJFB_10134] fail-open app=0xD2 code=0x0 p0=0x0 p1=0x0
[JJFB_10134] fail-open app=0xC8 code=0x0 p0=0x0 p1=0x0
...
```

这可能只是让流程不崩，但也可能让：

```text
资源 handle / bitmap object / loadingbar object 永远没构造出来。
```

不要永久 fail-open。要改成：

```text
1. 完整记录 0x10134 的调用上下文；
2. 读取 LR / caller / SP；
3. dump app 对应对象；
4. 记录调用前后内存；
5. 尝试识别它是否应返回 resource object / status / handle。
```

尤其对：

```text
app=0xF2
app=0xD2
app=0xC8
app=0xB4
app=0x96
app=0x168
app=0xF0
```

建立表：

```text
app value -> caller LR -> prior resource name -> expected result -> actual result
```

---

### 3.3 slogo / loadingbar 在 robotol 里存在，但没有进入 open 链

已知 robotol 内嵌：

```text
slogo!157!58.bmp
loadingbar!201!29.bmp
机甲 / 风暴 / 登录 / 加载
```

但 v35 未出现 open。说明现在不是“资源解码错”，而是：

```text
程序路径没有走到请求主体首屏资源的分支。
```

下一步必须追这些字符串的 xref / 运行时访问点。

---

## 4. v36 开发主线

## P0：从“显示管线”转向“首屏状态机”

新增状态追踪：

```text
state 主状态
substate / gate:
C44
C9D
CD1
CF5
7D8+0xC
ptr11B0
drawFP@1510
screen 824/828/830/834
```

每 tick 输出差异，不要每次刷屏刷爆日志：

```text
[JJFB_STATE_DIFF] tick=... state old->new C44 old->new ...
```

目标是找：

```text
哪个 gate 没开，导致 slogo/loadingbar 分支不走。
```

---

## P1：定位 slogo/loadingbar 字符串 xref

不要等 open 出现。主动找：

```text
slogo!157!58.bmp
loadingbar!201!29.bmp
logo
login
loading
server
start
enter
机甲
风暴
登录
加载
```

做两层：

### 静态

在 robotol.ext / dsm_gm.mrp 解压后的 code/data 中找字符串地址：

```text
string_addr
附近 +/- 0x100 bytes
可能的函数引用 / literal pool
```

### 动态

在 guest 内存中给这些字符串地址加读取观察：

```text
一旦 PC/LR 读取或传参，打印：
[JJFB_SPLASH_XREF] name=... pc=... lr=... r0-r7 sp...
```

目标：

```text
不是先修显示，而是确认 splash 代码是否被执行。
```

---

## P2：重审 2d92dc 资源加载点

目前 2d92dc 已看到 chrome 名字：

```text
wy_jiao*
wy_xian*
jiantou*
```

但没看到：

```text
slogo
loadingbar
logo
login
```

所以要记录 2d92dc 每次调用：

```text
name
caller LR
state/gate
return
prior/following 0x10132/0x10134
```

如果 2d92dc 只进 chrome，不进主体资源，说明状态路径仍偏在 UI chrome，而不是 splash page。

---

## P3：0x11F00 不再当首屏目标，除非 code 对象变了

当前 0x11F00 反复输出：

```text
CYFmhdNmS1roRgroRa==
```

这很可能是：

```text
富文本 token / glyph resource id / 编码块
```

不是原版标题。  
v36 中 0x11F00 只保留诊断：

```text
记录 code 指针是否变化
记录是否出现 GBK/UTF-16 “机甲/风暴/登录/加载”
```

不要继续把当前 token 画得更好看。

---

## P4：保留 chrome guard，不深挖 310BB4

当前为了先看到原版启动首屏：

```text
JJFB_ALLOW_CHROME=1
JJFB_CHROME_SKIP_310BB4=1
```

保持即可。  
`wy_jiao/wy_xian/jiantou` 是装饰框，不能让它继续拖慢首屏目标。

---

## 5. v36 建议运行矩阵

### Run A：自然 state，不 force

```powershell
$env:JJFB_FORCE_SPLASH_NUDGE="0"
$env:JJFB_ALLOW_CHROME="1"
$env:JJFB_CHROME_SKIP_310BB4="1"
.\RUN_V36_SPLASH_TRACE.ps1
```

### Run B：保留旧 force

```powershell
$env:JJFB_FORCE_SPLASH_NUDGE="1"
$env:JJFB_ALLOW_CHROME="1"
$env:JJFB_CHROME_SKIP_310BB4="1"
.\RUN_V36_SPLASH_TRACE.ps1
```

对比输出：

```text
是否出现 slogo/loadingbar 字符串访问
是否出现 slogo/loadingbar open
state/gate 是否不同
0x11F00 code 是否变化
0x10134 app/caller 是否不同
```

---

## 6. 成功标准

### 最低成功

```text
确认 slogo/loadingbar 字符串是否被执行路径访问。
```

### 中级成功

```text
看到 slogo/loadingbar/logo/login 等主体资源被 2d92dc 或 mr_open 请求。
```

### 高级成功

```text
主体资源对象构造成功，并写入 guest screen buffer。
```

### 用户可接受阶段成果

```text
窗口看到来自原版资源/原版字符串的机甲风暴启动/加载/登录首屏内容。
即使边框缺失、按钮缺图、native refresh 仍未出现，也可以接受。
```

---

## 7. 给 Cursor 的一句话

**v35 看到的不是原版游戏首屏，只是 guest buffer 的 debug/text bring-up。下一步不要继续美化 DEBUG_PRESENT，也不要优先修 310BB4 chrome。请转向 v36：做 no-force/force 对照，追 state/gate 为什么没有请求 slogo/loadingbar，给 slogo/loadingbar/logo/login 等字符串做静态 xref + 动态访问监控，并重点分析 0x10134 是否吞掉了主体资源构造。目标是让原版 splash/loading/login 资源进入加载链。**
