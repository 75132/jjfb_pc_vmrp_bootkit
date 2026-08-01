# P17 — Post-Case9 Lifecycle Verdict

## Verdict

**PASS (lifecycle observation)** — Case-9 stays `REACHED_STOP` across 3×180s; nest/fallthrough closed; first genuine platform API still absent. Natural lock is a **stateless 0x1E209 re-issue cycle** paced by host lifecycle timer.

## Guard (permanent)

| Depth | Role |
|-------|------|
| `hook_depth` | MAP_FUNC / `UC_HOOK_CODE` enter→leave |
| `guest_run_depth` | around `uc_emu_start` in `guest_memory_uc_run_entry_ex` |
| `family_drain_depth` | family drain outer boundary |

`hook_depth > 0` → refuse `guest_memory_uc_run_entry_ex`, log `NESTED_EMU_IN_CODE_HOOK_BLOCKED`, return `GWY_ENTRY_NESTED_EMU_BLOCKED`.

Unit: `test_nested_emu_guard` PASS.

Live: **blocked_events = 0 ×3** (defer path healthy; guard not forced to fire).

## Long-run matrix (same binary)

| run | case9_done | reached_stop | nested_block | data_trap | fallthrough | 1E209 | 10133 | genuine |
|-----|------------|--------------|--------------|-----------|-------------|-------|-------|---------|
| hit1 | 48 | 96 | 0 | 0 | 0 | 3191 | 48 | (none) |
| hit2 | 48 | 96 | 0 | 0 | 0 | 3073 | 48 | (none) |
| hit3 | 48 | 96 | 0 | 0 | 0 | 3175 | 48 | (none) |

## Case-9 → lifecycle path (observed)

```
sendAppEvent(0x1E209, app=0x9)
→ ENQUEUE handler=0x30D311
→ SCHEDULE_DRAIN_OUTSIDE_HOOK (PC=LR cont=0x304599)
→ DRAIN_OUTSIDE_HOOK / FAMILY_HANDLER
→ REACHED_STOP pc_after=stop
→ host PLATFORM_TIMER 50ms (lifecycle_10140_forced_host)
→ JJFB_LIFECYCLE FIRE handler=0x30631D
→ GUEST_INDIRECT_CALL pc=0x306762 → 0x2F9975
→ guest re-issues sendAppEvent(0x1E209) / also 0x10133 with r1=0x1E205
→ repeat
```

## Loop classification

**A (paced wait) + B (stateless)** hybrid:

- Host fires lifecycle every 50ms (`forced=yes`); classic `mr_timerStart` absent (`JJFB_TIMER_ARM_ABSENT`).
- Guest re-posts identical `0x1E209` with **single event digest** `0xFF5FCBC8` across the window.
- No `mr_sleep`, no drawBitmap/getCharBitmap, no GENUINE_ MAP_FUNC after Case-9.
- Hotspots: `0x80000`, `0x2F997A`, `0x306762` family, `0x280058` sendAppEvent stub.

Not pure busy-spin (timer-paced). Not module exit. Pump exists, but **progress condition is missing**.

## Genuine platform API

**None** in 3×180s (`class=GENUINE_` count = 0).

Bridge entries after Case-9 are dominated by family/sendAppEvent path, not new MAP_FUNC BL targets.

## BMP / Layer-1

| Check | Status |
|-------|--------|
| Five BMP natural load | **NOT_REACHED** |
| Layer-1 | **NOT_REACHED** |
| Real game frame | **no** |

Long window never enters old splash path — **not** scored as REGRESSION.

## Shell compare

See `reports/p17_shell_direct_lifecycle_compare.csv` (product vs P16 shell frame notes). Full `RUN_RESEARCH_GWY_SHELL` deferred after product matrix to keep the three hits on one binary; parent-scheduler gap remains the open question for P18.

## Result class

**Closest: B (missing progress after Case-9) with C flavor (host-forced lifecycle replaces absent classic timer).**

Not Result A (no first unimplemented genuine API). Not D/E (no network / no real frame).

## PASS answers

```
Case-9 是否持续 REACHED_STOP：是（48 DELIVER_DONE/run ×3，end_class=REACHED_STOP）
是否再次发生 Hook 内嵌套 emu：否（NESTED_EMU_IN_CODE_HOOK_BLOCKED=0；defer path 生效）
MAP_DATA / fallthrough 是否保持为 0：是（data_trap=0，fallthrough=0）
Case-9 后真实 lifecycle 路径：drain外交付 → host 50ms lifecycle → 0x30631D → 0x306762 → 再投 0x1E209
重复 0x1E209 是否正常：周期存在，但 digest 不变 → 无状态推进的定时等待，不是健康进度
第一个真实 Guest 平台 API：（无）
是否存在稳定等待/忙循环：是 — A/B 混合（50ms 节拍 + 无状态重投）
是否缺 timer/event pump：经典 mr_timerStart 缺失；host lifecycle pump 已在转；缺的是推进条件/父级事件
是否缺原冒泡父级 lifecycle：可疑（直启无父级 shell 续接；需 P18 差分确认）
五 BMP 状态：NOT_REACHED
Layer-1 状态：NOT_REACHED
是否出现真实游戏画面：否
当前唯一自然门锁：Case-9 返回后打破无状态 0x1E209 重投的条件（父级事件 / 缺省状态生产者 / 下一真实 API）
下一处最小通用平台缺口：查明原冒泡在 Case-9 返回后是否投递不同事件或父级 scheduler 动作；不要实现假 API 链
```

## Artifacts

- `out/p17/p17_build_identity.txt`
- `reports/p17_genuine_call_matrix.csv`
- `reports/p17_shell_direct_lifecycle_compare.csv`
- `reports/p17_long_run_matrix.csv`
- `out/p17/p17_post_case9_timeline.csv`
- `out/p17/p17_hotspots.txt`
- Runner: `research/runners/p17_run_post_case9_lifecycle.ps1`
- Guard test: `tests/unit/test_nested_emu_guard.c`
