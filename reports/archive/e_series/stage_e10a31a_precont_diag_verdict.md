# E10A-3.1a pre-continuation diagnostic verdict

## Summary

**Strong success.** Fresh deterministic env + unique overlay made `gbrwcore → shell_core_continue` work. Prior `NO_CONTINUATION` / ~12s exit was **not** a broken continue gate.

A/B interpretation: `PREVIOUS_EARLY_EXIT_WAS_NONDETERMINISTIC_OR_STALE_ENV` (both A and B reached gamelist).

## exit_trace (rebuild)

| field | value |
|-------|--------|
| primary | `SHELL_CORE_CONTINUE_REACHED` |
| decision | `GWY_CONTINUE_READY` |
| br_exit | yes |
| continue apply | yes |
| POST_CONT_PUMP | yes |
| GAMELIST_EXT_FIRST_PC | yes |
| fallback / process_exit | no |
| killed_by_runner | yes (~80s outer kill; process stayed alive after continue) |
| exit_code | 0 |

Continue gate CSV (br_exit_enter):

```text
decision=GWY_CONTINUE_READY
shell_mode=1 armed=1
gbrwcore_started=1 gbrwcore_first_pc=1
already_continued=0 gamelist_started=0
active_package=gwy/gbrwcore.mrp active_module=gbrwcore.ext
target=gwy/gamelist.mrp
ERW=0x2B0D18 helper=0x30CFE9
note=ready_to_continue
```

## ab_compare (-SkipBuild)

| case | TIMER_CONTEXT | result |
|------|---------------|--------|
| A | unset | continue + gamelist (exit 0) |
| B | `JJFB_E10A31_TIMER_CONTEXT=1` | continue + gamelist (exit 0) |

→ Not an E10A-3.1 trace side-effect. Prior early exit matches **stale overlay / inherited env / nondeterminism**.

## Environment

- `E10A31A_RUN_ENVIRONMENT_DETERMINISTIC`
- Full wipe of inherited `JJFB_` / `GWY_` / `VMRP_` before whitelist
- Unique overlay: `out/vmrp_run/overlay/e10a31a/<run_id>/`

## Artifacts

- `reports/e10a31a_environment_manifest.csv`
- `reports/e10a31a_runtime_manifest.csv`
- `reports/e10a31a_continue_gate_trace.csv`
- `reports/e10a31a_runtime_stop_trace.csv`
- `reports/e10a31a_gbrwcore_tail_trace.csv`
- `reports/e10a31a_ab_environment_diff.csv`
- `reports/e10a31a_ab_milestone_compare.csv`
- `logs/e10a31a_exit_trace_{stdout,stderr}.txt`
- `logs/e10a31a_{a,b}_{stdout,stderr}.txt`

## Next

Strong criteria met → safe to resume **E10A-3.1 `timer_context`** with the same deterministic runner pattern (full env wipe + unique overlay). Do not treat the old 12.3s `NO_CONTINUATION` as a live continue-gate bug.
