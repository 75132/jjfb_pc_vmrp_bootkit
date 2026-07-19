# JJFB v73 GWY Launcher Snapshot

Date: 2026-07-17
Purpose: Perfect reproducible baseline before further launch-chain work. Packet capture deferred.

## Hashes

- jjfb.mrp sha256 = `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036` (unchanged; not packed here)
- main.exe sha256 = `7c92c28092e38d131632f65a0ed5764a2f221f22bb466b7cc09a762048e1a57c`

## Contents

- `runtime/vmrp_win32/vmrp_win32_20220102/main.exe` — built binary
- `runtime/vmrp_src_build_v27/vmrp-master/` — key sources + headers to rebuild
- `RUN_JJFB.ps1` / `RUN_V71_PRESENT_COALESCE.ps1` — entry runners
- `HOW_TO_START.md` — how to run
- `.cursor/rules/Rules.mdc` — project hard rules
- `logs/` — evidence from splash / skip-net probe runs

## Restore

1. Copy `main.exe` back to `runtime/vmrp_win32/vmrp_win32_20220102/` OR rebuild from packed sources with MinGW32 `-DNETWORK_SUPPORT -DVMRP -m32`.
2. Keep `game_files/mythroad/320x480` as resource root (not in this zip).
3. From repo root: `.\RUN_JJFB.ps1`

## Env at snapshot (runner defaults)

- JJFB_GWY_LAUNCHER_MODE=1
- JJFB_MRP_ALIAS_CFUNCTION_ROBOTOL=1
- JJFB_SCREEN_W=240 / JJFB_SCREEN_H=320
- JJFB_Y828_ZERO=1 / JJFB_TEXTBAR_DEDUP=1
- JJFB_SKIP_NET_LOGIN was ON in this snapshot runner (TEMPORARY probe; will be default-OFF after this backup)

## Current blocker (do not treat as solved)

- Splash `ui_mode=0x45` loops; in-guest `conn_fail`
- `gwy/jjfbol/0@s0.map` FILEOPEN_MISS (download cache)
- Host `mr_connect` / `initNetwork` still 0 on GWY launcher path
- Do NOT fake UI / force ui_mode as official solution

## Explicitly not included

- Full `game_files/` tree (use repo copy)
- Unicorn/SDL third-party binaries (use existing `runtime/.../windows/`)
