# Stage E10A-3.1 Gamelist Context Verdict

- **Mode**: `timer_context`
- **run_id**: `1784647402479`
- **Requested seconds**: 120
- **Elapsed**: 12.3s
- **Process exited**: True (code=)
- **Killed by runner**: False
- **Stop reason**: `NO_CONTINUATION`
- **Exit verdict**: `E10A31_HOST_DIAGNOSTIC_EXIT_EARLY`
- **Primary**: `E10A31_INSUFFICIENT_TIMER_EVIDENCE`
- **timer_evidence**: False

## Focus
Package-scoped timer P/ERW/R9, launch-param handoff, cfg-open gate 鈥?not post-cfg update yet.

## Process exit
| Field | Value |
|------|-------|
| last_phase | SHELL_PHASE_GBRWCORE_START |
| last_module | guest_vfs |
| last_pc | 0x0 |
| last_helper | 0xA4178 |
| last_api | log |
| stop_reason | NO_CONTINUATION |

Note: `-Seconds` is outer kill deadline only; do not claim a 120s run if elapsed=12.3.

## Flags
| Flag | Value |
|------|-------|
| ext_first_pc | False |
| timer_arm_csv | False |
| timer_fire_csv | False |
| timer_evidence | False |
| timer_mixed | False |
| timer_mixed_multi | False |
| timer_coherent | False |
| timer_rebound | False |
| param_mem_read | False |
| param_reg_read | False |
| start_dsm_abi | True |
| cfg_gate | False |
| cfg_open | False |
| cfg_bin | False |

## CSV counts
timer=0 arm=0 fire=0 param=0 start_dsm=1 cfg_gate=0

## Verdicts
- `E10A31_HOST_DIAGNOSTIC_EXIT_EARLY`
- `E10A31_INSUFFICIENT_TIMER_EVIDENCE`
- `START_DSM_PARAM_ABI_CONFIRMED`
- `NO_REAL_CFG_BIN_OPEN`

## Artifacts
| Kind | Path |
|------|------|
| process exit | `reports/e10a31_process_exit_trace.csv` |
| timer binding | `reports/e10a31_timer_binding_trace.csv` |
| param read | `reports/e10a31_launch_param_read_trace.csv` |
| start_dsm ABI | `reports/e10a31_start_dsm_param_abi.csv` |
| cfg gate | `reports/e10a31_cfg_gate_predicates.csv` |
| cfg annotate | `out/e10a31/gamelist_cfg_gate_annotated.txt` |
| log | `logs/e10a31_gamelist_context_stdout.txt` |