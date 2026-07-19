# Cursor 继续开发说明：v38 — 去掉 eager 假坐标，绑定真实 bitmap 对象与 slogo 首屏

> v37 已经是关键突破：**原版 splash 资源真的进入了 240×320 guest buffer**。  
> 下一步不要回退，也不要继续美化 0x11F00 token。  
> 当前主线是：**从 EAGER_BLIT 临时显示，收敛到 guest 自己的真实布局/bitmap 对象/310BB4 blit，并查 slogo 为什么不进请求链。**

---

## 1. 固定规则：240×320，不再改

后续所有逻辑统一：

```text
screen_w = 240
screen_h = 320
format   = RGB565
bytes    = 240 * 320 * 2 = 153600 = 0x25800
```

guest 字段固定：

```text
0x824 = 240
0x828 = 0
0x830 = 320
0x834 = 240
```

注意：

```text
240×320 是逻辑画布，不是资源尺寸。
资源保持原尺寸，例如 loadingbar 201×29，textbar 120×30。
```

---

## 2. v37 已确认的强事实

v37 链路已经成立：

```text
FORCE -> ui_mode/state 0x45
-> SPLASH_ENTER
-> 2d92dc 请求原版 splash 资源
-> 0x10134 app == W*H*2
-> 构造 RGB565 buffer
-> EAGER_BLIT 写入 240×320 guest buffer
-> DEBUG_PRESENT 显示 guest buffer
```

关键资源：

```text
loadingbar!201!29.bmp @ EAGER (19,200)
bar!16!18.bmp         @ EAGER (112,210)
textbar!120!30.bmp    @ EAGER (60,240)
```

关键公式已验证：

```text
loadingbar: 201 * 29 * 2 = 0x2D8A
bar:        16  * 18 * 2 = 0x240
textbar:    120 * 30 * 2 = 0x1C20
```

所以：

```text
0x10134 不是普通 unknown，不要再 fail-open。
它至少是 bitmap pixel buffer / resource object 构造相关接口。
```

---

## 3. 当前还不是最终原版布局

v37 可见资源是实质突破，但仍然有临时成分：

```text
EAGER_BLIT 坐标是 host 临时居中坐标；
不是 guest 真实坐标；
原生 DispUpEx / mrc_refreshScreen 仍为 0；
slogo 缓存存在，但运行时常不请求。
```

所以 v38 的目标是：

```text
把 splash 资源显示，从 “host eager 坐标” 收敛到 “guest 真坐标 / 真对象 / 真 310BB4 blit”。
```

---

## 4. P0：用 guest 真坐标替换 EAGER 坐标

现在 EAGER 坐标只是临时策略：

```text
loadingbar (19,200)
bar        (112,210)
textbar    (60,240)
```

下一步要找 guest 自己的坐标来源。

### 需要记录

对 loadingbar/bar/textbar 从 `2d92dc` 到 `0x10134` 再到后续消费者，记录：

```text
resource name
caller LR
return value
guest_pixels
调用前 r0-r7/sp
调用后 r0-r7/sp
后续谁读 guest_pixels
后续谁读 resource object
```

重点：

```text
给 guest_pixels 地址加 read watch / access log。
一旦被读取，打印 PC/LR/r0-r7/SP。
```

目标：

```text
找到 loadingbar/bar/textbar 的真实绘制调用点和真实 x/y。
```

---

## 5. P1：绑定 0x10134 返回值到真实 bitmap object

当前 `0x10134` 返回的是 pixel buffer ptr，这已经能让缓存/像素稳定。  
但 guest 后续是否真的拿它当 bitmap 还不确定。

要测试三种模式，每次只改一个变量：

### 模式 A：返回 pixel buffer ptr

```text
当前 v37 模式。
优点：已能显示资源。
缺点：可能无法形成完整 bitmap object，导致 310BB4 对主体资源不自然调用。
```

### 模式 B：返回 bitmap object ptr

构造最小对象：

```c
struct JjfbBitmapObject {
    uint32_t pixels;
    int16_t  w;
    int16_t  h;
    int16_t  stride;
    int16_t  format; // RGB565
    ...
};
```

返回 object ptr，看后续 `310BB4` 是否能自动消费。

### 模式 C：返回 0 表示成功，写出指针到调用方期望位置

需要通过 watch 调用前后内存确认。

### 结论目标

```text
让 loadingbar/bar/textbar 不依赖 EAGER_BLIT；
而是自然进入 310BB4 或同类 blit 调用。
```

---

## 6. P2：310BB4 坐标/对象绑定

日志已说明 chrome 的 310BB4 坐标模式大概率是：

```text
r1 = y
r2 = x
```

例如：

```text
x=15 y=265
x=15 y=311
x=30 y=265
...
```

v38 要扩展到 splash 主体资源：

```text
如果 310BB4 对 loadingbar/bar/textbar 出现：
    用 r1/r2 做 y/x
    用 bitmap object 的 w/h
    clip 到 240×320
    写 guest screen buffer
```

如果 loadingbar 仍不触发 310BB4：

```text
说明 0x10134 返回/对象绑定还不对，回到 P1。
```

---

## 7. P3：查 slogo 请求条件

zip 里已经有：

```text
slogo!157!58.bmp.rgb565
```

但运行时常不请求。说明资源存在，问题是状态路径/条件没有走到。

不要手动 eager slogo。要追它为什么不请求。

### 静态

找：

```text
slogo!157!58.bmp
```

得到：

```text
string_addr
xref function
附近分支条件
```

### 动态

给 slogo 字符串地址做 read/access watch：

```text
[JJFB_SLOGO_XREF] pc lr r0-r7 sp ui_mode state gates
```

### A/B 运行

比较：

```text
JJFB_FORCE_SPLASH_NUDGE=45
JJFB_FORCE_SPLASH_NUDGE=0
其他可能 ui_mode 值
```

重点看：

```text
哪个状态进入 slogo 分支；
是否 0x45 只进 loadingbar，不进 logo；
是否某个 gate/flag 已经标记 logo 显示过。
```

---

## 8. P4：保留 DEBUG_PRESENT，但不要冒充原生 refresh

当前可以继续用：

```text
DEBUG_PRESENT
```

显示 guest buffer，因为它显示的是游戏资源写入后的 buffer，不是 host overlay。

但日志必须继续明确：

```text
DEBUG_PRESENT only
native DispUpEx = 0
native mrc_refreshScreen = 0
```

不要把 DEBUG_PRESENT 说成原生 refresh。  
最终仍然要追：

```text
DispUpEx / mrc_refreshScreen / native dirty gate
```

但 v38 优先级低于真实坐标和 slogo。

---

## 9. 不要做

```text
❌ 不要回退到 FORCE state 0 -> 1。
❌ 不要再让 0x10134 fail-open。
❌ 不要把资源拉伸到 240×320。
❌ 不要美化 0x11F00 token。
❌ 不要 host overlay。
❌ 不要把 EAGER 坐标当最终原版布局。
❌ 不要手动把 slogo 画上去冒充原版路径。
```

---

## 10. v38 成功标准

### 最低成功

```text
确认 loadingbar/bar/textbar 的 guest 真坐标来源；
或确认它们为什么没有自然触发 310BB4。
```

### 中级成功

```text
0x10134 返回/对象绑定后，loadingbar/bar/textbar 能自然进入 310BB4 或真实 blit 消费路径。
```

### 高级成功

```text
slogo!157!58.bmp 被运行时请求；
原版 splash 布局中出现 logo + loadingbar/textbar。
```

### 阶段成果

```text
240×320 窗口中看到来自原版资源、位置更接近原版逻辑的 splash/loading 画面。
```

---

## 11. 给 Cursor 的一句话

**v37 已打通真 splash 资源：FORCE 0x45 后 loadingbar/bar/textbar 经 0x10134 构造 RGB565 并进入 240×320 guest buffer。v38 不要继续美化 DEBUG_PRESENT，也不要把 eager 坐标当最终结果；请追 0x10134 返回值的真实对象语义和 guest_pixels 的后续读者，让 loadingbar/textbar 自然进入 310BB4/真实 blit，并静态+动态追 slogo 请求条件。**
