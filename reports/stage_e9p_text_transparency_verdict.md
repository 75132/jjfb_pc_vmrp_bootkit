# Stage E9P Platform Text Transparency Verdict

- **Case**: transparent
- **Mode**: transparent
- **Blit**: transparent
- **Class**: PLATFORM_TEXT_TRANSPARENT_RENDERED
- **Evidence**: OBSERVED
- **Elapsed**: 103.3s
- **Transparent**: True
- **Colorkey**: False
- **Opaque (regress baseline)**: False
- **E9N textshim used**: False
- **Lit / skipped**: 164 / 412
- **Loading UI / progress**: True / True
- **HWND hold**: True
- **Log**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\e9p_text_transparency_stdout.txt`
- **Compare CSV**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\e9p_text_blit_compare.csv`
- **HWND**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9p_actual_window_capture.png`
- **UI**: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\screenshots\e9p_text_transparent.png`

## Semantics
- Platform `0x11F00` expects transparent glyph blit over existing splash UI.
- Only non-key pixels modify SDL surface (`guiDrawBitmapSpriteKey`).
- Guest string / x/y / color still from `0x11F00` args (no hardcode).

## Assists still active (NOT product)
- BD0 / DisplayFirst / TEXTCTX / measure shim remain.