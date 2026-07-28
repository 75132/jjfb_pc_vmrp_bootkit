# RAW_BLOB Object Contract (P3-3)

Closed from bitmap handle ABI (Task 15/16) plus 0x304BF0 entry observe for
`target!65!25.bmp`, `target.ani`, and `.txt` requests.

## Call ABI (`0x304BF0`)

| Reg | Role |
|-----|------|
| r0 | package path (guest C-string) |
| r1 | member name (guest C-string) |
| r2 | optional size/pixels slot |
| r3 | **caller-owned out-object** |
| return r0 | `0` = ok, non-zero = fail |

## Out-object layout (poke 0x14)

Shared between BITMAP and RAW_BLOB:

| Offset | Type | BITMAP | RAW_BLOB |
|--------|------|--------|----------|
| +0x00 | u32 | decoded RGB565 byte count | decoded raw byte count |
| +0x04 | u32 | pixels VA (**0 until 0x10134**) | data VA (**prefilled guest mallocExt USER ptr**) |
| +0x08 | u16 | width | **0** |
| +0x0A | u16 | height | **0** |
| +0x10 | u8 | flag = 1 | flag = 1 |

## Ownership

- **BITMAP:** host pending owns `host_pixels` until 0x10134 commit; guest then owns mallocExt buffer and writes `handle+4`.
- **RAW_BLOB:** host allocates via `gwy_ext_obs_guest_malloc0`, copies bytes, stores VA at `out+4`. Guest may `mr_free` that USER ptr. **RAW must not enter PendingBitmap / 0x10134 FIFO.**

## Return status

`r0 = 0` after restore of entry SP/R4–R11/R9 and PC=`lr|1`.

## Evidence

- Bitmap fields: product stub + Task 15/16 reports.
- RAW differentiation: size at +0, data at +4, zero w/h, no 10134 enqueue; `ANI_RAW_LOADED` / `ANI_MAGIC_CHECK` for JCANI011.
- Atlas alias rules (P3-5) apply only to exact ANI-referenced names.
