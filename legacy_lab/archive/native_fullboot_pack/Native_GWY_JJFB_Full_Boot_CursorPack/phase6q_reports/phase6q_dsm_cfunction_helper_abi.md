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
