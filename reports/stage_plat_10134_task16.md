# Task 16 — platform `0x10134` RGB565 construct (post-Task-15 gap)

## Verdict

**Advance.** Natural chrome path reaches **DrawFP** and presents
`loadingbar!201!29.bmp` via the product stack (no FAST splash / AC8 / FORCE ui_mode).

Best empty-first run: `out/path_a_record_task16/B9` (100s, no early bind):

| Marker | Count |
|--------|------:|
| postmatch (`loadingbar` / `bar` / `textbar`) | yes (3 members) |
| `PLATFORM_10134` | 3 |
| `bind_10134` | **0** (correct — guest owns handle store) |
| `mr_free invalid` / `P3_FAULT` | **0** |
| `DRAW_FP_CALL_ENTER` / `DRAWN` | 1 / 1 |
| `FIRST_REAL_FRAME_REACHED` | yes |
| `LRT_2FC26C_LEAVE` / `LRT_EVENT15` | still 0 |

Observed blit:

```
DRAW_FP_ARGS pixels=0x6BBBB4 dst=19,220 wh=201x29
JJFB_E8Z_SPRITE_BLIT x=19 y=220 w=201 h=29
screenshots: out/path_a_record_task16/B9/e8z_first_real_frame*.bmp
```

Guest hits `_DrawBitmap` at **`0x2EC6B9`** (Thumb). Prior “0x2EC6B8=0” searches were false negatives.

## bar / textbar (not a DrawFP gap)

After textbar `0x10134`, guest only DrawFP’s **loadingbar** handle (`0x6BBB98` @ `(19,220)`).
`bar!16!18` and `textbar!120!30` are loaded+constructed for later progress/text; they are
not second/third DrawFP calls in this window. Not a missing-10134 bug.

## Root cause (Task 15 → 16)

After natural postmatch, guest called `0x10134(size=W*H*2)` and got
`default_status` → `ret=0`, then never reached draw.

Legacy/bridge ABI (`my_mallocExt`):

- `R1/app = W*H*2`
- Return **mallocExt USER** pointer (header word at `user-4`)
- Prefer fill from the just-decoded member

## Delivered

| Piece | Location |
|-------|----------|
| `0x10134` classify | `platform_send_app_event.c` (`JJFB_PLATFORM_10134_CONTRACT`) |
| mallocExt USER alloc + cache copy | `gwy_ext_obs.c` ALLOC path |
| size→pixels cache | `platform_mrp_resource_note_pixels` / `_pixels_by_bytes` |
| Handle pixels deferred (0) | avoid DSM `mr_free` of non-heap map VA |
| null-src memcpy noop after 10134 | `platform_memory_ops.c` |
| `0x12340` glyph metric write | `platform_send_app_event.c` |
| `0x11F00` STATUS ack | `platform_send_app_event.c` (glyph blit still optional) |
| Screenshot dir | `out/vmrp_run/screenshots/` |
| Unit coverage | `test_platform_send_app_event` 10134 alloc/copy classify |

## Pitfalls fixed

1. Returning raw map VA `0x3920000` → guest `@0x3045E4` reads `*(ptr-4)` → unmapped `0x391FFFC`.
2. Double header (`alloc(n+4)` on top of `my_mallocExt`) → bad free.
3. Installing map/heap-synthetic pixels into the handle → DSM `mr_free invalid`.
4. Deferring handle.pixels=0 → guest `memcpy(new, 0, n)` failed; 10134 prefill + memcpy noop.
5. **Early `bind_10134` after alloc** (B8): guest `@0x2D9590` still treats `handle.pixels` as OLD
   and calls `3045E5(free)`. Binding early made `old==new` → frees the fresh buffer →
   `mr_free invalid` ×3. Guest stores new into the object after free returns — do not poke early.

## Still open

- Real `0x11F00` glyph/present side effects (ack only today).
- `leave_2FC26C` / event15 / E6C still missing on empty-first.
- Product **record-first** still C0-gated (`0x2FEC3C`) — Task 13 if switching back.
- HWND capture still pending (`NOT_HWND_CAPTURE_YET`).

## Env

- `JJFB_PLATFORM_10134_CONTRACT=1`
- `JJFB_PLATFORM_MRP_RESOURCE=1`
- `JJFB_DRAWFP_BINDING=1`
- `JJFB_PATH_A_EMPTY_FIRST=1` for this experiment only (not product default)
