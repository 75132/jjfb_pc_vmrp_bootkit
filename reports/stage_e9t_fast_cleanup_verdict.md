# Stage E9T Fast Cleanup Verdict

- **Case**: reduce ladder (final = case3_no_c9d)
- **Class**: `C9D_ASSIST_REMOVED_SPLASH_VISIBLE` (FAST splash path)
- **Evidence**: OBSERVED
- **Elapsed**: case1~95s / case2~91s / case3~93s

## Ladder results

| Case | Config | Verdict |
|------|--------|---------|
| 1 upstream | `2FC03C` attempt + DisplayFirst/UI/C9D assists | `SPLASH_VISIBLE_WITH_UPSTREAM_BD0_CALLER` |
| 2 no UI | + `E9T_NO_UI_MODE_ASSIST` | `STATE_ASSIST_REMOVED_SPLASH_VISIBLE` |
| 3 no C9D | + `E9T_NO_C9D_ASSIST` | `C9D_ASSIST_REMOVED_SPLASH_VISIBLE` |

## Upstream BD0 caller

- `0x2FC03C` **not safe** on current path: LDMIA object at `R9+0x11EC+0x24` is all-zero → would `READ_UNMAPPED@0x4` inside `0x2FED14`
- Class: `BD0_UPSTREAM_CALLER_REQUIRES_OBJECT`
- Fallback: bare real `0x2FC418` (same BD0 STR as `2FC05E` tail) → BD0 `0x0→0x2A8364`
- `BA0+0x2C` count still **0** → progress segments still missing (`BD0_WRITER_REQUIRES_RESOURCE_OBJECT`)
- No progress-object poke assist

## Visible UI (case3 log)

- loadingbar: True
- transparent text / `0x11F00`: True
- progress segments: False
- HWND: `screenshots/e9t_actual_window_capture.png`

## Assist reduction honesty

- UI_MODE poke assist can be off: `0x2FC418` still writes `ui_mode=0x45`
- C9D **branch** assist can be off on this path because splash is still reached via `JJFB_FAST_SPLASH_CALL` (not natural idle through C9D)
- Therefore case3 does **not** prove natural C9D/idle productization — only that C9D branch assist is unused while FAST splash remains
- DisplayFirst / FAST splash / F6C / F74 assists remain

## Remaining (NOT product)

1. Natural reach of `0x2FC03C` with live object (or other caller that also writes `BA0+0x2C`)
2. Progress count natural writer
3. True DisplayFirst/C9D/idle naturalization (without `FAST_SPLASH_CALL`)

## Artifacts

- Log: `logs/e9t_fast_cleanup_stdout.txt`
- Upstream CSV: `reports/e9t_bd0_upstream_trace.csv`
- Count CSV: `reports/e9t_count_writer_trace.csv`
- Ladder CSV: `reports/e9t_assist_reduction_ladder.csv`
- HWND: `screenshots/e9t_actual_window_capture.png`
- UI: `screenshots/e9t_splash_visible.png`
