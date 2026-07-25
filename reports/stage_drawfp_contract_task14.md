# Task 14 — Robotol DrawFP contract

## Verdict

**DrawFP product surface is shipped and linked.** Variant C keeps the record-first → B71 → C0 baseline. Empty-first still does **not** reach `LEAVE_2FC26C` / `DRAW_FP_CALL_*` because chrome stalls **before** wrapper `0x2EC6B8` (not primarily because `ER_RW+0x150C` is bad).

## Delivered product code

| Piece | Location |
|-------|----------|
| API | `include/gwy_launcher/platform_display.h` |
| Impl | `src/platform/platform_display.c` |
| Guest CB | `platform_guest_draw_bitmap` → always R0=1 |
| Slot publish | `platform_drawfp_cache_publish` → `*(ER_RW+0x150C)=*(mr_table+0x1E0)` |
| Bridge MAP | `third_party/vmrp_upstream/bridge.c` `0x1E0` → `br__DrawBitmap` |
| CMake | `platform_display.c` in `launcher_core` |
| A/B empty-first | `JJFB_PATH_A_EMPTY_FIRST=1` → `PRIMING_EMPTY` (not product default) |
| DrawFP A/B | `JJFB_DRAWFP_BINDING=0` skips 150C publish; default ON |

### Collateral fix needed to even approach DrawFP

`0x2FDD5C` (inside `2FC26C`) BLX's `*(ER_RW+0x1420)`. Live value was DSM memcpy `0x94E94`, short-circuited by nested-cfunction → hang.

Also published / bound:

- `ER_RW+0x1420` ← `mr_table+0xC` (memcpy trampoline `0x280010`)
- DSM body hook at `0x94E94` → `platform_guest_memcpy` (same model as strlen `@0xAC374`)

## A/B/C (out/path_a_record_task14)

| Variant | Env | Result |
|---------|-----|--------|
| A | `DRAWFP_BINDING=0` + `EMPTY_FIRST=1` | Enter `2FC26C`; 150C publish skipped; historic 2FDD5C hang when 1420=DSM |
| B | DrawFP ON + `EMPTY_FIRST=1` | `DRAW_FP_SLOT_PUBLISHED` (`150C=0x2801E4`); enter `2FC26C`; **no** `2EC6B8` / **no** `DRAW_FP_CALL_*` / **no** leave |
| C | record-first default + DrawFP ON | `B71_NATURALLY_WRITTEN`; `CALL_FAMILY_C0`; fault `2FEC3C` with `E6C=0` (unchanged Case 5) |

Evidence dirs: `out/path_a_record_task14/{A,B,C}/stdout.txt` (latest B/C = `*4_*` runs).

## Answers (Task 14 §十六)

1. **ER_RW+0x150C original:** already `0x2801E4` at first publish (`wrote=0`).
2. **Why 0/0x270F?** Not observed this round; slot already held mr_table trampoline.
3. **mr_table+0x1E0 empty?** No — trampoline `0x2801E4` present at `bridge_init`.
4. **Formal trampoline VA:** `0x2801E4` (`mt=0x280004 + 0x1E0`).
5. **Publish stage:** ER_RW bind / libc+drawfp cache publish; re-bound at `LRT_2FC26C` enter.
6. **2EC71A BLX to callback?** **No** — wrapper `0x2EC6B8` never entered.
7–9. ABI / args / pixels: **N/A** (no call).
10. **Leave `0x2FC3E6`?** **No.**
11–15. nested 1E201 / event15 / `2E5E60` / E6C natural: **No.**
16. **`2FEC3C`?** Variant C still fails (`E6C=0`).
17. **B70 / UI_MODE=0x45?** No.
18. **No V67 skip-BLX / FAST / hard E6C / hard 150C.** Yes.

## Next gap (not DrawFP slot)

After memcpy cache fix, empty-first chrome progresses past `2FDD5C` into `0x3072xx` / `0x2FE1xx` resource paths but never reaches `0x2EC6B8`. Next work: finish natural chrome/BMP path (or find next misbound FP), then observe DrawFP call + leave + event15.
