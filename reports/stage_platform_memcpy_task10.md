# Task 10: Formal platform memcpy import

**Date:** 2026-07-25  
**Runs:** `pm10_{A,B,C,D}_*` under `out/platform_memcpy_task10/`  
**Primary verdict:** `PLATFORM_MEMCPY_IMPORT_BOUND`

## Success criteria

| Criterion | C (import=1, FSC=0) |
|---|---|
| Scheme C off | ✅ `repaired=0` |
| Formal import on | ✅ `import_ok` @ slot `0x804A8` |
| inner → `FFFFFFFF` | ✅ framing copy `n=4` via import |
| `0x2F68E4` return | ✅ |
| `0x2DADC4` enter | ✅ |
| No PC-gated product copy | ✅ bind by import VA identity |

## A/B/C/D matrix

| V | import | FSC | helper ret | `0x2E4066` | `0x2DADC4` | notes |
|---|---|---|---|---|---|---|
| A | 0 | 0 | no | no | no | `r5=0x7374` baseline |
| B | 0 | 1 | yes | yes | yes | Scheme C calibration |
| C | 1 | 0 | yes | yes | yes | **product target** |
| D | 1 | 1 | yes | yes | yes | dual path, no bad_r5 |

## Implementation

- `platform_guest_memcpy` — validated Guest binary copy
- Bind misresolved Robotol copy import identity **DSM `0x804A8`** → host callback
- `br_memcpy` (`mr_table+0xC`) also uses `platform_guest_memcpy`
- Defaults: `JJFB_PLATFORM_MEMCPY_IMPORT=ON`, `JJFB_FIELD_STREAM_CONTRACT=OFF`
- No silent PC fallback when import fails

## Next

`platform_mrp_resource` — do not return to fixed-PC copy.
