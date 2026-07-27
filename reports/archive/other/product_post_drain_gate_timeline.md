# Post-Drain Gate Timeline

- **run_id:** t16_B8_20260727_203813
- **er_rw:** 0x2B1858
- **enter_30CBBC:** 1
- **true_enter_2E2520:** 1
- **enter_2DC4D8:** 0
- **store_15D:** 1 actual_store_pc=0x30CCF4
- **store_B71:** 1 actual_store_pc=0x2FE854
- **15D_writer_grade:** proven_natural_writer
- **B71_writer_grade:** store_from_unrelated_pc
- **successor_status:** POST_DRAIN_SUCCESSOR_BLOCKED
- **disp_trace:** 0 calls=0 branches=0 reads=0

## Dispatch calls

| id | r0 | event_code | target | block_pred |
|----|----|------------|--------|------------|

## Discipline

- Observe-only: no writes to 15D/B71/UI_MODE.
- Thumb +2 within hook span is not a second CODE_ENTER.
- Writer proof requires actual_store_pc on the candidate chain.
