# Stage E10A-3.1 Gamelist Context Verdict

- **Mode**: `timer_context`
- **run_id**: `1784646810107`
- **Elapsed**: 12.5s
- **Primary**: `GAMELIST_PACKAGE_CONTEXT_MIXED`

## Focus
Package-scoped timer P/ERW/R9, launch-param handoff, cfg-open gate 鈥?not post-cfg update yet.

## Flags
| Flag | Value |
|------|-------|
| ext_first_pc | False |
| timer_mixed | False |
| timer_coherent | False |
| timer_rebound | False |
| param_mem_read | False |
| param_reg_read | False |
| start_dsm_abi | True |
| cfg_gate | False |
| cfg_open | False |
| cfg_bin | False |

## CSV counts
timer=0 param=0 start_dsm=1 cfg_gate=0

## Verdicts
- `START_DSM_PARAM_ABI_CONFIRMED`
- `NO_REAL_CFG_BIN_OPEN`

## Artifacts
| Kind | Path |
|------|------|
| timer binding | `reports/e10a31_timer_binding_trace.csv` |
| param read | `reports/e10a31_launch_param_read_trace.csv` |
| start_dsm ABI | `reports/e10a31_start_dsm_param_abi.csv` |
| cfg gate | `reports/e10a31_cfg_gate_predicates.csv` |
| cfg annotate | `out/e10a31/gamelist_cfg_gate_annotated.txt` |
| log | `logs/e10a31_gamelist_context_stdout.txt` |