# Helper 0x2F68E4 CFG Timeline (Task 7)

**Run B:** `h2_B_20260725_012349` (65 s, sparse H2 + lite PAH)  
**Contracts:** `JJFB_PATH_A_EVENT_CONTRACT=1`, `JJFB_PLATFORM_10138_CONTRACT=1`, `JJFB_HELPER_2F68E4_TRACE=1`

## Milestones

| t (approx) | Event |
|---|---|
| T0 | `runtime_spawned` |
| +27 s | `guest_entry_called` / `waiting_for_first_frame` |
| +31 s | `platform_10138_entered` → `platform_10138_completed` |
| +34 s | Call1 `code=5` → `path_a_handler_entered` → **`path_a_helper_running`** / `[H2_ENTER]` |
| +34 s | Phase1: `0x308D98→0x30A0CC→0x10132`, **B60: 0x6→0x3D** |
| +34 s | Phase2: **`0x30A100..0x30A110` stable ring** (400k+ insns) |
| +50 s | `nested_path_a_published` / `nested_event_pending`; `[PAH_REENTER]` nested 2E4040 |
| hold end | **无** `path_a_helper_returned` / `0x2E4066` / `0x2DADC4` |

## CFG 摘要

```text
0x2F68E4  entry
  └─ BL 0x308D98        # B60++ (Phase1: 0x6..0x3D)
       └─ BL 0x30A0CC    # field parser
            ├─ BL 0x2D99AC → 0x10132  (closed)
            └─ loop head 0x30A100 ──┐
                   0x30A10A          │
                   0x30A102          │ Phase2: STABLE (r5=0x7374)
                   0x30A10C          │
                   0x30A104 ... ─────┘
  (未到达) 0x2F68FF → 0x2E4062
```

| 角色 | PC |
|---|---|
| 入口 block | `0x2F68E4` |
| **稳定循环头** | **`0x30A100`** |
| 循环体 | `0x30A100..0x30A110`（8 个 block 轮换） |
| 平台调用 block | `0x2D99E6` → `0x10132` |
| 嵌套发布 | `0x312A60`（helper 内，`nested=1` outstanding） |
| 清理/返回 | `0x2F68FF` → `0x2E4062`（**未到达**） |

## H2 周期快照（节选）

| insn | PC | R5 | q | nested | api |
|---|---|---|---|---|---|
| 5k | 0x30A10C | 0x7374 | 1 | 0 | 1 |
| 100k | 0x30A104 | 0x7374 | 1 | 0 | 1 |
| 195k | 0x30A10E | 0x7374 | 1 | 0 | 1 |
| 205k | 0x30A100 | 0x7374 | 2 | 1 | 8 |
| 400k | 0x30A10C | 0x7374 | 2 | 1 | 8 |

**Phase1→2 分界：** B60 在 `0x30A10C` 写至 `0x3D` 后不再变化；PC 进入 8-block 环。

## 退出谓词（推断）

```text
Phase1 (有进展):
  while (B60 < record_count) { 0x10132(field); B60++ }

Phase2 (零进展, @ 0x30A100):
  while (field_parse_not_done)   # R5=0x7374 冻结
    spin in 0x30A100..0x30A110
  # 未满足 → 未返回 0x30A0CC 外层 → 未 drain 到 BE(-1)
```

## 嵌套 Path-A

| 阶段 | 结果 |
|---|---|
| publish @312A60 | yes（during helper） |
| `[PAH_REENTER]` | nested 2E4040，**不**误关闭 H2 会话 |
| consume | **未观测 pop**；`nested=1` 持续 |
| 调度死锁 | 未分类为 `NESTED_EVENT_SCHEDULING_DEADLOCK`（consumer 曾运行） |

## Stop conditions

| 条件 | 结果 |
|---|---|
| `0x2F68E4_RETURNED` | ❌ |
| `0x2E4066` / `0x2DADC4` | ❌ |
| **`STABLE_HELPER_LOOP`** | **✅ @ 0x30A100** |
| `NEW_PLATFORM_API_MISSING` | ❌ |
| `GUEST_FAULT` | ❌ |

## Verdict

```text
STABLE_HELPER_LOOP (Phase 2 @ 0x30A100)
after HELPER_LONG_BUT_PROGRESSING (Phase 1, B60→0x3D)
```

**下一最小修复域：** 查明 `0x30A0CC` 在 `R5=0x7374`/`B60=0x3D` 下内环退出条件；评估 nested code=5 outstanding 是否阻止字段推进（事件/调度域，非新 platform stub）。
