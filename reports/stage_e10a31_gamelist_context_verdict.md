# Stage E10A-3.1 Gamelist Context Verdict

- **Mode**: `cfg_gate`
- **run_id**: `1784656128442`
- **overlay**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay\e10a31\1784656128442`
- **Requested seconds**: 120
- **Elapsed**: 61.2s
- **Process exited**: True (code=0)
- **Killed by runner**: False
- **Observe stop**: ``
- **Stop reason**: `GUEST_NORMAL_EXIT`
- **Exit verdict**: `E10A31_REACHED_GAMELIST_TIMER`
- **Primary**: `GAMELIST_TIMER_CONTEXT_COHERENT`
- **timer_evidence**: True (binding_ok=True owners_ok=True)
- **env**: `E10A31_RUN_ENVIRONMENT_DETERMINISTIC` (full JJFB_/GWY_/VMRP_ wipe + unique overlay)

## Timer sample
```
helper=0x2E3089 P=0x2AC8DC chunk=0x682B24 ERW=0x682B6C class=TIMER_CONTEXT_COHERENT helper_mod=4 chunk_mod=4 p_mod=4 erw_mod=4 module=gamelist.ext
```

## Process exit
| Field | Value |
|------|-------|
| last_phase | SHELL_PHASE_CFG_FMT_MAPPED |
| last_module | gamelist.ext |
| last_pc | 0x0 |
| last_helper | 0x6835A4 |
| last_api | PLATFORM_TIMER/FIRE_DONE |
| stop_reason | GUEST_NORMAL_EXIT |

## Flags
| Flag | Value |
|------|-------|
| ext_first_pc | True |
| post_cont | False |
| continue_apply | True |
| timer_arm_csv | True |
| timer_fire_csv | True |
| timer_evidence | True |
| timer_rebound | False |
| timer_coherent | True |
| timer_mixed_multi | False |
| erw_unpublished | False |
| chunk_retarget / foreign_erw | False / False |
| chunk_mm / p_mm / erw_mm | False / False / False |

## CSV counts (run_id filtered)
timer=3 arm=1 fire=1 fire_ret=1 param=310 start_dsm=0 cfg_gate=0

## Verdicts
- `E10A31_REACHED_GAMELIST_TIMER`
- `GAMELIST_EXT_FIRST_PC`
- `GAMELIST_TIMER_CONTEXT_COHERENT`
- `GAMELIST_CHUNK_REUSE_REFUSED`
- `GAMELIST_CHUNK_CREATED`
- `GAMELIST_ERW_PUBLISHED`
- `GAMELIST_TIMER_ARMED_WITH_OWN_CHUNK`
- `START_DSM_PARAM_REACHED_GAMELIST`
- `NO_REAL_CFG_BIN_OPEN`

## Artifacts
| Kind | Path |
|------|------|
| env manifest | `reports/e10a31_environment_manifest.csv` |
| runtime manifest | `reports/e10a31_runtime_manifest.csv` |
| process exit | `reports/e10a31_process_exit_trace.csv` |
| timer binding | `reports/e10a31_timer_binding_trace.csv` |
| log | `logs/e10a31_gamelist_context_stdout.txt` |