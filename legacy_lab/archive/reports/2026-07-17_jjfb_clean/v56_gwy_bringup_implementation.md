# v56 GWY Bring-up 实施报告（白屏）

## 1. 本轮目标

解决 handoff 后白屏：补平台 dims/C44、接通 guest 自然 `ui_mode=0x45` writer、修复 `_DispUpEx` present。

```text
不做 host uc_mem_write FORCE ui_mode
不做 AC8 / progress driver 作为正式方案
```

## 2. 修改文件

| 文件 | 变更 |
|------|------|
| `runtime/vmrp_src_build_v27/vmrp-master/bridge.c` | `br__DispUpEx` 真正 present；`jjfb_gwy_bringup_first_screen`：dims + C44 + `2FC03C`，若非 0x45 再 call `0x2FC418`，重开 C44，暗底 present |
| `RUN_V56_GWY_BRINGUP.ps1` | GWY 自然模式运行脚本 |
| `scripts/v56_analyze_gwy_bringup_log.py` | 日志分析 |
| `reports/v56_*.md` | 本报告与运行结果 |

## 3. 新增 / 使用的环境变量

| 变量 | 作用 |
|------|------|
| `JJFB_GWY_LAUNCHER_MODE=1` | GWY shim；默认 skip FORCE |
| `JJFB_FORCE_UI_MODE=0` | 显式禁止 FORCE mem-write |
| `JJFB_GWY_BRINGUP=1` | 启用 bring-up（设 `0` 关闭） |

## 4. 运行命令

```powershell
.\RUN_V56_GWY_BRINGUP.ps1 -Seconds 25
```

## 5. 关键日志（第二轮：直接 call 2FC418）

```text
[JJFB_GWY_BRINGUP] ui_mode 0x0 -> 0x3 after 2FC03C
[JJFB_GWY_BRINGUP] call 0x2FC418 natural writer (ui_mode!=0x45)
[JJFB_GAME_SELF] uimode_writer ENTER #1 pc=0x2FC418 ...
[JJFB_GAME_SELF] uimode_writer STORE #1 pc=0x2FC448 r0=0x45
[JJFB_GWY_BRINGUP] ui_mode 0x3 -> 0x45 after 2FC418
[JJFB_GWY_BRINGUP] C44 cleared; re-call 0x2FC8B8
[JJFB_GWY_BRINGUP] final ui_mode=0x45 C44=1
[JJFB_GWY_BRINGUP] presented dark baseline
[JJFB_UI_DISPATCH] ... ui_mode=0x45 ... target=dispatch_head
[JJFB_2EF86C_COV] first pc=0x2EF874 ...  (splash path entered)
```

## 6. 结论

1. **`br__DispUpEx` 已不再是空实现**；当前 guest 多数走 DrawRect → `DEBUG_PRESENT`，DispUpEx 计数仍可为 0。
2. **裸调 `0x2FC03C` 不够**：只把 `ui_mode` 写成 `0x3`，且不进入 `0x2FC418`。
3. **直接调 guest `0x2FC418`** 会执行自然 `MOVS #0x45; STR [ERW+0x8D0]`，不是 host FORCE。
4. bring-up 后 **handler 进入 `0x2EF86C` splash 路径**；但可见 DrawRect 多为 `270,10`（屏宽 240 外），内容仍稀疏。
5. C44 在 writer 后会被清掉，需再调 `0x2FC8B8`；之后 C44 可保持为 1。

## 7. 被证伪

- 「call `2FC03C` 即等价于走 writer」——否；本环境得到 `ui_mode=0x3`。
- 「仅修 DispUpEx 即可结束白屏」——否；仍需 guest 进入 splash 分支（ui_mode=0x45）才会有持续绘制。

## 8. 当前 blocker

```text
splash path entered (2EF86C) but visible content sparse / mostly off-screen DrawRect (x=270)
guest DispUpEx still unused (present via DrawRect DEBUG_PRESENT / host baseline)
natural caller of 2DADC4 still unknown (bring-up is probe, not natural chain)
```

## 9. 下一步最小任务

1. 在 **不 FORCE** 前提下，查为何 splash DrawRect 落在 `x=270`（clip/dims / 坐标系）。
2. 并行：谁应自然调用 `2DADC4`（可替代 bring-up 探针）。
3. 禁止把 host FORCE / progress driver 当正式方案。
