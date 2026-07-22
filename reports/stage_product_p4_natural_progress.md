# Stage Product P4 Natural Progress

- **run_id:** p4_validate_20260723_023558_96760
- **mode:** validate (after Round-1 progress_map)
- **verdict:** PLATFORM_COMPLETION_RESTORED_NO_DRAW_YET
- **runtime:** Gwy+stubs
- **seconds:** 60
- **process_exit:** killed
- **ok_callback_returns:** 59
- **SCHEDULER_NATURAL_CALLBACK:** yes
- **ROBOTOL_INIT_RETURN_ZERO:** yes
- **ROBOTOL_STABLE_WAIT_PREDICATE_FOUND:** yes (Round-1 progress_map)
- **PROVEN_PLATFORM_CONTRACT_FIXED:** yes
- **source_class (Round-1):** PLATFORM_EVENT_PENDING
- **callback_class:** CALLBACK_STABLE_POLL_LOOP
- **ledger_verdict (Round-1):** EVENT_REGISTERED_NOT_GENERATED
- **ledger_verdict (post-fix):** WORK_SOURCE_NOT_THE_BLOCKER
- **contract_verdict (Round-1):** PLATFORM_ASYNC_COMPLETION_MISSING
- **display_verdict:** DISPLAY_PATH_NOT_REACHED
- **same_path_same_state:** yes
- **early_change_then_stall:** no
- **FIRST_NATURAL_DRAW:** no
- **FIRST_NATURAL_REFRESH:** no
- **FRAMEBUFFER_NONEMPTY:** no
- **HWND_VISIBLE:** no
- **frame_captured:** no
- **forbidden_hits:** none
- **manifest:** reports/product_p4_manifest_p4_validate_20260723_023558_96760.txt
- **evidence_png:** none

## Required question

All captured natural 10140 callbacks execute the same control-flow path with the same state digest (stable poll). State does not progress toward draw until a separate registered family completion is delivered.

## Round 1 diagnosis (progress_map)

| Item | Result |
|------|--------|
| Callback class | CALLBACK_STABLE_POLL_LOOP |
| Stall predicate | ROBOTOL_STABLE_WAIT_PREDICATE_FOUND |
| Source class | PLATFORM_EVENT_PENDING |
| Ledger | EVENT_REGISTERED_NOT_GENERATED |
| Contract | PLATFORM_ASYNC_COMPLETION_MISSING |
| Display | DISPLAY_PATH_BLOCKED_BY_EVENT_STATE |

Proven gap: guest registered `0x10102` family `0x1E200` → handler `0x30D301` and `0x10165` enqueue handler `0x30D2F9`, then during the timer poller posted `sendAppEvent(0x1E209, 0x9)`. Platform returned STATUS/0 without delivering the registered family handler.

## Round 2 fix (generic platform contract)

Owner-scoped family-event dispatch:

1. Match `sendAppEvent` codes in a registered `0x10102` family band.
2. Enqueue `PLATFORM_EVENT` to the registered family handler (actual registry args).
3. Drain/deliver after the natural 10140 tick with ABI `(r0=app/subcode, r1=event_code)`.
4. Marker: `[PROVEN_PLATFORM_CONTRACT_FIXED] contract=family_event_dispatch via=10102_family`.

Observed delivery: `[PLATFORM_FAMILY_EVENT] op=DELIVER_DONE ok=1 ret=0 end=stop_at_base handler=0x30D301`.

## Exact marker gates

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | yes |
| ROBOTOL_INIT_RETURN_ZERO | yes |
| ROBOTOL_STABLE_WAIT_PREDICATE_FOUND | yes (R1) |
| PROVEN_PLATFORM_CONTRACT_FIXED | yes |
| FIRST_NATURAL_DRAW | no |
| FIRST_NATURAL_REFRESH api=_DispUpEx | no |
| FRAMEBUFFER_NONEMPTY | no |
| HWND_VISIBLE | no |
| FIRST_NATURAL_FRAME_CAPTURED | no |

## Artifacts

- reports/product_p4_callback_progress.csv
- reports/product_p4_callback_signatures.csv
- reports/product_p4_stall_predicates.csv
- reports/product_p4_work_source_ledger.csv
- reports/product_p4_display_reachability.csv
- reports/product_p4_platform_contracts.csv
- reports/product_p4_stall_verdict.md
- reports/product_p4_visual_trace.csv
- logs/product_p4_stdout.txt
- logs/product_p4_stderr.txt

## Notes

- BIND_REFRESH is not counted as DRAW/REFRESH
- No E8E drain / synthetic 10165 probes in product P4
- No fixed Robotol PC/ER_RW writes; dispatch uses live registry handlers
- Valid intermediate: PLATFORM_COMPLETION_RESTORED_NO_DRAW_YET
- Strong success still open: PRODUCT_FIRST_NATURAL_FRAME_STABLE (guest `_DispUpEx` not yet reached)
