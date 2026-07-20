# Stage E9J: Splash Progress Object / Post-r4 UI

**Verdict:** `SPLASH_LOADING_UI_VISIBLE` — **NOT product success.**

| Field | Value |
|-------|-------|
| Mode | `r4assist` |
| Evidence | OBSERVED |
| Elapsed | ~95s |
| Loadingbar | YES — `(19,220)` natural coords, no `COORD_ASSIST` |
| Progress segments | YES — **4×** `bar!16!18.bmp` at x=`47,59,71,83` y=`226` |
| Textbar blit | NO |
| Post-r4 (`0x2EFB0E` / `0x305BFC`) | NO (deferred hold at blit_n=5 before gate exit) |
| Progress object assist | YES (`JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST`) |
| Rewrite / member bridge | NO |

## Artifacts

- Log: `logs/e9j_splash_progress_stdout.txt`
- Writer CSV: `reports/e9j_bd0_b6c_writer_trace.csv`
- Post-r4 / progress CSV: `reports/e9j_post_r4_path_trace.csv`
- HWND: `screenshots/e9j_actual_window_capture.png` (GDI BitBlt, nonblank=21124)
- SDL surface: `screenshots/e9j_splash_progress_ui.png`

## Decoded layout (Task 1–3)

| Offset | Role |
|--------|------|
| `R9+0xBA0` | splash_obj base |
| `+0x20` | `bar!16!18` handle |
| `+0x24` | `textbar!120!30` handle |
| `+0x28` | `loadingbar!201!29` handle |
| `+0x2C` | progress count (loop bound) |
| `+0x30` / `R9+0xBD0` | status C-string → loaded into **r4** at `0x2EF886` |

- Natural BD0 writer: `0x2FC418` → concat → `STR` at `0x2FC444` (not reached this run; assist seeded instead).
- Gate: `*(R9+0xB6C)!=0` skips r4 path; else `cmp r4,#0` at `0x2EFAFA`.
- Progress loop: `0x2EFA9E..0x2EFAE2` draws `bar!` while `count > idx` (up to 12).
- Post-r4 text path: `0x2EFB0E` → textbar width at `+0x24` → `0x305BFC`.

## Assist rules (NOT product)

- Seeds real robotol GBK status string at `0x314338` (`"请稍候"`) into `R9+0xBD0` **before** splash so `0x2EF886` loads nonzero r4.
- Seeds `BA0+0x2C` progress count (default 4).
- Does **not** invent pixels, rewrite requests, or claim `REAL_MRP_MEMBER_BRIDGE` success.
- Deferred hold (`JJFB_E9C_DEFER_HOLD` + `JJFB_E9J_HOLD_AFTER_BLIT=5`) so first loadingbar blit does not stop-kill before progress segments.

## Observed sequence (r4assist)

1. `SPLASH_PROGRESS_OBJECT_SOURCE_FOUND_NEXT_GAP` — natural writer `0x2FC418/444` not yet reached; BD0 was 0.
2. Assist: `BD0=0x314338`, `count=4`.
3. Natural postmatch: loadingbar + sibling resolve `bar!` / `textbar!` (original MRP bytes).
4. Blit loadingbar `(19,220)` → `SPLASH_LOADINGBAR_NATURAL_COORD_VISIBLE`.
5. Progress loop: four `bar!` blits via `0x2EFAE2` → `0x2EC6B8` → `SPLASH_PROGRESS_DRAWN_NO_REWRITE`.
6. `SPLASH_LOADING_UI_VISIBLE` (mask includes loadingbar + progress).
7. `JJFB_E9J_DEFERRED_HOLD` → HWND capture nonblank.

## Gaps / next

1. **Natural BD0 writer** — reach `0x2FC418` callers so assist is unnecessary (`natural` mode → expect `SPLASH_BLOCKED_BY_BD0_ZERO` / `SOURCE_FOUND_NEXT_GAP`).
2. **Post-r4 / textbar** — allow guest past progress loop to `0x2EFAFA` / `0x2EFB0E` / `0x305BFC` (raise hold-after or hold after gate).
3. Magenta chroma on `bar!` segments still visible in HWND (keying / sprite path polish — not product blocker for this stage).
4. Still **not** full product state-machine naturalization (B6C/AC8/134D and later UI).

## Class meanings

| Class | Meaning |
|-------|---------|
| `SPLASH_LOADING_UI_VISIBLE` | loadingbar + ≥1 progress segment on HWND |
| `SPLASH_PROGRESS_DRAWN_NO_REWRITE` | `bar!` via real MRP pixels |
| `SPLASH_PROGRESS_OBJECT_SOURCE_FOUND_NEXT_GAP` | static writer known; not hit naturally |
| `SPLASH_BLOCKED_BY_BD0_ZERO` | r4 gate blocked without assist |
