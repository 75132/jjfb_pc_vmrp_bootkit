# Phase E7 — LIFECYCLE → natural DRAW

- **date:** 2026-07-19
- **verdict:** `PARTIAL` — lifecycle ARM+FIRE work; handler faults before DRAW
- **decision next:** `HANDLER_ABI_FAULT` (UC_ERR_INSN_INVALID @ registered 10140)

## Gates

| Gate | Result | Evidence |
|------|--------|----------|
| ret0=0 | yes | JJFB_INIT_SEQ |
| 10140 REGISTER store | yes | registry from live plat call |
| 10165 alloc+handler store | yes | classify side-register |
| 10800 ack=1 | yes | CROSS_TARGET+docs/06 |
| LIFECYCLE ARM 50ms | yes | no classic timerStart |
| LIFECYCLE FIRE | yes | host_loop → lifecycle_on_timer_due |
| FIRE ok | no | `UC_ERR_INSN_INVALID` @ `pc=0x306338` |
| DRAW/REFRESH | no | — |
| jjfb hash | unchanged | `52c13182…5fc036` |
| audit_launcher_core | pass | findings=[] |

## What landed (generic)

1. `platform_handler_registry` — live VA only, no fixed JJFB PCs in core defaults
2. Classify: 10165 ALLOC keeps `reg_handler`; 10800 `status_ret=1`
3. Post-`start_dsm`: if 10140 registered and no classic timer → host 50ms tick
4. Deliver via bridge when no EXT chunk; R9 = owning module `start_of_er_rw`
5. Do **not** gate on `module_r9_switch_depth` (stays ≥1 after start_dsm — TARGET_OBSERVED)

## Fault (TARGET_OBSERVED)

```text
handler=0x30630D owner=robotol.ext r9=0x2B1858
pc_after=0x306338 uc_err=10 UC_ERR_INSN_INVALID
sp_after=0x280000
```

Not a missing timer arm. Handler enters then hits invalid insn — next cut is ABI/context (SP / CPSR / BLX target), still without game-state force.

Fault bytes @ `0x306338`: `63 D1 F8 F7 2B FC EF 48` (likely Thumb-2 BL/BLX — HYPOTHESIS until decoded).

## Logs

- `logs/stage_e7_jjfb_lifecycle_stdout.txt`
- `reports/stage_e7_verdict.md`
- Runner: `RUN_E7_LIFECYCLE_DRAW.ps1`
