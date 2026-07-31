# Phase 6J — MRPGCMAP entry decode

Header entry candidate policy in core: `guest_code_base + 8` (**DOCUMENTED** image+8 / 8-byte `MRPGCMAP` prefix).

## Live anchors (if stdout provided)

- shell EXT base: `0x2eb7e0`
- header_entry_candidate (live): `0x80008`
- observed_first_pc: `0xa4178`
- fault function_start: `0x30ccd0`
- fault memory_access_pc: `0x30ccf8`

## `gbrwcore.mrp:gbrwcore.ext`

- magic: `MRPGCMAP`
- prefix_hex: `4d525047434d4150`
- code_size: `147196`
- entry_offset_candidate: `8` (evidence=DOCUMENTED image+8 / 8-byte MRPGCMAP prefix)
- entry_va_if_base=0x2eb7e0: `0x2eb7e8`
- matches live header_entry_candidate: `no`
- first_pc file_offset: `0x-247668`
- first_pc == header+8: `no` (TARGET_OBSERVED mismatch → WRONG_ENTRY_SELECTION)
- fault function file_offset: `0x214F0`

## `gamelist.mrp:gamelist.ext`

- magic: `MRPGCMAP`
- prefix_hex: `4d525047434d4150`
- code_size: `91532`
- entry_offset_candidate: `8` (evidence=DOCUMENTED image+8 / 8-byte MRPGCMAP prefix)

## `gbrwshell.mrp:gbrwshell.ext`

- magic: `MRPGCMAP`
- prefix_hex: `4d525047434d4150`
- code_size: `45216`
- entry_offset_candidate: `8` (evidence=DOCUMENTED image+8 / 8-byte MRPGCMAP prefix)

## `jjfb.mrp:robotol.ext`

- magic: `MRPGCMAP`
- prefix_hex: `4d525047434d4150`
- code_size: `253420`
- entry_offset_candidate: `8` (evidence=DOCUMENTED image+8 / 8-byte MRPGCMAP prefix)

## `jjfb.mrp:mrc_loader.ext`

- magic: `MRPGCMAP`
- prefix_hex: `4d525047434d4150`
- code_size: `232`
- entry_offset_candidate: `8` (evidence=DOCUMENTED image+8 / 8-byte MRPGCMAP prefix)

## `wxjwq.mrp:mrc_loader.ext`

- magic: `MRPGCMAP`
- prefix_hex: `4d525047434d4150`
- code_size: `232`
- entry_offset_candidate: `8` (evidence=DOCUMENTED image+8 / 8-byte MRPGCMAP prefix)

## Interpretation

- If `first_pc` is a callback continuation (e.g. after `_mr_c_function_new`) and not `image+8`, publication/init at header entry may never run (**HYPOTHESIS** → conclusion B).
- `mrc_loader.ext` size is tiny; compare jjfb vs wxjwq hashes in cross-target report.
