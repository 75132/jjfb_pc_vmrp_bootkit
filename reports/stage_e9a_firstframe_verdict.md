# Stage E9A-FirstFrame Verdict

**Verdict:** `REAL_MEMBER_BRIDGE_FIRST_FRAME`

**Assist level:** `REAL_MEMBER_BRIDGE`

**NOT product success.** Goal: reproducible first frame + naturalize `0x304BF0` member resolve.

## Cases

| Case | Verdict | Elapsed | Level | bmp | other |
| --- | --- | --- | --- | --- | --- |
| demo_visible_frame | FIRST_FRAME_DEMO_STABLE | ~47s | FAST_REAL_BMP_HANDLE_FALLBACK | 0x3920000 | 109 |
| B_real_member_bridge | REAL_MEMBER_BRIDGE_FIRST_FRAME | ~84s | REAL_MEMBER_BRIDGE | 0x3920000 | 109 |

One-command demo:

```text
.\RUN_E9A_FIRSTFRAME_DEMO.ps1
```

Bridge / naturalize path:

```text
.\RUN_E9A_FIRSTFRAME_DEMO.ps1 -Mode bridgeonly
```

## Confirmed draw chain

```text
0x2F449C → 0x2D92E4 → 0x304BF0 [JJFB_REAL_MRP_MEMBER_BRIDGE]
→ A64 store → 0x2F45A2 draw continue → 0x310BBC → mr_drawBitmap
bmp=0x3920000 (nonzero), other=109, screenshot saved
```

Member: `wy_jiao1!11!11.bmp` from original `jjfb.mrp`

- bytes=242 RGB565 11x11
- sha256=`edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13`

## 0x304BF0 diagnosis (exact)

| Item | Finding |
| --- | --- |
| Package path | `mythroad/gwy/jjfb.mrp` (VFS open/read/seek OK) |
| Blocker class | Guest index scan loop `0x304F26/0x304F7A/0x304F92` never matches → infinite wait (Case A) |
| Likely root | Guest index read length/alignment vs host `first_data` boundary |
| ABI (critical) | `0x304BF0` returns **status**: `r0==0` success, `r0==-1` miss — **not** a handle pointer |
| Caller use | `0x2D92E4` keeps object in `r4`; on `r5==0` returns `r4` as handle |
| Out-params | `r3`=object (`+0` size, `+4` pixels), `r2`=`r4+4` pixels slot |

Returning the handle pointer from the bridge made `0x2D92E4` take the failure path (`CMP r5,#0` / nonzero). Fixed bridge returns `status_r0=0` and fills the object; then `0x2D92E4` returns `r0=0x2A83C4`.

## Assist reduction (E9A)

1. **Replaced** `FAST_REAL_BMP_HANDLE` on bridge path with `JJFB_REAL_MRP_MEMBER_BRIDGE` (host `mrp_archive` decode of exact original member bytes).
2. After first bridged A64 store, `JJFB_E9A_POST_A64_DRAW_SKIP` jumps to natural continue `0x2F45A2` (skips ~15 sibling `0x2D92E4` loads that stall under tracing). Mirrors the **same real handle** into A58..A6C (still original MRP pixels, not invented).
3. Demo path still uses FAST fallback for the quick one-command visible check.

## Artifacts

- `screenshots/e9a_first_frame.png`
- `logs/e9a_first_frame_stdout.txt`
- `logs/stage_e9a_B_real_member_bridge_stdout.txt`
- `reports/stage_e9a_firstframe_demo.json`
- `reports/stage_e9a_firstframe_summary.jsonl`
- `out/e9a_resources/wy_jiao1_11_11.bmp`

## Next (not E9A scope)

- Fix guest index match inside `0x304BF0` → `NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME`
- Then drop POST_A64 sibling skip / FAST demo path
- Later: F6C/F74, C9D, DISPLAY_FIRST helper skip, natural case156/C44
