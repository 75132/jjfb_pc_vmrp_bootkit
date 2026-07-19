# Stage D6 — PRODUCT_DESCRIPTOR_DIRECT_BOOT

- **task:** DESCRIPTOR / RUNTIME — cfg36 → original `gwy/jjfb.mrp` without gamelist shell
- **verdict:** `COMPLETE` for product path gate; game chain `PARTIAL` (mrc_init not yet)
- **source tag:** `descriptor_launcher` (honest — **not** `native_shell`)

## Proven

| Gate | Result |
|------|--------|
| `gwy_launcher validate` cfg36 / hash | PASS |
| DSM target `gwy/jjfb.mrp` (not gbrwcore) | PASS |
| No `JJFB_GAMELIST_STARTED` / no host_runapp_equiv | PASS |
| `mrc_loader.ext` EXTRACTED → ENTRY_CALLED | PASS |
| `cfunction.ext` → `robotol.ext` via `reg_primary` | PASS |
| jjfb sha256 unchanged | PASS |
| `audit_launcher_core.py` | ok |

## Not yet

| Gate | Result |
|------|--------|
| `robotol.ext` MODULE ENTRY_CALLED (full map) | not proven in 30s log |
| `[JJFB_MRC_INIT]` / guest mrc_init | **0** |
| natural DRAW/REFRESH on product track | **0** in this run |

Note: log lines `BOOTSTRAP_SEQ event=ROBOTOL_ENTER` / `HELPER_ABI stage=ROBOTOL_ENTER` currently also fire for `mrc_loader` / DSM — do **not** treat as robotol.ext game enter.

## Artifacts

- `RUN_D6_DESCRIPTOR_DIRECT.ps1`
- `reports/d6_descriptor_direct_boot.md`
- `logs/d6_descriptor_direct_stdout.txt`
- `launch_runtime.c`: `source=descriptor_launcher` spawn tags

## Next (Stage E/F on product track)

Smallest: keep `source=descriptor_launcher`, clear shell envs, extend boot until:

```text
[MODULE_REGISTRY] ... robotol.ext ... state=ENTRY_CALLED
[JJFB_MRC_INIT] ...
```

If blocked on mrc_loader ER_RW / R9 (`R9_SWITCH_BLOCKED`), fix **generic** platform publication for loader package — not gamelist UI. Parallel sanity: same runner with `wxjwq.mrp`.
