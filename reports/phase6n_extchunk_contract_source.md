# Phase 6N — mrc_extChunk contract source

Evidence class: **DOCUMENTED** (unless noted).

## Sources

- `third_party/vmrp_upstream/mythroad/include/mr_helper.h` — `mr_c_function_st`, `mrc_extChunk_st`
- `third_party/vmrp_upstream/doc/整理的ext重要函数.c` — `mrc_extLoad` fill sequence

## `mr_c_function_st` (publication surface)

| off | field | meaning |
|---|---|---|
| +0x00 | `start_of_ER_RW` | RW segment |
| +0x04 | `ER_RW_Length` | RW length |
| +0x08 | `ext_type` | ext start type |
| +0x0C | `mrc_extChunk` | pointer to `mrc_extChunk_st` (**publish target**) |
| +0x10 | `stack` | stack |

## `mrc_extChunk_st` (platform-owned object)

| off | field | meaning | Phase 6N fill |
|---|---|---|---|
| +0x00 | `check` | magic `0x7FD854EB` | constant DOCUMENTED |
| +0x04 | `init_func` | `mr_c_function_load` | `guest_code_base+8` when known |
| +0x08 | `event` | `mr_helper` | registered helper |
| +0x0C | `code_buf` | ext image base | module map |
| +0x10 | `code_len` | ext length | module map |
| +0x14 | `var_buf` | RW base | when known |
| +0x18 | `var_len` | RW length | when known |
| +0x1C | `global_p_buf` | `mr_c_function_st*` | `P` guest |
| +0x20 | `global_p_len` | sizeof P | typically 20 |
| +0x24 | `timer` | timer handle | 0 (observe phase) |
| +0x28 | `sendAppEvent` | `mrc_extMainSendAppMsg_t` | bridge MAP_FUNC observe stub |
| +0x2C | `extMrTable` | `mr_table*` | platform mr_table guest when known |

Minimum allocation size used by provider: `0x40` (covers DOCUMENTED fields through `extMrTable` + pad).

## Publication sequence (DOCUMENTED pattern)

1. Allocate extChunk in MRP memory (same allocator family as P).
2. Fill `check`, `init_func`, `event`, `sendAppEvent`, code/global_p fields.
3. Write `pSt->mrc_extChunk = ext_handle` (`P+0x0C`).

## Env gate (implementation)

`JJFB_EXTCHUNK_PROVIDER=off|gbrwcore_only|gbrwcore_wxjwq|gwy_shell`

Tags must use reasons: `mr_c_function_new_contract` | `ext_register_contract` | `platform_publication_restore`.
Never claim platform publish is a natural guest write.
