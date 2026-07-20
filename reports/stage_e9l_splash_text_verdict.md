# Stage E9L Splash Text / 818-81C Verdict

- **Mode**: textctxassist
- **Class**: TEXTCTX_ASSIST_REACHED_305BFC
- **Evidence**: OBSERVED
- **Elapsed**: 126.2s
- **Loading UI / progress**: True / True
- **Post-r4**: True
- **0x2F2174**: True
- **0x305BFC**: True
- **Textctx assist**: True
- **BD0 assist kept**: True
- **HWND hold**: True
- **Rewrite**: False
- **Log**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\e9l_splash_text_stdout.txt`
- **Writer CSV**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9l_818_81c_writer_trace.csv`
- **305E78 CSV**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9l_305e78_trace.csv`
- **Text CSV**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9l_text_draw_trace.csv`
- **HWND**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9l_actual_window_capture.png`

## Decoded
- `R9+0x818/0x81C` are screen/layout dims (same family as `830/834`), read into r2/r1 before `BL 0x305E78`.
- No robotol `STR` writer found; seed under `FAST_TEXTCTX_ASSIST` uses real 240x320.
- `0x305E78`: measure helper 鈫?`0x303C50` (DSM) 鈫?`0x304558` platform text measure.
- Post-r4: `0x2EFB0E` 鈫?`0x305E78` 鈫?`0x2F2174` 鈫?`0x305BFC`.

## Rules
- NOT product success. BD0 + textctx assists remain diagnostic.
- No rewrite / no bridge / no direct `0x305BFC` / no invented pixels.
- BD0 naturalization still deferred.
