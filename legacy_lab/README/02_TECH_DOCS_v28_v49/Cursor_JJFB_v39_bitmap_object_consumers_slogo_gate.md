# Cursor 继续开发说明：v39 — 追 bitmap object 消费者与 slogo 分支条件

> v38 已达到最低成功标准：`0x10134` 返回语义和 bitmap object 绑定已经确定。  
> 下一步不要再纠结 10134 返回 object/pixel，也不要继续用 eager 坐标当成果。  
> v39 主线：**从 bitmap object 的后续读者反追真实绘制调用，查 slogo 分支条件。**

---

## 1. 当前已定，不再反复验证

### 1.1 `0x10134` 返回语义

结论：

```text
0x10134 必须返回 pixel ptr。
mode A pixels 正确。
mode B object 会破坏后续链。
```

不要再改成 object 返回。

---

### 1.2 guest 真实 bitmap object 布局

已经在 `pc=0x2D958C` 抓到对象绑定：

```text
object+0x00 = W*H*2
object+0x04 = pixels ptr    ← 0x10134 返回值写到这里
object+0x08 = packed w/h
object+0x18 = size echo
object+0x1C = 可能 inline/pixel/cache 区
```

示例：

```text
loadingbar pixels=0x6BD0AC -> mem@0x6BD094  obj≈0x6BD090
bar        pixels=0x6BFE5C -> mem@0x6BFE44  obj≈0x6BFE40
textbar    pixels=0x6C00C4 -> mem@0x6C00AC  obj≈0x6C00A8
```

`object+0x08` 已验证能对应：

```text
loadingbar = 201×29
bar        = 16×18
textbar    = 120×30
```

---

### 1.3 当前为什么还不是原版布局

虽然对象造好了，但没有自然消费：

```text
无 loadingbar/textbar 的 310BB4_BLIT
无 splash 的 drawBitmap
无 splash guest_pixels MEM_READ
无 JJFB_30680C 实际执行
```

所以：

```text
对象已构造，但 splash 绘制消费链没走到。
EAGER_BLIT 仍只是临时显示，不是原版布局。
```

---

### 1.4 slogo 状态

```text
slogo!157!58.bmp 字符串 VA = 0x314EF4
缓存 slogo!157!58.bmp.rgb565 存在
ui_mode=0x45 下 MEM_READ watch = 0 次
```

结论：

```text
不是缺资源；
是 0x45 splash 路径没有进入 slogo 分支。
```

不要手动画 slogo 冒充原版路径。

---

## 2. v39 总目标

从“资源对象已构造”推进到：

```text
资源对象被真实 UI 代码消费
真实 x/y 来源明确
loadingbar/bar/textbar 自然 blit 到 240×320
slogo 分支条件明确
```

---

## 3. P0：从 bitmap object 追读者

这是 v39 最高优先级。

对以下地址建立读 watch / 访问日志：

```text
loadingbar object 地址，例如 0x6BD090
loadingbar object+0x04，即 pixels 字段
loadingbar pixels，例如 0x6BD0AC

bar object
bar object+0x04
bar pixels

textbar object
textbar object+0x04
textbar pixels
```

不要只 watch pixels，因为如果 UI 先读 object 字段，再决定坐标，watch pixels 可能抓不到。

日志格式：

```text
[JJFB_BMP_OBJ_READ]
name=loadingbar
addr=0x...
which=obj|obj+4|pixels
pc=0x...
lr=0x...
r0-r7
sp
nearby disasm/callsite
```

如果 30 秒内没有读者，说明对象被构造后没有挂入 UI 列表或后续状态没推进。

---

## 4. P1：重点看 `0x2D958C` 之后的数据流

`0x2D958C` 是当前最关键的绑定点。

要加 trace：

```text
[JJFB_OBJ_BIND] enter/exit
pc=0x2D958C
lr=0x304601
obj
pixels
w/h
return
```

然后继续追：

```text
绑定后的 obj 被写入哪个父结构？
父结构地址是多少？
父结构后续是否被读？
父结构是否进入某个 list/table/array？
```

建议对 `obj` 周边和“写入 obj 指针的位置”都加 watch：

```text
object ptr 本身
mem@0x6BD094 周边
存放 object ptr 的上级 slot
```

目标是找出：

```text
splash resource list / UI element list / image widget object
```

而不是继续猜 310BB4。

---

## 5. P2：追 `0x2EF86C` 后半段

`0x2EF86C` 是 splash 入口。现在它能请求 loadingbar/bar/textbar，但后续没有画。

需要把 `0x2EF86C` 后半段切出来：

```text
进入 0x2EF86C
请求 loadingbar/bar/textbar
0x2D958C 绑定 object
之后每个 branch / call
函数返回点
```

日志不要太大，建议只打关键分支：

```text
[JJFB_SPLASH_FLOW] pc=... call=... lr=... state=... gate=...
[JJFB_SPLASH_RET] func=... ret=... r0=...
```

重点判断：

```text
A. 0x2EF86C 请求完资源后是否提前 return？
B. 是否等待异步资源加载完成 flag？
C. 是否需要下一 tick 再绘制？
D. 是否因为 0x10134 返回/状态码导致认为资源未加载？
E. 是否进入了另一个绘制函数，但我们没 hook 到？
```

---

## 6. P3：如果没有读者，就查“资源 loaded flag”

当前 `0x10134` 返回 pixels 能构造对象，但 guest 可能还要求某个 loaded flag。

检查 object 周边字段：

```text
+0x0C
+0x10
+0x14
+0x18
+0x1C
```

在 object bind 后做 A/B 最小实验：

### 实验 A：只保持现状

```text
返回 pixel ptr
不改 object 其他字段
```

### 实验 B：设置可能 loaded flag

只在对象明确属于 loadingbar/bar/textbar 时尝试：

```text
object+0x0C = 1
或 object+0x10 = 1
或 object+0x14 = 1
```

每次只试一个字段，观察：

```text
是否出现 object read
是否出现 310BB4 / 30680C / drawBitmap
是否出现新的 splash 分支
```

不要一次乱填多个字段。

---

## 7. P4：slogo 分支条件

`slogo` 不被读，说明它在另一个条件分支。

### 静态任务

围绕字符串地址：

```text
slogo string VA = 0x314EF4
loadingbar string VA ≈ 0x314314
```

做 xref：

```text
谁引用 loadingbar？
谁引用 slogo？
两个引用函数是否相邻？
slogo 前面的 branch 条件是什么？
```

把结果整理成：

```text
resource name -> string addr -> xref pc/lr -> required state/gate
```

### 动态任务

扩大动态 watch：

```text
watch slogo 字符串本体
watch slogo 字符串地址被加载的 literal pool
watch 相关 xref 函数入口
```

如果 slogo xref 函数从不进，说明 ui_mode/gate 错。

需要测试 ui_mode：

```text
0x45 当前能进 loadingbar/textbar
尝试相邻或候选 mode：0x40, 0x41, 0x42, 0x44, 0x45, 0x46
```

每次记录：

```text
是否 SPLASH_ENTER
是否 loadingbar
是否 slogo read
是否 textbar
是否 state 崩坏
```

---

## 8. P5：继续固定 240×320

所有 blit / clip 都按：

```text
W=240
H=320
RGB565
```

不要再动态改分辨率。  
如果坐标超出，先 clip，不要改屏幕尺寸。

---

## 9. 禁止事项

```text
❌ 不要把 0x10134 改回 fail-open
❌ 不要返回 object ptr
❌ 不要手动画 slogo
❌ 不要把 eager 坐标当最终坐标
❌ 不要继续美化 0x11F00 token
❌ 不要改分辨率
❌ 不要把 DEBUG_PRESENT 说成原生 refresh
```

---

## 10. v39 成功标准

### 最低成功

```text
找到 loadingbar/bar/textbar object 的后续读者；
或者证明对象构造后完全无人读取。
```

### 中级成功

```text
找到对象挂入的父结构/list/table；
确认真实 x/y 来源。
```

### 高级成功

```text
loadingbar/bar/textbar 自然触发 310BB4/30680C/drawBitmap 或同类 blit。
```

### slogo 成功

```text
确认 slogo 的 xref 分支条件；
或找到能触发 slogo 请求的 ui_mode/gate。
```

---

## 11. 给 Cursor 的一句话

**v38 已确认 0x10134 必须返回 pixel ptr，guest 在 0x2D958C 将 pixels 写入 bitmap object+4，object+8 的 w/h 正确。但对象构造后没有进入绘制消费链，slogo 在 ui_mode=0x45 下也从未被读。v39 请停止改 10134 返回语义，转为 watch object/object+4/pixels 的后续读者，追 0x2D958C 之后对象挂入哪个父结构，以及 0x2EF86C 后半段为什么不绘制；并静态+动态定位 slogo 的分支条件。**
