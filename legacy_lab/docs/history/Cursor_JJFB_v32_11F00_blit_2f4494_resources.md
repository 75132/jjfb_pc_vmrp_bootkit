# Cursor 继续开发说明：v32 — 0x11F00 真 blit / 2f4494 阻塞 / 资源野读

> **完整交接包（给 GPT）：** `HANDOFF_LATEST_FOR_GPT.md` / `00_HANDOFF_LATEST.md`  
> 当前最新状态：v32 present 中层已通（`0x11F00` 写屏 + DEBUG present；`2f4494` 已定性并 nop）。  
> 现在不是 loader、robotol、timer、registerAPP、12340 的问题了；当前主线是：**加深 0x11F00 真字素、修 chrome FP、等原生 DispUpEx**。

---

## 1. 当前已确认事实

### 1.1 0x12340 已基本定性

```text
0x12340 是逐字侧效应/文本度量接口
调用链：305e70 -> 304550 -> 0x12340
返回值被忽略
关键动作应是写 *param1，例如测高/度量/字符状态
```

所以不要继续把主精力放在 `0x12340` 返回值上。

---

### 1.2 0x830 / 0x834 已确认不是 clock

```text
0x830 = 屏高，来自 2f9968
0x834 = 屏宽，来自 2f995c
```

已停止用 uptime 污染，这是正确的。

当前屏参 seed：

```text
824w = 240
828y = 0
830h = 320
834w = 240
```

这个方向有效。

---

### 1.3 已经打到 present 中层

绕过 `2f284c` 后：

```text
305bf4
  -> 305c34
  -> 2f2358
  -> 0x11F00
```

当前持续可见：

```text
JJFB_11F00 ≈ 19 次
0x2f2358 ≈ 172 次，每 tick 在跑
```

这说明 UI/绘制链确实在运行，不是白屏空转。

---

## 2. 当前真正卡点

### 卡点 A：0x11F00 还只是 log，没有执行真实 blit

现在 `0x11F00` 应该作为真实图形接口处理，而不是 unhandled / log-only。

重点：

```text
rect @ p0 已有 dump
0x2f2358 每 tick 调用
说明 0x11F00 很可能是 drawText / blit / fillRect / glyphDraw / regionDraw 之类接口
```

下一步应优先完整 RE 0x11F00 的参数结构。

---

### 卡点 B：drawFP@1510 seed ABI 错

当前 seed 到 helper `drawBitmap` 后出现：

```text
bmp=0
240x0
```

说明 ABI 不匹配，继续硬接会制造假象。

建议：

```text
先禁用/移除 drawFP@1510 的错误 seed；
只记录自然写入点；
除非能确认函数签名，否则不要把它硬接到 drawBitmap。
```

---

### 卡点 C：2f4494 不返回 — 已定性（v32）

真实阻塞链：

```text
2e87ac -> 2f284c -> 2ea180(mode=0xa) -> 2f4494 不返回
```

**结论：不是等待事件/锁，是装饰框绘制函数被掐断。**

```text
2f4494 = UI chrome/frame（wy_jiao*/wy_xian*/jiantou* bmp via 2d92dc）
mode 0xa 路径：
  懒加载 bmp 对象（flag @ ERW+0xA64）
  -> 303d94 -> blx r7
  -> 310bb4 贴图循环
```

根因：

```text
303d94 的 r7 = 0x80278（落在 CODE_ADDRESS 区，非真实 mr_table stub）
blx 后 emu 停在 stopAddr，表现为“不返回”
bmp 对象本身可以加载成功（r4 有效，obj+0xa=6）
即便把 r7 重定向到 mr_table.DrawRect@+0x1E8，后面 310bb4 循环仍卡住
```

当前处理（比永久 bypass 2f284c 更精确）：

```text
nop/early-return @ 0x2F4494（跳过 chrome）
-> 2e885a -> 305bf4 -> 0x11F00 正常跑
保留 303d94 FP 修复探针，供以后恢复框线绘制
```

---

### 卡点 D：缺资源导致野读

当前缺图路径：

```text
vmright!…@vmimage.bmp
taskbutton!…
```

失败后：

```text
r4 = 0xE0D3A3CA
UC_ERR_READ_UNMAPPED @0x83254
```

已经改为不 exit，让 tick 能继续，这是正确的。  
但还需要做资源路径修复或空指针防护，否则会持续污染绘制链。

---

## 3. 下一步优先级

## 优先级 1：完整实现 / 模拟 0x11F00

不要先管 SDL present。  
先把 `0x11F00` 的参数搞清楚。

每次 `0x11F00` 记录：

```text
extCode
app
code
p0
p1
p2
p3
param1_on_stack
LR
SP
return
```

对所有疑似指针 dump：

```text
p0 指向结构 0x80 bytes
p1 指向结构 0x80 bytes
p2 指向结构 0x80 bytes
p3 指向结构 0x80 bytes
SP[0..0x40]
```

重点解析 `p0 rect`：

```text
x
y
w
h
color
alpha
src/dst pointer
stride/pitch
font/glyph ptr
bitmap ptr
```

判断 0x11F00 类型：

```text
如果 p0 像 rect + color：可能 fillRect / drawRect
如果 p0 像 rect + bitmap：可能 blit
如果 p0 像 glyph/string：可能 drawText/glyphDraw
如果 p0 有 src/dst/pitch：可能 region copy
```

---

## 优先级 2：给 0x11F00 做最小真实副作用

不要直接假 refresh。  
应该让 0x11F00 至少写 screen buffer 或调用正确图形 helper。

可选策略：

### 策略 A：如果 0x11F00 是 fill/blit

实现：

```text
把 rect 区域写入 w_getScreenBuffer 对应区域
支持 RGB565
clip 到 240x320
记录 JJFB_DRAW
```

### 策略 B：如果 0x11F00 是 drawText/glyph

实现：

```text
先用简单 8x16 ASCII bitmap 或占位方块写 screen buffer
中文/未知 glyph 先画 8x16 block
clip 到 240x320
记录 JJFB_DRAW_TEXT
```

### 策略 C：如果 0x11F00 是测量/布局

则不要写屏。  
要返回合理 width/height 或写出 param struct，避免布局分支卡死。

---

## 优先级 3：present 链不要 fake，但可以自然触发

当 0x11F00 已产生真实 screen buffer 写入后，再看：

```text
是否调用 mrc_refreshScreen / DispUpEx
是否调用 SDL_UpdateTexture / SDL_RenderPresent
```

如果一直没有 refresh，但 screen buffer 已经改变，可以临时加 debug present：

```text
仅在 screen_dirty=1 且 tick 结束时 present
日志标记为 DEBUG_PRESENT，不要伪装成原生 mrc_refreshScreen
```

这样能区分：

```text
图形内容有了但原生 refresh 没来
vs
图形内容本身还没写入
```

---

## 优先级 4：查 2f4494 不返回

对 `2f4494` 做入口/出口 trace：

```text
[JJFB_2F4494] enter pc lr sp r0-r7
[JJFB_2F4494] loop pc=... counter=...
[JJFB_2F4494] call target=...
[JJFB_2F4494] exit ret=...
```

重点看它是否在等待：

```text
C44
7D8
screen param 824/828/830/834
drawFP@1510
resource pointer
timer flag
event queue
```

如果是等待状态位，找写入者；  
如果是等待回调，补回调；  
如果是死循环，记录循环体内读写地址。

---

## 优先级 5：修资源缺失 / 空指针防护

当前缺图路径：

```text
vmright!…@vmimage.bmp
taskbutton!…
```

建议两条线并行：

### 路线 A：资源路径映射

对 `mr_open` / 资源 open 失败做路径归一化：

```text
大小写兼容
斜杠兼容
! / @ 特殊路径解析
MRP 内资源表查找
gwy/jjfbol/ 前缀补全
```

重点记录：

```text
open failed path
是否在 dsm_gm.mrp 内资源表存在相似项
是否在 robotol.ext 资源区存在
```

### 路线 B：空对象防护

如果 bitmap/resource load 失败，不要让野指针继续进入绘图：

```text
返回 NULL / 0
设置 error code
调用方看到失败后 skip draw
不要把 r4 保持成 0xE0D3A3CA 这种毒值
```

---

## 4. drawFP@1510 处理建议

当前结论：

```text
drawFP@1510 不是可以随便接 drawBitmap 的普通函数指针。
```

下一步：

```text
1. 禁用错误 seed。
2. trace 谁写 1510。
3. trace 谁读/调用 1510。
4. 记录调用参数。
5. 再决定它是 drawBitmap、drawText、refresh、还是 region blit。
```

日志：

```text
[JJFB_1510_WRITE] old new pc lr
[JJFB_1510_CALL] target r0-r7 sp dump
```

---

## 5. 成功标准

最低成功：

```text
0x11F00 参数结构基本解析清楚；
能判断它是画字、blit、fill、还是测量。
```

中级成功：

```text
0x11F00 产生真实 screen buffer 写入；
JJFB_DRAW > 0；
screen_dirty=1。
```

高级成功：

```text
mrc_refreshScreen / DispUpEx / SDL present 出现；
窗口不再纯白。
```

额外成功：

```text
2f4494 不再需要永久绕过；
缺图野读不再触发 UC_ERR_READ_UNMAPPED；
module/network 继续推进。
```

---

## 6. 给 Cursor 的一句话总结

**v31 已经推进到 present 中层：0x12340 定性为逐字/度量副作用接口，返回值不关键；绕过 2f284c 后已打到 2f2358 -> 0x11F00。当前主线应转为完整 RE 并实现 0x11F00 的图形副作用，禁用错误 drawFP@1510 seed，查清 2f4494 不返回原因，并处理缺图导致的 0x83254 野读。不要回滚 loader，不要 fake refresh。**
