# Stage E3 — DOCUMENTED helper `code=0` (mrc_init)

- **task:** RUNTIME — after game_package ER_RW/R9 ready, deliver mythroad init seq `6→8→0`
- **verdict:** `COMPLETE` for E3 marker `[JJFB_MRC_INIT]`
- **source:** `descriptor_launcher` (cfg36 → `gwy/jjfb.mrp`)

## Proven

| Gate | Result |
|------|--------|
| Helper retarget DSM→mrc_loader→robotol | PASS — `[JJFB_HELPER_RETARGET]` … `0x304AED` |
| Queue after `[JJFB_R9_SWITCH_OK]` | PASS — `[JJFB_INIT_SEQ] queued=1` |
| Deliver `6→8→0` on host helper (not Unicorn hook) | PASS — `deliver_after_code_1` |
| `[JJFB_INIT_SEQ] helper=… code=6/8/0` | PASS — P=`0x2AC8DC` er_rw=`0x2B1858` |
| `[JJFB_MRC_INIT] module=robotol.ext … method=0` | PASS — route=`mr_extHelper` |
| jjfb sha256 | PASS — `52c13182…5fc036` |
| `audit_launcher_core.py` | PASS — `ok: true` |
| unit `ext_er_rw_bind_restore` | PASS |

## Evidence class

| Claim | Class |
|-------|-------|
| case_801 codes 6→8→0 after load | DOCUMENTED (`mythroad.c` / `start.mr`) |
| Host must retarget `mr_extHelper` on nested `_mr_c_function_new` | DOCUMENTED + OBSERVED |
| Queue during long `MR_START_DSM` helper; deliver on helper exit | TARGET_OBSERVED (timing) |
| Product marker from helper `code=0` | DOCUMENTED |

## Fixes landed

1. LOG_PARSE nested `_mr_c_function_new` retargets host `mr_extHelper_addr` + guest P.
2. After game_package `R9_SWITCH_OK`, `gwy_ext_obs_request_ext_init_seq()` queues (launcher_core; PE-safe, not weak-in-bridge).
3. Bridge delivers on helper enter **and** exit / post-`start_dsm` (`deliver_after_code_1` hit).

## Observed return

- `delivered ret6=0 ret8=0 ret0=-1` — marker PASS; `ret0=-1` is OBSERVED guest/helper return, not proof of full app draw.

## Artifacts

- `RUN_E2_GAME_PACKAGE_ER_RW_MRCINIT.ps1`
- `logs/stage_e2_jjfb_game_package_er_rw_stdout.txt`
- `reports/stage_e2_verdict.md`

## Next (exactly one)

Stage E4: discriminate `ret0=-1` / post-`mrc_init` continuation (event scheduler / natural draw) without UI force or fake login.
