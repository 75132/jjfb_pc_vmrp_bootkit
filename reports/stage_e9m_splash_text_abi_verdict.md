# Stage E9M Splash Text ABI Verdict

- **Mode**: measureshim
- **Class**: TEXT_305BFC_REACHED_VALID_ARGS_NEXT_GAP
- **Evidence**: OBSERVED
- **Elapsed**: 121.9s
- **Loading UI / progress**: True / True
- **Post-r4**: True
- **0x305BFC**: True
- **Valid args**: True (`r0=0x314338 r1=0x70 r2=0x105 r3=0xFF`)
- **305C3C draw core**: True (clip pass)
- **Measure shim**: True (`old_h=3202555 → w=36 h=16`)
- **Layout assist**: False (not needed after measure fix)
- **Visible splash text pixels**: False
- **HWND hold**: True
- **Rewrite**: False
- **Log**: `logs/e9m_splash_text_stdout.txt`
- **ABI CSV**: `reports/e9m_305bfc_abi_trace.csv`
- **Measure CSV**: `reports/e9m_text_measure_trace.csv`
- **Layout CSV**: `reports/e9m_layout_arg_trace.csv`
- **HWND**: `screenshots/e9m_actual_window_capture.png`

## Decoded ABI

### `0x305BFC` (wrapper → `0x305C3C`)
| Arg | Meaning |
|-----|---------|
| R0 | status C-string (live `0x314338` 「请稍候」) |
| R1 | **x** (layout) |
| R2 | **y** (layout; live `0x105`) |
| R3 / stack | color components (`0xFF`) + `R9+81C/818` dims |
| Clip | signed `*(R9+0x830) < x` → skip draw |

### Bad `r1=0xFFE7917B` provenance
Caller `0x2EFBA2` computes:
```
x = ( *(R9+0x830) - height_out ) / 2
y = r5 + ((width+10 - width) / 2)   → 0x105
```
`height_out` came from platform measure `0x304558(code=0x12340)` as garbage (`3202555` / `2621400`), so x underflowed.

### Measure path
```
0x305E78 → 0x303C50 (DSM) → 0x304558(0x12340) → outs width/height
         → *width += 2
```
`JJFB_FAST_TEXT_MEASURE_SHIM` at `0x305EA0` wrote `w=36 h=16` (GBK 3 glyphs × 12, h=16).
After `+=2`: width=38 → **x=(240-16)/2=0x70** (valid).

## Result
- Measure ABI fixed; **305BFC reached with valid args**; **305C3C entered**.
- HWND still shows loadingbar + 4 progress only — **no splash text pixels**.
- Later log also saw an inner `clip_skip` path; glyph/platform draw inside `0x305C3C` is the next gap.

## Rules
- NOT product success. Measure shim remains diagnostic.
- No rewrite / no bridge / no direct `0x305BFC` / no host-side text paint.
- BD0 naturalization still deferred.

## Next (suggested)
Instrument `0x305C3C` internals (font / `mr_platDrawChar` / blit) — do not prioritize BD0.
