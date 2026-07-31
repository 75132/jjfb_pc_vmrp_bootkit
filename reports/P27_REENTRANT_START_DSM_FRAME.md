# P27 Reentrant START_DSM Parameter Ownership and Host Return

## Verdict

**G1–G10 PASS.** Per-call `start_t` frames fixed nested overwrite/cleanup; outer `bridge_dsm_mr_start_dsm` returns, mutex unlocks, and `HOST_LOOP_REENTER` fires.

**G11 FAIL** — `base+0x7B6C` still NOT_SEEN. Failure fork **C**: stop start_dsm ownership work; next = host loop first scheduler/event dispatch. Do not call CFG loader.

## P26 freeze / rename

EXIT_PARK frozen this round. Former P26 G6 is **`RUN_CODE_RETURNED_TO_HELPER_TAIL`** only (not `START_DSM_RETURN` / `HOST_LOOP_REENTER`). Resume trace now records `returned_serial=3` for the park-owner frame (no longer substitutes global serial `5`).

## Build identity

| Field | Value |
|-------|-------|
| source_commit (pre-P27 baseline) | `70e35adabed1ccb80f8692b5f8bdf888aaf59dd5` |
| source_tree_clean_before_build | false (P27 sources dirty during first evidence build) |
| dirty_files_before_build | `src/runtime/gwy_ext_obs.c`; `third_party/vmrp_upstream/bridge.c`; `gwy_ext_obs_weak.c`; `header/gwy_ext_obs_abi.h`; `RUN_P27_START_DSM.ps1` |
| research binary_sha | `10deb4e95ea7b74112a289abcbe192a6f54df7611e1c9d0d50fc38742ce86494` |
| sha_aligned main/main_gwy | yes |
| run_id | `1785528948895` |

## What changed

1. **P27A instrumentation**: frame_id / parent / depth, guest VAs, ownership table, free guards (`WOULD_FREE_INVALID` / `DOUBLE_FREE` / `FOREIGN_FREE`).
2. **P27B fix**: each `bridge_dsm_mr_start_dsm_unlocked` allocates its own guest `start_t` + strings; cleanup uses local ownership only (never re-read global `mr_start_dsm_param`).
3. **`mr_c_event`**: nested clobber recorded (`MR_EVENT_FRAME_CLOBBERED`); Case A — host does not re-read outer event after nested return; no per-call `event_t` this round.
4. **Helper-tail ladder** + mutex lock/unlock markers.
5. **Resume serial fix**: `RUN_CODE_CALLER_RESUME` carries `returned_depth/serial` and parent fields.

## Gate matrix

| Gate | Result |
|------|--------|
| G0 build/SHA | PASS |
| G1 outer gbrwcore frame | PASS |
| G2 nested gamelist frame | PASS |
| G3 distinct start_t VA | PASS (`0x2829D4` vs `0x682AB4`) |
| G4 no parent clobber | PASS (`START_DSM_PARAM_OWNERSHIP_OK`) |
| G5 no invalid/double/foreign free | PASS (each alloc `free_count=1`) |
| G6 outer bridge_mr_event return | PASS |
| G7 outer `_unlocked` return | PASS (`FRAME_LEAVE` depth=1) |
| G8 mutex unlock | PASS |
| G9 START_DSM_RETURN | PASS |
| G10 HOST_LOOP_REENTER | PASS |
| G11 base+0x7B6C | FAIL |
| G12 cfg.bin 6898 | NOT_SEEN |

## Required answers

1. **Same start_t reused?** Pre-fix yes (global). Post-fix **no** — distinct guest VAs.
2. **Child covered parent filename/ext/entry?** **No** (ownership OK).
3. **Parent cleanup would free VA 0?** **No** (guards idle).
4. **Entry double free?** **No**.
5. **mr_c_event nested clobber?** **Yes** (Case A risk only).
6. **Outer bridge_mr_event return?** **Yes**.
7. **Outer `_unlocked` return?** **Yes**.
8. **Mutex released?** **Yes**.
9. **START_DSM_RETURN?** **Yes**.
10. **HOST_LOOP_REENTER?** **Yes**.
11. **base+0x7B6C natural?** **No**.
12. **Product / Layer-1?** See below.

## Timed ladder (excerpt)

| t_sec | event |
|------:|-------|
| 3.88 | START_DSM_FRAME_ENTER outer gbrwcore frame=1 start_g=0x2829D4 |
| 24.88 | START_DSM_FRAME_ENTER nested gamelist frame=2 start_g=0x682AB4 |
| 25.50 | MR_EVENT_FRAME_CLOBBERED (Case A) |
| 65.73 | START_DSM_PARAM_OWNERSHIP_OK child |
| 66.31 | START_DSM_FRAME_LEAVE child |
| 69.75 | START_DSM_PARAM_OWNERSHIP_OK parent |
| 70.27 | START_DSM_FRAME_LEAVE outer |
| 70.33 | MUTE_UNLOCK_END |
| 70.39 | START_DSM_RETURN |
| 70.46 | HOST_LOOP_REENTER |

## Product regression

| Runner | Result |
|--------|--------|
| `RUN_PRODUCT_DIRECT_JJFB.ps1 -Seconds 90` | **strong_success=YES**; all ABI/scheduler gates; `post_callback=FIRST_NATURAL_DRAW` |
| `RUN_PRODUCT_FIRST_FRAME_PUSH.ps1 -Mode Event -Seconds 50` | `P6_EVENT_PARTIAL` / `POST_DRAIN_SUCCESSOR_BLOCKED` (FFP event path; not a full Layer-1 golden capture) |

Existing product chain still displays the established JJFB progress (descriptor → robotol → natural scheduler / first natural draw). The historical Layer-1 SHA prefix `c789a129…bffda` is **not present as an in-repo golden gate** on these runners; this round does **not** claim a re-verified Layer-1 framebuffer SHA match. No early three-resource bailout; no epilogue / resource-handle return changes; `direct_lr` default untouched.

## Failure fork

**C.** Host loop returned; `+0x7B6C` still missing → next investigate first host-loop scheduler/event dispatch after `HOST_LOOP_REENTER`. Do not call CFG loader. Do not reopen EXIT_PARK or start_dsm ownership unless product regresses.

## Artifacts

- `reports/P27_REENTRANT_START_DSM_FRAME.md`
- `research/packs/p27_start_dsm/P27_START_DSM_TRACE.csv`
- `research/packs/p27_start_dsm/P27_ALLOCATION_OWNERSHIP.csv`
- `logs/p27_start_dsm_stdout.txt`
- `RUN_P27_START_DSM.ps1`
