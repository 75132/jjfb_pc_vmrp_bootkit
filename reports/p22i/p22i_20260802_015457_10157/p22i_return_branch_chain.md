# run_id=p22i_20260802_015457_10157
# P22I return branch chain (dynamic returns + static consumer at LR)

Natural sequence evidence: saw_m6=1 saw_m0=1 saw_m1=1 saw_m8=0

Callsite (ARM): `0x89BEC BLX r12` → LR=`0x89BF0`

Static pre-call setup (same for 6/0/1):
```
0x89BD8  LDR r12,[r7,#0x70]!     ; R12 ← helper pointer field
0x89BDC  LDR r0,[r7,#4]
0x89BE0  MOV r3,r6
0x89BE4  MOV r2,r5
0x89BE8  MOV r1,r8               ; method from R8 (6 then 0 then 1)
0x89BEC  BLX r12
```

Static post-return consumer at continuation `0x89BF0`:
```
0x89BF0  LDR r1,[sp,#0x64]
0x89BF4  MOV r5,r0               ; first consumer of return R0 → R5
0x89BF8  CMP r1,#0               ; branch predicate on stacked field, not R0
0x89BFC  LDRNE r2,[sp,#0x68]
...
```

## method 6 return

- call_id=1 return_r0=0 return_pc=0x89BF0
- first_consumer=`MOV r5,r0` @0x89BF4
- CMP @0x89BF8 on r1 (stack), not return code
- next method produced via R8→R1 before next BLX

## method 0 return

- call_id=2 return_r0=0 return_pc=0x89BF0
- same consumer site @0x89BF4

## method 1 return

- call_id=3 return_r0=0 return_pc=0x89BF0
- same consumer site @0x89BF4
- no method8; post-init watch: no +0xF670/+0x10740/+0x7B6C

## chain summary

```
method 6 return 0 → MOV r5,r0; CMP [sp,#0x64],#0 → method 0
method 0 return 0 → same consumer → method 1
method 1 return 0 → post-init (no callback / no 10740 this run)
```

method8: METHOD8_REQUIREMENT_UNPROVEN (no natural call; hist 6→8→0 SUPERSEDED_BY_NATURAL_601)
