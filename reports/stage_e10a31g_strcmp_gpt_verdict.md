# Stage E10A-3.1g strcmp / GPT-tag Verdict

- **Mode**: `live`
- **run_id**: `1784666878768`
- **Primary verdict**: `METHOD0_FAIL_IS_GPT_TAG_MISMATCH`
- **True fail PC**: `0xAC2E8` (dsm:`cfunction.ext` strcmp)

## Headline

method0 `ret0=-1` is a **GPT tag mismatch**: gamelist reads 3 bytes at file-buffer offset `0x349`, gets **NUL**, then `strcmp` against literal `"GPT"` fails inside `cfunction.ext`.

Not causal: appInfo / ver=1006 / sentinel `0x2E1C24`.

## Live pointer chain (proven)

```
*(0x2D431C + 0x38) = object = 0x281EFC     ; inside cfunction ERW (+0x1AFC)
*(object + 0x1C0)  = buffer_base = 0x280CD4 ; == &cfunction_ERW[0x8D4]
memcpy(dst, buffer_base + 0x349, 3)        ; src=0x28101D → all NUL
strcmp(dst, "GPT") @ 0xAC2D0               ; fail @ 0xAC2E8
```

| Probe | Live value |
|-------|------------|
| object (r4) | `0x281EFC` |
| `*(object+0x1C0)` | `0x280CD4` |
| 16 bytes @ base | **all `00`** |
| `R9+0x8D0/8D4/8D8` word values | **all `0`** |
| buf_base vs `&(R9+0x8D4)` | **equal** (pointer is address-of field, not deref of workbuf) |
| RHS @ `0x2E845C` | `"GPT"` |

## Causal chain

```
memset(sp+0x30, 0, 16)
BL  0x2E3180(id=0x349, dst=sp+0x30, len=3)
  BLX memcpy(dst, src=0x28101D, 3)
strcmp(sp+0x30, "GPT") → -1 @ 0xAC2E8
```

## Milestones

- `TRUE_FAIL_IS_STRCMP_IN_CFUNCTION`
- `STRCMP_EMPTY_VS_GPT_LITERAL`
- `GPT_TAG_FIELD_READ_OFF_349_LEN_3`
- `GPT_SOURCE_BYTES_ARE_NUL`
- `GPT_BUF_BASE_FROM_OBJ_1C0`
- `GPT_BUF_BASE_EQUALS_ADDR_OF_ERW_8D4`
- `METHOD0_FAIL_IS_GPT_TAG_MISMATCH`
- `APPINFO_NOT_ON_TRUE_FAIL_PATH`

## Interpretation

1. GPT field source is **`*(object+0x1C0) + 0x349`**, not appInfo.
2. That buffer base is the **address of cfunction ERW+0x8D4**, whose contents are still zero.
3. Adjacent workbuf slot `R9+0x8D8` is also **0** at fail time (no published workbuf on this R9).
4. `mem_get` did allocate `0x282A54` (4MB) earlier in the same run — not selected as this GPT buffer.

## Next (E10A-3.1h, still no cfg)

1. Find **writer** of `object+0x1C0` (= `0x2820BC`) that stored `0x280CD4`.
2. Determine intended buffer (MRP/filebuf vs workbuf `R9+0x8D8` vs `mem_get` base) and why GPT@`0x349` was never filled.
3. Re-evaluate method0 `input=0` vs mythroad `filebuf` as a **publisher** of this slot (diagnostic A/B only — no force ret0).

## Artifacts

- `out/e10a31g/cfunction_ac2d0_strcmp_annotated.txt`
- `out/e10a31h/gamelist_2e3180_ptrchain.txt`
- `reports/e10a31g_strcmp_arg_trace.csv`
- `logs/e10a31g_strcmp_gpt_stdout.txt`
- `RUN_E10A31G_STRCMP_GPT.ps1`
