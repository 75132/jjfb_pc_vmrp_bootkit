# Stage E9B-VisibleWindow Present Verdict

**Verdict:** `VISIBLE_WINDOW_DEMO_STABLE`

**NOT product success.** E9A PNG != visible HWND.

## Screenshot source (exact)

- Internal ``e9a/e9b_*surface*.png``: ``SDL_GetWindowSurface`` + ``SDL_SaveBMP`` (software surface dump).
- **Not** HWND/PrintWindow/BitBlt unless ``e9b_actual_window_capture.png`` exists.
- E9A ``FIRST_REAL_FRAME_REACHED`` was framebuffer/surface success only.

## Fixes applied

- Removed ``SDL_WINDOW_OPENGL`` (UpdateWindowSurface was dumping surface while HWND stayed white).
- ``guiPumpEvents`` during Unicorn slices + present hold.
- Optional ``JJFB_WINDOW_ZOOM`` (nearest-neighbor display scale only).
- HWND client capture via GDI BitBlt.

## Results

- presented=True hold_done=True hwnd_nonwhite=440
- bmp=0x3920000 other=109 zoom=4
- fast_real_bmp_handle=True original_mrp_pixels=True

## Artifacts

- ``screenshots/e9b_actual_window_capture.png``
- ``screenshots/e9b_internal_surface.png``
- ``logs/e9b_visible_window_stdout.txt``
- ``reports/stage_e9b_visible_window_summary.json``

