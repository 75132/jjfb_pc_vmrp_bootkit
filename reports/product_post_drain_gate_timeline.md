# Post-Drain Gate Timeline

- **run_id:** p12_r2_20260730_010830
- **er_rw:** 0x2B1858
- **enter_30CBBC:** 1
- **true_enter_2E2520:** 2
- **enter_2DC4D8:** 0
- **store_15D:** 1 actual_store_pc=0x30CCF4
- **store_B71:** 1 actual_store_pc=0x305EE4
- **15D_writer_grade:** proven_natural_writer
- **B71_writer_grade:** store_from_unrelated_pc
- **successor_status:** POST_DRAIN_SUCCESSOR_REACHED
- **disp_trace:** 1 calls=2 branches=4 reads=20

## Dispatch calls

| id | r0 | event_code | target | block_pred |
|----|----|------------|--------|------------|
| 1 | 0x6BBAC8 | 5 | 0x2E4040 | switch_case_not_MR_MOUSE_UP |
| 2 | 0x6C1118 | 5 | 0x2E4040 | switch_case_not_MR_MOUSE_UP |

## Discipline

- Observe-only: no writes to 15D/B71/UI_MODE.
- Thumb +2 within hook span is not a second CODE_ENTER.
- Writer proof requires actual_store_pc on the candidate chain.
