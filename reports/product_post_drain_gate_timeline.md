# Post-Drain Gate Timeline

- **run_id:** ffp_event_20260724_200514_39822
- **er_rw:** 0x2B1854
- **enter_30CBBC:** 0
- **true_enter_2E2520:** 0
- **enter_2DC4D8:** 0
- **store_15D:** 0 actual_store_pc=0x0
- **store_B71:** 0 actual_store_pc=0x0
- **15D_writer_grade:** candidate_unproven
- **B71_writer_grade:** candidate_entered_no_dispatch
- **successor_status:** POST_DRAIN_SUCCESSOR_BLOCKED
- **disp_trace:** 0 calls=0 branches=0 reads=0

## Dispatch calls

| id | r0 | event_code | target | block_pred |
|----|----|------------|--------|------------|

## Discipline

- Observe-only: no writes to 15D/B71/UI_MODE.
- Thumb +2 within hook span is not a second CODE_ENTER.
- Writer proof requires actual_store_pc on the candidate chain.
