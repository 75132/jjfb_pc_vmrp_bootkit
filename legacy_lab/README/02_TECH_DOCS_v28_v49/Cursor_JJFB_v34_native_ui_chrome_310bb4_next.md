# Cursor 继续开发说明：v34 — 从 DEBUG present 逼近原生 UI

> 最新包结论：v33 是实质推进。  
> 但用户目标是 **原版 MRP 自己出画面**，所以 DEBUG_PRESENT 只能当诊断，不算最终画面。  
> 当前下一步不是回滚，也不是 host overlay，而是：**放行/修复 2f4494 chrome + 310bb4 贴图 ABI + 0x11F00 code/glyph 真解析 + 原生 refresh gate**。

---

## 1. 最新状态

### 已经稳定

```text
loader / mrc_loader.ext / robotol.ext / mrc_init / timer / app24 / 305bf4 / 0x11F00 已通。
```

### v33 新进展

```text
1. 0x11F00 已经按 code 对象 glyph 数画 8×16 ASCII。
2. code@0x2A845C:
   +0x14 len=20
   +0x18 "CYFmhdNmS1roRgroRa=="
3. shadow mr_table@0x281EFC 的 gfx 槽已局部 heal。
4. +0x1E8 已从 0x80278 修到 0x2801EC，也就是 DrawRect stub。
5. 整表 retarget shadow→host mr_table 已证伪，会打爆 c2u，导致 code=-1。
```

### 仍未完成

```text
1. 2f4494 仍 nop。
2. 0x11F00 仍是 DEBUG present，不是原生 refresh。
3. 原生 DispUpEx / mrc_refreshScreen 仍未出现。
4. 资源路径和 chrome 贴图链仍未自然跑通。
```

注意：日志里出现 `DispUpEx/mrc_refreshScreen untouched` 是 DEBUG 日志文本，不代表原生 refresh 计数 > 0。

---

## 2. 当前不要做

```text
❌ 不要回滚 loader / mrc_loader / robotol。
❌ 不要再猜 sdk_key。
❌ 不要把 DEBUG_PRESENT 说成原生 refresh。
❌ 不要做 host overlay。
❌ 不要整表 retarget shadow→host mr_table。
❌ 不要重新 seed drawFP@1510。
❌ 不要继续长期 nop 2f4494 而不研究 310bb4。
```

---

## 3. 当前最重要判断

v33 已经证明：

```text
shadow 表不能整表替换；
但 shadow 表里的 gfx slots 可以局部 heal。
```

这意味着当前应该走：

```text
局部修 graphics ctx / bitmap ABI / resource path
```

而不是：

```text
大范围替换函数表
```

---

## 4. 下一步优先级

## P0：在保护条件下放行 2f4494，专攻 310bb4

现在 `2f4494` 的致命 FP 已经被 heal：

```text
shadow + 0x1E8 = 0x2801EC
```

所以可以做一个受控实验：

```text
1. 临时取消 2f4494 nop。
2. 保留 shadow gfx heal。
3. 给 303d94 / DrawRect / 310bb4 加详细日志。
4. 一旦 310bb4 进入异常循环或参数明显坏，自动 fail-open：
   - 跳过当前 chrome blit 子调用
   - 不退出模拟器
   - 返回到 2f4494 后续或 epilogue
```

不要一放行就让它卡死。加 guard：

```text
max_chrome_blit_calls_per_tick
max_same_pc_loop
bad_bmp_guard
bad_rect_guard
```

建议日志：

```text
[JJFB_2F4494_ALLOW] tick=... mode=...
[JJFB_303D94] tbl=... fp=... args...
[JJFB_DRAWRECT] x y w h color ret
[JJFB_310BB4] bmp=... x y w h clip src dst stride ...
[JJFB_310BB4_BAD] reason=...
```

目标：

```text
让 2f4494 尽量自然跑；
如果 310bb4 坏，只跳过坏贴图子调用；
不要继续整个 2f4494 nop。
```

---

## P1：解析 310bb4 贴图 ABI

`2f4494` 是 chrome 装饰框绘制，资源包括：

```text
wy_jiao*
wy_xian*
jiantou*
```

`310bb4` 很可能是 bitmap blit / image tile / chrome patch 绘制函数。  
需要 dump 参数和对象：

```text
r0-r7
sp[0..0x40]
bmp object 0x80 bytes
clip/rect object 0x80 bytes
src/dst pointer
w/h/stride/pixel format
```

判断对象是否合法：

```text
bmp ptr 是否在 guest heap / ERW / resource cache
w/h 是否合理
pixel ptr 是否可读
是否 RGB565 / ARGB / indexed
```

如果对象有效，就实现最小 blit 到 screen buffer。  
如果对象无效，就返回失败但不毒化寄存器，不让 UC_ERR_READ_UNMAPPED 继续污染。

---

## P2：资源路径修复

当前 chrome 依赖资源：

```text
wy_jiao*
wy_xian*
jiantou*
vmimage.mrp
taskbutton
vmright!…@vmimage.bmp
name!w!h.bmp
```

资源打开层需要支持：

```text
1. name!w!h.bmp 解析；
2. xxx@vmimage.bmp 解析；
3. gwy/jjfbol/vmimage.mrp 内资源查找；
4. 当前 MRP / dsm_gm.mrp / jjfb.mrp / vmimage.mrp 多包查找；
5. 大小写和斜杠兼容；
6. 失败时返回安全空对象，不返回毒指针。
```

目标不是只防崩，而是让 chrome 资源能真的加载。

---

## P3：0x11F00 code/glyph 真解析

现在 0x11F00 已经能画多字素 ASCII，但它画的是从 code 对象里抓到的：

```text
"CYFmhdNmS1roRgroRa=="
```

这可能是富文本/glyph对象里的编码字段，不一定是最终显示文本。  
下一步要解析：

```text
code object header
len 字段
string/glyph run
颜色/字体/alpha
字符编码
base64-like 字段用途
```

不要只把可打印 ASCII 当原文。  
建议增加：

```text
[JJFB_11F00_CODE] dump once per code ptr
[JJFB_11F00_FIELD] off value guess
```

---

## P4：追原生 refresh gate

当前：

```text
DEBUG_PRESENT > 0
native DispUpEx = 0
native mrc_refreshScreen = 0
```

后续要查：

```text
1. 0x11F00 后是否应该只写绘制命令；
2. 真正 refresh 是否在 2f4494 / 310bb4 / 另一个 app/code 后触发；
3. 是否因为 2f4494 nop 导致原生 refresh gate 没走；
4. 是否有 dirty flag / C44 / 7D8 某字段没有置位。
```

允许保留 DEBUG_PRESENT 作诊断，但必须保持标记：

```text
host debug only; not native refresh
```

---

## 5. 成功标准

### 近期成功

```text
取消 2f4494 nop 后不再卡死；
310bb4 参数结构清楚；
chrome 子调用能 fail-open 或成功 blit；
0x11F00 code 对象字段更清楚。
```

### 中期成功

```text
wy_jiao / wy_xian / jiantou 资源加载成功；
chrome 可自然绘制；
窗口画面不再只是 0x11F00 debug text。
```

### 最终 UI 成功

```text
原生 DispUpEx 或 mrc_refreshScreen 出现；
DEBUG_PRESENT 不再是唯一可见路径；
窗口显示 jjfb/robotol 原生 UI。
```

---

## 6. 给 Cursor 的一句话

**v33 已经验证局部 shadow gfx heal 是正确方向，但 2f4494 仍被 nop，原生 refresh 仍为 0。下一步不要整表 retarget，也不要 fake refresh；请在 guard 保护下放行 2f4494，专攻 310bb4 贴图 ABI 和 wy_jiao/wy_xian/jiantou 资源路径，同时继续解析 0x11F00 code/glyph 对象，目标是让原生 chrome/refresh 链自然出现。**
