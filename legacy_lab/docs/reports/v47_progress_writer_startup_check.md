# v47 结论：progress/AC8 writer 未执行；progress=12 仍进不了检查更新

## 一句话

**图形链 OK；自然 writer 缺失；linear progress driver 能把 bar 画到 12，但检查更新字符串仍 0 次读取 → progress 不是唯一 gate。**

---

## 1. 0x10140 / timer 闭环

```text
[JJFB_10140_REG] handler=0x306305 lr=0x304581 app=0x5 code=0x2829DC
```

| 项 | 值 |
|----|-----|
| 注册 handler | **0x306305**（紧挨 dispatch `0x306344`） |
| timer 每 tick | 调用该 handler（r0=0） |
| before→after | **delta_prog=0 / delta_ac8=0**（无 driver 时） |
| 实际效果 | 进 `0x306344` → `0x45` 时 `0x2EF86C`；**不写 progress/AC8** |

Timer 活着，但只是反复进 splash 显示路径，**不是 progress 驱动器**。

---

## 2. Writer 追踪

### 已执行

```text
pc=0x2EFA34 → ERW+0xBC8 loadingbar_obj
pc=0x2EFA44 → ERW+0xBC0 bar_obj
pc=0x2EFA52 → ERW+0xBC4 textbar_obj
```

### 从未执行

```text
ERW+0xBCC progress_count  — 0 guest writes
ERW+0xAC8  AC8            — 0 guest writes
ERW+0x8D0  ui_mode        — 仅 host FORCE
```

### 静态线索

```text
imm_BA0 @ 0x2EFC58
imm_AC8 @ 0x2EFC6C
```

在 splash 函数尾部附近；**能读写 BA0/AC8 的代码存在，但 progress 的 STR 路径没走到**。

详见 `v47_state_xref.md`。

---

## 3. Progress driver 探针（非最终方案）

```text
JJFB_PROGRESS_DRIVER=linear
```

| 指标 | 结果 |
|------|------|
| count 0→12 | 是 |
| bar PROGRESS_DRAW | 是（首段 guest 循环 + 后续 driver_probe blit） |
| STARTUP_STR 命中 | **0** |
| AC8 变化 | 否 |
| ui_mode 离开 0x45 | 否 |

**判定：progress writer 是必要缺口，但不是充分条件。**  
即使 progress 跑满，检查网络/更新 UI 仍不出现 → 还有下一道 gate。

---

## 4. 检查更新字符串

存在于 guest 镜像；runtime watch 与字面量扫描均显示 **未进入引用阶段**。  
见 `v47_startup_string_xref.md`。

---

## 5. 下一步（v48 方向）

1. 反汇编 `0x2EFC40..0x2EFD20`：谁 STR `BA0+0x2C` / `AC8`，前置条件是什么。  
2. 找 textbar / 检查更新绘制函数的 branch 条件（不只 progress）。  
3. 查未到达的 sendAppEvent / 网络/更新回调。  
4. 保留 linear driver 仅作动画验收；**最终必须自然 writer**。

---

## 运行

```powershell
.\RUN_V47_STATE_WRITER_TRACE.ps1
.\RUN_V47_STARTUP_PROGRESS_DRIVER.ps1 -Driver linear
.\RUN_V47_STARTUP_PROGRESS_DRIVER.ps1 -Driver linear -Ac8Mode pulse -Ac8PulseTicks 3
```
