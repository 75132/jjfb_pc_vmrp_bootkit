# Stage E9Y-Fix Downimage Event Verdict

- **Stage**: E9Y-Fix — downimage/update-ready event contract
- **Product success**: **NO** (`NOT_PRODUCT`)
- **Primary class**: `AC8_STILL_BLOCKED_BY_MISSING_UPDATE_EVENT`
- **Evidence**: OBSERVED

## Cases

| Case | Mode | Class | Notes |
|------|------|-------|-------|
| parse | parse | `DOWNIMAGE_CONTRACT_PARSED` | inventory + annotated splash |
| event_trace | event_trace | `AC8_STILL_DISPLAYFIRST_ONLY` | A80 watch + contract CSV; AC8 stays 0 |
| event_call | event_call | `AC8_STILL_BLOCKED_BY_MISSING_UPDATE_EVENT` | real guest call `0x2FE82C`; also hit `A80_AE0_SPLASH_STRUCT_SOURCE_FOUND` |

## Confirmed tags

| Tag | Status |
|-----|--------|
| `DOWNIMAGE_CONTRACT_PARSED` | YES |
| `A80_AE0_SPLASH_STRUCT_SOURCE_FOUND` | YES — writer `0x30CCB2` → `R9+0xABC=0xFFFFFFFF` inside `0x30CBBC` |
| `DOWNIMAGE_READY_EVENT_CANDIDATE_FOUND` | NO — called candidates do not raise AC8 |
| `DOWNIMAGE_READY_EVENT_REACHED_LOGO_BRANCH` | NO |
| `AC8_LOGO_BRANCH_REACHED_BY_REAL_EVENT` | NO |
| `SPLASH_FULL_PARITY_WITH_REAL_DOWNIMAGE_EVENT` | NO |
| `AC8_STILL_BLOCKED_BY_MISSING_UPDATE_EVENT` | YES |
| `PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN` | YES |

## Hard findings

1. **Resources OK**: `show1!232!100@downimage1.bmp` lives in `jjfbol/downimage1.mrp` (downimage pack), not main-MRP-only. Loading siblings resolve from main/sibling packs.
2. **AC8 semantics**: bool logo gate at `0x2EF8AE` (`CMP #0` / `BLE` loading). Not progress count. Strings build `show` + `!232!100` + `@downimage` + `.bmp` at runtime.
3. **Workbuf natural**: `0x30CBBC` → `0x30CD82` → `R9+0x8D8` (e.g. `0x2AC994`) with `WORKBUF_SEED=0`.
4. **Timer ≠ AC8**: `0x2F55FA` (`lr=0x3056D5`) reached; AC8 still 0.
5. **A80..AE0 band**:
   - `0x30CCB2` writes `ABC`
   - `0x2FE84C` / `0x2FE860` clear/keep-zero `AC8` and `A94`
6. **FAST real call**: `JJFB_FAST_DOWNIMAGE_READY_EVENT` → guest `0x2FE82C` (entry of observed clear site). `ok=1`, `AC8_after=0`, digest unchanged → **not** the update-ready setter.

## Next candidates (do not poke AC8)

- Find the guest/update path that runs **before** DisplayFirst and sets logo-ready without going through clear-only `0x2FE82C`.
- Follow who should write non-zero into `R9+0xAC8` after downimage pack prepare (still no natural STR AC8=1).
- Keep `0x30D300` / `0x300158` / `0x10140` contract traces; look for missing enqueue event that never fires on FAST path.

## Artifacts

| Kind | Path |
|------|------|
| Inventory | `reports/e9y_downimage_contract_inventory.csv` |
| A80 struct | `reports/e9y_a80_ae0_struct_trace.csv` |
| Event contract | `reports/e9y_event_contract_trace.csv` |
| Annotated splash | `out/e9y_fix/2ef86c_logo_branch_annotated.txt` |
| String xrefs | `out/e9y_fix/downimage_string_xrefs.txt` |
| Log | `logs/e9y_fix_downimage_event_stdout.txt` |
| Screenshot | `screenshots/e9y_fix_no_debug_full_splash.png` |
| Runner | `RUN_E9Y_FIX_DOWNIMAGE_EVENT.ps1` |

## Forbidden (held)

- No AC8 poke / `0x2EF8AE` patch / PC jump to logo
- No 8D8 seed as success
- No show1 hardcode / fake UI / MRP-EXT edits
