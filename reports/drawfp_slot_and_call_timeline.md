# DrawFP slot and call timeline (Task 14)

## Slot publication timeline (Variant B / C)

```
bridge_init
  DRAW_FP_TABLE_SLOT mt=0x280004 off=0x1E0 trampoline=0x2801E4
  DRAW_FP_SURFACE bind 240x320 (local fb if SDL rgb565 null)

robotol ER_RW ready (0x2B1858)
  PLATFORM_LIBC_CACHE
    memcpy 0x94E94 -> 0x280010 (ER_RW+0x1420)
    memset  0x94F04 -> 0x28003C (ER_RW+0x144C)
    strlen  kept/bound via mr_table+0x3C -> 0x280040 (ER_RW+0x1450)
  DRAW_FP_SLOT_PUBLISHED
    ER_RW+0x150C: old=0x2801E4 new=0x2801E4 wrote=0
```

## Call timeline (expected vs observed)

| PC / marker | Role | Observed (empty-first B) |
|-------------|------|---------------------------|
| `0x2FC26C` | chrome alt enter | **yes** `LRT_2FC26C_ALT` |
| `0x2FDD5C` | BLX `*(ER_RW+0x1420)` memcpy | after fix: **no hang** (was DSM `0x94E94`) |
| `0x2EC6B8` | DrawBitmap wrapper | **never** |
| `0x2EC714` / `0x2EC71A` | load/BLX `*(ER_RW+0x150C)` | **never** |
| `DRAW_FP_CALL_ENTER` | `platform_guest_draw_bitmap` | **0** |
| `DRAW_FP_CALL_RETURN` | R0=1 return | **0** |
| `0x2FC3E6` | leave / pop | **never** `LRT_2FC26C_LEAVE` |

## Variant A (binding OFF)

`DRAW_FP_ERW_SLOT_WRITE skip binding_off` — publish intentionally skipped for baseline compare. Hang root on that path was historically `2FDD5C` + bad/DSM `+0x1420`, not a missing `+0x150C` alone.

## Implication

Publishing `+0x150C` is necessary but **not sufficient** this round: chrome does not reach the DrawFP wrapper yet. Slot identity `0x2801E4` is already correct when publish runs.
