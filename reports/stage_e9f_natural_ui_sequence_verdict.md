# Stage E9F Natural UI Sequence Verdict

**Verdict:** `NATURAL_POSTMATCH_LOADINGBAR_FRAME`

**NOT product success.**

## Confirmed approach

- Reuses E9E natural index match + postmatch (original `jjfb.mrp` bytes).
- No `REAL_MRP_MEMBER_BRIDGE` success path.
- Logo/loading modes may use `JJFB_E9F_REWRITE_REQUEST` to redirect DisplayFirst `wy_jiao1` to preferred UI member (still natural strcmp + postmatch).
- Draw dim fix: guest sometimes passes square `w×w`; host overrides from `JJFB_E9E_LAST_WH` (postmatch member size) via `[JJFB_E9F_DRAW_DIM_FIX]`.

## Result (logo mode — accepted)

- mode=logo prefer=`slogo!157!58.bmp` → fallback=`loadingbar!201!29.bmp` (rewrite)
- class=`SLOGO_ABSENT_FROM_GUEST_INDEX` (MRP has slogo; guest `0x304BF0` index does not)
- natural match entry **#4**, postmatch OK: offset=3782, decoded=11658, 201×29, handle=`0x2A83C4`, pixels=`0x3920000`
- `mr_drawBitmap` + HWND capture; `FIRST_REAL_FRAME_REACHED`; hwnd nonwhite observed
- nearest_blocker: slogo absent from guest index

## Auto mode (observed)

- verdict=`GAME_REQUEST_TRACE_FOUND_NEXT_GAP`
- DisplayFirst / C9D / F6C/F74 assist path only requests `wy_jiao1!11!11.bmp`
- Splash chrome (`slogo` / `loadingbar` / `textbar` / `top`) gated by UI_MODE `0x45` / `0x2EF86C` — not entered under current assists

## Artifacts

- `screenshots/e9f_logo_natural.png` / `e9f_loading_sequence.png` / `e9f_actual_window_capture.png`
- `reports/e9f_natural_resource_request_trace.csv`
- `reports/e9f_resource_results.jsonl`
- `reports/e9f_natural_resource_flow.json`
- `logs/e9f_natural_ui_sequence_stdout.txt`
