# Path-A Handler Timeline

- **run_id:** task13_C2_211334
- **valid_dispatch:** 1
- **handler_entered:** 1 call_id=1
- **handler_returned:** 1 ret=0xC0 insn=0
- **entry+0/+4/+8/+C:** 0x5 / 0x6BBAB8 / 0x1F / 0x0
- **calls:** 0  **er_rw_stores:** 0  **gate_samples:** 5
- **seen:** 2E4066=1 2F68E4=1 2DADC4=1 2E4194=0
- **side_effects:** new_event=1 requeue=1 freed=0 callback=0 resource=1 disp=0
- **post_ticks:** 0
- **verdict:** `PATH_A_HANDLER_TRIGGERED_RESOURCE_FLOW`

## Gate samples

| stage | 15C | 15D | B71 | 134D | C76 | UI |
|---|---|---|---|---|---|---|
| before_2E4040_via_2E2520 | 1 | 1 | 0 | 0 | 0 | 0x0 |
| handler_enter | 1 | 1 | 0 | 0 | 0 | 0x0 |
| inside_2E4066 | 1 | 1 | 0 | 0 | 0 | 0x0 |
| bl_2DADC4 | 1 | 1 | 0 | 0 | 0 | 0x0 |
| handler_return | 1 | 1 | 1 | 0 | 0 | 0x14 |

## Calls (first 32)

