# Stage E9S BD0 Naturalization Verdict

- **Case**: initcall
- **Mode**: initcall
- **Class**: BD0_ASSIST_REMOVED_SPLASH_VISIBLE
- **Evidence**: OBSERVED
- **Elapsed**: 107.2s
- **Real 0x2FC418 initcall writes BD0**: True
- **PROGRESS_OBJECT poke assist used**: False
- **0x2FC418 / 0x2FC444 hit**: True / True
- **BA0+0x2C count gap**: True
- **0x12340 measure / 0x11F00 draw**: True / True
- **Transparent text**: True
- **Loading UI / progress**: True / False
- **HWND hold**: True
- **Log**: `logs/e9s_bd0_naturalize_stdout.txt`
- **Writer CSV**: `reports/e9s_bd0_writer_trace.csv`
- **Fn CSV**: `reports/e9s_2fc418_2fc444_trace.csv`
- **HWND**: `screenshots/e9s_actual_window_capture.png`
- **UI**: `screenshots/e9s_bd0_naturalized_window.png`

## Decoded ABI (static)
- `0x2FC418(r0=C-string)`: `r4=R9+BA0`; free old `+0x30`; `BL 0x2D9648`; `STR` @ `0x2FC444` to `BA0+0x30` (=`R9+BD0`); `ui_mode=0x45`
- Natural BL callers: `0x2FC05E` (via `0x2FC03C`), `0x30EE8A` / `0x30EE92` — not reached on DisplayFirst path
- Default initcall arg: `0x3146C4` (`2FC03C` natural string)
- Live: BD0 `0x0 -> 0x2A8364` via real STR (no direct poke); text draws that guest string (w=72)

## Remaining (NOT product)
- DisplayFirst helper skip / C9D branch / state / UI_MODE
- `BA0+0x2C` progress count still 0 (progress segments not drawn without old poke assist)
