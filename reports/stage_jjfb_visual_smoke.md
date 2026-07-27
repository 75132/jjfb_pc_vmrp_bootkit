# Visual smoke gate (jjfb.mrp first frame)

## Verdict

**PASS** after hot-path quieting. Product launcher reaches guest DrawFP of `loadingbar!201!29.bmp` and writes a nonempty SDL screenshot.

## Changes this round

1. **`bridge.c` `_DispUpEx` / `mr_drawBitmap`** — default present-only; per-frame SHA256 / CSV / verbose logs require `JJFB_FB_HASH_TRACE=1` or `JJFB_REFRESH_TRACE=1`.
2. **`gwy_ext_obs_note_product_framebuffer`** — first-only markers; CSV only on first capture or hash-trace.
3. **`main.c` timer** — `[PLATFORM_TIMER]` gated by `JJFB_TIMER_TRACE=1`.
4. **`jjfb_launcher_main.c`** — heavy FFP CSV traces only with `--debug` / `--diagnostic`.
5. **`product_runtime_progress.c`** — dedupe noisy `platform_strlen/strcpy/strcmp/memcpy` milestones (was flooding jsonl).
6. **`RUN_JJFB_VISUAL_SMOKE.ps1`** — daily jjfb visual gate (HoldSeconds + screenshot/progress check).

## Evidence

| Marker | Result |
|--------|--------|
| `guest_entry_called` target=`gwy/jjfb.mrp` | yes |
| `loadingbar!201!29.bmp` member_loaded | yes |
| `drawfp_first_drawn` | yes |
| screenshot `out/vmrp_run/screenshots/launcher_first_frame.bmp` | ~230454 bytes |

## How to retest

```powershell
.\RUN_JJFB_VISUAL_SMOKE.ps1 -HoldSeconds 45
# or interactive:
.\RUN_JJFB_LAUNCHER.ps1
```

## Next (lifecycle, not frame gate)

Still open from Task 13: natural B54 `event_code=15` → `E6C_NATURAL_STORE` → C0 past `0x2FEC3C` → `B70` → `UI_MODE=0x45`. Frame path remains the default product acceptance for daily visual tests.
