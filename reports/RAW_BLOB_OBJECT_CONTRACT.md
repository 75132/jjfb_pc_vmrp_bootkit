# RAW_BLOB Object Contract (P3-3 / P6 status)

## Status (authoritative)

```text
RAW_BLOB_CONTRACT = HOST_PROBE_CANDIDATE
RAW_BLOB_NATURAL_FLOW_VALIDATED = NO
```

Natural product census (70s Layer-1) showed:

```text
ani_candidate = 0
text_candidate = 0
first_ani = false
first_txt = false
```

Host can allocate guest memory and fill the candidate layout below, but this has
**not** been validated through a natural ANI/TXT request → guest read → free lifecycle.

Do not treat RAW_BLOB as a closed product acceptance item until a natural
`0x304BF0` request for `.ani` / `.txt` appears (Successor Gate).

## Candidate layout (host probe)

| Offset | Type | BITMAP | RAW_BLOB candidate |
|--------|------|--------|--------------------|
| +0x00 | u32 | decoded RGB565 bytes | decoded raw bytes |
| +0x04 | u32 | pixels VA (0 until 0x10134) | guest mallocExt USER ptr |
| +0x08 | u16 | width | 0 |
| +0x0A | u16 | height | 0 |
| +0x10 | u8 | flag=1 | flag=1 |

RAW must not enter PendingBitmap / 0x10134 FIFO.

## Next gate

Only after `unique resource count >= 6` with a natural 6th `0x304BF0` request
(preferably `caller_lr != 0x2D93D1` or non-empty active jjfbol package) should
ANI_RAW_LOADED / 0x11F00 / animation frame changes be acceptance-weighted.
