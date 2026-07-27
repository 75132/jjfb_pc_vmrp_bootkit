# Platform 0x10138 Timeline (Task 6)

**Run B:** `p10138_B_20260725_001704`  
**Contracts:** `JJFB_PATH_A_EVENT_CONTRACT=1`, `JJFB_PLATFORM_10138_CONTRACT=1`  
**Traces:** PAH + `JJFB_PLATFORM_10138_TRACE=1`

## Milestones

| t (approx) | Event |
|---|---|
| T0 | `runtime_spawned` |
| +30s | `guest_entry_called` / `waiting_for_first_frame` |
| +34s | `platform_10138_entered` → `platform_10138_completed` (heap) |
| +34s | `[PLATFORM_10138] mode=heap site_lr=0x2D9A73 free/out5=0x200000 erw=… gates=1 ret=0` |
| +34s | `[PLATFORM_ALLOC] 0x10132 size=0x8 → block` (then size=0x10…) |
| +36s | `event_path_a_seen` / list push |
| +39s | Call1 `word0=5` → `path_a_valid_dispatch` → `path_a_handler_entered` `0x2E4040` |
| +39s | `BL 0x2F68E4` |
| +56s | `nested_path_a_published` (`push_312A60`) |
| hold end | PAH insn budget; **no** `path_a_helper_returned` / `0x2E4066` / `0x2DADC4` |

## ABI proof snapshot

```text
site_lr ≈ 0x2D9A73 (heap)
R0 ret = 0                    # MR_SUCCESS (correct)
*out5 = 0x200000              # free heap
ER_RW+ED8 = 0, gates 7DC/7DD… = 1
next: sendAppEvent(0x10132, size=8) → guest block base
caller: user_ptr = block+4
```

## Handler enter (unchanged shape)

```text
PC=0x2E4040 LR=0x2DC8D9
R4=entry  +0=5  +4=inner  +8=4
BL 0x2F68E4
```

## Nested Path-A

- `PAH_QUEUE op=PUSH pc=0x312A60 during=1`
- Progress: `nested_path_a_published`
- No `0x312A78` R4=0 fault in this run

## Variant A contrast

`JJFB_PLATFORM_10138_CONTRACT=0`: no MULTI_OUT writes; early `0x94E40` identity still appears; Path-A dispatch can still reach `0x2E4040` via alternate alloc paths once `0x10132` size-malloc exists — proves multi-out is the documented heap query, not “ret must be nonzero”.

## Open gap

```text
0x2F68E4  (still active past 200k dense-hook insns)
↛ 0x2E4066
↛ 0x2DADC4
```

Next work: map remaining helper stream/platform needs **without** forcing PC or fabricating lifecycle flags.
