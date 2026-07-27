# Stage E9W Splash Full Parity Verdict

- **Case**: debuglogo_ac8_force
- **Mode**: debuglogo
- **Class**: SPLASH_FULL_VISUAL_PARITY_IMPROVED
- **Evidence**: OBSERVED
- **Elapsed**: 69.1s
- **Product success**: **NO** (DEBUG AC8 + workbuf seed still required)

## Visual (HWND)
- Upper `show1@downimage1` panel drawn (y=120)
- `loadingbar` drawn under it (y=220)
- Hold after both: `e9w_full_splash_logo_and_bar` (mask=0x31)
- Artifact: `screenshots/e9w_after_logo_branch.png`

## Lane A — AC8
- DEBUG force: True (required for logo branch)
- Natural AC8>0: False
- Without DEBUG (`-Mode ac8`): **AC8_STILL_BLOCKED_BY_DISPLAYFIRST_STATE**
- Static: only AC8 **clear** at `0x2FB28C`; no guest store sets AC8>0

## Lane B — resolve
- `@`-pack: `show1!232!100@downimage1.bmp` → `gwy/jjfbol/downimage1.mrp`
- loadingbar via guest_index / postmatch

## Assists still on (NOT_PRODUCT)
- `JJFB_DEBUG_AC8_FORCE`
- `R9+0x8D8` workbuf seed (`0x3970000`)
- FAST DisplayFirst / tick path

## Next (product path)
1. Natural AC8>0 via event/timer (E9Y) — drop DEBUG force
2. Natural `0x30CD82` workbuf alloc — drop seed
3. Optional: progress segments + textbar on same held frame
4. E9X multi-MRP after AC8 naturalizes
