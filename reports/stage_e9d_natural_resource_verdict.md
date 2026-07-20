# Stage E9D Natural Resource Verdict

**Verdict:** `NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME`

**NOT product success.** Natural name match works; post-match decode/draw and full UI state machine still pending.

## Root cause (exact)

Guest index walk at `0x304BF0` was **not** a wrong MRP member name / `!` separator problem.

| PC | Real role | Failure |
| --- | --- | --- |
| `0x304F26` | memcpy 4B `name_len` from index | DSM table `+0xc` stub → full R9 switch per entry (too slow) |
| `0x304F7A` | memcpy name bytes into temp | same slow stub path |
| `0x304F92` | `strcmp(requested, temp)` | DSM table `+0x28` → stub `0xAC2D0` does `movs r0,#1`, **destroys** requested-name pointer before dispatch |

Exact class: **`CALLBACK_RETURN_ABI_WRONG`**

Index bytes and names (including `!w!h`) are correct in original `jjfb.mrp` and overlay `game_jjfb_cfunction.mrp`. Guest lookup algorithm hits `wy_jiao1!11!11.bmp` at entry #20.

## Fix (runtime only; MRP/EXT untouched)

- Host memcpy shim at `0x304F26` / `0x304F7A`
- Host strcmp shim at `0x304F92`
- Logs: `[JJFB_E9D_MEMCPY_SHIM]`, `[JJFB_E9D_NAME_COMPARE]`, `[JJFB_E9D_NATURAL_MATCH]`

## Evidence

- Request: `wy_jiao1!11!11.bmp`
- Compare #20: req == idx, `host_strcmp=0`, cause `OK_HOST_STRCMP`
- No `JJFB_REAL_MRP_MEMBER_BRIDGE` hit in `-Mode natural`
- CSV: `reports/e9d_name_compare_debug.csv`

## Remaining gaps

- After name match, guest post-match load/decode still uses DSM helpers; within 90s budget `0x2D92E4` has not yet returned / drawn in pure-natural mode.
- Next: accelerate post-match I/O/inflate helpers; then prove game-requested `slogo` / `loadingbar` / `textbar` sequence.
- Still: `PRODUCT_STILL_NEEDS_STATE_MACHINE_NATURALIZATION`

## Artifacts

- `reports/e9d_name_compare_debug.csv`
- `reports/e9d_resource_request_trace.csv`
- `reports/stage_e9d_natural_resource_summary.json`
- `logs/e9d_natural_resource_stdout.txt`
- `RUN_E9D_NATURAL_RESOURCE_UI_DEMO.ps1`
