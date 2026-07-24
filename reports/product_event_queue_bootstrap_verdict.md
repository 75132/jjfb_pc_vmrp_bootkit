# Product Event Queue Bootstrap Verdict

- run_id: `ffp_validate_20260724_040748_40125`
- A1 identity: owner_store vs B54 **differ** (ER_RW+owner_store offset vs ER_RW+0xB54)
- A2 B54 history (pre-fix): `B54_IS_INTERNAL_LIST_HEAD`
- A3: 10162/10165 sized alloc + optional handler; returns are guest pointers; queue list is **not** the 10165 buffer
- A4: B54 holds list control object; ctor `0x312AA4`; push `0x312A60`
- A5 first causal break: **E_platform_owned_queue_missing_initializer_trigger**
  - Proven init chain: family case app=2 `0x30E15E` -> `0x30CBBC` -> `0x2FE82C` -> `0x312AA4` -> STR R9+0xB54
  - Case 9 Path-A loads R9+0xB54 at `0x2E4EEC` then BL `0x312A60`
- Round B fix: **Case E** PlatformEventQueue (8-byte list control into live B54)
- Round B result: list_head_ready=1; Path A reached 30D2F9/2E4D6C/312A60
- Round B stop: new exact platform blocker after Path-A enqueue — DSM/cfunction mem_fault @0x94E40 r0=0 (ENTRY_ARGUMENT) during list-node alloc/helper path; UI_MODE still 0; state not advanced
