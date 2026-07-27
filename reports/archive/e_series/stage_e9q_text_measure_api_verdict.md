# Stage E9Q Platform Text Measure 0x12340 Verdict

- **Case**: api
- **Mode**: api
- **Class**: PLATFORM_TEXT_MEASURE_12340_RENDERED
- **Evidence**: OBSERVED
- **Elapsed**: 92.6s
- **Platform 0x12340 API**: True
- **Measure shim removed**: True
- **FAST_TEXT_MEASURE_SHIM used**: False
- **0x11F00 draw**: True
- **Transparent text**: True
- **Loading UI / progress**: True / True
- **HWND hold**: True
- **Log**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\e9q_text_measure_api_stdout.txt`
- **Measure CSV**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9q_platform_12340_measure_calls.csv`
- **Metrics CSV**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9q_measure_vs_draw_metrics.csv`
- **HWND**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9q_actual_window_capture.png`
- **UI**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9q_text_measure_api_window.png`

## ABI
- `sendAppEvent(0x12340, app, code_obj, ...)`
- After return @ `0x305EA0`: R4=&width, R7=&height (u32)
- Guest string from 305E78 chain / code-object scan
- Font metrics: same GDI face/size as 0x11F00 draw

## Assists still active (NOT product)
- BD0 / DisplayFirst / TEXTCTX remain
- Measure shim must be OFF on api success path