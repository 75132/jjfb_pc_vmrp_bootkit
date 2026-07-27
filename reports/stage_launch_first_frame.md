# Launch first frame (product gate)

## Verdict

**PASS.** Default product path (no `SDL_VIDEODRIVER=dummy`, no `JJFB_CHROME_SKIP_DRAW`) reaches real guest DrawFP blit of `loadingbar!201!29.bmp` and writes a nonempty SDL screenshot.

## Evidence

| Run | Result |
|-----|--------|
| `out/path_a_record_task13/F1_frame/` | `DRAW_FP_CALL_ENTER`×28, `DRAW_FP_DRAWN`×28, `JJFB_FIRST_REAL_FRAME_REACHED`, screenshot 230454 bytes |
| `JJFB_Launcher.bat` / `RUN_JJFB_LAUNCHER.ps1 -HoldSeconds 45` | `drawfp_first_drawn` + repeated `drawfp_call drawn` in `runtime_progress.jsonl` |

First blit (F1):

```
DRAW_FP_ARGS pixels=0x6BBBB4 dst=19,220 wh=201x29
DRAW_FP_DRAWN n=1 …
JJFB_FIRST_REAL_FRAME_REACHED … other=5627
screenshot: out/path_a_record_task13/F1_frame/launcher_first_frame.bmp
```

## What changed

1. **Chrome policy** ([`src/runtime/gwy_ext_obs.c`](src/runtime/gwy_ext_obs.c))  
   - Product default: do **not** skip `0x30630C` / `0x30A2FC` / early-ret `0x311FD4` (those killed DrawFP).  
   - Research leave-fast only: `JJFB_CHROME_SKIP_DRAW=1`.  
   - Keep: `2F449C` nop only under `2FC26C`; natural `2FC3BE` + default2 sibling MRP.

2. **DrawFP markers** ([`src/platform/platform_display.c`](src/platform/platform_display.c))  
   - Always log first `DRAW_FP_CALL_ENTER` / `DRAW_FP_DRAWN`; emit `drawfp_first_drawn` progress.

3. **Present → HWND** ([`third_party/vmrp_upstream/bridge.c`](third_party/vmrp_upstream/bridge.c))  
   - After sprite present, call `guiProductShowWindowIfReady(1)`.

4. **Launcher env** ([`src/product/jjfb_launcher_main.c`](src/product/jjfb_launcher_main.c))  
   - Clear `SDL_VIDEODRIVER` / `JJFB_CHROME_SKIP_DRAW` / HWND-until-DispUp.  
   - Pin DrawFP + MRP + `0x10134`.  
   - Screenshot path: `out/vmrp_run/screenshots/launcher_first_frame.bmp`.

## Acceptance bar (this gate)

Real guest blit (loadingbar class) on real SDL. **Not** required: `UI_MODE=0x45`, event15/E6C, B70, `leave_2FC26C`.

## Secondary gap (Task 13)

With frame path default, `LRT_2FC26C_LEAVE` may stay 0 (F1). Case 5 E6C / B70 / UI_MODE=0x45 remains the next lifecycle gap; use `JJFB_CHROME_SKIP_DRAW=1` only for leave-fast research, not product launch.
