# Cursor 继续开发说明：v37 — 固定 240×320，专攻 0x10134 bitmap 构造 + 310BB4 原版资源 blit

> 用户要求：分辨率统一按 **240×320**。  
> 当前目标不是继续美化 DEBUG 文本，也不是重写完整模拟器，而是让 **机甲风暴原版 splash/loading 资源** 真正进入 guest screen buffer。

---

## 1. 这包日志的关键结论

v35b 已经修正了一个大方向错误：

```text
旧 FORCE state 0 -> 1 会跳过真 splash
新 FORCE state/ui_mode -> 0x45 后进入 0x2EF86C 真 splash
```

证据：

```text
[JJFB_FIRST_SCREEN] FORCE state/ui_mode 0x0 -> 0x45
[JJFB_FIRST_SCREEN] SPLASH_ENTER #1 lr=0x306631 ui_mode=0x45
[JJFB_310BB4_RESOURCE] name="loadingbar!201!29.bmp"
[JJFB_310BB4_RESOURCE] name="bar!16!18.bmp"
[JJFB_310BB4_RESOURCE] name="textbar!120!30.bmp"
```

这说明现在已经进入 **原版 splash/loading 分支**。  
下一步不是再追 state，而是让这些资源真的变成 bitmap 并画到 240×320 屏幕上。

---

## 2. 分辨率统一规则

从现在开始所有屏幕相关逻辑统一：

```text
screen_w = 240
screen_h = 320
screen_bpp = 16
screen_format = RGB565
screen_bytes = 240 * 320 * 2 = 153600 = 0x25800
```

已有 guest 字段应保持：

```text
0x824 = 240   // screen width
0x828 = 0
0x830 = 320   // screen height
0x834 = 240   // screen width
```

注意：

```text
0x830 / 0x834 不是 clock，不要再写 uptime。
```

SDL 可以窗口放大显示，但逻辑画布必须是：

```text
240×320
```

不要把资源本身都拉伸到 240×320。  
资源保持自身尺寸，例如 loadingbar 是 201×29，只是 blit 到 240×320 canvas 上。

---

## 3. 0x10134 的重要发现

日志里 0x10134 的 `app` 参数不是随机值，它正好等于：

```text
资源宽 × 资源高 × 2
```

即 RGB565 像素字节数。

### 例子

```text
loadingbar!201!29.bmp
201 * 29 * 2 = 11658 = 0x2D8A
日志：0x10134 app=0x2D8A
```

```text
bar!16!18.bmp
16 * 18 * 2 = 576 = 0x240
日志：0x10134 app=0x240
```

```text
textbar!120!30.bmp
120 * 30 * 2 = 7200 = 0x1C20
日志：0x10134 app=0x1C20
```

```text
wy_jiao1!11!11.bmp
11 * 11 * 2 = 242 = 0xF2
日志：0x10134 app=0xF2
```

所以 0x10134 高概率是：

```text
bitmap pixel buffer alloc / construct / decode complete / cache sync 相关接口
```

不能继续简单 fail-open，否则资源名出来了，但 bitmap 对象无法稳定进入绘制链。

---

## 4. 当前最快推进点

现在不要优先继续画 `CYFmhdNmS1roRgroRa==`。  
真正应该做的是：

```text
2d92dc 识别资源名
  -> 解析 name!w!h.bmp
  -> 0x10134 app == w*h*2
  -> 构造/返回 RGB565 bitmap buffer
  -> 310BB4 把 bitmap blit 到 240×320 screen buffer
  -> DEBUG_PRESENT 只显示 guest buffer
```

这条才是“原版 loadingbar / splash 资源可见”的最短路径。

---

## 5. 资源尺寸解析

在 `2d92dc` 看到资源名时，建立当前资源上下文：

```c
struct JjfbBmpReq {
    char name[128];
    int w;
    int h;
    int bytes;        // w * h * 2
    uint32_t caller_lr;
    uint32_t name_ptr;
};
```

解析规则：

```text
xxx!W!H.bmp
```

例如：

```text
loadingbar!201!29.bmp  -> w=201 h=29 bytes=0x2D8A
bar!16!18.bmp          -> w=16  h=18 bytes=0x240
textbar!120!30.bmp     -> w=120 h=30 bytes=0x1C20
```

记录：

```text
[JJFB_BMP_REQ] name=loadingbar!201!29.bmp w=201 h=29 bytes=0x2D8A lr=...
```

---

## 6. 0x10134 最小实现方向

当收到：

```text
extCode = 0x10134
app     = expected_bytes
code    = 0
param0  = 0
param1  = 0
```

并且存在最近的 `JjfbBmpReq`，且：

```text
app == req.w * req.h * 2
```

则执行：

```text
1. 分配 guest RGB565 buffer，大小 app。
2. 从 MRP 资源中读取/解码对应 bmp 到 RGB565。
3. 暂时无法解码时，至少返回一个稳定的 guest buffer，不要返回毒指针。
4. 记录 buffer ptr、w、h、bytes、resource name。
5. 返回值优先试 guest pixel buffer 指针。
```

日志：

```text
[JJFB_10134_BMP] name=... w=... h=... bytes=... guest_pixels=0x...
```

如果返回值不对，再试：

```text
A. 返回 pixel buffer ptr
B. 返回 bitmap object ptr
C. 返回 0 表示成功，同时把 ptr 写入调用方期望位置
```

但每次只改一个变量，避免混乱。

---

## 7. 310BB4 原版资源 blit

日志已经显示 `310BB4` 进入后有类似像素数据：

```text
dump 310BB4_r5_p4 @0x2AD140
1F F8 1F F8 ...
```

这看起来就是 RGB565 图像数据。

下一步不要只 skip `310BB4`，而是做 **guarded host-side blit**：

```text
1. 仍然不执行危险贴图循环主体。
2. 在 hook 里解析 r5 bitmap object。
3. 找到 pixel ptr / w / h。
4. 如果对象字段不清楚，先用最近 JjfbBmpReq 的 w/h 和 r5_p4 当像素数据。
5. 按 240×320 clip 后写入 guest screen buffer。
6. 标记 screen_dirty。
7. DEBUG_PRESENT 显示 guest buffer。
```

坐标推断：

```text
310BB4 日志里 r1 常见 265/275/290/305，r2 常见 5/15/30/40/45。
在 240×320 竖屏中，r1 更像 y，r2 更像 x。
```

所以优先尝试：

```text
dst_y = r1
dst_x = r2
```

如果位置明显不对，再 A/B 测试：

```text
dst_x = r1, dst_y = r2
```

但默认按 240×320 裁剪：

```c
if (dst_x >= 240 || dst_y >= 320) skip;
clip_w = min(w, 240 - dst_x);
clip_h = min(h, 320 - dst_y);
```

日志：

```text
[JJFB_310BB4_BLIT] name=... x=... y=... w=... h=... src=0x... dst=screen clip=...
```

---

## 8. 不要再做的事

```text
❌ 不要把所有资源拉伸成 240×320。
❌ 不要把 240×320 理解为资源尺寸。
❌ 不要继续把 0x10134 fail-open 当长期方案。
❌ 不要继续只画 0x11F00 ASCII token。
❌ 不要 host overlay。
❌ 不要 fake 原生 mrc_refreshScreen / DispUpEx。
❌ 不要深挖完整 chrome，先让 loadingbar/splash 可见。
```

---

## 9. 推荐运行策略

### Run 1：固定 240×320 + 10134 资源构造

```powershell
$env:JJFB_SCREEN_W="240"
$env:JJFB_SCREEN_H="320"
$env:JJFB_FORCE_SPLASH_NUDGE="45"
$env:JJFB_ALLOW_CHROME="1"
$env:JJFB_CHROME_SKIP_310BB4="1"
.\RUN_V37_240x320_BMP.ps1
```

目标：

```text
看到 2d92dc 请求 loadingbar/bar/textbar；
看到 0x10134_BMP；
确认 app == w*h*2；
确认 guest buffer 分配成功。
```

### Run 2：开启 310BB4 hook blit，不执行原循环

```powershell
$env:JJFB_SCREEN_W="240"
$env:JJFB_SCREEN_H="320"
$env:JJFB_FORCE_SPLASH_NUDGE="45"
$env:JJFB_ALLOW_CHROME="1"
$env:JJFB_CHROME_SKIP_310BB4="1"
$env:JJFB_310BB4_HOST_BLIT="1"
.\RUN_V37_240x320_BMP.ps1
```

目标：

```text
loadingbar / bar / textbar 这类原版 bitmap 能被 blit 到 guest screen buffer。
```

---

## 10. 成功标准

最低成功：

```text
0x10134 不再只是 fail-open；
能根据资源名和 app 构造 RGB565 bitmap buffer；
日志证明 app == w*h*2。
```

中级成功：

```text
310BB4 hook 能把 loadingbar/bar/textbar 原版像素写入 240×320 guest screen buffer。
```

阶段成功：

```text
窗口中能看到来自原版资源的 loadingbar / textbar / splash 元素。
即使边框不完整、slogo 还没出、原生 refresh 仍为 0，也算本阶段成功。
```

---

## 11. 给 Cursor 的一句话

**v35b 已经确认 FORCE 到 0x45 进入真 splash，并请求 loadingbar/bar/textbar。分辨率统一固定为 240×320。下一步不要继续美化 0x11F00 token，也不要把 0x10134 fail-open；请利用 `资源名!W!H.bmp` 和 `0x10134 app == W*H*2` 这个强证据，先实现 bitmap buffer 构造，再在 310BB4 hook 中按 240×320 clip 做原版资源 blit，让 loadingbar/splash 元素真正显示出来。**
