# Stage E10A-3 Gamelist Post-Selection Verdict

- **Modes run**: `postselect`, `event10180`, `services` (last run_id `1784644937112`)
- **Primary**: `GAMELIST_WAITING_FOR_EVENT`
- **Product success**: **NO** (`NOT_PRODUCT`)

## Executive summary

E10A-3 instrumentation is live and the first three knives agree on the blocker:

**This is not yet a post-selection stall.** Gamelist reaches `init_ok`, runs timer helper `0x2E3089`, and completes `0x10180` synchronously, but never opens `cfg.bin`, never reads the gwyblink launch param, and never performs runtime named-service lookup. The old `SHELL_PHASE_CFG_RECORD_SELECTED` milestone was a false positive (format-string map only); it is now split into `SHELL_PHASE_CFG_FMT_MAPPED` vs real `SHELL_PHASE_CFG_RECORD_SELECTED`.

## Hypothesis order (prove/falsify)

| # | Hypothesis | Result | Evidence |
|---|------------|--------|----------|
| H1 | `0x10180` async callback missing | **Falsified** | 7 request/return pairs; classification `synchronous_query`; blob `0x682B24`; caller `0x2E239D`; subcodes R1=0,1,5,A,C,3,4 |
| H2 | Root platform assembly bypassed | **Likely** | No `cfg.bin` open; no `PARAM_READ`; string-VA registry only; no dispatcher |
| H3 | gbrwcore named-service lookup incomplete | **Confirmed (pre-cfg)** | 5 `register` ops (string VA); **0** `lookup`/`call` from gamelist after init |
| H4 | Timer wait on another event | **Confirmed** | `0x2E3089` FIRE_EXT ret=0 streak>=3; `erw=0x2B0D18` (gbrwcore leftover) |

## Observed flags (services run)

| Flag | Value |
|------|-------|
| gamelist init_ok | True |
| cfg_fmt_mapped | True |
| cfg_real_selected | **False** |
| cfg_descriptor | False |
| userinfo_request/response | True / True |
| plat_10180 sync | True |
| named register (string VA) | True |
| named runtime lookup | **False** |
| cfg.bin open | **False** |
| param_read (gwyblink) | **False** |
| timer 0x2E3089 wait loop | True |

## Verdicts (ordered)

- `GAMELIST_POSTSELECT_FLOW_CAPTURED`
- `EVENT_10180_CONTRACT_PARSED`
- `EVENT_10180_COMPLETED`
- `GBRWCORE_SERVICE_REGISTRY_BUILT`
- `GAMELIST_WAIT_PREDICATE_FOUND`
- `GBRWCORE_NAMED_SERVICE_PROVIDER_MISSING` (runtime lookup/dispatcher)
- `GAMELIST_WAITING_FOR_UI_EVENT`
- `SHELL_CONTINUE_MISSING_ROOT_SERVICES`
- `GAMELIST_WAITING_FOR_EVENT`
- `PRODUCT_STILL_NEEDS_NATIVE_SHELL_COMPLETION`

**Not reached:** `SHELL_PHASE_CFG_RECORD_SELECTED` (real), `SHELL_PHASE_UPDATE_*`, `SHELL_PHASE_STARTGAME_*`, `SHELL_PHASE_RUNAPP_*`

## Branch decision (first three knives)

```
10180 sync completes, but named lookup does not appear
→ chase gamelist pre-cfg consume (cfg.bin / gwyblink param / UI event)
→ do NOT fake userinfo or implement offline no-update yet
```

## Lane F: no fix implemented yet

Per E10A-3 rules, no contract patch was applied because the proven missing step is **before** cfg36 selection:

- gwyblink param is mapped at DSM start but never read by gamelist guest code
- `gwy/cfg.bin` never opened
- named services exist as static strings in gbrwcore.ext, not as live broker entries

Next proven fix candidate (E10A-3.1): trace why gamelist never enters cfg-open path after init — likely missing root bootstrap side effects or launch-param handoff into gamelist state machine.

## Artifacts

| Kind | Path |
|------|------|
| Verdict | `reports/stage_e10a3_gamelist_postselect_verdict.md` |
| postselect trace | `reports/e10a3_cfg36_postselect_trace.csv` |
| 10180 contract | `reports/e10a3_event_10180_contract.csv` |
| named service | `reports/e10a3_named_service_trace.csv` |
| service registry | `reports/e10a3_gbrwcore_service_registry.csv` |
| root vs continue | `reports/e10a3_root_vs_continue_context.csv` |
| wait state | `reports/e10a3_gamelist_wait_state.csv` |
| timer annotate | `out/e10a3/gamelist_timer_2e3089_annotated.txt` |
| log | `logs/e10a3_postselect_stdout.txt` |

## Runner

```powershell
.\RUN_E10A3_GAMELIST_POSTSELECT.ps1 -Mode postselect -Seconds 120
.\RUN_E10A3_GAMELIST_POSTSELECT.ps1 -Mode event10180 -Seconds 120
.\RUN_E10A3_GAMELIST_POSTSELECT.ps1 -Mode services -Seconds 120
```
