# P0–P2 JJFB Direct Visual Stable — phase gate results

## Verdict

All phases **PASS**. Product path stays on real DrawFP / `gwy/jjfb.mrp`; no Event15/E6C injection.

| Phase | Result | Notes |
|-------|--------|-------|
| 0 baseline | PASS | First frame SHA `c789a129…bffda` |
| 1 resource_root | PASS | Default/explicit `240x320`; `--root` + profile SHA |
| 2 catalog-only | PASS | packages=59 versions=60 downVersion=1006 |
| 3 context lookup | PASS | `jjfbol_scope` + composite POSTMATCH + lookup order |
| 4 pending FIFO | PASS | reserve/commit; bind_10134 forbidden |
| 5 120s diag | PASS | 5 members; pending_fifo; Layer2 distinct_sha=2; no inject |
| 6 aux regress | PASS | scan + unit tests (`guest_vfs`, `mrp_resource`, catalog, root, 10134) |

## Artifacts

- `out/visual_baseline/phase0/`
- `out/visual_baseline/gate_*` / `diag_*`
- `tools/JjfbLayer1Gate.ps1`
- `research/runners/RUN_JJFB_VISUAL_STABLE_DIAG.ps1`
- `research/runners/test_jjfbol_catalog_real.ps1`
