# Task 15 — platform_mrp_resource + DSM strcmp (E9E legal port)

## Verdict

**Partial advance.** Product now completes a **natural** MRP member postmatch for
`loadingbar!201!29.bmp` via true `strcmp` + `MrpArchive` exact decode. Still **no**
`DRAW_FP_CALL` / `0x2EC6B8` / `leave_2FC26C` — next gap is post-handle draw path.

## Historical reference (user “能显示”)

- Visual wins were **E9E/E9H/E9U** (commits around `145198b` / `e2dfd1f`), not `4bdb4b5`.
- Mechanism: FAST splash assists + host postmatch decode → `mr_drawBitmap` → HWND.
- **Not product.** This task ports only the legal piece: archive exact decode after
  natural name match (Task 12 Phase C).

## Delivered

| Piece | Location |
|-------|----------|
| DSM `strcmp` @ `0xAC2D0` | `platform_memory_ops.c` (true compare, never force-equal) |
| Match hook | `platform_strcmp_set_match_hook` → MRP postmatch |
| `platform_mrp_resource` | `src/platform/platform_mrp_resource.c` |
| Entry ABI capture | CODE hook @ `0x304BF0` (frame only) |
| Unit test | `tests/unit/test_platform_mrp_resource.c` — **PASS** |

### Regression sample (Task 12 §14)

```
wy_jiao1!11!11.bmp decoded=242
sha256=edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13
```

## Empty-first run (`out/path_a_record_task15/B`, 70s)

| Marker | Count |
|--------|------:|
| `PLATFORM_STRCMP` | 13 |
| `PLATFORM_MRP_RES` loaded | 1 |
| `postmatch_complete` | 1 |
| `LRT_2FC26C_ALT` | 1 |
| `DRAW_FP_CALL` / `2EC6B8` / leave | **0** |

Observed load:

```
loadingbar!201!29.bmp
  offset=3782 stored=1385 decoded=11658 w=201 h=29
  handle=0x6BBB98 pixels=0x3920000
  ret_lr=0x2D93D1  (same return site as E9E)
```

## Next gap

After postmatch, guest continues `0x2D9546` → plat `0x10134(size=0x2D8A)` **ret=0**,
then re-enters `0x2D92E4` with `r0=0` and never reaches `0x310BBC` / DrawFP.

Do **not** revive FAST splash / AC8 force / V67 skip-BLX.

Next: prove `0x10134` / handle→draw ABI (generic platform), or observe the next
natural member request and complete it the same way.
