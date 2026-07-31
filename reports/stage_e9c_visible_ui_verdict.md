# Stage E9C Visible UI Verdict

**Verdict:** `MEANINGFUL_VISIBLE_UI_FRAME`

**Natural 0x304BF0:** `NATURAL_MEMBER_RESOLVE_STILL_BLOCKED`  
Exact class: `NATURAL_MEMBER_RESOLVE_BLOCKED_BY_NAME`

**NOT product success.** This is a larger original title logo, not natural full game UI state machine.

## Inventory

- 50 members in original `jjfb.mrp`; ~27 image-like.
- No full 240×320 background in the package.
- Best UI candidate: `slogo!157!58.bmp` (157×58, 18212B RGB565, sha256 `919d94d5…84773fa9`) — title logo「机甲风暴」.
- Next: `loadingbar!201!29.bmp`, `textbar!120!30.bmp`, `top!76!28.bmp`.

## Demo result

- Mode: `candidate` → drew `slogo!157!58.bmp` via `mr_drawBitmap` hook → `guiDrawBitmapSprite`.
- HWND capture nonwhite=26236; sprite other=6584; zoom=2; hold 30s with message pump.
- Path: original MRP member bytes (extracted RGB565), not invented pixels / not direct FB paint.
- Larger and more meaningful than E9B `wy_jiao1!11!11.bmp` (11×11).

## Natural member resolve (0x304BF0)

- Guest index scan at `0x304F26/0x304F7A/0x304F92` still never matches → stall/loop.
- Blocker: `NATURAL_MEMBER_RESOLVE_BLOCKED_BY_NAME` (name compare / package index context).
- Assist: `JJFB_REAL_MRP_MEMBER_BRIDGE_ALL=1` can load any exact original member; ABI success `r0=0`, miss `r0=-1`.

## Artifacts

- `reports/e9c_mrp_member_inventory.csv`
- `reports/e9c_mrp_image_inventory.csv`
- `out/e9c_resource_previews/`
- `screenshots/e9c_actual_window_capture.png`
- `screenshots/e9c_internal_surface.png`
- `logs/e9c_visible_ui_stdout.txt`
- `RUN_E9C_VISIBLE_UI_DEMO.ps1`

