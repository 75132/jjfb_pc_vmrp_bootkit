# Full Boot 02 — Shell Module Context

- gamelist EXTCHUNK: 0
- gamelist ER_RW: 0
- gamelist R9_OK: 0
- init_ok lines: 2

## EXTCHUNK

- `[JJFB_EXTCHUNK_ALLOC] module=gbrwcore.ext module_id=3 guest=0x682A5C size=0x40 helper=0x30CFE9 evidence=DOCUMENTED`
- `[JJFB_EXTCHUNK_PUBLISH] module=gbrwcore.ext P=0x2AC8DC off=0x0C old=0x0 new=0x682A5C reason=mr_c_function_new_contract evidence=DOCUMENTED`
- `[JJFB_EXTCHUNK_ALLOC] module=gbrwcore.ext module_id=3 guest=0x682A5C size=0x40 helper=0x30CFE9 evidence=DOCUMENTED`
- `[JJFB_EXTCHUNK_PUBLISH] module=gbrwcore.ext P=0x2AC8DC off=0x0C old=0x682A5C new=0x682A5C reason=platform_publication_restore evidence=DOCUMENTED`
- `[JJFB_EXTCHUNK_ALLOC] module=gbrwcore.ext module_id=3 guest=0x682A5C size=0x40 helper=0x30CFE9 evidence=DOCUMENTED`
- `[JJFB_EXTCHUNK_PUBLISH] module=gbrwcore.ext P=0x2AC8DC off=0x0C old=0x0 new=0x682A5C reason=platform_publication_restore evidence=DOCUMENTED`
## ER_RW_BIND

- `[JJFB_ER_RW_BIND] module=gbrwcore.ext module_id=3 P=0x2AC8DC p_base=0x2B0D14 p_len=0x19A8 registry_base=0x2B0D14 registry_len=0x19A8 reason=mr_c_function_st_metadata_bind evidence=DOCUMENTED`
- `[JJFB_ER_RW_BIND] module=gbrwcore.ext module_id=3 P=0x2AC8DC p_base=0x2B0D18 p_len=0x19A8 registry_base=0x2B0D18 registry_len=0x19A8 reason=platform_er_rw_publication_restore evidence=DOCUMENTED`
## R9_SWITCH_OK

- `[JJFB_R9_SWITCH_OK] module=gbrwcore.ext module_id=3 r9=0x2B0D14 er_rw_len=0x19A8 evidence=DOCUMENTED`
- `[JJFB_R9_SWITCH_OK] module=gbrwcore.ext module_id=3 r9=0x2B0D18 er_rw_len=0x19A8 evidence=DOCUMENTED`
## SHELL_CORE init_ok

- `[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=init_ok evidence=TARGET_OBSERVED`
- `[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=init_ok evidence=TARGET_OBSERVED`
