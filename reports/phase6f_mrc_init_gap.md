# Phase 6F — mrc_init gap

- mrc_init_seen: `no`
- note: `fault_or_stop_before_mrc_init_or_path_never_entered`

## Answers (observe-only)

1. Normal plugin/shell paths may call guest `mrc_init` after extChunk is published;
   DOCUMENTED layout uses `mrc_extChunk->sendAppEvent` (@+0x28).
2. Whether `mrc_init` is owned by jjfb vs gwy/startGame is **HYPOTHESIS** without shell execution.
3. Current direct jjfb launch: not observed before NEW_ABI_FAULT (Phase 6E/6F).
4. Fault at `LDR [r0,#0x28]` with NULL P+0xC occurs in post-continuation path —
   consistent with init never reached (**TARGET_OBSERVED** ordering).
5. Missing runapp/shell context remains the leading explanation for skipped publication + init.

