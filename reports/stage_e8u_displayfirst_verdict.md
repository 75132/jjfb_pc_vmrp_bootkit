# Stage E8U-DisplayFirst Verdict

**Verdict:** `C9D_GATE_BYPASSED_NEXT_GAP`

**NOT product success.** DISPLAY_FIRST is runtime branch assist only — no C9D/CF5 memory poke, no fake DRAW, no host framebuffer paint.

## What landed

Real C44 unlock (`0x2FC8C0`) + runtime assists reached the **real idle exit path**:

1. Skip C44 post-helper `BL 0x2E87B4` @ `0x3066B8` (avoids unimplemented SVC #0xAB fault)
2. Bypass C9D BNE @ `0x3066C6` → continue `0x3066C8`
3. Case A: with `CD1==0` and `11B0==0`, **no CF5 assist needed** — natural fall-through to success
4. Hit real success sites `0x306740` → `0x2E88CC` (`[JJFB_E8U_IDLE_SUCCESS]`)

**No `mr_drawBitmap` / screenshot yet.** Next gap is inside/after `0x2E88CC` (resource / UI object / further platform), not C9D.

## Lane E — SVC_AB audit (E8T discrepancy)

- E8T `A_unlock_c9d_watch` `hit_SVC_AB=true` was **real**, not a stale tag.
- Evidence: `[JJFB_E8H_SVC_AB]` + `[JJFB_FAST_SVC_AB]` with `mode=observe` then `SVC_AB_STOP`.
- Main report deprioritized SVC because it sat on the FAST fire / C44-helper path, not as a dedicated “find C9D writer” task.
- Root interaction for DisplayFirst: after `C44=1`, idle always `BL 0x2E87B4` before C9D; that helper hits SVC #0xAB. `return0` then faults (`WRITE_UNMAPPED@0xFFFFFFF0`). So C9D was unreachable until the helper was skipped under DISPLAY_FIRST.
- E8U Case A uses helper-skip + C9D branch assist; SVC no longer blocks idle exit.

## Cases

| Case | Verdict | Elapsed | C44 | C9D-assist | IdleOK | DRAW | Frame | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A_c44_bypass_c9d | C9D_GATE_BYPASSED_NEXT_GAP | 94.5s | yes | yes | yes | no | no | helper-skip + C9D only; CF5 assist off |
| B_bypass_plus_ui_upstream | C9D_GATE_BYPASSED_NEXT_GAP | 94.5s | yes | yes | yes | no | no | + CF5 assist + UI BP hits |
| C_state20_ui_init_bypass | UI_INIT_REACHED_2E4840_NEXT_GAP | 94.3s | yes | no | no | no | no | UI-init skipped (no object); BP hit 2E4840 |

## Artifacts

- `reports/stage_e8u_displayfirst_summary.jsonl`
- `logs/stage_e8u_*_stdout.txt`
- `logs/e8u_first_real_draw_stdout.txt`
- `RUN_E8U_DISPLAYFIRST.ps1`
- Runtime: `JJFB_DISPLAY_FIRST` / `JJFB_BYPASS_C9D_GATE` in `robotol_flag_writer_trace.c`
- Screenshot hook ready in `third_party/vmrp_upstream/main.c` (`JJFB_E8U_FIRST_REAL_FRAME`) — unused until real draw

## Next (not claimed done)

- Trace `0x2E88CC` for first real draw/refresh/resource call
- Acquire live UI object for `0x2E4788` (refuse `r0=0`)
- Do not claim FIRST_REAL_FRAME until `[JJFB_DRAW]` + saved screenshot
