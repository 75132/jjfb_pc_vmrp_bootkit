# Stage E2 — game_package ER_RW / R9 (E2-min)

- **task:** RUNTIME / EXT — guest P+0/+4 → registry bind → deferred R9 for `robotol.ext`
- **verdict:** `COMPLETE` for E2-min; E2-mid `[JJFB_MRC_INIT]` still open
- **source:** `descriptor_launcher` (cfg36 → `gwy/jjfb.mrp`)

## Proven (E2-min)

| Gate | Result |
|------|--------|
| FixR9 P-slot publish (`image+4` / helper MRP) | PASS — `[JJFB_CFN_P_SLOT]` |
| Guest fill `P+0/+4` after continuation | PASS — `er_rw=0x2B1854` size=5404 |
| `[JJFB_ER_RW_BIND] ... module=robotol.ext` | PASS |
| `[JJFB_R9_SWITCH_OK] package=gwy/jjfb.mrp module=robotol.ext` | PASS — r9=`0x2B1854` |
| jjfb sha256 unchanged | PASS |
| No gamelist / no host UI force | PASS |

## Post-R9 guest progress (TARGET_OBSERVED)

After deferred R9 switch, guest continued with callee ER_RW:

- `POST_CONT` YIELD/NORMAL_RETURN with `r9=0x2B1854`
- Further DSM helper events (`method=1`) and robotol `GUEST_INDIRECT_CALL` (methods 2/3 seen)
- `POST_CONT_SUMMARY`: `robotol_runtime_ready=yes`, class=`MISSING_EVENT_SCHEDULING`

## Not yet (E2-mid)

| Gate | Result |
|------|--------|
| `[JJFB_MRC_INIT] module=robotol.ext` (helper `code=0`) | **0** |
| `JJFB_MRC_INIT_ATTEMPT` | only `bootstrap_first_pc` (not proof) |
| Natural DRAW/REFRESH | open |

## Discriminating notes

1. Early peek still logs `P_ZERO_SKIP` before guest fill; bind runs from `ROBOTOL_RUNTIME_READY` / `ROBOTOL_ER_RW_READY` hooks.
2. First robotol PC remains `WRONG_ENTRY_SELECTION` (`0x303B92` vs header/helper); ER_RW birth is `AFTER_ENTRY`.
3. Product `[JJFB_MRC_INIT]` requires DOCUMENTED helper `code=0` (`mythroad.c:case_801`); that call was not observed in 90s after R9_OK.
4. Runner must not stop on `JJFB_R9_SWITCH_OK` or `JJFB_MRC_INIT_ATTEMPT` (fixed).

## Artifacts

- `RUN_E2_GAME_PACKAGE_ER_RW_MRCINIT.ps1`
- `logs/stage_e2_jjfb_game_package_er_rw_stdout.txt`
- `reports/stage_e2_verdict.md`
- `reports/stage_e2_game_package_er_rw_timeline.md`

## Next (exactly one)

Stage E2-D / E3: deliver DOCUMENTED helper `code=0` (`mrc_init`) after game_package context is ready — prefer event/scheduler or correct helper entry over UI/shell.

## wxjwq control (same runner, 60s)

- `mmochat.ext` EXTRACTED→ENTRY_CALLED; package scope OK
- No `[JJFB_ER_RW_BIND]` / R9_OK (ready hooks still robotol-branded) — guest reached `r9=0x2B1858` then `UC_MEM_FETCH_UNMAPPED @0x0`
- Control log: `logs/stage_e2_wxjwq_game_package_er_rw_stdout.txt`
