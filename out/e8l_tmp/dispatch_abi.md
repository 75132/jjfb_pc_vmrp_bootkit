# E8L 0x10102 / 0x30D300 dispatch ABI (static)

## Register / stack ABI

### R0
- role: switch_case_index
- first_use: CMP r0, #0x157; table index
- null_ok: False
- range: 0 .. 0x156

### R1
- role: primary_payload_or_event_code
- first_use: MOV r4, r1 (saved)
- case_forward: case arms do MOV r0, r4; BL target — so callee R0 = original R1
- case310_semantics: 0x2DFC3C saves R0→r4 then gates on [R9+0xE6C]+0x7C; both arms still forward r4 (original R1) into later calls — R1 is payload, not the null-check subject
- case156_semantics: 0x300158 saves R0 to r4 then later MOV r0,r4; in-module BL callers pass const #18/#20 — R1 should be event code int

### R2
- role: secondary_arg_saved_to_r5
- first_use: MOV r5, r2
- used_by_case_310_156: False
- used_by_other_cases: arms with MOV r1,r5 forward it

### R3
- role: tertiary_arg_moved_to_r1_then_bound_clobber
- first_use: MOV r1, r3; then r3 reused as 0x157 bound
- note: original R3 preserved only indirectly if needed before clobber; stack args preferred for extras

### stack_arg0
- role: arg4
- load: LDR r6, [sp,#0x20] after PUSH+SUB
- aapcs: first stack arg on entry

### stack_arg1
- role: arg5
- load: LDR r2, [sp,#0x24] after PUSH+SUB

## Prologue

- PUSH {r4-r6,lr}
- r4=R1; r1=R3; r5=R2; SUB sp,#16
- r3=0x157; CMP R0,r3; BCS default
- load stack arg4→r6, arg5→r2
- computed goto via halfword table

## Case 156 / 310

### case 156 (0x9C)
- arm `0x30DDF4` → `['MOV r0, r4', 'BL 0x300158', 'MOVS r0,#0', 'B epilogue']`
- target `0x300158` (parent_dispatcher_entry)
- R1 hyp: integer event code (18 or 20 from static parent callers)
- requires_pointer_r1=False

### case 310 (0x136)
- arm `0x30D72E` → `['MOV r0, r4', 'BL 0x2DFC3C', 'MOVS r0,#0', 'B epilogue']`
- target `0x2DFC3C` (hot_cluster)
- R1 hyp: context/event object forwarded as r4 into later BLs (may be int or ptr)
- requires_pointer_r1=unknown_until_live

## 0x2DFC3C null path

{
  "entry": 3013692,
  "saves_incoming_r0_to_r4": "0x2DFC40",
  "e6c_gate": {
    "load": "LDR r2,=#0xE6C; ADD r2,r9; LDR r0,[r2,#0x7C]",
    "branch": "0x2DFC48 CMP r0,#0; BEQ 0x2DFCAC",
    "effect": "R9+0xE6C object missing \u2192 alternate arm; both arms still use r4"
  },
  "e6c_present_path": {
    "path": "BL 0x2F5B38 (size 4), walk, BLX, BL 0x30E55C / 0x30C008 / 0x30F0D0",
    "uses_r4": "0x2DFC72 MOV r0,r4 before BLX"
  },
  "e6c_absent_path": {
    "entry": "0x2DFCAC",
    "uses_r4": "0x2DFCB0 MOV r1,r4 then BL"
  },
  "calls_parent": false,
  "note": "does not BL 0x300158; prep/side path; needs R9+E6C and meaningful R1"
}

## 0x300158 R0 handling

{
  "entry": 3146072,
  "saves_r0_to_r4": "0x30015C MOV r4,r0",
  "restores_r0_from_r4": "0x3001DC MOV r0,r4",
  "early_r0_clobber": "loads from R9+0x7D8 queue before using saved r4",
  "static_caller_r0": [
    18,
    20
  ]
}

## Recommended probes

- A: R0=156 R1=0 R2=0 R3=0 — ZERO payload baseline
- B: R0=156 R1=18 R2=0 R3=0 — event code #18 from parent census
- C: R0=156 R1=20 R2=0 R3=0 — event code #20 from parent census
- D310null: R0=310 R1=0 R2=0 R3=0 — confirm null path (E8K)

## Hypotheses

1. `MISSING_10102_APP_INIT_EVENT` — host never delivers into registered 0x30D301
1. `CASE_156_REQUIRES_PAYLOAD` — R1 must be event code (18/20); R0=case alone insufficient semantics
1. `CASE_310_REQUIRES_PAYLOAD` — R1 must be non-NULL pointer for 2DFC3C main path
1. `CASE_156_REACHED_NEXT_GAP` — if R1=18/20 enters parent/dispatcher — next is state=38 / idle flags

BP: `e:0x30D300,e:0x30DDF4,e:0x30D72E,e:0x30D730,e:0x2DFC3C,e:0x2DFCAC,p:0x300158,p:0x3002C0,p:0x300714,p:0x30103C,p:0x3020C8,q:0x2DC80C`

