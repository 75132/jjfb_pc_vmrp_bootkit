# Research track — E10A-3.1r verdict (static first cut)

- **Status:** static mapping complete; live callee unresolved
- **PC:** `0xA1B8C` / file `0x21B8C` in cfunction.ext
  `sha256=8f85e3cf8f0ed4a8e09eb658f9daba566989fbc06c510e4e76cd474dd275cad5`
- **Function entry:** `0xA1B50`
- **Fail instruction:** `BLX r1` with `r1 = *(*(r9+lit) + 0x50)` — **not** immediate −1
- **source_class (candidate):** `PLATFORM_TABLE`
- **Promotion to product:** **blocked** until live callee ABI + cross-target evidence
- **Artifacts:** `research/e10a31r/*`
- **Runner:** `RUN_E10A31R_A1B8C_PROVENANCE.ps1` (GwyResearch only)

Do not continue 3.1s/t/u SMSCFG string chasing for this PC.
