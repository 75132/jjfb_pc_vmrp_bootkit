# P3–P5 JJFB Visual Completeness — stage notes

## Implemented

| Phase | Deliverable |
|-------|-------------|
| P3-0 | `platform_mrp_resource_census` → `reports/p3_resource_request_census.csv` + summary.json; FIRST_REJECTED/ANI/TXT |
| P3-1 | `jjfb_resolve_bitmap_geometry` (no 11×11); pending `host_pixels` + `pending_copy_pixels` |
| P3-2 | `GwyMrpResourceKind` BITMAP / RAW_BLOB / MODULE / UNKNOWN |
| P3-3 | `reports/RAW_BLOB_OBJECT_CONTRACT.md` + `[RAW_BLOB_ABI_ENTRY]` traces |
| P3-4 | RAW complete for `.ani`/`.txt` via guest mallocExt; `ANI_RAW_LOADED` / `ANI_MAGIC_CHECK` |
| P3-5 | Three audited atlas aliases |
| P4 | Product `JjfbUc2Font` + `platform_text_api` for 0x11F00/0x12340 (`font_fallback=0`) |

## Constraints kept

Direct `gwy/jjfb.mrp`, no Event15/E6C inject, no ANI-as-RGB565, no host fake UI.
