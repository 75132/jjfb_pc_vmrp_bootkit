# Phase 6H — gbrwcore export / dispatcher resolution

Evidence: **TARGET_OBSERVED** static analysis of `gbrwcore.ext`.
String offsets are **not** function entries.

- member_size: `147196`
- header_magic: `b'MRPGCMAP'`

## Export name table (`lib.*` strings)

| export name | string offset | xref pc | candidate dispatcher | candidate function pointer | runtime VA | called | args |
|---|---|---|---|---|---|---|---|
| `lib.startGame` | `0x223D4` | `none_found` | `lib_name_table_or_strcmp_dispatcher (HYPOTHESIS)` | `unknown` | `load_base+0x223D4` | live_see_phase6h | pending |
| `lib.checkmrpver` | `0x22444` | `none_found` | `lib_name_table_or_strcmp_dispatcher (HYPOTHESIS)` | `unknown` | `load_base+0x22444` | live_see_phase6h | pending |
| `lib.isFileOnServerNewer` | `0x22454` | `none_found` | `lib_name_table_or_strcmp_dispatcher (HYPOTHESIS)` | `unknown` | `load_base+0x22454` | live_see_phase6h | pending |
| `lib.download` | `0x224A4` | `none_found` | `lib_name_table_or_strcmp_dispatcher (HYPOTHESIS)` | `unknown` | `load_base+0x224A4` | live_see_phase6h | pending |
| `lib.runapp` | `0x224C0` | `none_found` | `lib_name_table_or_strcmp_dispatcher (HYPOTHESIS)` | `unknown` | `load_base+0x224C0` | live_see_phase6h | pending |

## Notes

1. `lib.startGame` / `lib.runapp` live in a packed C-string export table inside `gbrwcore.ext` (CROSS_TARGET pattern across GWY shell packages).
2. No absolute pointer xrefs to the string offsets were found; resolution is likely via strcmp against the name table + function-pointer side table (HYPOTHESIS until guest export-call is observed).
3. Phase 6H runtime must log `[JJFB_SHELL_EXPORT]` as `kind=string_va_not_entry` and only mark `called=yes` when guest args/PC prove a real call.

- lib.* string count in image: **31**
