# v44 结论：卡在 forced splash，不是 UI 坐标问题

日期：2026-07-17  
目标：跑通《机甲风暴》启动状态机（离开 splash → login/server/network），不是修画面。

---

## 一句话结论

**当前不是“loadingbar 画偏了”，而是上游状态机从未自然推进：guest 从不写 `ui_mode` / AC8 / progress；强推 `0x45` 只是让 `0x2EF86C` 每 tick 空转。**  
`JJFB_2EC6B0_RET=0|1|obj|pixels` 对状态推进**无影响**。

---

## 1. ui_mode / state 存储地址

| 名称 | 偏移 | 实测地址 (ERW=0x2B1850) |
|------|------|-------------------------|
| state / ui_mode | ERW+0x8D0 | **0x2B2120** |
| AC8 (slogo gate) | ERW+0xAC8 | **0x2B2318** |
| progress count | ERW+0xBA0+0x2C | **0x2B241C**（不是 loadingbar slot） |
| loadingbar obj | ERW+0xBA0+0x28 | 0x2B2418 |
| bar obj | ERW+0xBA0+0x20 | 0x2B2410 |

Watch 已安装：

```text
[JJFB_UIMODE] watching state/ui_mode @0x2B2120
[JJFB_AC8] watching ERW+0xAC8 @0x2B2318
[JJFB_PROGRESS] watching count @0x2B241C (BA0+0x2C; NOT loadingbar slot)
```

---

## 2. 谁写 ui_mode=0x45？自然值是什么？

### Run A：`JJFB_FORCE_SPLASH_NUDGE=0`（不强推）

```text
[JJFB_FIRST_SCREEN] NO FORCE ui_mode (natural) state=0x0 tick=10
[JJFB_UI_DISPATCH] pc=0x306344 ... ui_mode=0x0 ... target=dispatch_head
```

- **SPLASH_ENTER = 0**
- **UIMODE_WRITE = 0**（guest 从不写）
- dispatch 只进 `0x306344`，**从不**进 `0x30662C` / `0x2EF86C`
- 自然停在 **ui_mode=0**

### Run B：强推 `0x45`

```text
[JJFB_FIRST_SCREEN] FORCE state/ui_mode 0x0 -> 0x45
```

- **唯一**把 ui_mode 写成 `0x45` 的是 **host FORCE**（`uc_mem_write`，不触发 write hook）
- 之后全程 **UIMODE_WRITE=0** → guest **从不离开 0x45**

---

## 3. 调用链：`0x306344` / `0x306631` / `0x2EF86C` / `0x304619`

```text
timer/handler
  → 0x306344  (dispatch head: 读 ui_mode)
      → cmp #0x45
      → 0x30662C  bl 0x2EF86C   (lr 写入后为 0x306631)
          → 0x2EF86C  splash 体
              → AC8 读 @0x2EF8A0，lr=0x304619（splash 内部回边）
              → loadingbar/bar/textbar 加载
              → progress 环：count=0 → 全 SKIP
```

实测：

```text
[JJFB_UI_DISPATCH] pc=0x30662C lr=0x306337 ui_mode=0x45 target=0x2EF86C
[JJFB_UI_DISPATCH] ui_mode=0x45 target=0x2EF86C caller=0x306631
[JJFB_AC8_READ] value=0 pc=0x2EF8A0 lr=0x304619
```

`0x304619` 不是“外部事件进 splash”的调用者，而是 **splash 函数内部**读完 AC8 后的返回地址。  
外层真正反复进 splash 的是 **`0x306631`（来自 `0x30662C`）**。

---

## 4. AC8：有无自然写入？

| 指标 | noforce | force45 + 任意 RET |
|------|---------|-------------------|
| AC8_WRITE | 0 | **0** |
| AC8_READ | 0 | 有（始终 value=0） |

读点：`pc=0x2EF8A0`，`value=0` → 走 loadingbar 分支（不是 slogo）。

**没有任何 guest 写 AC8。**  
`force_slogo_once` 仍是 host 人工写，不是原版流程。

---

## 5. BA0+0x2C progress：有无自然写入？

| 指标 | 全部 runs |
|------|-----------|
| PROGRESS_COUNT_WRITE | **0** |
| progress loop | count=0 → idx 0..11 全 `SKIP(count<=idx)` |

```text
[JJFB_PROGRESS] idx=0..11 count=0 bar_obj=... SKIP(count<=idx)
```

bar 对象已绑定（`bar!16!18.bmp`），但 **count 永不增加** → 进度条段永不画。

---

## 6. `2EC6B0` RET A/B（状态推进视角）

| RET | splash 次数 | UIMODE_W | PROG_W | AC8_W | NET |
|-----|------------|----------|--------|-------|-----|
| 1 | ~112 | 0 | 0 | 0 | 0 |
| 0 | ~70 | 0 | 0 | 0 | 0 |
| obj | ~66 | 0 | 0 | 0 | 0 |
| pixels | ~102 | 0 | 0 | 0 | 0 |

**结论：返回值变体不改变状态机。** 画面可能略有差异，但 progress/AC8/ui_mode/NET 全无推进。

另：`obj=0` 调用（如 `x=185 y=5`、`x=236 y=5`）已 **SKIP，不 blit、不 dirty** —— 这正是此前右上角异常色块的嫌疑路径之一。

---

## 7. 卡在 0x45 的最小根因

```text
1) 自然启动：ui_mode 停在 0，从不进入 splash（缺“谁该写 ui_mode”的上游）。
2) 强推 0x45：进入局部 splash 循环，但：
   - AC8 无写入
   - progress count 无写入
   - ui_mode 无离开 0x45
3) host 画 loadingbar 只是旁路显示，不替代上述写入副作用。
4) 2EC6B0 guard 的 r0 不是 blocker。
```

因此：**blocker = 上游未完成的初始化 / 平台回调 / 事件，导致 ui_mode 与 splash 内部计数器不推进**，不是坐标或 colorkey。

---

## 8. 下一步（游戏推进，不是 UI）

P0（按优先级）：

1. **追谁该把 ui_mode 从 0 写成非 0**  
   - 在 `0x306344` 之前的 init / `mrc_event` / `0x10140` handler 路径上找 store 到 `ERW+0x8D0`  
   - 对照原机：进 splash 前应经历哪些 mode

2. **追谁该写 `BA0+0x2C`**  
   - 资源加载完成回调、timer、某 ext_call 返回值  
   - 与 `0x10134` / `2d92dc` 完成路径绑定

3. **追谁该写 AC8**（若原版 splash 需要 slogo→loading 衔接）  
   - 当前强推路径下 AC8 读点明确（`0x2EF8A0`），写点仍缺失

4. **不要**再把 chrome / wy_* / 310BB4 当主线。

---

## 运行方式

```powershell
# Run A：自然入口
.\RUN_V44_GAME_STATE_TRACE.ps1 -Mode 0 -Ret 1 -Tag noforce_r1

# Run B：强推 + RET 矩阵
.\RUN_V44_GAME_STATE_TRACE.ps1 -Mode 45 -Ret 1 -Tag force45_r1
.\RUN_V44_GAME_STATE_TRACE.ps1 -Mode 45 -Ret 0 -Tag force45_r0
.\RUN_V44_GAME_STATE_TRACE.ps1 -Mode 45 -Ret obj -Tag force45_robj
.\RUN_V44_GAME_STATE_TRACE.ps1 -Mode 45 -Ret pixels -Tag force45_rpx
```

日志：`logs/v44_*_stdout.txt`

---

## 日志误标修正（已做）

`0x2B241C` 不再标成 `loadingbar parent_slot`，读写分别记为：

- `[JJFB_PROGRESS_COUNT_READ]`
- `[JJFB_PROGRESS_COUNT_WRITE]`
