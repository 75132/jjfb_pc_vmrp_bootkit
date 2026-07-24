# Path-A Handler Timeline

- **run_id:** h2_B_20260725_011531
- **valid_dispatch:** 1
- **handler_entered:** 1 call_id=1
- **handler_returned:** 1 ret=0x2 insn=200000
- **entry+0/+4/+8/+C:** 0x5 / 0x2829E8 / 0x4 / 0x5F32313D
- **calls:** 8  **er_rw_stores:** 64  **gate_samples:** 4
- **seen:** 2E4066=0 2F68E4=1 2DADC4=0 2E4194=0
- **side_effects:** new_event=1 requeue=1 freed=0 callback=0 resource=0 disp=0
- **post_ticks:** 0
- **verdict:** `PATH_A_HANDLER_SCHEDULED_NEXT_EVENT`

## Gate samples

| stage | 15C | 15D | B71 | 134D | C76 | UI |
|---|---|---|---|---|---|---|
| before_2E4040_via_2E2520 | 1 | 0 | 0 | 0 | 0 | 0x0 |
| handler_enter | 1 | 0 | 0 | 0 | 0 | 0x0 |
| before_2E4040_via_2E2520 | 1 | 0 | 0 | 0 | 0 | 0x0 |
| handler_return | 1 | 0 | 0 | 0 | 0 | 0x0 |

## Calls (first 32)

- d=1 BL 0x2E405E -> 0x2F68E4 r0=0x0
- d=2 BL 0x2F68FA -> 0x308D98 r0=0x2829E8
- d=2 BL 0x2F6908 -> 0x30A0CC r0=0x2829E8
- d=3 BL 0x30A0F6 -> 0x2D99AC r0=0x7375
- d=4 BL 0x2D99E6 -> 0x304558 r0=0x10132
- d=5 BLX_Rm 0x304586 -> 0x280058 r0=0x10132
- d=6 BLX_Rm 0x2D9AD0 -> 0x94E34 r0=0x6BB324
- d=7 BL 0x30633A -> 0x2FEB94 r0=0x2B24FB
