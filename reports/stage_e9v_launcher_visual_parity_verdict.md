# Stage E9V Launcher Visual Parity Verdict

- **Case**: visual_colorkey_center
- **Mode**: visual
- **Class**: SPLASH_VISUAL_PARITY_IMPROVED
- **Evidence**: OBSERVED
- **Elapsed**: 95.9s
- **Product success**: **NO** (`NOT_PRODUCT_SUCCESS` / `PRODUCT_STILL_NEEDS_DISPLAYFIRST_C9D_CLEANUP`)

## Lane A 鈥?bitmap color-key
- **BITMAP_COLORKEY_TRANSPARENT_RENDERED**: True
- **Magenta pixels before (crop)**: 2360
- **Magenta pixels after (crop)**: 0
- **JJFB_COLORKEY**: auto (corners/sample; no member-name hardcode)

## Lane B 鈥?0x12340 measure / centered text
- **TEXT_MEASURE_LAYOUT_CENTERED**: True
- **Measure / layout logs**: True / True
- **0x11F00 draw**: True
- ABI: R7=horizontal extent 鈫?text width; R4=vertical 鈫?text height
- Caller `0x2EFBA2`: `x=(screen_w - *R7)/2` (no layout-assist poke)

## Lane C 鈥?progress timer
- **0x2F55FA reached**: False
- **Natural 0x3124D8 tick**: False
- **PLATFORM_TIMER_DISPATCH**: False
- **FAST_PROGRESS_TICK_CALL used**: True
- Note: visual mode may keep FAST tick 脳12 for visible segments; timer mode drops it.

## Visible UI
- Splash / loading / progress: True / True / True

## Artifacts
| Kind | Path |
|------|------|
| Verdict | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\stage_e9v_launcher_visual_parity_verdict.md` |
| Log | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\e9v_launcher_visual_parity_stdout.txt` |
| Color-key CSV | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9v_bitmap_colorkey_trace.csv` |
| Measure CSV | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9v_text_measure_layout_trace.csv` |
| Timer CSV | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9v_timer_progress_trace.csv` |
| Before | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9v_before_visual_fix.png` |
| After | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9v_after_colorkey_centered_text.png` |
| HWND | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9v_actual_window_capture.png` |
| Progress frame | `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9v_progress_timer_frame.png` |

## Forbidden checks
- No game-specific resource-name hardcode
- No fake UI / invented pixels
- No MRP/EXT edits / request rewrite
- No direct BA0+0x2C poke as success
- No direct C9D poke