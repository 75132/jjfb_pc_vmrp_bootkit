# Phase 6G — startGame/runapp entry map

Evidence: **TARGET_OBSERVED** string/file offsets in shell packages.
Runtime VA = module load base + file offset (base varies per session).

| module | member | needle | file_offset | boundary_lo | boundary_hi | caller | input_args | executed |
|---|---|---|---|---|---|---|---|---|
| gbrwcore.mrp | `gbrwcore.ext` | `lib.startGame` | `0x223D4` | `0x21BD4` | `0x225D4` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gbrwcore.mrp | `gbrwcore.ext` | `lib.runapp` | `0x224C0` | `0x21CC0` | `0x226C0` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gbrwcore.mrp | `gbrwcore.ext` | `startGame` | `0x223D8` | `0x21BD8` | `0x225D8` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gbrwcore.mrp | `gbrwcore.ext` | `runapp` | `0x224C4` | `0x21CC4` | `0x226C4` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gbrwcore.mrp | `gbrwcore.ext` | `isFileOnServerNewer` | `0x22458` | `0x21C58` | `0x22658` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gbrwcore.mrp | `gbrwcore.ext` | `checkmrpver` | `0x22448` | `0x21C48` | `0x22648` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gamelist.mrp | `gamelist.ext` | `napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink` | `0x1412C` | `0x1392C` | `0x1432C` | static_xref_pending_disasm | cfg36_fields | live_see_phase6g_stdout |
| gamelist.mrp | `gamelist.ext` | `gwyblink` | `0x14120` | `0x13920` | `0x14320` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gamelist.mrp | `gamelist.ext` | `gwyblink` | `0x14168` | `0x13968` | `0x14368` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |
| gamelist.mrp | `gamelist.ext` | `gwyblink` | `0x141B0` | `0x139B0` | `0x143B0` | static_xref_pending_disasm | lib_export_name | live_see_phase6g_stdout |

## Notes

- `lib.startGame` / `lib.runapp` live in `gbrwcore.ext` string table (Phase 6F).
- Phase 6G host chain: shell DSM (`gbrwcore`) → no_update stub → `bridge_dsm_mr_start_dsm(mythroad/gwy/jjfb.mrp)` as runapp-equivalent.
- Guest-native call of lib.startGame remains preferred when shell reaches it.
