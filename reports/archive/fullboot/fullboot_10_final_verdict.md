# Full Boot 10 — Final Verdict

- **verdict:** `PARTIAL`
- **class:** `PACKAGE_SCOPE_ACTIVE_NO_CONTEXT`
- **level:** `stage_a_partial`
- package_scope: `True`
- gamelist init_ok: `False`
- gamelist platform context: `False`
- cfg36: `False` post_update: `False`
- native_shell runapp: `False` jjfb open: `False`
- mrc_init: `False` visual: `False`
- host_runapp_equivalent used: `False`
- OOM: `False` heap_free events: `2`

## Blockers

- gamelist.ext init_ok missing
- gamelist EXTCHUNK/ER_RW/R9 incomplete
- no CFG36_BUILD
- no RUNAPP source=native_shell
- jjfb.mrp not opened
- mrc_init not reached

## DSM heap

## JJFB_DSM_HEAP

- `[JJFB_DSM_HEAP] action=keep_for_reuse guest=0x282A54 len=4194304 reason=shell_core_continue evidence=TARGET_OBSERVED`
- `[JJFB_DSM_HEAP] action=reuse guest=0x282A54 len=4194304 reason=shell_package_restart evidence=TARGET_OBSERVED`
