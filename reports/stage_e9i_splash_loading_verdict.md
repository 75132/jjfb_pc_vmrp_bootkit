# Stage E9I Splash Loading UI Verdict

- **Mode**: natural
- **Class**: `SPLASH_LOADINGBAR_NATURAL_COORD_VISIBLE`
- **Evidence**: OBSERVED
- **Product success**: NO
- **Rewrite used**: NO
- **COORD_ASSIST used**: NO
- **Bridge success path**: NO

## Observed (natural run)

| Item | Result |
|------|--------|
| Screen dim seed | `R9+0x830=240`, `R9+0x834=320` |
| Guest xy at blit | **(19, 220)** natural — not assisted |
| loadingbar | natural postmatch `0x2A83C4` / pixels `0x3920000` / drawn via `guiDrawBitmapSprite` |
| bar!16!18.bmp | sibling resolve handle `0x3910080` → slot `R9+0xBA0+0x20` |
| textbar!120!30.bmp | sibling resolve handle `0x39100C0` → slot `R9+0xBA0+0x24` |
| top! / target! | not requested on this splash path |
| HWND | `screenshots/e9i_actual_window_capture.png` `nonwhite_or_nonblack=23316` |
| Hold | 20s message pump |

## Task 1 — Natural coordinates (decoded)

- `0x2F9970` → `*(R9+0x830)` = screen **W**
- `0x2F9964` → `*(R9+0x834)` = **Ybase**
- `x = (W - bmp_w) / 2` → **R6** (`ASRS R6, R0, #1`)
- `y = Ybase - 100` → **R5** (`SUBS R5, #0x64`)
- E9H `(-100,-100)` = unseeded W=0 / Ybase=0
- E9I replaces COORD_ASSIST by seeding those fields before splash
- Path: `0x2EFA46` bar → `0x2EFA5C` y-calc → `0x2EFA9A` callsite → `0x2EC6B8` blit  
  (`0x2EFA9E` is **after** blit, not the target)

## Task 2 — Sibling binds

| Member | Index / resolve | Handle | Slot | Contributes to r4? | Contributes to xy? |
|--------|-----------------|--------|------|--------------------|--------------------|
| loadingbar!201!29.bmp | natural 304BF0 postmatch | `0x2A83C4` | `+0x28` | no | width for center-x |
| bar!16!18.bmp | E9I sibling resolve (guest index miss stall) | `0x3910080` | `+0x20` | no | no |
| textbar!120!30.bmp | E9I sibling resolve | `0x39100C0` | `+0x24` | no | no |

Sibling resolve uses **original jjfb.mrp bytes** (same payload path as E9E postmatch), not `REAL_MRP_MEMBER_BRIDGE`, not request rewrite.

## Task 3 — r4 after blit

- **r4 source**: `*(R9+0xBD0)` = splash_obj (`R9+0xBA0`) **+0x30**, loaded at `0x2EF886`
- **Not** bar/textbar/loadingbar slots
- Observed: `R9_BD0=0`, `r4_reg=0` → class `SPLASH_R4_SOURCE_FOUND_NEXT_GAP`
- Gate: `0x2EFAF2` peeks `*(R9+0xB6C)`; if zero, `cmp r4,#0` at `0x2EFAFA` blocks later text/progress UI
- Filling siblings alone does **not** clear the r4 gate

## Artifacts

- `reports/e9i_splash_resource_sequence.csv`
- `reports/e9i_coord_trace.csv`
- `reports/e9i_r4_after_blit_trace.csv`
- `screenshots/e9i_splash_loading_ui.png`
- `screenshots/e9i_actual_window_capture.png`
- `logs/e9i_splash_loading_stdout.txt`
- Script: `RUN_E9I_SPLASH_LOADING_UI.ps1` (`natural` / `coordassist` / `trace`)

## Next gap

- Construct / naturalize `*(R9+0xBD0)` progress object so post-blit gate passes
- Draw textbar/top on the post-r4 path (or progress ticks at `0x2EFAE2`) for `SPLASH_LOADING_UI_VISIBLE`
