# D6 — Descriptor Direct Boot

- **source:** `descriptor_launcher` (not `native_shell`)
- **cfg_index:** 36
- **target:** `gwy/jjfb.mrp`
- **hash:** `52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036` unchanged
- **runner:** `RUN_D6_DESCRIPTOR_DIRECT.ps1` (30s)

## Gates

| Check | Result |
|-------|--------|
| hash_unchanged | OK |
| target_jjfb | OK |
| source_descriptor | OK |
| no_gbrwcore_dsm | OK |
| no_gamelist_started | OK |
| no_host_runapp_equiv | OK |
| mrc_loader | OK |
| robotol resolve (reg_primary) | OK |
| ROBOTOL_ENTER log (name overloaded) | OK / see verdict caveat |
| mrc_init | **FAIL / not reached** |

## Key log excerpts

```text
[GWY_LAUNCH] target=gwy/jjfb.mrp
[EXT_RESOLVE] ... mrc_loader.ext ... strategy=exact ... HIT
[REG_PRIMARY] ... primary=robotol.ext
[EXT_RESOLVE] ... cfunction.ext resolved=robotol.ext strategy=reg_primary ... HIT
[MODULE_REGISTRY] ... mrc_loader.ext ... state=ENTRY_CALLED
```

## Relation to research track

D1–D5b proved gamelist timer ≠ cfg36 trigger. D6 bypasses abandoned GWY list shell and starts the original game MRP via LaunchDescriptor — product Level B→C path.
