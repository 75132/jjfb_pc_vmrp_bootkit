# Stage D3 — post-timer plat 0x10204 / 0x10500

- **task:** PLATFORM — survive `FIRE_EXT` plat calls; progress toward runapp
- **verdict:** `COMPLETE` for plat fault; Stage D runapp still open (`PARTIAL`)
- **evidence:** TARGET_OBSERVED (gamelist.ext Thumb @ `0x2E7754`–`0x2E77BE`)

## Classification

| Code | Args (R0,R1,R2) | Required return | Evidence |
|------|-----------------|-----------------|----------|
| `0x10204` | `(4, 0xFFFFFFFF)` | guest ptr; `LDRH[ptr]==1\|3` | TARGET_OBSERVED |
| `0x10204` | `(0xF, buf)` | status `0` | TARGET_OBSERVED |
| `0x10500` | `(2, 0x105)` | status `0` (`cmp r0,#0`) | TARGET_OBSERVED |

## Proven

1. Tagged alloc: `[PLATFORM_ALLOC] code=0x10204 ... tag_u16=1 ret=0x68…`
2. No `UC_MEM_READ_UNMAPPED @0` after fire
3. `FIRE_EXT_DONE ret=0` then re-arm `period_ms=5000`
4. **9×** successful `FIRE_EXT` / `FIRE_EXT_DONE` cycles in 70s quiet boot
5. Unit: `test_platform_userinfo` covers classify; `audit_launcher_core` ok
6. jjfb sha256 unchanged

## Not yet

- No `[JJFB_RUNAPP] source=native_shell` / `[JJFB_SHELL_EXPORT_CALL]`
- Gamelist timer path only does `0x10204`/`0x10500` + re-arm; does not call `lib.runapp`

## Next smallest experiment (D4)

Observe what `bl 0x2E3460` / cfg36 `nmrpname=gwy/jjfb.mrp` needs beyond periodic timer (input event, network, counter, export resolve) — without forcing UI or host_runapp.
