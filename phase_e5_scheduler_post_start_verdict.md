# Stage E5 — SCHEDULER post-`start_dsm` audit

- **task:** SCHEDULER — after `START_DSM_RETURN`, why no timer arm → fire → method=1 → DRAW
- **verdict:** `PASS` (E5-min/mid observation) → next branch **`PLATFORM_RET0_CAUSE`**
- **source:** `descriptor_launcher` (cfg36 → `gwy/jjfb.mrp`)

## Discriminating results

| Observation | Result | Class |
|-------------|--------|-------|
| Host post-start loop polled | **Yes** — `[JJFB_POST_START_LOOP]` + `[JJFB_TIMER_POLL]` for ~60s | TARGET_OBSERVED |
| Guest `timerStart` / `mr_timerStart` | **Absent** — `arm_count=0`, `[JJFB_TIMER_ARM_ABSENT]` | TARGET_OBSERVED |
| sendAppEvent “ARM_ATTEMPT” | Noise only (`kind=0` status); not TIMER_START; no `[JJFB_TIMER_ARM]` | OBSERVED |
| FIRE / DELIVERED / DRAW | **No** | TARGET_OBSERVED |
| INIT_SEQ 6/8/0 + `[JJFB_MRC_INIT]` | **Yes** — `ret6=0 ret8=0 ret0=-1`; `start_dsm ret=0` | DOCUMENTED + TARGET_OBSERVED |

## Decision (per E5 table)

| Signal | Value |
|--------|-------|
| post_loop | yes |
| timer API arm | no |
| fire / delivered / draw | no |
| ret0=0 | no |

→ **`PLATFORM_RET0_CAUSE`** (not SCHEDULER_FIX): host polls correctly; guest never armed a timer after failed init.

Alternate `LIFECYCLE_EVENT_REQUIRED` remains possible only if a later prove shows guest waits for host lifecycle *after* ret0=0. With ret0=-1 and no arm, platform/init failure is the discriminating next cut.

## Runner env fix (blocker unblocked)

Product track previously relied on ambient shell leftover:

1. Missing `GWY_MODULE_R9_SWITCH=1` → no DSM R9 guard → `UC_MEM_READ_UNMAPPED` @0x1F58
2. Missing `GWY_CALLBACK_FRAME=1` → no post_cont arm → no ER_RW_BIND / INIT_SEQ
3. Do **not** set `GWY_POST_CONT_AUDIT=1` (auto-enables P_EXTCHUNK flood and stalls boot)

Now explicit in `RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1` / `RUN_E5_SCHEDULER_POST_START.ps1`.

## wxjwq control

`mmochat.ext` ENTRY_CALLED in 45s; no POST_START_LOOP in that window (shorter run / no START_DSM_RETURN sample). Not used to overturn jjfb decision.

## Artifacts

- `logs/stage_e5_jjfb_scheduler_stdout.txt`
- `logs/stage_e5_wxjwq_scheduler_stdout.txt`
- `reports/stage_e5_*.md`
- `RUN_E5_SCHEDULER_POST_START.ps1`

## Checks

- build: prior Gwy binary (SkipBuild); env-only runner fix this slice
- unit: not re-run (no core logic change this slice)
- audit: `ok=true` findings=[]
- hash: `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036`

## Next (exactly one)

**E6 / PLATFORM_RET0_CAUSE:** during DOCUMENTED helper code=0, which platform/sendAppEvent path makes robotol return `-1` (observe plat codes + returns; no UI force).
