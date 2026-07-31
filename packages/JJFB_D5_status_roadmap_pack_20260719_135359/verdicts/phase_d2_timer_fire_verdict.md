# Stage D2 — timer fire toward runapp

- **task:** SCHEDULER — deliver EXT timer after `PLATFORM_TIMER START`
- **verdict:** `COMPLETE` (fire path); Stage D runapp still open
- **evidence level:** DOCUMENTED (`mrc_extHelper` case 2 → `mrc_timerTimeout`) + TARGET_OBSERVED (Full Boot)

## Proven

1. **D1 retained:** `PLATFORM_TIMER START/STOP` chunk ABI, period 10000
2. **Blocker root cause:** nested `continue` finishes gamelist init while outer `start_dsm` still holds the bridge mutex → host `loop()` never runs; emu slices also stop after init return
3. **POST_CONT_PUMP:** after nested gamelist `start_dsm`, pump deadlines under the held lock
4. **FIRE path:**
   - `FIRE_CB` (SDL)
   - `FIRE_DUE via=emu_slice_poll`
   - `FIRE_EXT code=2 helper=0x2E3089 P=0x2AC8DC chunk=0x682A5C` (gamelist)
5. **Host loop plumbing kept:** `WaitEventTimeout` + `host_timer_poll` for when outer `start_dsm` returns
6. jjfb sha256 unchanged; `test_platform_timer` + `audit_launcher_core` green on prior build

## Not yet (Stage D / D3)

- No `[JJFB_RUNAPP] source=native_shell`
- After `FIRE_EXT`, guest hits `UC_MEM_READ_UNMAPPED @0` following plat `0x10204` / `0x10500` (returned 0) — next discriminating task: classify those sendAppEvent codes (DOCUMENTED/CROSS_TARGET) or observe required return without forcing UI

## Shipped

| Piece | Role |
|--------|------|
| `platform_timer` + emu slice / sleep poll | deadline while emu held |
| `main.c` host_loop timeout + USEREVENT | post-`start_dsm` deliver |
| `br_exit` `POST_CONT_PUMP` | deliver while nested continue holds mutex |
| `bridge_deliver_timer_body` | prefer EXT helper **code=2** over DSM `MR_TIMER` |

## Next smallest experiment

Classify / implement minimal fail-closed handling for post-timer `sendAppEvent` `0x10204` / `0x10500`, then re-run quiet Full Boot for `FIRE_EXT_DONE` → `[JJFB_SHELL_EXPORT_CALL]` / `[JJFB_RUNAPP] source=native_shell`.
