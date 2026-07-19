# Cursor 继续开发说明：v41 — 修正 240×320 下的布局轴映射，避免 loadingbar 横向出界

> v40 已经找到真正贴图入口 `0x2EC6B0`，并且 loadingbar 能按 guest 公式进入 blit。  
> 但用户实际看到 UI 位置不对，日志也证明位置确实不对：`x=59,w=201` 在 240 宽画布上右侧出界，只能显示 181 像素。  
> v41 的第一优先级不是继续追网络，也不是继续修 slogo，而是先把 **240×320 下的 layout width / layout height 映射修正**。

---

## 1. v40 的明确问题

v40 日志：

```text
[JJFB_2EC6B0] obj=0x6BD090 x=59 y=140 sp4=0xC9(201) sp8=0x1D(29)
[JJFB_2EC6B0_BLIT] loadingbar!201!29.bmp x=59 y=140 w=201 h=29
[JJFB_DEBUG_PRESENT] dirty 59,140 181x29
```

问题：

```text
240 宽屏幕下，x=59,w=201：
59 + 201 = 260 > 240
所以右侧被裁掉 20 像素，只剩 181。
```

这不是原版布局应该有的结果。

---

## 2. 根因判断：2f9968 / 2f995c 的语义大概率被反了

v40 反汇编公式：

```text
x = (2f9968() - bmp.w) / 2
y = 2f995c() - 100
```

当前实现/日志等价于：

```text
2f9968() = 320
2f995c() = 240
```

所以算出：

```text
x = (320 - 201) / 2 = 59
y = 240 - 100 = 140
```

但在 240×320 竖屏中，横向居中应该用 width=240，纵向位置应该用 height=320：

```text
x = (240 - 201) / 2 = 19
y = 320 - 100 = 220
```

这与 v37 eager 的大致位置 `(19,200)` 接近，也不会横向裁剪。

所以 v41 先验证：

```text
2f9968 应作为 layoutWidth 返回 240
2f995c 应作为 layoutHeight 返回 320
```

不要再把 2f9968 简单理解成 screen height。

---

## 3. 不要直接手改 loadingbar 坐标

禁止：

```text
❌ x=19,y=220 直接硬塞
❌ 对 loadingbar 做单独特判
❌ 拉伸资源到 240×320
```

应该做：

```text
让 guest 原公式自己算出正确坐标。
```

也就是修：

```text
2f9968 / 2f995c 的返回值或其底层字段映射。
```

---

## 4. v41 A/B 测试

新增环境变量：

```text
JJFB_DIM_MAP=old
JJFB_DIM_MAP=swap
```

### old：当前 v40 行为

```text
2f9968() = 320
2f995c() = 240
预期 loadingbar x=59 y=140 clip=181x29
```

### swap：v41 候选行为

```text
2f9968() = 240
2f995c() = 320
预期 loadingbar x=19 y=220 clip=201x29
```

日志必须打印：

```text
[JJFB_DIM] mode=swap 2f9968=240 2f995c=320 screen=240x320
[JJFB_2EFA92_FORMULA] bmp=201x29 width_for_x=240 height_for_y=320 -> x=19 y=220
[JJFB_2EC6B0_BLIT] name=loadingbar x=19 y=220 w=201 h=29 clip=201x29
```

---

## 5. 需要保留的固定规则

逻辑画布仍然是：

```text
screen_w = 240
screen_h = 320
format = RGB565
bytes = 0x25800
```

资源尺寸仍然保持原始：

```text
loadingbar = 201×29
bar = 16×18
textbar = 120×30
slogo = 157×58
```

只是修横纵轴映射，不是改分辨率。

---

## 6. 注意已有字段可能不要一次全改

之前曾记录：

```text
0x824 = 240
0x828 = 0
0x830 = 320
0x834 = 240
```

不要一上来全局交换所有字段，因为其他路径可能依赖旧字段。

优先做低风险改法：

```text
只在 2f9968 / 2f995c 的 host hook 或返回适配层中交换返回值。
```

如果它们不是 host hook，而是 guest 函数读字段，那么加日志确认：

```text
2f9968 读哪个字段？
2f995c 读哪个字段？
```

再决定是否改字段。

---

## 7. v41 运行建议

### Run A：复现旧位置

```powershell
$env:JJFB_DIM_MAP="old"
.\RUN_V41_DIMMAP.ps1 -Mode 45
```

### Run B：交换布局轴

```powershell
$env:JJFB_DIM_MAP="swap"
.\RUN_V41_DIMMAP.ps1 -Mode 45
```

对比：

```text
loadingbar x/y
clip 宽高
画面是否更接近原版竖屏 loading 位置
是否仍进入 SPLASH_ENTER
是否仍请求 loadingbar/bar/textbar
是否影响后续 bar/textbar/progress
```

---

## 8. 之后再推进的内容

等坐标轴修正后，再继续 v40/v41 后半目标：

```text
1. 查谁应该写 ERW+0x150C
2. 放行 splash 前半计数器，让 0x2EF9DA 的 #0x1c / slogo 候选路径跑起来
3. 让 progress 循环自然画 bar/textbar
4. 继续区分 DEBUG_PRESENT 和原生 refresh
```

但顺序上，先修坐标轴。否则后续所有资源位置都会偏。

---

## 9. 成功标准

### 最低成功

```text
loadingbar 不再横向出界；
x≈19，clip=201x29。
```

### 中级成功

```text
loadingbar / bar / textbar 的位置均更符合 240×320 竖屏逻辑。
```

### 高级成功

```text
后续 slogo / progress 资源也按同一坐标体系自然显示，不需要单独修坐标。
```

---

## 10. 给 Cursor 的一句话

**v40 找到了真正贴图入口，但 loadingbar 位置不对不是资源问题，而是 240×320 下布局轴映射错了：当前用 2f9968=320 算横向居中，导致 x=59,w=201 在 240 宽屏上被裁。v41 请先做 `JJFB_DIM_MAP=old/swap` A/B 测试，让 2f9968 作为 layoutWidth=240、2f995c 作为 layoutHeight=320，目标是 guest 原公式自然算出 loadingbar x=19,y≈220，clip=201×29。不要硬塞坐标，不要改资源尺寸。**
