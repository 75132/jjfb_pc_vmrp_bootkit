# Stage E9Z GWY Resource Ready Verdict

- **Stage**: E9Z — GWY downimage/update prelaunch contract
- **Product success**: **NO** (`NOT_PRODUCT`)
- **Primary class**: `AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL`
- **Evidence**: OBSERVED

## Confirmed tags

| Tag | Status |
|-----|--------|
| `GWY_PACK_REGISTRY_BUILT` | YES — 59 packs under `gwy/jjfbol/` (stem+`ol`) |
| `GWY_UPDATE_MANIFEST_PARSED` | YES |
| `GWY_RESOURCE_READY_EVENT_CANDIDATE_FOUND` | YES — real dispatcher `0x30D300` → `0x300158`, evt=`0x14` |
| `GWY_RESOURCE_READY_EVENT_REACHED_LOGO_BRANCH` | NO |
| `DOWNIMAGE_UPDATE_CONTRACT_RESTORED` | NO |
| `SPLASH_FULL_PARITY_NO_DEBUG_AC8` | NO |
| `AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL` | YES |
| `PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN` | YES (secondary) |

## Hard findings

1. **Pack registry (common launcher)**: scans target sibling `<stem>ol/` only; indexes all `*.mrp` (includes `downimage1/2/3` without name hardcode). CSV: `reports/e9z_gwy_pack_registry.csv`.
2. **Manifest inventory**: side packs + EXT/cfg strings + observed evt `0x14`. CSV: `reports/e9z_update_manifest_inventory.csv`.
3. **Resource-ready via real dispatcher** (`GWY_PLATFORM_RESOURCE_READY_EVENT`):
   - `0x30D300` r0=pack_count(`0x3B`), r1=`0x14` → ok, `AC8_after=0`
   - `0x300158` r0=`0x14` → ok, `AC8_after=0`
   - Nested `0x10140` intentionally **not** re-fired (lifecycle owns drain; nested FIRE stalled pre_splash)
4. **AC8 still 0** with `DEBUG_AC8_FORCE=0` and no 8D8 seed → packing/registry alone + evt `0x14` is **not** the logo-ready setter.
5. **Blocker class A**: AC8 appears to require a GWY shell / update-complete path not currently loaded or not replaying the right prelaunch contract beyond side-pack mount + evt `0x14`.

## Blocker ladder (stop condition)

| Id | Class | Fit |
|----|-------|-----|
| A | `AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL` | **Best fit** — packs ready, dispatcher ran, gate unchanged |
| B | `AC8_BLOCKED_BY_UPDATE_RESPONSE` | Possible later if shell expects network update ack |
| C | `AC8_BLOCKED_BY_PERSISTENT_CACHE` | No evidence yet |
| D | `AC8_BLOCKED_BY_DISPLAYFIRST_SKIP` | DisplayFirst still used; not sole blocker |
| E | struct-copy A80..AE0 | Band writers exist; none set AC8≠0 |
| F | dead/unused AC8 | Contradicted by DEBUG_AC8 logo path (E9W/E9Y) |

## Artifacts

| Kind | Path |
|------|------|
| Runner | `RUN_E9Z_GWY_RESOURCE_READY.ps1` |
| Pack registry API | `include/gwy_launcher/gwy_pack_registry.h`, `src/formats/gwy_pack_registry.c` |
| Pack CSV | `reports/e9z_gwy_pack_registry.csv` |
| Manifest CSV | `reports/e9z_update_manifest_inventory.csv` |
| Import CSV | `reports/e9z_external_event_import_trace.csv` |
| Event CSV | `reports/e9z_resource_ready_event_trace.csv` |
| No-debug CSV | `reports/e9z_no_debug_validation.csv` |
| Log | `logs/e9z_gwy_resource_ready_stdout.txt` |
| Screenshot | `screenshots/e9z_resource_ready_no_debug_full_splash.png` |

## Forbidden (held)

- No AC8 poke / `0x2EF8AE` patch / PC jump to logo
- No 8D8 seed / BA0+2C poke as success
- No show1/downimage1/jjfb hardcode / fake UI / MRP-EXT edits

## Next (not E9Z)

Do **not** resume AC8 STR hunting. Next is either:

1. Identify the **exact** GWY shell module/API that originally set logo-ready after side-pack prepare (beyond evt `0x14`), or
2. E10A DisplayFirst/C9D naturalization **only after** a shell-level ready contract that actually raises AC8.
