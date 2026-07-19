# Stage E6 — PLATFORM_RET0_CAUSE

- **task:** PLATFORM — why `mrc_init` returns `-1` under DOCUMENTED 6/8/0
- **verdict:** `PASS` for ret0 — next branch **`LIFECYCLE_EVENT_REQUIRED`**
- **source:** `descriptor_launcher` → `gwy/jjfb.mrp`

## Discriminating chain (code=0 window)

| Plat code | Before | After | Class |
|-----------|--------|-------|-------|
| `0x10113` | status 0, no `*out` write | write `_DrawBitmap` fp to `*R3`, ret=0 | CROSS_TARGET + docs/06 |
| `0x10102` | status 0 | REGISTER ack ret=1 | CROSS_TARGET + docs/06 |
| `0x10120` | status 0 | REGISTER ack ret=1 | CROSS_TARGET + docs/06 |
| `0x10140` | (reached only after 10120 fixed) status 0 | REGISTER ack ret=1 | CROSS_TARGET + docs/06 |
| `0x10162` | status 0 (NULL) | ALLOC `size=R1`, return guest ptr | CROSS_TARGET + docs/06 |

Also wired `mr_table._DrawBitmap` / `mr_drawBitmap` → `br_mr_drawBitmap` (DOCUMENTED display path for 10113).

## Result

```text
[JJFB_MRC_INIT] ... ret=0
[JJFB_INIT_SEQ] delivered ret6=0 ret8=0 ret0=0
```

No DRAW/REFRESH in this 60s E6 window → decision table: **LIFECYCLE_EVENT_REQUIRED** (post-init event/timer arm path).

## Anti-drift

- No fixed JJFB ERW/addresses in core
- No UI/login/network fake
- guest mem R/W via `guest_memory_uc_peek/poke`
- jjfb hash unchanged
- `audit_launcher_core.py`: ok

## Artifacts

- `RUN_E6_PLATFORM_RET0_CAUSE.ps1`
- `logs/stage_e6_jjfb_plat_ret0_stdout.txt`
- `reports/stage_e6_verdict.md`
- `tests/unit/test_platform_send_app_event.c`

## Next (exactly one)

**E7 / LIFECYCLE_EVENT_REQUIRED:** after `ret0=0`, why guest still does not arm timer / receive lifecycle → natural DRAW (observe-only first; no force).
