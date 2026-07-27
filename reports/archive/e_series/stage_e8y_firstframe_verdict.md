# Stage E8Y-FirstFrame Verdict

**Verdict:** `A64_RESOURCE_ASSIST_REACHED_DRAW_API`

**NOT product success.** Not `FIRST_REAL_FRAME`. Case C hit real `[JJFB_DRAW] api=mr_drawBitmap` only via structural A64 assist with **null pixels** (`bmp=0`).

## What advanced

E8X stopped at resource load `BL 0x2D92E4` (`wy_jiao1!11!11.bmp`). E8Y traced that path and proved the next gaps:

```text
0x2F449C
  → BL 0x2D92E4 (name resolve / MRP member lookup via 0x304BF0)
      → never returns in Case A/B (stuck in member I/O)
  → [Case C assist] A58/A5C/A60/A64 structural handles
      → BL 0x310BBC (r0=scratch handle)
          → mr_drawBitmap bmp=0  ← platform DRAW, empty bitmap
```

## Static / runtime ABI

| Site | Role |
| --- | --- |
| `0x2D92E4` | BMP/resource resolver; r0==0 → strlen via `*(R9+0x1450)`; returns 0x14-byte handle |
| First name | `wy_jiao1!11!11.bmp` @ `0x313514` |
| `0x304BF0` | MRP member open/read/seek helper (file I/O to `mythroad/gwy/jjfb.mrp`) |
| Mode10 `@0x2F465C` | Uses `A58/A5C/A60` then `BL 0x310BBC` |
| `0x310BBC` | Bridge into platform `mr_drawBitmap` |

Legacy lab loaded the same member successfully (`[JJFB_BMP_LOAD] ... bytes=242`); current product path opens the package but does not finish resolve before insn/time budget.

## Cases

| Case | Verdict | Elapsed | 2D92E4 | ret | 310BBC | DRAW | name / note |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A_resource_init_deep | RESOURCE_INIT_2D92E4_BLOCKED_BY_MISSING_FILE | 116.2s | yes | no | no | no | `wy_jiao1!11!11.bmp`; enters `0x304BF0` + FILEOPEN/READ/SEEK; no return |
| B_A64_writer_watch | RESOURCE_INIT_2D92E4_BLOCKED_BY_MISSING_FILE | 94.6s | yes | no | no | no | same as A (writer watch; no A64 store) |
| C_A64_struct_assist | **A64_RESOURCE_ASSIST_REACHED_DRAW_API** | 43.4s | no (skipped) | — | **yes** | **yes** | structural handles; `mr_drawBitmap bmp=0` |

### Case C detail (best, tagged assist)

```text
[JJFB_FAST_A64_RESOURCE_ASSIST] after A58=0x3910000 ...
[JJFB_E8X_DRAW_PATH] pc=0x310BBC r0=0x3910080 r2=0xB r3=0xA
[JJFB_E8Y_310BBC] hit=1
[JJFB_DRAW] api=mr_drawBitmap bmp=0x0 x=0 y=11 w=10 h=10
[JJFB_E8U_DRAW] api=mr_drawBitmap ... note=real_platform_draw
```

No nontrivial screenshot / `JJFB_E8U_FIRST_REAL_FRAME`.

## Next gap (E8Z candidate)

1. **Real member resolve:** finish `0x2D92E4` / `0x304BF0` so `wy_jiao*!w!h.bmp` loads from `jjfb.mrp` (legacy had 242-byte BMP) — populate real handles into A58/A5C/A60/A64.
2. Or: once real pixels exist, survive `0x310BBC` → nontrivial framebuffer delta → screenshot.

Do **not** claim product success for `JJFB_FAST_A64_RESOURCE_ASSIST`.

## Artifacts

- `reports/stage_e8y_firstframe_summary.jsonl`
- `reports/stage_e8y_firstframe_verdict.md`
- `logs/stage_e8y_*_stdout.txt`
- `logs/e8y_first_real_draw_stdout.txt` (Case C)
- `out/e8y_tmp/e8y_decode_report.json`
- `tools/e8y_2d92e4_decode.py`
- `RUN_E8Y_FIRSTFRAME.ps1`
