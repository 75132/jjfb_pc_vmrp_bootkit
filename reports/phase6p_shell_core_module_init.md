# Phase 6P Shell Core Module Init

- `[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=entry evidence=TARGET_OBSERVED`
- `[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=entry evidence=TARGET_OBSERVED`
- `[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=init_ok evidence=TARGET_OBSERVED`
- `[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=init_ok evidence=TARGET_OBSERVED`
- `[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=entry evidence=TARGET_OBSERVED`
- `[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=entry evidence=TARGET_OBSERVED`

## Continue

- `[JJFB_SHELL_CORE_CONTINUE] from=gbrwcore.mrp to=gwy/gamelist.mrp via=start_dsm reason=continue_after_gbrwcore_init evidence=TARGET_OBSERVED`
- `[JJFB_SHELL_CORE_CONTINUE] apply=bridge_dsm_mr_start_dsm target=gwy/gamelist.mrp reason=continue_after_gbrwcore_init`
