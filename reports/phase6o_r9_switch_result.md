# Phase 6O R9 Switch

- attempts=2
- JJFB_R9_SWITCH_OK=2
- classic ENTER new_r9 for gbrwcore=2
- early CALLEE_ER_RW_NOT_AVAILABLE=1
- JJFB deferred blocked=0

## Attempts

- `[JJFB_R9_SWITCH_ATTEMPT] module=gbrwcore.ext module_id=3 callee_er_rw=0x2B0D14 caller_r9=0x280400 call_kind=RUNTIME_ENTRY reason=platform_er_rw_publication_restore evidence=DOCUMENTED`
- `[JJFB_R9_SWITCH_ATTEMPT] module=gbrwcore.ext module_id=3 callee_er_rw=0x2B0D18 caller_r9=0x280400 call_kind=RUNTIME_ENTRY reason=platform_er_rw_publication_restore evidence=DOCUMENTED`

## OK

- `[JJFB_R9_SWITCH_OK] module=gbrwcore.ext module_id=3 r9=0x2B0D14 er_rw_len=0x19A8 evidence=DOCUMENTED`
- `[JJFB_R9_SWITCH_OK] module=gbrwcore.ext module_id=3 r9=0x2B0D18 er_rw_len=0x19A8 evidence=DOCUMENTED`
