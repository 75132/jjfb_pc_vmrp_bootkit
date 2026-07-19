# E8E callsite ABI samples
targets=0x30D24D,0x30D2F9 (even: 0x30D24C, 0x30D2F8)
direct_BL_count=1
literal_pool_refs=0

## Finding

Only the thin trampoline at 0x30D2F8 BL→0x30D24C exists inside robotol.
Enqueue is reached via platform code 0x10165 → registered handler 0x30D2F9,
not via additional internal BL sites. ABI must come from prologue + long path.

## call 0x30D2FA
- interesting_imms: [0]
- immediates: [0]
  - 0x30D2CA: ADDS r3, r7, #0  ; mov
  - 0x30D2CC: ADD r2, r15
  - 0x30D2D6: LDR r1, [sp, #0xC]
  - 0x30D2DC: MOVS r0, #0
  - 0x30D2F0: STR r0, [r4, #0x74]
  - 0x30D2F4: STR r4, [r1, #0x74]

## Literal pool hits for handler VAs

## Trampoline ABI (passthrough)
- 0x30D2F8: PUSH {r3,lr}; BL 0x30D24C; POP {r3,pc}
- R0/R1/R2 preserved into core; R3 scratch saved.