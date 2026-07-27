# Stage E8X-FirstFrame Verdict

**Verdict:** `2F2854_REACHED_RESOURCE_LOAD_NEXT_GAP`

**NOT product success.** Not `FIRST_REAL_FRAME`. No `[JJFB_DRAW]` / screenshot.

## What advanced

E8W stopped at first hit of `0x2F2854` with zero args. E8X traced through:

```text
0x2F2854 (wrapper, r0 always 0)
  → 0x2EA188 (mode from stack slot = 10)
    → 0x2F449C (real draw worker)
      → BL 0x2D92E4 @ 0x2F44C4   ← RESOURCE LOAD (R9+0xA64 == 0 init path)
```

No reach of `0x310BBC` / `mr_drawBitmap` / framebuffer delta yet.

## Static ABI (confirmed at runtime)

| Site | Role |
| --- | --- |
| `0x2F2854` | Thin wrapper → `BL 0x2EA188`; caller `r0` always forced 0 |
| `0x2EA188` | Dispatches on stack mode; mode `10` → `BL 0x2F449C` |
| Caller `r2` | `BL 0x2F9970` = `*(R9+0x830)` |
| Caller `r1` | layout delta (still 0 without real F74/`0x2F9964` path) |
| `0x2F99D0` | Real F74 producer @ `0x2E891A`; skipped when F74 already nonzero |
| `0x2F449C` | If `*(R9+0xA64)==0` → string/table init via `0x2D92E4` |

## Cases

| Case | Verdict | Elapsed | 2F2854 | 2F449C | resource | 310BBC | DRAW | class | R9_830 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C_descriptor_assist | 2F2854_REACHED_RESOURCE_LOAD_NEXT_GAP | 116.1s | yes | yes | yes | no | no | 2F2854_NONZERO_ARGS | 0xF0 |
| A_2F2854_deep | 2F2854_REACHED_RESOURCE_LOAD_NEXT_GAP | 95.5s | yes | yes | yes | no | no | 2F2854_ZERO_LAYOUT_ARGS | 0x0 |
| B_real_F74_producer | 2F2854_REACHED_RESOURCE_LOAD_NEXT_GAP | 95.5s | yes | yes | yes | no | no | 2F2854_ZERO_LAYOUT_ARGS | 0x0 |

### Case C (best)

- `JJFB_FAST_F74_DESCRIPTOR_ASSIST` at gate-retry: `R9+0x830=0xF0` (240), `0x818=0xF0`, `0x81C=0x140` (320)
- Callsite `0x2E89A8`: `r2=0xF0` (was 0 in E8W)
- Still `R9+0xA64/A68/A6C=0` → enters `0x2D92E4` resource/string init at `0x2F44C4`
- `r1` layout still 0 (needs real F74/`0x2F9964` or layout math)

### Case B

- Called real `0x2F99D0` with `r0=F70_scratch`, `r1=0xD2`
- Returned `r0_after=0`, F74 stayed 0 → producer branch unmet for table build
- Still reached draw path via F70-open + later assist

## Exact next blocker

```text
Inside 0x2F449C:
  *(R9+0xA64)==0 → BL 0x2D92E4 resource/string table init
Next gap = survive/complete that init (or reach natural A64 producers)
         → then 0x310BBC / platform draw / [JJFB_DRAW]
```

Do **not** fake DRAW or paint framebuffer. Prefer natural writers of `R9+0xA64` / resource path over inventing bitmap pixels.

## Artifacts

- `reports/stage_e8x_firstframe_summary.jsonl`
- `logs/stage_e8x_*_stdout.txt`
- `logs/e8x_first_real_draw_stdout.txt`
- `out/e8x_tmp/e8x_decode_report.json`
