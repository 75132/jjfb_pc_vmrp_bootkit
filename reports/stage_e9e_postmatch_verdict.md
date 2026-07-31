# Stage E9E Post-Match Verdict

**Verdict:** `NATURAL_POSTMATCH_FIRST_FRAME`

**NOT product success.**

## Confirmed

- Natural name match at entry #20 (`wy_jiao1!11!11.bmp`) via E9D host strcmp shim at `0x304F92`.
- No `JJFB_REAL_MRP_MEMBER_BRIDGE] hit=` (lookup remained natural).
- Post-match host shim delivered **original** `jjfb.mrp` member bytes:
  - offset=8414 stored=90 decoded=242 w=11 h=11
  - sha256=`edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13`
- `0x304BF0` returned `r0=0` with entry ABI restored (SP / R4–R11 / R9).
- `0x2D92E4` returned handle `0x2A83C4`.
- `0x310BBC` → `mr_drawBitmap` with `bmp=0x3920000` (original pixels).
- HWND capture nonwhite_or_nonblack=440.

## Blockers fixed in E9E

| Class | Fix |
|---|---|
| `POST_MATCH_READ_HELPER_SLOW` | Host decode of matched member after natural strcmp |
| `POST_MATCH_RETURN_ABI_WRONG` | Restore entry SP/R4–R11/R9 before returning to `0x2D93D0` |
| DSM return side-stack full after skipped BLX | `cancel_dsm_helper_blx` + `clear_dsm_return_side_stack` on postmatch |
| Early kill before HWND | E9E+E9B stop waits for `VISIBLE_WINDOW_HOLD_DONE` |

## Flow

```text
0x304BF0 natural index scan
  → 0x304F92 host_strcmp=0 (entry #20)
  → E9E postmatch: original member bytes → caller object
  → return r0=0 with entry ABI
0x2D92E4 returns handle
  → A64 table fill / post-A64 draw skip
  → 0x310BBC → mr_drawBitmap → HWND present
```

## Result

- natural_match=True postmatch=True bridge=False
- ret_2d92e4=True first_frame=True hwnd_nw=440
- member=`wy_jiao1!11!11.bmp`

## Artifacts

- `screenshots/e9e_natural_member_first_frame.png`
- `screenshots/e9e_actual_window_capture.png`
- `logs/e9e_natural_member_stdout.txt`
- `reports/e9e_natural_resource_flow.json`
- `reports/e9e_postmatch_helper_trace.csv`

## Next (optional, not required for E9E stop)

Repeat natural post-match for `slogo!157!58.bmp` / `loadingbar!201!29.bmp` / `textbar!120!30.bmp` / `top!76!28.bmp` toward `NATURAL_POSTMATCH_LOGO_FRAME`.
