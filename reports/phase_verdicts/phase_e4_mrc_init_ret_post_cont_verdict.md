# Stage E4 — mrc_init `ret0` / post-init continuation

- **task:** RUNTIME + SCHEDULER observe — why `ret0=-1`, does host continue after init?
- **verdict:** `PARTIAL` — DOCUMENTED init args fixed; `ret0=-1` and no DRAW/REFRESH in 90s remain
- **source:** `descriptor_launcher` (cfg36 → `gwy/jjfb.mrp`)

## Discriminating results

| Hypothesis | Result | Class |
|------------|--------|-------|
| Host delivered 6/8/0 with `input=0,len=0` (missing DOCUMENTED args) | **Fixed** — now `version=2011`, `appinfo=0x…`, `len=2011/16` | DOCUMENTED (`mythroad_mini.c:mr_doExt`, `反汇编研究.c`) |
| Missing args alone caused `ret0=-1` | **Falsified** — still `ret0=-1` after correct args | TARGET_OBSERVED |
| `mr_doExt` fails closed on `mrc_init!=0` | **Falsified** — DOCUMENTED ignores init ret; `start_dsm` still `ret=0` | DOCUMENTED |
| Post-`start_dsm` host delivers timer/event/draw within 90s | **Fail** — log ends at `START_DSM_RETURN`; no `[JJFB_DRAW]`/`[JJFB_REFRESH]`/`FIRE` | TARGET_OBSERVED |

## Proven still true (E3)

- `[JJFB_MRC_INIT] module=robotol.ext … method=0`
- Helper retarget + R9_OK + init-seq deliver on helper exit
- jjfb hash unchanged

## Fixes landed (E4)

1. `bridge_deliver_ext_init_seq`: pass `MR_VERSION` as code-6/0 `input_len`; guest `mrc_appInfo` blob for code-8 (`GWY_PACKAGE_APPID`/`APPVER` from runner).
2. E4 runner stop: do not stop on `[JJFB_MRC_INIT]`; wait for DRAW/REFRESH/timer/fault/timeout.

## Blocker (next smallest)

Two open branches — pick **one** for E5:

1. **SCHEDULER:** after `START_DSM_RETURN`, why `host_timer_poll` never yields `FIRE` / `MR_TIMER` / method=1 to robotol (arm missing vs deliver missing).
2. **RUNTIME/PLATFORM:** why `mrc_init` returns `-1` despite DOCUMENTED 6/8/0 args (guest plat/sendAppEvent path during code=0; possible double-work after bootstrap `0x303B92`).

Do **not** force UI/login/network success.

## Artifacts

- `logs/stage_e2_jjfb_game_package_er_rw_stdout.txt` (ends ~line 752 after start_dsm)
- `RUN_E2_GAME_PACKAGE_ER_RW_MRCINIT.ps1` / `RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1`
- `reports/stage_e2_verdict.md`

## Checks

- build: PASS  
- hash: `52c13182…5fc036`  
- audit: not re-run this slice (bridge-only + runner); core unchanged aside from prior E3 stubs  
