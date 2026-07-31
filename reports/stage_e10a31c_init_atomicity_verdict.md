# Stage E10A-3.1c Init Atomicity Verdict

- **Mode**: `init_atomicity`
- **run_id**: `1784660268309`
- **overlay**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay\e10a31c\1784660268309`
- **Requested seconds**: 90
- **Elapsed**: 91.4s
- **Process exited**: True (code=0 class=NORMAL_EXIT)
- **Killed by runner**: True
- **Observe stop**: `OBSERVE_STOP_DEFERRED_FIRE_AFTER_INIT_TX`
- **Primary**: `GAMELIST_INIT_METHOD0_FAILED`
- **Label**: FAST_REAL_GAMELIST_INIT_SEQUENCE / NOT_PRODUCT (diagnostic reconstruction)

## Success order
1. GUEST_TIMER_REENTRANCY_PREVENTED
2. GAMELIST_INIT_SEQUENCE_COMPLETE
3. DEFERRED_TIMER_FIRED_AFTER_INIT
4. (later) MEM_GET_RETURN_PROVEN / cfg gate

## Flags
| Flag | Value |
|------|-------|
| reentrancy_prevented | True |
| timer_deferred_init | True |
| init6 enter/return | True / True |
| init8 enter/return | True / True |
| init0 enter/return | True / True |
| init_complete | False |
| init_interrupted | False |
| deferred_fire | True |
| mem_get enter/return | True / True |
| cfg_gate / cfg_bin | False / False |

## Verdicts
- `GAMELIST_INIT_METHOD0_FAILED`
- `GUEST_TIMER_REENTRANCY_PREVENTED`
- `TIMER_DEFERRED_DURING_INIT`
- `DEFERRED_TIMER_FIRED_AFTER_INIT`
- `MEM_GET_RETURN_PROVEN`
- `FAST_REAL_GAMELIST_INIT_SEQUENCE_NOT_PRODUCT`

## Artifacts
| Kind | Path |
|------|------|
| init sequence | `reports/e10a31c_init_sequence_trace.csv` |
| mem_get | `reports/e10a31c_mem_get_trace.csv` |
| unknown API | `reports/e10a31c_unknown_platform_api_trace.csv` |
| process exit | `reports/e10a31c_process_exit_trace.csv` |
| native exception | `reports/e10a31c_native_exception_trace.csv` |
| env manifest | `reports/e10a31c_environment_manifest.csv` |
| runtime manifest | `reports/e10a31c_runtime_manifest.csv` |
| log | `logs/e10a31c_init_atomicity_stdout.txt` |