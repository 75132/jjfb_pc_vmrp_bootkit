# JJFB v39 结论：对象有读者；父结构已找到；slogo 不在 0x45

生成时间：2026-07-17

## 一句话

```text
loadingbar/bar/textbar 对象构造后会被挂入父表 @0x2B2410 一带，
并在 0x2EFA58 / 0x2EFA6A / 0x2EC6E2 被读取做布局运算。
不是“完全无人读”，而是“读了但还没走到我们已 hook 的 blit”。
slogo 字符串在 ui_mode=0x45 仍从未被读；0x44 进不了 SPLASH_ENTER。
```

## 最低成功：已达成

### 1) 对象绑定（再次确认）

```text
[JJFB_OBJ_BIND] loadingbar pixels -> obj≈0x6BD090  pc=0x2D958E
[JJFB_OBJ_BIND] bar        pixels -> obj≈0x6BFE40
[JJFB_OBJ_BIND] textbar    pixels -> obj≈0x6C00A8
```

`0x2D958C` 一带：`r4=obj`，`r5=pixels`。

### 2) 父结构 / splash resource table

对象指针被写入：

```text
bar      obj -> slot@0x2B2410   pc=0x2EFA44
textbar  obj -> slot@0x2B2414   pc=0x2EFA52
loadingbar obj -> slot@0x2B2418 pc=0x2EFA34
```

附近常见基址：

```text
r6/r7 ≈ 0x2B2028 / 0x2B23F0
```

这就是 splash 资源挂载表（至少三个 slot 连续）。

### 3) 后续读者（不是零）

```text
[JJFB_BMP_OBJ_READ] which=parent_slot  @0x2EFA58 / 0x2EFA6A
[JJFB_BMP_OBJ_READ] which=obj / obj+4  @0x2EFA74 / 0x2EC6E2
```

在 `0x2EC6E2`：

```text
r0=0xC9 (201=w)  r1=0x1D (29=h)  r4/r7=loadingbar obj
r5=0x3B (59)     r6=0x8C (140)
```

说明：**已经在用真实 w/h 做布局**，坐标候选接近 `y≈140` / 相关量 `59`，不是 EAGER 的 (19,200)。

### 4) 仍未见到的

```text
仍无 loadingbar 的 310BB4_BLIT / drawBitmap / 30680C_BLIT
```

所以下一跳应是：**从 0x2EC6E2 继续追真正的 blit 调用**（可能是另一条绘制函数）。

## slogo 门控

| ui_mode | SPLASH_ENTER | loadingbar 字符串读 | slogo 字符串读 |
|---------|--------------|---------------------|----------------|
| 0x45    | 有           | 有（strlen 路径）   | **0**          |
| 0x44    | **无**       | 无                  | 0              |

结论：

```text
0x45 = loadingbar/bar/textbar 路径
slogo 需要别的 gate/mode/flag，不是简单改成 0x44
不要手动画 slogo
```

## 已锁定（勿回退）

```text
0x10134 返回 pixel ptr
240×320 固定
EAGER 不是最终布局
```

## 跑法

```powershell
.\RUN_V39_OBJ_CONSUMERS.ps1 -Mode 45
.\RUN_V39_OBJ_CONSUMERS.ps1 -Mode 45 -Flag 10   # obj+0x10=1 实验
.\RUN_V39_OBJ_CONSUMERS.ps1 -Mode 44            # 对照：无 splash
```

`JJFB_FORCE_SPLASH_NUDGE` 按 **十六进制** 解析（`45` => `0x45`）。

## 下一步（v40）

```text
1. 以 0x2EC6E2 / 0x2EFA74 为中心反汇编，找到真正 blit/draw 调用
2. dump 父表 0x2B2410 结构字段（x/y/flags）
3. 用读到的真坐标做 host blit 对照（仍标 DEBUG_PRESENT）
4. 继续找 slogo xref（可能在别的 ui_mode 或首次进入前的一次性分支）
```
