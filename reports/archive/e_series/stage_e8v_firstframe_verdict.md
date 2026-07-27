# Stage E8V-FirstFrame Verdict

**Verdict:** `E88CC_REQUIRES_UI_OBJECT`

**NOT product success.** E8V deep-traces `0x2E88CC` after E8U idle success -- no C9D/CF5 poke, no fake DRAW, no host framebuffer paint.

## What landed

E8U Case A path still reaches real idle success (`0x306740` -> `0x2E88CC`). Inside `0x2E88CC`:

1. **D14 gate passes** (`D14_s16=0` -> `pass_D14_gate` @ `0x2E88E6`)
2. Side path hits `0x305E78` (scheduler helper, **not** platform draw)
3. **Null object early-out** @ `0x2E8914`: `R9+0xF6C == 0` (so F6C+4/+8 unavailable)
4. Epilogue @ `0x2E898C` -- never reaches draw BLs `0x2F2854` / `0x305BFC` / `0x2EA058`
5. Case C FAST call `0x2E993C` returns cleanly (`stop_at_base`) with **no DRAW**

Tick2 snapshot: `e88cc_entries>=1 early_exit>=2 draw_cand=0 insn~37 F6C=0x0`.

## Cases

| Case | Verdict | Elapsed | IdleOK | E88CC | Class | DRAW | Frame | F6C+4 | F6C+8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A_e88cc_deep_trace | E88CC_REQUIRES_UI_OBJECT | 110.8s | True | True | REQUIRES_UI_OBJECT | False | False | 0x0 | 0x0 |
| B_ui_bp_watch | E88CC_REQUIRES_UI_OBJECT | 94.5s | True | True | REQUIRES_UI_OBJECT | False | False | 0x0 | 0x0 |
| C_downstream_2e993c | E88CC_REQUIRES_UI_OBJECT | 94.5s | True | True | REQUIRES_UI_OBJECT | False | False | 0x0 | 0x0 |

## Rules held

- Real C44 unlock + helper skip + C9D branch assist (no C9D/CF5 memory poke)
- No `r0=0` UI-init; no host framebuffer paint
- Bridge observe stubs for `_DrawText` / `DrawRect` / `_DispUpEx` armed (unused this run)

## Artifacts

- `reports/stage_e8v_firstframe_summary.jsonl`
- `logs/stage_e8v_*_stdout.txt`
- `logs/e8v_first_real_draw_stdout.txt`
- `out/e8v_tmp/e88cc_static_notes.md`
- `RUN_E8V_FIRSTFRAME.ps1`
- Runtime: `JJFB_E8V_MODE` / `JJFB_E8V_E88CC_TRACE` / `JJFB_E8V_CALL_2E993C` in `robotol_flag_writer_trace.c`

## Next gap (not claimed done)

- Acquire / construct live object at `R9+0xF6C` (writer of F6C, or UI path that fills +4/+8)
- Then re-enter `0x2E88CC` and watch for real `[JJFB_DRAW]` / screenshot
- Do **not** claim `FIRST_REAL_FRAME` until platform draw + saved frame
