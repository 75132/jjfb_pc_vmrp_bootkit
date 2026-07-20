# Stage E9A-FirstFrame Verdict

**Verdict:** `FIRST_FRAME_DEMO_STABLE`

**Assist level:** `FAST_REAL_BMP_HANDLE_FALLBACK`

**NOT product success.** Goal: reproducible first frame + naturalize ``0x304BF0`` member resolve.

## Cases

| Case | Verdict | Elapsed | Level | bmp |
| --- | --- | --- | --- | --- |
| demo_visible_frame | FIRST_FRAME_DEMO_STABLE | 73.2s | FAST_REAL_BMP_HANDLE_FALLBACK | 0x3920000 |

## 0x304BF0 diagnosis

- Guest opens ``mythroad/gwy/jjfb.mrp`` successfully (VFS HIT).
- Index scan loop at ``0x304F26/0x304F7A/0x304F92`` never matches member name 鈫?infinite loop.
- Likely cause: guest index read length/alignment vs host ``first_data`` boundary.
- ``JJFB_REAL_MRP_MEMBER_BRIDGE`` decodes exact members via ``mrp_archive`` and returns handle to ``0x2D92E4``.

## Artifacts

- ``screenshots/e9a_first_frame.png``
- ``logs/e9a_first_frame_stdout.txt``
- ``reports/stage_e9a_firstframe_demo.json``
- ``reports/stage_e9a_firstframe_summary.jsonl``

