# Stage E9N Splash Text Render Verdict

- **Case**: textshim
- **Mode**: textshim
- **Class**: PLATFORM_TEXT_DRAW_COMPAT_RENDERED
- **Evidence**: OBSERVED
- **Elapsed**: 95s
- **305C3C enter**: True
- **305C3C return**: True
- **Platform draw / glyph blit**: True
- **Platform text compat**: True
- **Loading UI / progress**: True / True
- **HWND hold**: True
- **Log**: `logs/e9n_splash_text_render_stdout.txt`
- **305C3C CSV**: `reports/e9n_305c3c_trace.csv`
- **Clip CSV**: `reports/e9n_clip_skip_trace.csv`
- **Glyph CSV**: `reports/e9n_font_glyph_trace.csv`
- **Disasm**: `out/e9n_tmp/e9n_disasm.txt`
- **HWND**: `screenshots/e9n_actual_window_capture.png`

## Cases
| Mode | Verdict |
|------|---------|
| trace | `TEXT_305C3C_BLOCKED_BY_PLATFORM_TEXT_API` (0x11F00 unhandled, no pixels) |
| textshim | `PLATFORM_TEXT_DRAW_COMPAT_RENDERED` (guest string @112,261 via host blit) |

## Static decode (305C3C)
- Entry ABI: R0=C-string, R1=x, R2=y, R3=color/flags; stack carries draw mode + clip box.
- Inner clip: `CMP *(R9+0x830), x` @ `0x305C54` -> `BLT 0x305CA0`.
- x>=0 fast path skips per-char loop @ `0x305C7C` -> `0x305CE2`.
- Draw gate: `BL 0x303C50` @ `0x305D08`; `CMP [sp+0x68], #1` @ `0x305D16`.
- Glyph blit: first branch `BL 0x2F2360` @ `0x305D58`; positive-x path often `BL 0x2F2360` @ `0x305D90` -> platform `0x11F00`.
- `0x305C32` / `0x305CA0` are shared return/epilogue sites (not always clip failure).
- Real gap without shim: platform `sendAppEvent 0x11F00` drawText returns 0 (no pixels).

## Rules
- NOT product success. Measure shim + optional platform text compat only.
- No rewrite / no bridge success / no invented layout.
- BD0 naturalization still deferred.
