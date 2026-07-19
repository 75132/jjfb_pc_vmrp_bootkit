# v54 Natural Game-Self 实施报告

## 1. 本轮目标

v53 已在本机确认：

```text
alias br_log ext_base+0xD4
→ robotol
→ start_dsm MR_IGNORE(1)
→ gated host 6→8→0
→ timer RUNNING
```

v53 文档要求下一轮看「游戏事件/网络或首屏」。审计发现：即便 RUN 脚本清掉 FORCE 环境变量，`bridge.c` 仍**默认**在 tick=10 强制 `ui_mode=0x45`。这违反 GWY Launcher Shim 主线。

v54 唯一任务：

```text
GWY Launcher Mode 下默认 NO FORCE ui_mode
保留 v53 handoff
观察自然推进（事件/网络/FILEOPEN）
```

## 2. 修改文件

| 文件 | 变更 |
|------|------|
| `runtime/.../bridge.c` | GWY mode 默认 `skip_force`；显式 `FORCE_UI_MODE!=0` 才允许旧 probe；新增 `[JJFB_GAME_SELF] natural_mode=1` |
| `RUN_V54_NATURAL_GAME_SELF.ps1` | 新脚本；显式 `JJFB_FORCE_UI_MODE=0` / `JJFB_FORCE_SPLASH_NUDGE=0` |
| `scripts/v54_analyze_post_handoff.py` | 分析 handoff + FORCE 门 + 自然状态 |
| `reports/v54_*.md` | 运行/实施报告 |
| `README/01_HANDOFF_CONTEXT/LATEST_DIRECTION_v54.md` | 最新路线 |

未修改：`jjfb.mrp` / `robotol.ext` / `mrc_loader.ext`。

## 3. 新增/关键环境变量

| 变量 | 值 | 含义 |
|------|-----|------|
| `JJFB_GWY_LAUNCHER_MODE` | `1` | 保留；且默认关闭 FORCE |
| `JJFB_MRP_ALIAS_CFUNCTION_ROBOTOL` | `1` | 保留 v53 别名 |
| `JJFB_ACCEPT_START_IGNORE_AFTER_ROBOTOL` | `1` | 保留 v53 handoff |
| `JJFB_FORCE_UI_MODE` | `0` | 显式关闭 |
| `JJFB_FORCE_SPLASH_NUDGE` | `0` | 显式关闭 |

诊断仍可用：`JJFB_FORCE_UI_MODE=45` 显式打开旧 splash FORCE（非正式方案）。

## 4. 运行命令

```powershell
.\RUN_V54_NATURAL_GAME_SELF.ps1 -Seconds 25
```

## 5. 关键日志（本机）

```text
mr_get_method(1514)
mr_get_method(219)
[JJFB_MRP_ALIAS] ... method=ext_base_0xD4
mr_get_method(161178)
[JJFB_ROBOTOL_LOAD] ... source=br_log
[JJFB_START_HANDOFF] ... action=run_host_801_recovery
[JJFB_801] host version(6) ret=0
[JJFB_801] host appInfo(8) ret=0
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer ... RUNNING=1
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
[JJFB_FIRST_SCREEN] NO FORCE ui_mode (natural) state=0x0 tick=10
```

- FILEOPEN_MISS：0
- FORCE 0x45：否
- ui_mode 全程：`0x0`
- connecting 字符串 watch hits：0
- UI dispatch 仍以 `event_r1=0x13` 进入，但未写 ui_mode

## 6. 实验结论

1. v53 handoff 在关闭 FORCE 后仍然完整。
2. 默认 FORCE 是 v53 报告后「假首屏」的主要来源；关掉后真实状态停在 `ui_mode=0`。
3. 游戏尚未自然进入 splash/检查流；`connecting` xref 未命中。

## 7. 被证伪的假设

- 「清掉 FORCE 环境变量 = 不强制」——错误；未设置时 C 默认仍 FORCE。
- 「timer RUNNING 就等于游戏已自然进入 0x45」——错误；此前 0x45 来自 host FORCE。

## 8. 当前 blocker

```text
ui_mode stuck at 0 without FORCE
```

timer/handler/`event=0x13` 在跑，但没有自然写入 `ERW+0x8D0`（ui_mode）。网络/`_strCom`/connecting 路径未触发。

## 9. 下一步最小任务（v55）

定位**谁本应**写 `ERW+0x8D0` / 启动检查流：

1. 审计 `0x306344` 在 `ui_mode=0` 时的分支（event `0x13` 实际做什么）；
2. 审计是否缺平台事件、`_strCom`、网络回调才让游戏自写状态；
3. 禁止再 FORCE `0x45` / AC8 / progress。

```text
回到 GWY Launcher Shim：补齐平台事件/网络契约，让原始 jjfb 自己推进，而不是 host 写 ui_mode。
```
