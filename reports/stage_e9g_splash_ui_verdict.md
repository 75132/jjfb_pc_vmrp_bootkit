# Stage E9G Splash/Loading UI Verdict

**Verdict:** `SPLASH_2EF86C_REACHED_NEXT_GAP`

**NOT product success.**

## Confirmed (observed)

- `UI_MODE` at **R9+0x8D0**; splash value **0x45**.
- Natural dispatch: `0x306344` cmp → `0x30662C` bl → **`0x2EF86C`**.
- Natural writer PC: **`0x2FC418`** (not hit under DisplayFirst; assist poke used for FAST call).
- `JJFB_FAST_SPLASH_CALL` → real `0x2EF86C` with ABI `r0=0x45 r1=0x13`.
- **No request rewrite** (`JJFB_E9F_REWRITE_REQUEST` off).
- Splash path **naturally requested** `loadingbar!201!29.bmp` (LR=`0x2EFA3B`).
- Natural index match entry #4 + postmatch: handle=`0x2A83C4` pixels=`0x3920000` 201×29.
- Timer tick with `UI_MODE=0x45` also re-enters splash via `0x30662C` (natural gate).
- Slogo: `SLOGO_ABSENT_FROM_GUEST_INDEX_CONFIRMED` / `SLOGO_RAW_INVENTORY_ONLY`.
- Per-bmp draw dim meta (`jjfb_bmp_meta`) replaces sole reliance on global `LAST_WH`.

## Next gap

Splash binds loadingbar then progress loop exits early (`r4==0` @ `0x2EFAF2`, max PC ~`0x2EFAF4`).
Sibling binds (`bar!` / `textbar`) are slow under host tracing; sticky skip to `0x2EFA9E` avoids stall but does **not** reach blit.
Blind mid-fn jump to `0x2F45A2` faults (`UC_MEM_READ_UNMAPPED @0`).
**Need:** real splash blit path (e.g. `0x2EC6B0` / post-`r4` tail) with correct ABI so `mr_drawBitmap` + HWND complete — without rewrite.

## Auto mode

`PRODUCT_STILL_NEEDS_STATE_MACHINE_NATURALIZATION` — without `FAST_SPLASH_CALL`, DisplayFirst still only requests `wy_jiao1`.

## Artifacts

- `reports/stage_e9g_splash_ui_verdict.md`
- `reports/e9g_game_ui_request_trace.csv`
- `reports/e9g_ui_mode_trace.csv`
- `reports/e9g_splash_ui_flow.json`
- `logs/e9g_splash_ui_stdout.txt`
- `RUN_E9G_SPLASH_UI_DEMO.ps1` (`-Mode auto|splash|debug`)
