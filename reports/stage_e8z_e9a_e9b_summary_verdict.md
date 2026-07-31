# Stage E8Z → E9A → E9B Summary Verdict

**Overall (current):** `VISIBLE_WINDOW_DEMO_STABLE`

**NOT product success.** Real first sprite is drawable and presentable to the Windows HWND; product UI / natural case156 / C44 path is still unfinished.

---

## One-line arc

```text
E8Z = first real pixels via mr_drawBitmap (PNG from SDL surface)
E9A = stabilize + naturalize 0x304BF0 member bytes (still PNG/surface)
E9B = present that real framebuffer to the actual VMRP window (HWND)
```

---

## Stage verdicts

| Stage | Verdict | Meaning |
| --- | --- | --- |
| E8Z | `FIRST_REAL_FRAME_REACHED` | Real `wy_jiao1!11!11.bmp` (242B RGB565) drawn through `0x310BBC → mr_drawBitmap`, `bmp≠0` |
| E9A demo | `FIRST_FRAME_DEMO_STABLE` | One-command reproducible surface PNG |
| E9A bridge | `REAL_MEMBER_BRIDGE_FIRST_FRAME` | Same pixels via `JJFB_REAL_MRP_MEMBER_BRIDGE` (not FAST handle) |
| E9B | `VISIBLE_WINDOW_DEMO_STABLE` | HWND present + GDI capture nonblank + hold responsive |

---

## Confirmed real draw chain

```text
0x2F449C
→ 0x2D92E4
→ 0x304BF0  [guest stall OR REAL_MRP_MEMBER_BRIDGE]
→ A64 store
→ 0x310BBC
→ mr_drawBitmap  bmp=0x3920000
→ guiDrawBitmapSprite → SDL surface → UpdateWindowSurface → HWND
```

Member (original `jjfb.mrp`, no MRP/EXT edits):

- name: `wy_jiao1!11!11.bmp`
- bytes: 242, RGB565, 11×11 (draw h≈10)
- sha256: `edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13`
- non-background pixels: `other≈109`

---

## Screenshot sources (do not confuse)

| File | Exact source |
| --- | --- |
| `screenshots/e9a_first_frame.png` / `e9b_internal_surface.png` | `SDL_GetWindowSurface` + `SDL_SaveBMP` (software surface dump) |
| `screenshots/e9b_actual_window_capture.png` | **Actual HWND** client via GDI `BitBlt` |

E9A “有截图” ≠ “窗口里看见了”。E9B 才证明窗口 present。

---

## E9A: `0x304BF0` diagnosis

| Item | Finding |
| --- | --- |
| Package | `mythroad/gwy/jjfb.mrp` VFS open/read OK |
| Natural Case A | Guest index scan `0x304F26/7A/92` never matches → infinite loop |
| ABI | Success = `r0==0`, miss = `r0==-1`; object stays in caller `r4` |
| Bridge fix | Host `mrp_archive` decode → fill object → return status 0 → `0x2D92E4` returns handle |
| Assist left | Post-A64 sibling-load skip (`0x2F45A2`); FAST path still used by one-command demo |

### `real_bmp=false` ambiguity (fixed in metadata)

- `real_bmp` / `fast_real_bmp_handle` = whether **FAST_REAL_BMP_HANDLE assist** was used  
- `original_mrp_pixels` = whether bytes came from original `jjfb.mrp`  
- Bridge: `fast_real_bmp_handle=false`, `original_mrp_pixels=true`

---

## E9B: why window was white / Not responding / oversized

1. **White HWND:** `SDL_WINDOW_OPENGL` + `SDL_UpdateWindowSurface` → surface PNG OK, window often blank  
2. **Not responding:** Unicorn held UI thread with no message pump  
3. **Harness kill:** stopped on first PNG before user could see present  
4. **Oversized window:** default `ZOOM=4` → 960×1280 exceeded many screens  

Fixes:

- Software window (no OPENGL)
- `guiPumpEvents` during Unicorn slices + present hold
- HWND BitBlt capture
- Default zoom **2** (480×640) + **clamp to primary display** (`JJFB_WINDOW_ZOOM_CLAMP`)

---

## How to run

```powershell
# Quick visible window demo (default zoom=2, auto-clamped to screen)
.\RUN_E9B_VISIBLE_WINDOW_DEMO.ps1 -HoldSec 30 -SkipBuild

# Optional larger zoom (still clamped to fit screen)
.\RUN_E9B_VISIBLE_WINDOW_DEMO.ps1 -Zoom 3 -HoldSec 30 -SkipBuild

# E9A one-command surface PNG only (not HWND proof)
.\RUN_E9A_FIRSTFRAME_DEMO.ps1 -SkipBuild

# E9A real member bridge path
.\RUN_E9A_FIRSTFRAME_DEMO.ps1 -Mode bridgeonly -SkipBuild
```

---

## Assist reduction status

| Assist | E9A/E9B status |
| --- | --- |
| FAST_REAL_BMP_HANDLE | Demo still uses; bridge path replaced with real member bridge |
| REAL_MRP_MEMBER_BRIDGE | Working (ABI-correct) |
| POST_A64 sibling skip | Still needed under tracing |
| F6C/F74, C9D, DISPLAY_FIRST skip | Still present; not E9B scope |
| Natural `0x304BF0` index match | **Still broken** (next naturalization target) |

---

## Artifacts

- `screenshots/e9b_actual_window_capture.png` — HWND truth
- `screenshots/e9b_internal_surface.png` — SDL surface dump
- `screenshots/e9a_first_frame.png` — E9A surface dump
- `logs/e9b_visible_window_stdout.txt`
- `logs/e9a_first_frame_stdout.txt`
- `reports/stage_e9b_visible_window_verdict.md`
- `reports/stage_e9b_visible_window_summary.json`
- `reports/stage_e9a_firstframe_verdict.md`

---

## Next (suggested)

1. Fix guest `0x304BF0` index match → `NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME`
2. Drop POST_A64 skip / reduce FAST demo path
3. Keep window zoom ≤2 by default; optional overlay for tiny sprite debug only
4. Do **not** claim product success until natural UI-init / case156 / C44 path works without DISPLAY_FIRST stack
