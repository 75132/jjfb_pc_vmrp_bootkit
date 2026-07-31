# Post-Drain Gate Timeline

- **run_id:** pah_B_20260724_234746
- **er_rw:** 0x2B1854
- **enter_30CBBC:** 0
- **true_enter_2E2520:** 2
- **enter_2DC4D8:** 0
- **store_15D:** 0 actual_store_pc=0x0
- **store_B71:** 0 actual_store_pc=0x0
- **15D_writer_grade:** candidate_unproven
- **B71_writer_grade:** candidate_entered_no_dispatch
- **successor_status:** POST_DRAIN_SUCCESSOR_BLOCKED
- **disp_trace:** 1 calls=2 branches=6 reads=20

## Dispatch calls

| id | r0 | event_code | target | block_pred |
|----|----|------------|--------|------------|
| 1 | 0x2A8374 | 0 | 0x2E4194 | BCS_index_out_of_range |
| 2 | 0x2A83C4 | 0 | 0x2E4194 | BCS_index_out_of_range |

## Discipline

- Observe-only: no writes to 15D/B71/UI_MODE.
- Thumb +2 within hook span is not a second CODE_ENTER.
- Writer proof requires actual_store_pc on the candidate chain.
