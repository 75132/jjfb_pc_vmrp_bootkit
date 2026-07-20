# Stage E8Z-FirstPixel Verdict

**Verdict:** `FIRST_REAL_FRAME_REACHED`

**NOT product success** (DISPLAY_FIRST + FAST_REAL_BMP_HANDLE assist). But this is a real visible frame: original `jjfb.mrp` member pixels through real `mr_drawBitmap`.

## What landed

```text
FAST_REAL_BMP_HANDLE
  → handle+4 = 0x3920000 (242B RGB565 from wy_jiao1!11!11.bmp)
  → 0x2F449C → 0x310BBC (r0=handle)
  → [JJFB_DRAW] mr_drawBitmap bmp=0x3920000 x=0 y=11 w=11 h=10
  → sprite blit (pitch=w) other=109
  → screenshots/e8z_first_real_frame.png
```

Observed:

```text
[JJFB_E8Z_BMP_LOAD] ... bytes=242 w=11 h=11 note=real_jjfb_mrp_member
[JJFB_DRAW] api=mr_drawBitmap bmp=0x3920000 ...
[JJFB_E8Z_PIXEL_READ] bmp=0x3920000 bytes=220 first=0xF81F
[JJFB_E8Z_SPRITE_BLIT] ... other=109 key=1
[JJFB_FIRST_REAL_FRAME_REACHED] path=.../e8z_first_real_frame.bmp
```

## Resource probe

| Field | Value |
| --- | --- |
| Member | `wy_jiao1!11!11.bmp` |
| Source | original `game_files/mythroad/320x480/gwy/jjfb.mrp` |
| Decoded | 242 bytes raw RGB565 (11×11) |
| SHA256 | `edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13` |
| Artifact | `out/e8z_resources/wy_jiao1_11_11.bmp` |
| Report | `reports/e8z_resource_probe.json` |

No invented pixels. No MRP/EXT edits.

## Cases

| Case | Verdict | Elapsed | Notes |
| --- | --- | --- | --- |
| A_member_resolve_deep | `MEMBER_RESOLVE_304BF0_STILL_BLOCKED` | 116s | Enters `0x2D92E4` → `0x304BF0` FILE I/O; no complete return |
| **B_member_fastpath_real_bytes** | **`FIRST_REAL_FRAME_REACHED`** | 67.5s | Real handle+4 pixels; `mr_drawBitmap bmp=0x3920000`; screenshot saved |
| C_real_bmp_handle_draw | skipped | — | Stopped early after B success |

## Technical notes

1. **`0x310BBC`** loads `LDR r0,[handle,#4]` into `mr_drawBitmap` — E8Y structural stubs left `+4=0`.
2. **Unicorn scratch ≠ MRP heap:** `getMrpMemPtr(0x3920000)` is invalid; E8Z reads pixels via `uc_mem_read` then sprite-blits with pitch=`w` (Mythroad LCD pitch path would OOB).
3. Case A still proves natural `0x304BF0` member resolve remains the next gap for an unassisted path.

## Artifacts

- `screenshots/e8z_first_real_frame.png` (+ before/after)
- `reports/stage_e8z_firstpixel_summary.jsonl`
- `logs/e8z_first_real_draw_stdout.txt` (Case B)
- `RUN_E8Z_FIRSTPIXEL.ps1`
- `tools/e8z_resource_probe.py`
