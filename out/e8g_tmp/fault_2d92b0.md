# E8G counterfactual fault site 0x2D92B0 (refined)

## Live (COUNTERFACTUAL_ONLY ALL)

- tick2 after C44=C9D=CF5=1: `UC_ERR_EXCEPTION` pc_after=`0x2D92B0` r0=`3` lr=`0x2D91E5`
- No DRAW

## Static decode of containing stub `0x2D92A4`

```
0x2D92A4: PUSH {r3, lr}
0x2D92A6: ADD  r3, sp, #0
0x2D92A8: STRB r0, [r3]
0x2D92AA: MOVS r0, #3
0x2D92AC: MOV  r1, sp
0x2D92AE: SVC  #0xAB          ; Thumb DFAB — software interrupt
0x2D92B0: ADD  SP, #4         ; pc_after lands here / next after SVC
0x2D92B2: POP  {r3}
0x2D92B4: BX   r3
```

## Classification (TARGET_OBSERVED + HYPOTHESIS)

| Field | Value |
|-------|-------|
| cause class | **explicit trap / unimplemented SVC** |
| svc imm | `0xAB` (171) |
| r0 at call | `3` (syscall/service selector HYPOTHESIS) |
| r1 | SP (arg block pointer HYPOTHESIS) |
| Unicorn | does not implement this SVC → `UC_ERR_EXCEPTION` |
| meaning | After idle flags are forced, lifecycle takes a path that **requires a host/platform SVC handler**, not merely three flag bytes |

Upstream BL callers of stub fn: `0x2D91E0`, `0x2D91EE`, `0x2D9202`, `0x2D920E` (live lr=`0x2D91E5`).

## Product implication

Do **not** force flags. Second gate is **missing SVC/platform implementation or prior context that avoids this SVC**, not “write C44/C9D/CF5 harder”.
