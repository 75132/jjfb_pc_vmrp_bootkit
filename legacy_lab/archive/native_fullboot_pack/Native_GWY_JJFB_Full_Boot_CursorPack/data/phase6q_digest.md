# Phase 6Q Digest

## Conclusion

# CONCLUSION — Phase 6Q Gamelist Native Primary ABI

**Verdict:** MID_SUCCESS (`GAMELIST_PRIMARY_HELPER_CLEARED`)

| Gate | Result |
|---|---|
| Minimum: gamelist reg_primary + generated member_view | PASS |
| Mid: helper/entry cleared (no 0x8CC00 ENTRY_ARGUMENT terminal) | PASS |
| High: CFG36 / POST_UPDATE / EXPORT_CALL | observe-only (none) |
| Slot matrix | no SLOT_CALL (correct) |

## Next

- Mid success — chase gamelist cfg36/post-update/export.


## Verdict

# Phase 6Q Verdict

- **verdict:** `MID_SUCCESS`
- **class:** `GAMELIST_PRIMARY_HELPER_CLEARED`
- **gamelist member_view:** `True`
- **0x8CC00 / ENTRY_ARGUMENT cleared or reclassed:** `True`
- **cfg36 / post_update / export:** `False`
- **SLOT_CALL:** `0`
- **GAMELIST_STARTED:** `True`

## Evidence

- reg.ext embedded primary name: **CROSS_TARGET**
- mr_extHelper with r0=P: **DOCUMENTED**
- Prior 0x8CC00 path: **TARGET_OBSERVED**


## Member view

# Phase 6Q Gamelist Member View

- reg_primary failed: `False`
- reg_primary_installed: `True` primary=`gamelist.ext`
- generated overlay seen: `True`

## REG_PRIMARY

- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/gamelist.mrp primary=gamelist.ext evidence=CROSS_TARGET source=reg.ext+embedded_package_stem`

## FILEOPEN

- `[JJFB_FILEOPEN] requested="mythroad/gwy/gamelist.mrp" normalized="mythroad/gwy/gamelist.mrp" canonical="gwy/gamelist.mrp" host="C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/shell_gamelist_cfunction.mrp" ok=1 backend=generated rule=generated`
- `[JJFB_FILEOPEN] guest="gwy/gamelist.mrp" host="C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/shell_gamelist_cfunction.mrp" ok=1 note=shell_package`


## Gamelist platform context

# Phase 6Q Gamelist Platform Context

- EXTCHUNK gamelist: 0
- ER_RW_BIND gamelist: 0
- R9_SWITCH_OK gamelist: 0
- SHELL_CORE init_ok: 0

## ExtChunk

- (none)

## ER_RW

- (none)

## R9

- (none)


## DSM cfunction helper ABI

# Phase 6Q DSM cfunction Helper ABI

- WRONG_HELPER hits: 0
- LEGAL_HELPER_EVENT_WITH_P hits: 4
- fault_pc=0x8CC00: `False`
- ENTRY_ARGUMENT: `False`

## Answers (from live tags)

1. Helper enter via `bridge_mr_extHelper` / HOST_BRIDGE when method!=0.
2. `0xA4178` is registered helper (event path), not header_entry `image+8`.
3. Event path should use mr_table helper with r0=P (LEGAL_HELPER_EVENT_WITH_P).
4. LR_PROXY may show call_site_r0=method then enter_r0=P — see ARG_FLOW.
5/6. P / chunk_field_04 from EXT_ENTRY_CTX / CHUNK_FIELD04 tags.

## TRACE

- `[JJFB_HELPER_ABI_TRACE] module=dsm:cfunction.ext helper=0xA4178 kind=LEGAL_HELPER_EVENT_WITH_P header_entry=0x80008 chunk_field_04=0x0 origin=HOST_BRIDGE evidence=TARGET_OBSERVED`
- `[JJFB_HELPER_ABI_TRACE] module=gbrwcore.ext helper=0x30CFE9 kind=LEGAL_HEADER_ENTRY_NULL_R0 header_entry=0x2EB7E8 chunk_field_04=0x2EB7E8 origin=GUEST_NESTED evidence=TARGET_OBSERVED`
- `[JJFB_HELPER_ABI_TRACE] module=dsm:cfunction.ext helper=0xA4178 kind=LEGAL_HELPER_EVENT_WITH_P header_entry=0x80008 chunk_field_04=0x0 origin=HOST_BRIDGE evidence=TARGET_OBSERVED`

## ROUTE

- `[JJFB_HELPER_CALL_ROUTE] route=mr_table_helper module=dsm:cfunction.ext entry_pc=0xA4178 method=1 evidence=DOCUMENTED`
- `[JJFB_HELPER_CALL_ROUTE] route=module_entry module=gbrwcore.ext entry_pc=0x2EB7E8 method=0 evidence=DOCUMENTED`
- `[JJFB_HELPER_CALL_ROUTE] route=mr_table_helper module=dsm:cfunction.ext entry_pc=0xA4178 method=1 evidence=DOCUMENTED`

## ARG_FLOW

- `[JJFB_HELPER_ARG_FLOW] call_site_r0=pending thunk_r0=pending enter_r0=0x2803E4 lr_proxy=yes method=1 evidence=TARGET_OBSERVED`
- `[JJFB_HELPER_ARG_FLOW] call_site_r0=pending thunk_r0=pending enter_r0=0x1 lr_proxy=no method=0 evidence=TARGET_OBSERVED`
- `[JJFB_HELPER_ARG_FLOW] call_site_r0=pending thunk_r0=pending enter_r0=0x2803E4 lr_proxy=yes method=1 evidence=TARGET_OBSERVED`


## Slot trigger

# Phase 6Q Slot Trigger

- SLOT_CALL count: 0
- action: no slot call — do not expand matrix

