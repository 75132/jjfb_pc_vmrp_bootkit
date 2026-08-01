# P16 — Family Callback Frame Verdict

## Verdict

**PASS** — Case-9 `REACHED_STOP` ×3; MAP_DATA / linear fallthrough closed.

## Fix applied

1. Strict `end_class` / `reached_stop` (`ok=1` alone is not natural return).
2. `MAP_DATA` → `BRIDGE_DATA_EXEC_TRAP` + sticky lockout + `uc_emu_stop` (no PC/LR rewrite as repair, no fallthrough into MAP_FUNC).
3. Family drain is **not** nested inside `UC_HOOK_CODE`: enqueue during `sendAppEvent`, `SCHEDULE_DRAIN_OUTSIDE_HOOK` after `PC=LR`, deliver after outer emu returns (lifecycle / runCode). That stops Unicorn from resuming at `mr_table` stub+4 (`0x28005C`).

## Root cause (closed)

```
PC=0x28005C
← Unicorn resumed at MAP_FUNC stub+4 after nested Case-9 inside sendAppEvent CODE hook
← nested guest_memory_uc_run_entry_ex inside the hook broke PC=LR
← P13/P14 “API chain” was LINEAR_SLOT_FALLTHROUGH, not game progress
```

## Handler shape

- `0x30D311` = Thumb entry at `0x30D310` = `PUSH {R4-R6,LR}` — **independent function**, not a mid-function label.
- Neighbor thin wrapper at `0x30D308` calls `0x30D25D`; plat `0x10102` registers `0x30D311`.

## Acceptance (3× same binary)

| run | reached_stop | end_class | data_trap | fallthrough | mr_plat | 10133 | first_genuine |
|-----|--------------|-----------|-----------|-------------|---------|-------|---------------|
| hit1 | 1 | REACHED_STOP | 0 | 0 | 0 | 1 | (none in watch window) |
| hit2 | 1 | REACHED_STOP | 0 | 0 | 0 | 1 | (none in watch window) |
| hit3 | 1 | REACHED_STOP | 0 | 0 | 0 | 1 | (none in watch window) |

pass_count=3 / 3

## Final answers

```
0x30D311 是独立函数还是内部标签：独立函数（0x30D310 PUSH {R4-R6,LR}）
原冒泡 callback wrapper 地址：Guest BLX → mr_table；注册目标即 0x30D311（邻接 0x30D308 薄包装不是 10102 注册值）
原冒泡 handler entry SP/LR：见 p16_original_shell_callback_compare.csv；直调路径 stop LR=0x80000 + handler 自带 PUSH
当前直调与原包装器的差异：P16 前在 MAP_FUNC hook 内嵌套 emu；P16 后 hook 外 defer drain，保留 PC=LR
0x28005C 的精确来源：MAP_FUNC@0x280058 在 hook 内嵌套 Case-9 后，Unicorn 落到 stub+4
是否修复真实 continuation：是
Case-9 是否真正 REACHED_STOP：是（×3，end_class=REACHED_STOP，pc_after=stop）
MAP_DATA 是否仍被执行：否（×3 data_trap=0）
P13/P14 假调用是否全部消失：是（×3 fallthrough=0，mr_plat=0）
修复后第一个真实 Guest 行为：sendAppEvent 返回后继续 lifecycle helper（0x306762…），不再表走；随后 timer 自然再发 0x1E209
第一个真实平台 API：本窗口未观察到 GENUINE_ drawBitmap/getCharBitmap/…（假链已消失）
是否恢复五 BMP / Layer-1：本轮未作为硬门
是否出现真实游戏画面：本轮未断言
下一处唯一门锁：Case-9 真正返回之后，第一个 GENUINE_ 平台 API / 自然画面推进
```

## Artifacts

- `out/p16/p16_build_identity.txt`
- `reports/p16_true_call_matrix.csv`
- `reports/p16_original_shell_callback_compare.csv`
- `reports/p16_family_callback_frame_verdict.md`
- Runner: `research/runners/p16_run_family_callback_frame.ps1`
