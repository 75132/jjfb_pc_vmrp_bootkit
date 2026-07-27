# Multi-game visual/boot smoke

## Verdict

**jjfb remains the only title with a proven DrawFP frame path** (via `RUN_JJFB_LAUNCHER` / `RUN_JJFB_VISUAL_SMOKE`).  
Other catalog titles **accept the launch contract** (`GWY_LAUNCH_TARGET` → `mr_open` of that MRP) but currently **`mr_exit` before game EXT / DrawFP**.

## Catalog

`gwy_launcher scan` → **19** local games under `game_files/mythroad/320x480`.

## Latest subset (`HoldSeconds=50`, no jjfb profile for others)

| Title | cfg | Target | Entry | Target open | DrawFP | mr_exit | Note |
|-------|----:|--------|:-----:|:-----------:|:------:|:-------:|------|
| jjfb | 36 | `gwy/jjfb.mrp` | yes | yes | no* | no | Path-A / chrome reached; bare `main.exe` less reliable than Launcher for frame |
| sanguo | 6 | `gwy/sanguo.mrp` | — | yes | no | **yes** | target open then exit |
| tlbb | 13 | `gwy/tlbb.mrp` | — | yes | no | **yes** | same |
| ajss | 11 | `gwy/ajss.mrp` | — | yes | no | **yes** | same |
| ssjx | 5 | `gwy/ssjx.mrp` | — | yes | no | **yes** | same |

\*jjfb DrawFP **PASS** under `RUN_JJFB_VISUAL_SMOKE` / `JJFB_Launcher` (loadingbar screenshot ~230KB). Multi-game bare spawn often stops after Path-A priming without frame in the same window.

## What this proves

1. **Launch wiring is generic**: `GWY_LAUNCH=1` + target/param correctly opens non-jjfb MRPs.
2. **Display stack is still jjfb-tuned**: robotol alias, Path-A, chrome/DrawFP assume jjfb/`robotol.ext`.
3. **Other titles need more than descriptor launch**:
   - cfg rows often `napptype=0/nextid=0/ncode=0` (weak vs jjfb’s 12/482/512)
   - packages use own EXT (`sanguo.ext`, `dream.ext`, `ajss.ext`, …) — not robotol
   - many exit from `start.mr` without loading that EXT (likely shell/gamelist/network gate)

## How to retest

```powershell
# jjfb frame gate (primary)
.\RUN_JJFB_VISUAL_SMOKE.ps1 -HoldSeconds 45

# cross-title launch contract
.\RUN_MULTI_GAME_VISUAL_SMOKE.ps1 -HoldSeconds 50 -SkipBuild -SkipVmrpBuild
.\RUN_MULTI_GAME_VISUAL_SMOKE.ps1 -Only 'jjfb,sanguo,tlbb' -HoldSeconds 50 -SkipBuild -SkipVmrpBuild

# interactive catalog picker
.\RUN_GAMES.ps1 -SkipBuild
```

Evidence: `out/multi_game_smoke/<title>/`

## Next (if expanding beyond jjfb)

1. Per-game minimal profiles (no robotol alias; map package primary EXT).
2. Or launch via shell/`gamelist`/`gbrwcore` so cfg fields are filled like real GWY.
3. Keep daily visual gate on **jjfb only** until one other title reaches DrawFP.
