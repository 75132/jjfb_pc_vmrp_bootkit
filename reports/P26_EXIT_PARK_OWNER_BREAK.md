# P26 EXIT_PARK Owner-Scoped runCode Break

## Verdict

**G1–G6 PASS** (owner-scoped park + `uc_emu_stop` + owner consume + no DSM/`_mr_` FETCH after break).  
**G7 FAIL** — `base+0x7B6C` still NOT_SEEN. Per failure fork **A**: stop further EXIT_PARK edits; next knife is `RUN_CODE_CALLER_RESUME → HOST_LOOP_REENTER` caller/dispatcher (start_dsm did not return / host SDL loop not re-entered within the run window).

## Build identity

| Field | Value |
|-------|-------|
| source_commit | `6552d4a3dbd5a8ad4598779ed60e8a81554ef1aa` |
| source_tree_clean_before_build | false (intentional P26 sources + build artifacts) |
| dirty_files_before_build (source) | `src/runtime/gwy_ext_obs.c`; `src/runtime/p22_selection_gates.c`; `third_party/vmrp_upstream/bridge.c`; `third_party/vmrp_upstream/vmrp.c`; `third_party/vmrp_upstream/gwy_ext_obs_weak.c`; `third_party/vmrp_upstream/header/gwy_ext_obs_abi.h`; `RUN_P26_EXIT_PARK.ps1` |
| research binary_sha (GwyResearch) | `3a5de7fc92a3e880c803cbf0a6b7b1911c421c123c575e93cfce1902066acb64` |
| sha_aligned main/main_gwy | yes |
| report_commit | `6552d4a3dbd5a8ad4598779ed60e8a81554ef1aa` |
| run_id | `1785527926237` |

P25 freeze retained: false BPs at +0x01AF8 / +0x0CE8A; real CFG at +0x7B6C / +0x7B9C / +0xD768; no cfg.bin→gwy/cfg.bin alias; `0x10112` stays IMPLEMENTED_NOT_OBSERVED; POST_CONT `FIRST_FIRE_NO_WAIT` unchanged; no setjmp/longjmp.

## What changed

1. **Owner/depth/serial** on every `runCode` (`g_runCode_serial_stack[]`); park records active-frame serial, not the global counter.
2. **`br_exit`**: `PARK_SET` + immediate `uc_emu_stop`; PC/LR → unmapped `0xDEAD0000` (unicorn 1.0.2 `FETCH_PROT` on mapped RO is unreliable).
3. **`runCode` order**: `uc_emu_start` → park owner check → **then** timer_poll / guiPumpEvents.
4. **Park clear**: only matching owner consumes (`PARK_CONSUMED_BY_OWNER`); non-owner logs `PARK_SEEN_NON_OWNER` only.
5. **Stop hook**: not armed at park time (re-fired on MAP_FUNC stub and blocked return); lazy fallback only.
6. **Timed CSV**: `JJFB_P26_TRACE_CSV` with monotonic `t_sec` (not post-hoc summary rows).

## Gate matrix

| Gate | Result |
|------|--------|
| G0 build/identity | PASS |
| G1 PARK_SET owner-scoped | PASS (depth=1 serial=3) |
| G2 uc_emu_stop in callback | PASS |
| G3 uc_emu_start returns | PASS (~0.13s PARK→RETURN) |
| G4 park consumed once by owner | PASS |
| G5 no DSM/_mr_ FETCH / no fallback | PASS |
| G6 RUN_CODE_RETURNED_TO_HELPER_TAIL (not START_DSM_RETURN) | PASS |
| G7 CFG_LOADER base+0x7B6C | FAIL |
| G8 internal cfg.bin 6898 | NOT_SEEN |
| G9 external base+0xD768 | NOT_SEEN |

## Required answers

1. **uc_emu_stop in br_exit callback?** YES (`EMU_STOP_REQUESTED` same callback as `PARK_SET`).
2. **park owner depth/serial?** depth=`1` serial=`3` (outer sticky `start=0xA4178`).
3. **park uniquely consumed by owner?** YES (`PARK_CONSUMED_BY_OWNER`; no `PARK_SEEN_NON_OWNER`).
4. **timer_poll before park check?** NO — ordered correctly after park check.
5. **DSM after RUN_CODE_BREAK_OWNER?** NO.
6. **outer runCode returned to caller?** YES (`RUN_CODE_LEAVE` + `RUN_CODE_CALLER_RESUME` depth→0).
7. **host loop regained control?** NO — no `HOST_LOOP_REENTER` / `START_DSM_RETURN` before runner timeout (process stayed alive; mutex/helper leave path still holds host).
8. **base+0x7B6C hit?** NO.
9. **internal cfg.bin 6898?** NOT_SEEN.
10. **product path?** `RUN_PRODUCT_DIRECT_JJFB.ps1 -Seconds 90` → **strong_success=YES** (all required product gates; Mode=Gwy + stubs). Shared `bridge.c` park changes did not break the direct JJFB product chain. First-frame SHA `c789a129…bffda` is outside this runner’s gate set; product verdict is ABI/scheduler strong success, not a new Layer-1 capture in this script.

## Control-flow ladder (timed)

From `research/packs/p26_exit_park/P26_CONTROL_FLOW_TRACE.csv` (monotonic `t_sec`):

| t_sec | event |
|------:|-------|
| 22.25 | BR_EXIT_ENTER (early observe; park not yet set) |
| 25–57 | nested `runCode` depth=2 serial=4 (pre-park; owner still 1/3 sticky) |
| 61.94 | nested serial=5 leave → resume owner 1/3 |
| 62.07 | PARK_SET (owner 1/3, pc=0xDEAD0000, phase=shell_core_continue) |
| 62.12 | EMU_STOP_REQUESTED |
| 62.21 | UC_EMU_START_RETURN (uc_err=8; PC/LR residual garbage `0x5F726D5E`/`0x8E6D4`) |
| 62.24 | RUN_CODE_BREAK_OWNER |
| 62.28 | PARK_CONSUMED_BY_OWNER |
| 62.31 | RUN_CODE_LEAVE |
| 62.35 | RUN_CODE_CALLER_RESUME (depth=0) |
| — | HOST_LOOP_REENTER **missing** |
| — | CFG_LOADER_ENTRY **missing** |

Note: post-stop PC/LR still show DSM-ish residues; ownership/stop itself succeeded (~0.13s PARK→RETURN). No `PARK_STOP_HOOK_FALLBACK`, no `PARK_SEEN_NON_OWNER`.

## Failure fork

**A.** G1–G6 PASS, G7 not hit → **do not modify EXIT_PARK further**. Next: why `start_dsm` / host SDL loop does not regain control after `RUN_CODE_CALLER_RESUME` (helper leave, init-seq deliver, mutex unlock). Do not call `0x7B6C` directly.

## Product regression

- Script: `RUN_PRODUCT_DIRECT_JJFB.ps1 -Seconds 90`
- Result: **strong_success YES**
- Forbidden paths: clean (no gamelist_fast / SMSCFG hardwrite / fixed-PC / host fake UI / forced callback)
- Report: `reports/product_direct_jjfb_verdict.md`

## Artifacts

- `reports/P26_EXIT_PARK_OWNER_BREAK.md`
- `research/packs/p26_exit_park/P26_CONTROL_FLOW_TRACE.csv`
- `logs/p26_exit_park_stdout.txt`
- `RUN_P26_EXIT_PARK.ps1`
