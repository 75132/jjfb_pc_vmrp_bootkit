# v46 结论：0x45 是启动检查 UI 状态机 —— 动画卡在 progress 一次循环前

日期：2026-07-17  
目标：让《机甲风暴》**原版启动检查网络/检查更新 UI 与动画跑完**（不跳过 0x45，不假 UI）。

---

## 一句话

**不要离开 0x45；要修的是 0x45 内部启动动画。**  
最小阻塞：**`progress_count` 在首次（也几乎是唯一一次）progress 循环前必须 >0，但 guest 从不写它；检查网络/更新字符串存在但从未被读。**

---

## 1. 0x45 内部阶段（实测）

```text
dispatch 0x306344
  → ui_mode==0x45 → 0x30662C → 0x2EF86C
      → AC8 读 @0x2EF8A0
           AC8>0 → slogo 请求/blit
           AC8=0 → loadingbar / bar / textbar 请求
                → loadingbar blit (x=19,y=220)
                → progress 循环 0x2EFA9E..0x2EFADA（一次性）
                     count>idx → bar DRAW
                     count=0   → 全 SKIP
      → （检查网络/更新文字：本阶段未到达）
```

后续 `SPLASH_ENTER` 仍每 tick 进入，但 **progress 循环不再重跑**（资源已绑定后的短路径）。

---

## 2. AC8 表示什么？

| 值 | 行为 |
|----|------|
| >0 | slogo 分支 |
| 0 | loadingbar/bar/textbar 分支 |

实验：

| 模式 | 结果 |
|------|------|
| natural | 始终 AC8=0 → 只有 loading 路径；**无 guest AC8_WRITE** |
| force_slogo_once | slogo 出现；释放后可进 loading（host 写） |
| pulse hold=5 | tick12 AC8=1 → tick17 释放 0；**同一次运行可见 slogo + loadingbar** |

**结论：** AC8 是 splash 前半/后半 gate（slogo ↔ loading），不是“检查网络完成标志”。  
自然运行下 **无人写 AC8** → 永远停在 loading 半边；slogo→loading 自然衔接缺失。

---

## 3. progress_count 表示什么？

地址：`ERW+0xBA0+0x2C = 0x2B241C`（**不是** loadingbar slot）。

语义（由循环证实）：

```text
for idx in 0..11:
  if progress_count > idx: DRAW bar 段
```

| 探针 | 结果 |
|------|------|
| 循环**之后** timer 写 1/6/12 | **DRAW=0**（循环已过） |
| 首次 `0x2EF86C` **入口**写 count=1 | **DRAW=3**（idx=0 的 bar 段画出） |

```text
[JJFB_PROGRESS_PHASE] splash_enter write 0 -> 1 n=1 (before progress loop)
[JJFB_PROGRESS_DRAW] #1 pc=0x2EFA9E idx=0 count=1 ... name=bar!16!18.bmp
```

**结论：** progress_count = 启动条动画进度（0..12 量级）。  
**必须在首次 progress 循环前由 guest（或合法回调）写入；** 事后 nudge 无效。

---

## 4. slogo → loadingbar 能否自然衔接？

- natural：**否**（无 AC8 写，只有 loading）
- pulse / force_slogo_once：**host 人工**可同次出现两者
- 最终方案仍需找到 **谁该写 AC8**

---

## 5. bar/textbar 为什么不画？

1. 对象已构造（`bar=0x6BFE40` `textbar=0x6C00A8`）
2. progress 循环在 count=0 时全 SKIP
3. 循环基本只跑一次 → 事后写 count 也救不了
4. 入口写 count=1 → **bar 可 DRAW**（动画通路本身可用）

textbar / 状态文字仍未观察到绘制（可能另依赖 check 字符串阶段）。

---

## 6. 检查网络/检查更新字符串

已装 read watch（guest VA）：

| VA | 含义 |
|----|------|
| 0x313B30 | 连接超时，请检查网络 |
| 0x313C48 | 连接失败，请重试 |
| 0x313C5C | 连接中，请稍等 |
| 0x313C74 | 正在下载资源文件… |
| 0x313CF4 | 检查更新列表 |
| 0x314204 | 正在登陆,请稍等 |

运行结果：**仅有 `watching` 安装日志，0 次 `JJFB_STARTUP_STR] #` 命中。**

→ 检查网络/更新 UI **还没进到读这些字符串的阶段**；不是“字符串不存在”。

---

## 7. 哪个事件/返回值能让启动动画推进？

已排除：

- `JJFB_2EC6B0_RET` 变体 → 不推进 progress/AC8（v44）
- 事后 progress nudge → 赶不上一次性循环

已确认有效（仅作探针）：

- **在首次 splash 进入前把 progress_count>0** → bar DRAW

仍未知（下一刀）：

- 谁在正常流程里于 `0x2EFA9E` 之前写 `BA0+0x2C`？
- 谁写 AC8？
- 哪个 timer/`0x10140`/sendAppEvent 本应驱动上述写入？
- 什么条件触发读 `检查更新列表` 等字符串？

---

## 8. 当前最小阻塞点

```text
P0-a: guest 从不写 progress_count，且循环只在资源首绑时跑一遍
      → bar 动画永远 SKIP（除非入口探针）
P0-b: guest 从不写 AC8
      → slogo 与 loading 不能自然衔接
P0-c: 检查网络/更新字符串从未被读
      → 启动检查文案阶段未到达（卡在 progress/前序）
```

**不是** chrome/310BB4/坐标主问题；loadingbar 位置与 colorkey 已可用。

---

## 运行

```powershell
.\RUN_V46_STARTUP_CHECK_UI.ps1 -Ac8Mode natural -Mode 45 -Tag A_natural
.\RUN_V46_STARTUP_CHECK_UI.ps1 -Ac8Mode pulse -Ac8PulseTicks 5 -Mode 45 -Tag B_pulse5
.\RUN_V46_STARTUP_CHECK_UI.ps1 -Ac8Mode natural -ProgressScan 1 -Mode 45 -Tag C_prog_scan
.\RUN_V46_STARTUP_EVENT_MATRIX.ps1
```

日志：`logs/v46_*_stdout.txt`

---

## 下一步（仍服务启动动画，不跳 0x45）

1. 静态/动态找 **STR 到 0x2B241C** 的 guest PC（progress writer）
2. 找 **STR 到 0x2B2318**（AC8 writer）
3. 在 writer 上游挂事件/timer：缺哪个回调导致不写
4. progress 动画跑起来后，再盯 `JJFB_STARTUP_STR` 是否开始命中
