# Cursor 继续开发说明：v33 — 真字素 0x11F00 / chrome ctx / 原生 present

> 最新结论：v32 已经把链路推进到 **present 中层**。  
> `loader → robotol → mrc_init → timer → UI tick → 305bf4 → 0x11F00 → DEBUG present` 已通。  
> 当前不要回滚 loader，不要假冒 `mrc_refreshScreen`，不要再盲接 `drawFP@1510`。下一步主攻：**0x11F00 真字素、2f4494 graphics ctx、原生 DispUpEx / refresh、资源路径**。

---

## 1. 当前状态一句话

```text
0x11F00 已经能写 host RGB565，占位块能 DEBUG present；
2f4494 已定性为 UI chrome 装饰框绘制，不是 wait/lock；
2f4494 现在精确 nop，避免 chrome 坏 FP 阻断主 UI；
原生 DispUpEx / mrc_refreshScreen 仍为 0；
下一步 = 0x11F00 真字素 + 修 chrome graphics ctx + 等原生 present。
```

---

## 2. 已确认，不要再反复验证

### 2.1 2f4494 根因

```text
2f284c → 2ea180(mode=0xa) → 2f4494
  → 303d94 blx r7=0x80278
```

其中：

```text
r7=0x80278 是 CODE 区附近垃圾 FP / stopAddr 附近
不是 mr_table stub
不是 DrawRect
不是锁等待
不是事件等待
```

现象：

```text
emu 停在 stopAddr，看起来像 2f4494 “不返回”。
```

所以当前处理正确：

```text
只 nop 0x2F4494 入口，让 PC=LR；
不再整段 bypass 2f284c。
```

这比以前绕过 `2f284c` 更精确。

---

### 2.2 present 中层已通

当前链路：

```text
2e87ac
  → 2f284c
  → 2f4494 当前 nop
  → 2e885a
  → 305bf4
  → 305c34
  → 2f2358
  → sendAppEvent(0x11F00)
  → host 写 jjfb_screen565
  → JJFB_DEBUG_PRESENT
```

证据：

```text
JJFB_DRAW_TEXT > 0
JJFB_DEBUG_PRESENT > 0
enter 0x305bf4 > 0
0x2f2358 每 tick 在跑
```

---

### 2.3 0x12340 已降级

```text
0x12340 = 逐字侧效应 / 文本度量接口
305e70 → 304550 → 0x12340
返回值被忽略
主要写 *param1
不是当前 present 阻塞点
```

不要继续围着 0x12340 返回值打转。

---

### 2.4 drawFP@1510 不要再 seed

```text
drawFP@1510 自然值常为 0x270F；
曾错误 seed 到 drawBitmap，出现 bmp=0 / 240x0；
ABI 不匹配。
```

当前策略正确：

```text
不 seed；
只记录谁写、谁读、谁调用。
```

---

## 3. 当前主攻 P0：0x11F00 真字素 / 真 blit

### 3.1 已知 ABI

```text
app  = 7
code = 富文本 / glyph 对象，不是纯 C 字符串
p0   = pack:
       +0x00 y, int16
       +0x02 x, int16
       +0x2C RGB888，例如 FF FF FF
```

当前 host 行为：

```text
用 p0+0x2C 的 RGB 写 8×16 占位块到 jjfb_screen565；
DEBUG present；
不声明为原生 mrc_refreshScreen。
```

这是 bring-up 正确做法，但还不是最终。

---

### 3.2 下一步必须解析 code 对象

对每次 0x11F00 调用，记录并保存：

```text
app/code/p0/p1/p2/p3/SP[0]
LR/PC/SP
p0 结构 0x80 bytes
code 对象 0x100~0x400 bytes
code 对象附近可打印串 / UTF-16 / GBK / base64-like 片段
```

建议将每种不同 `code` 指针只 dump 一次，避免日志爆炸：

```text
seen_code_ptr set
hash(code[0:0x100])
```

重点解析：

```text
1. code 对象 header
2. glyph run 数量
3. 字符编码
4. 每个 glyph 的 x/y/advance
5. 字体/颜色/alpha 字段
6. 是否引用 PNG/BMP/字库对象
```

---

### 3.3 判断 0x11F00 类型

按以下逻辑判断，不要先入为主：

```text
如果 code 对象含字符串 / glyph run：
    0x11F00 = drawText / glyphDraw

如果 code 对象含 bitmap pointer / w/h/stride：
    0x11F00 = bitmap blit / region copy

如果 p0 只有 rect/color，code 只是样式：
    0x11F00 = fillRect / drawRect / text background

如果只写测量输出，不写屏：
    0x11F00 = layout helper
```

但目前已有 `DEBUG present` 和屏幕写入，因此优先按 **drawText/glyphDraw** 处理。

---

### 3.4 最小真实实现建议

不要继续只画固定 8×16 色块。改成分层实现：

#### 阶段 A：占位字素，但按 glyph/run 数量排布

```text
每个 glyph 画 8×16 block；
按 code 对象里的 run / 字符数量横向推进；
支持 x/y/color；
clip 到 240×320。
```

#### 阶段 B：ASCII / 数字可读

```text
数字/ASCII 用 8×16 内置字模；
中文/未知仍画 block；
记录 JJFB_DRAW_TEXT_ASCII。
```

#### 阶段 C：真实字库

```text
解析 code 对象引用的 glyph / font / bitmap；
绘制真实字素。
```

---

## 4. P1：修 2f4494 graphics ctx，而不是长期 nop

当前 `2f4494` 可以先 nop，保证主 UI 继续跑。  
但最终要恢复 chrome。

### 4.1 当前坏点

```text
303d94 中 r7 = 0x80278
应为真实 mr_table stub，例如 DrawRect 约 mr_table+0x1E8 ≈ 0x2801EC
```

### 4.2 不要只把 r7 硬改 DrawRect

实验已经证明：

```text
r7 改到 DrawRect 后会调用 DrawRect，
但 h 异常；
后续仍卡 310bb4 贴图循环。
```

说明问题不是单个 FP，而是：

```text
graphics ctx / 表基址 / 结构布局 / ABI 整体不对。
```

### 4.3 需要查

```text
1. 303d94 的 r7 从哪个 ctx 字段读出？
2. 这个 ctx 谁初始化？
3. 这个 ctx 是否应该指向 mr_table / extMrTable / graphics table？
4. 0x80278 是如何写进去的？
5. 310bb4 的贴图 ABI 是什么？
6. wy_jiao* / wy_xian* / jiantou* bmp 对象结构是否完整？
```

建议日志：

```text
[JJFB_CTX_WRITE] addr old new pc lr
[JJFB_303D94] ctx=... r7=... args...
[JJFB_310BB4] bmp=... x/y/w/h/clip...
```

### 4.4 恢复策略

```text
先保留 2f4494 nop；
在旁路中调试 ctx；
确认 r7/310bb4 ABI 后再放行；
每次只放一个子调用，不要一次恢复整个 chrome。
```

---

## 5. P2：原生 present / refresh，不要假冒

当前：

```text
DEBUG_PRESENT > 0
DispUpEx = 0
mrc_refreshScreen = 0
```

这不是坏事，说明现在只是 host debug 显示。

下一步要找原生 present gate：

```text
1. 谁应该调用 DispUpEx？
2. 2f2358 / 305c34 后是否有 refresh 分支？
3. 0x11F00 是否只负责写绘制命令，真正 refresh 由另一个 code 触发？
4. 是否还缺某个 event/timer/app state，导致 refresh 不发？
```

允许：

```text
screen_dirty=1 时 DEBUG present 继续保留，作为调试显示。
```

禁止：

```text
把 DEBUG_PRESENT 伪装成 mrc_refreshScreen；
伪造原生 refresh 计数；
为了看图强行 fake DispUpEx。
```

---

## 6. P3：资源路径 / 缺图防护

当前已知：

```text
vmimage.mrp 在 mythroad\gwy\jjfbol\
wy_jiao* / wy_xian* / jiantou* 字符串在 jjfb.mrp / dsm_gm.mrp 内
```

后续要修：

```text
name!w!h.bmp
vmright!…@vmimage.bmp
taskbutton!…
```

建议资源打开层做：

```text
1. 大小写兼容
2. / 与 \ 兼容
3. name!w!h.bmp 解析
4. @vmimage.bmp 指向对应 MRP 包内资源
5. gwy/jjfbol/ 前缀补全
6. open failed 时返回安全空对象，不返回毒指针
```

---

## 7. 当前不要做

```text
❌ 不要回滚 loader / robotol / mrc_init / timer
❌ 不要猜 sdk_key
❌ 不要用 UCRT64
❌ 不要再把 uptime 写入 0x830/0x834
❌ 不要再盲 seed drawFP@1510
❌ 不要大范围 bypass 2f284c
❌ 不要把 DEBUG present 当原生 refresh
❌ 不要为了快速出图 fake mrc_refreshScreen
```

---

## 8. 成功标准

### 最低成功

```text
0x11F00 code 对象结构基本解析；
能根据 glyph/run 数量画出多个占位字块，而不是固定一块。
```

### 中级成功

```text
ASCII / 数字可读；
窗口出现可辨认文本或 UI 标记；
仍保持 DEBUG_PRESENT 和原生 refresh 区分。
```

### 高级成功

```text
原生 DispUpEx / mrc_refreshScreen 出现；
2f4494 chrome 可自然放行；
wy_jiao / wy_xian / jiantou 装饰框正常绘制。
```

### 后续成功

```text
state 自然推进；
module / initNetwork 出现；
再看 20000 / 21002 / 6009。
```

---

## 9. 给 Cursor 的一句话总结

**v32 已确认 present 中层打通：2f4494 是 chrome 绘制且因 graphics ctx 坏 FP 掐断，当前精确 nop 后 305bf4→0x11F00→DEBUG present 已恢复。下一步不要回滚，不要假 refresh；请主攻 0x11F00 code/glyph 对象解析和真实字素绘制，同时旁路修 303d94/310bb4 的 graphics ctx，让 2f4494 最终可自然放行。**
