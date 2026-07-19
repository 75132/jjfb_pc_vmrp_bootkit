# Cursor 继续开发说明：v31 — 0x12340 文本/字形接口与最终 present 链

> 当前最新状态：刷新链已经从“完全没挂”推进到“部分绘制逻辑在跑”。  
> 重点不再是 loader、robotol、timer、mr_registerAPP，而是：**0x12340 文本/字形函数池接口 + 后续 drawBitmap / DispUpEx / mrc_refreshScreen / SDL present 链。**

---

## 1. 当前已确认进展

已经确认：

```text
mr_registerAPP 已实现，ret=0；
app=24 -> 0x3130EC(code=0)，主要写流缓冲，不是最终刷屏；
真实 0x2FC8B8 调用后，刷新门控 C44 从 0 -> 1；
7D8+0xC 原为 0，seed 后打开通路；
drawFP@1510 原无效 0x270F，已试接 helper drawBitmap；
seed 后大量出现新码 0x12340，约 160 次；
libpng warning 出现，说明 MRP 图片资源确实被解码过。
```

关键判断：

```text
事件循环在跑；
部分 UI/文本绘制逻辑在跑；
图片资源已有解码迹象；
但还没有走到最终 blit / refresh / SDL present。
```

因此白屏现在不是“程序完全没动”，而是：

```text
UI 绘制链还停在中间层，未到最终屏幕提交。
```

---

## 2. 当前不要回滚的部分

不要回头做：

```text
1. 不要回滚 loader / mrc_loader / robotol。
2. 不要再猜 sdk_key。
3. 不要再改 mrc_loader.ext 为 extName。
4. 不要再修 mr_registerAPP 作为主线，它目前不是主阻塞。
5. 不要强行 fake 一个 mrc_refreshScreen 来制造画面。
6. 不要把 libpng warning 当失败，它反而说明图片解码链开始跑了。
```

---

## 3. 当前主线判断

`0x12340` 很可能是函数池里的文本/字形接口，可能对应以下几类之一：

```text
1. getTextWidth / getStringWidth
2. drawText / drawChar / fontDraw
3. glyph layout / text measure
4. rich text / bitmap font lookup
5. UI 控件文本绘制回调
```

现在它不能继续作为 unhandled code 简单 return。  
必须把它反汇编和平台映射搞清楚。

---

## 4. 0x12340 反汇编任务

### 任务 1：收集 0x12340 调用样本

每次出现 `0x12340` 时记录完整五参数：

```text
extCode
app
code
param0
param1
return
caller LR
SP 附近 0x40 bytes
```

同时 dump：

```text
param0 指向内容
param1 指向内容
code 指向内容
app 指向内容
```

注意：如果是 ARM 调用，LR / PC 附近也要 dump，定位调用点。

建议日志格式：

```text
[JJFB_12340] n=%d extCode=0x%X app=0x%X code=0x%X p0=0x%X p1=0x%X lr=0x%X sp=0x%X ret=%d
[JJFB_12340_DUMP] p0 bytes/string/utf16/base64-like
[JJFB_12340_DUMP] p1 bytes/string/utf16/base64-like
```

---

### 任务 2：区分“测宽”和“绘制”

文本函数通常分两类：

```text
A. 测宽/排版：
   输入字符串，返回 width / height / glyph count。
   不会触发 JJFB_DRAW。

B. 绘制：
   输入 x/y/color/string/font，写 screen buffer 或调用 drawFP。
   最终应触发 drawBitmap / DrawText / refresh。
```

所以要看 `0x12340` 返回值是否被后续用于：

```text
坐标计算
宽度判断
布局偏移
分支条件
```

如果 `0x12340` 只返回固定 1，可能会把布局逻辑搞坏。  
建议尝试根据字符串长度返回合理宽度：

```text
ASCII: width = len * 8
中文/双字节: width = chars * 12 或 16
height = 16
```

但先不要急着改返回值，先记录谁使用返回值。

---

### 任务 3：定位 0x305e70 -> 0x12340 后续

当前线索：

```text
0x305e70 -> 0x12340
```

要重点看 `0x305e70` 之后：

```text
1. 是否继续调用 drawFP@1510？
2. 是否写 7D8 结构？
3. 是否写 C44 门控？
4. 是否调用 0x2f2a00？
5. 是否调用 DispUpEx / drawBitmap / refreshScreen？
6. 是否因为 0x12340 返回值不对提前 return？
```

建议对 `0x305e70` 前后加 trace：

```text
[JJFB_UI] enter 0x305e70
[JJFB_UI] before 0x12340
[JJFB_UI] after 0x12340 ret=...
[JJFB_UI] branch target=...
[JJFB_UI] leaving 0x305e70 ret=...
```

---

## 5. drawFP@1510 的处理

当前：

```text
drawFP@1510 原为无效 0x270F；
已试接到 helper drawBitmap。
```

这说明 `drawFP@1510` 可能是一个函数指针槽，但 seed 值可能不自然。

下一步不要只硬接 drawBitmap，而要查：

```text
1. 1510 属于哪个结构？
2. 这个结构由谁初始化？
3. 真机应该填什么函数？
4. 是否由 state0 / 0x2e48xx UI init 自然填充？
5. 当前 seed 是否跳过了真正 init？
```

建议日志：

```text
[JJFB_FP] write drawFP@1510 old=0x%X new=0x%X pc=0x%X lr=0x%X
[JJFB_FP] call drawFP@1510 target=0x%X args=...
```

---

## 6. libpng warning 的意义

出现 libpng warning 说明：

```text
MRP 内图片资源确实被读取/解码；
资源链并非完全没启动。
```

下一步要找：

```text
1. 解码后的 bitmap 存到哪里？
2. bitmap 是否进入 screen buffer？
3. bitmap 是否被 drawBitmap / blit 调用？
4. 是否只是预加载资源，还没显示？
```

建议在 PNG 解码后加：

```text
[JJFB_PNG] decoded w=%d h=%d bpp=%d dst=0x%X size=%d
[JJFB_BMP] bitmap object ptr=0x%X w=%d h=%d data=0x%X
```

再追踪这个 `dst/data` 是否被后续使用。

---

## 7. present 链排查

当前 `JJFB_DRAW=0`，说明原有 draw 探针还没捕到最终绘制。  
要扩大探针范围：

```text
mrc_refreshScreen
mrc_clearScreen
w_getScreenBuffer
w_setScreenBuffer
mrc_drawText
mrc_drawBitmap
bitmapShow
mrc_bitmapShowEx
DispUpEx
_DrawRect
_DrawText
_drawChar / font draw
memcpy 到 screen buffer
SDL_UpdateTexture
SDL_RenderCopy
SDL_RenderPresent
```

分层判断：

```text
A. 有文本/图片函数，但无 screen buffer 写入：
   卡在平台 API 映射。

B. 有 screen buffer 写入，但无 refresh：
   卡在刷新链。

C. 有 refresh，但 SDL 不更新：
   卡在 SDL present / pixel format。

D. 有 SDL present，但仍白：
   可能颜色格式 / buffer 地址 / pitch 错。
```

---

## 8. 收敛 FORCE / seed

当前 seed 已打开通路，但后续要尽量收敛：

```text
不要永久依赖强制 seed。
要找到自然初始化 7D8 / C44 / drawFP 的路径。
```

优先反查：

```text
state0
0x2e48xx UI init
0x2FC8B8
7D8 结构写入点
C44 写入点
drawFP@1510 写入点
```

目标是让这些结构由原逻辑填好，而不是 host 侧硬塞。

---

## 9. 成功标准

最低成功：

```text
0x12340 不再 unhandled；
能解释它是测宽还是画字；
0x305e70 -> 0x12340 后续分支明确。
```

中级成功：

```text
出现真实 drawText / drawBitmap / screen buffer 写入；
JJFB_DRAW > 0。
```

高级成功：

```text
出现 mrc_refreshScreen / SDL_RenderPresent；
窗口不再白屏；
或出现 module.ext / initNetwork。
```

---

## 10. 给 Cursor 的一句话总结

**当前刷新链已经从“没挂”推进到“文本/资源绘制逻辑在跑”。下一步不要回滚 loader，也不要 fake refresh。请把 0x12340 当作文本/字形函数池接口完整反汇编，追踪 0x305e70 -> 0x12340 后续是否因返回值或函数指针导致未进入 drawBitmap/DispUpEx，同时收敛 7D8/C44/drawFP 的 FORCE/seed，让 UI init 自然填结构并最终到 mrc_refreshScreen / SDL present。**
