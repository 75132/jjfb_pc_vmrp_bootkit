# Stage E8W-FirstFrame Verdict

**Verdict:** `E88CC_REACHED_2F2854_NEXT_GAP`

**NOT product success.** Structural F74 assist opened the `0x2E88CC` gate and reached the first real draw-candidate BL. No platform `[JJFB_DRAW]` / screenshot yet.

## Layout correction (vs E8V)

`R9+0xF6C` is an **embedded struct base**, not a heap object pointer:

| Word | Addr | Gate role |
| --- | --- | --- |
| w0 | `R9+0xF6C` | list/handle; gate does not require nonzero |
| F70 | `R9+0xF70` | checked only if F74==0 |
| F74 | `R9+0xF74` | if nonzero, skip F70 check and continue |

- Early exit `0x2E8914` = BEQ when **F70==0** after F74 was already 0
- First draw BL: `0x2F2854` @ `0x2E8980` / `0x2E89A8` (before F74[] loop)
- Later: `0x305BFC` / `0x2EA058` index `F74[i]`
- Scroll sentinel `R9+0x200 == 0x3E7` forces epilogue @ `0x2E8994`

## What landed (Case C)

1. Idle success still reaches `0x306740 → 0x2E88CC` (F70=F74=0)
2. `JJFB_FAST_F6C_OBJECT_ASSIST` maps scratch `@0x3900000`, sets `F74=0x3900004`
3. `JJFB_E8W_GATE_RETRY` from `0x2E8914` → `0x2E8902`
4. Hit **`0x2F2854`** (`lr=0x2E89AD`, `r0=0 r1=0 r2=0 r3=0x20`)
5. **No** `mr_drawBitmap` / `[JJFB_DRAW]` / screenshot yet

## Writers (static, corrected scan)

Nonzero producers of interest:

- `0x2D9CFC` → F6C_w0 via `BL 0x312AA4` (10 callers)
- `0x2FBD18` → F70 via `0x2FBB6C` (caller `0x2E5DBE`)
- `0x2E8920` → F74 via `BL 0x2F99D0` (self, after gate)
- `0x30A9EC` clears F6C/F70/F74 (destructor)

**Cases A/B:** no natural writer reached on DisplayFirst path → `F6C_WRITER_NEVER_REACHED`.

## Cases

| Case | Verdict | Elapsed | IdleOK | Writer | Assist | 2F2854 | DRAW |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A_real_writer_watch | F6C_WRITER_NEVER_REACHED | 59.5s | True | False | False | False | False |
| B_upstream_ui_object | F6C_WRITER_NEVER_REACHED | 40.5s | True | False | False | False | False |
| C_minimal_f6c_assist | E88CC_REACHED_2F2854_NEXT_GAP | 41.1s | True | False | True | True | False |

## Artifacts

- `reports/stage_e8w_firstframe_summary.jsonl`
- `logs/stage_e8w_*_stdout.txt`
- `logs/e8w_first_real_draw_stdout.txt`
- `out/e8w_tmp/f6c_object_notes.md` / `f6c_writers_fixed.json`
- `RUN_E8W_FIRSTFRAME.ps1`
- Runtime: `JJFB_E8W_MODE` / `JJFB_FAST_F6C_OBJECT_ASSIST` / gate-retry in `robotol_flag_writer_trace.c`

## Next gap (not claimed done)

- Trace into `0x2F2854` for platform graphics / resource decode
- Prefer real F74 table from writer `0x2F99D0` / `0x2D9CFC` over scratch stubs
- Do **not** claim `FIRST_REAL_FRAME` until `[JJFB_DRAW]` + nontrivial screenshot
