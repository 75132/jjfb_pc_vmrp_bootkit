# Phase 6M — chunk_field_04 source

## DOCUMENTED layout (do not conflate)

- `mr_c_function_st + 0x0C` = `mrc_extChunk*` (watched `P+0xC`)
- `mrc_extChunk_st + 0x04` = `init_func` (registry `chunk_field_04`)

## Live

- CHUNK_FIELD04 tags: `4`
- MISSING tags: `yes`
- EXT_REGISTER chunk_field_04=0: `yes`
- writer NONE_BEFORE_SELECT: `yes`

### Interpretation

`chunk_field_04=0` means no observed write of **chunk+4 (init_func)** before DSM select, which is consistent with **no published `mrc_extChunk` object** (P+0xC stays 0 / zeroed). It is not the same field as P+0xC.

