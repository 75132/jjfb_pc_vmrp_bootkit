# Stage E9H Splash Blit Verdict

- **Mode**: blit
- **Class**: `SPLASH_LOADINGBAR_DRAWN_NO_REWRITE`
- **Evidence**: OBSERVED
- **Product success**: **NO**
- **Rewrite used**: NO
- **HWND**: `screenshots/e9h_blit_hwnd.png` — `nonwhite_or_nonblack=23316` (GDI client capture)
- **Blit**: `loadingbar!201!29.bmp` pixels=`0x3920000` via `guiDrawBitmapSprite` at `(19,220)` 201×29

## Path (robotol `code_base=0x2D8DF4`)

1. FAST splash `0x2EF86C` + UI_MODE=0x45
2. Natural postmatch `loadingbar!201!29.bmp` → handle `0x2A83C4`
3. Skip bar bind `0x2EFA46` → `0x2EFA5C` (preserve regs; run y-calc)
4. Call site `0x2EFA9A` → blit entry `0x2EC6B8`
5. Guest xy was `(-100,-100)` after incomplete y-calc → **COORD_ASSIST** to `(19,220)` using splash literal `MOVS R5,#220` + center-x from real `mw`
6. Real MRP pixels drawn; HWND presented

## Gaps (next)

- `r4` still 0 at later gate — progress/textbar loop not product-complete
- COORD_ASSIST is layout recovery, not natural guest xy; prefer fixing y-calc inputs (bar/textbar binds) later
- `JJFB_FAST_SPLASH_R4_ASSIST` not required for first loadingbar frame

## Artifacts

- Log: `logs/e9h_blit_stdout.txt` / `logs/stage_e_product_robotol_stdout.txt`
- R4 CSV: `reports/e9h_r4_provenance_trace.csv`
- Seq CSV: `reports/e9h_splash_resource_sequence.csv`
- Runner: `RUN_E9H_SPLASH_BLIT_DEMO.ps1 -Mode blit`
