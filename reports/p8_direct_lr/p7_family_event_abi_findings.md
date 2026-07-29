# P7-0 Family Event ABI (current commit)

Captured on Layer-1 PASS run with `boot_successor` + family deliver hooks.

## Guest request vs handler enter (0x1E209 / app=9)

| Stage | r0 | r1 | r2 | r3 | notes |
|-------|----|----|----|----|-------|
| Guest `sendAppEvent` (timeline FAMILY_1E209) | 0x1E209 | 0x9 | **0** | **0** | guest payload empty |
| Deliver → `0x30D301` (family switch) | **0x9** | **0x1E209** | **0x6AD11C** | 0 | host fills r2 with context obj |
| Deliver → `0x30D2F9` (Path-A enqueue) | 0x6AD11C | 0x69EF14 | 0 | 0 | different ABI |
| Deliver → `0x305EB9` | 0 | 0 | 0 | 0 | ack/free-ish |

`0x6AD11C` also appears as `sp[0]` / Path-A related object on the stack dump
(`sp0=0x6AD11C`, `sp4=0x30D2F9`).

## Stack (case 9 → 0x30D301)

From `reports/p7_family_event_abi.csv` sample:

```text
sp+0  = 0x6AD11C   (same as delivered r2)
sp+4  = 0x30D2F9
sp+32 = 0x0 or event-ish
sp+36 = 0x2B2030 / 0x1E209 (varies by nest depth)
```

## Closed enough to state

1. Guest does **not** supply r2/r3 for `sendAppEvent(0x1E209,9)`.
2. Host deliver already synthesizes `r2=context(0x6AD11C)` for `0x30D301`.
3. **P7-1 not applied**: do not invent additional non-null pointers until case-9
   disassembly proves which of `sp+32/sp+36` are required vs leftover nest frames.

## Next proof (before mutating deliver)

Disassemble `0x30D301` case 9 and list exact loads of r2/r3/`[sp,#32]`/`[sp,#36]`.
Only then restore missing **generic** registered context / stack args.
