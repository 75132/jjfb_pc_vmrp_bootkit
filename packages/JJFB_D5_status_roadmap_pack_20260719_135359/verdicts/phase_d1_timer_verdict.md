# Stage D1 — sendAppEvent ext timer ABI

- **task:** PLATFORM — recognize DOCUMENTED classic + TARGET_OBSERVED chunk-first timer via `sendAppEvent(+0x28)`
- **verdict:** `COMPLETE` (discriminating proof only; Stage D runapp still open)
- **evidence:**
  - `[PLATFORM_TIMER] op=STOP chunk=0x682A5C name=ext_timer_stop_chunk_r0 evidence=TARGET_OBSERVED`
  - `[JJFB_EXTCHUNK_SLOT] module=gamelist.ext off=0x24 value=0x1 meaning=timer evidence=DOCUMENTED`
  - `[PLATFORM_TIMER] op=START chunk=0x682A5C period_ms=10000 id=0x1 name=ext_timer_start_chunk_r0 evidence=TARGET_OBSERVED`
  - SLOT_CALL `(chunk,1)` then `(chunk,0,_,0x2710)` matched
  - Natural `[JJFB_DRAW]` / `[JJFB_REFRESH]` still present (not forced)
- **gates:** build OK; `test_platform_userinfo` OK (timer classify); `audit_launcher_core` ok; jjfb sha256 unchanged
- **not proven yet:** timer fire → guest callback → `lib.runapp` / `JJFB_RUNAPP source=native_shell`
- **next:** Stage D2 — observe post-timer progress toward export/runapp (quiet Full Boot ≥ period_ms+margin)
