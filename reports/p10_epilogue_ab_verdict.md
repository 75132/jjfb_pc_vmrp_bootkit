# P10 epilogue A/B matrix verdict

**Date:** 2026-07-29 23:40:57  
**Status:** MATRIX_COMPLETE — do **not** promote `epilogue` to product default.

## Verdict (short)

| question | answer |
|----------|--------|
| Is standalone Unicorn stack proof enough? | **No** — only proves bytes/SP math. |
| Is `epilogue` + R0=0 better than `direct_lr`? | **No** — equivalent on Layer-1 / 5 BMP / SP delta 0 (same frame SHA). |
| Is `epilogue` + R0=handle better / successor-producing? | **No** — **worse**: stalls at 3 resources, no first-frame BMP, early `P3_FAULT` @ `0x30736A`. |
| Promote `epilogue` to product default? | **No.** Keep `direct_lr`. |
| Is `0x304BF0` resume the current primary blocker? | **Unlikely for the 6th-resource gap** — A and B both stop at 5 with no 6th natural resource. |

## Build identity (source ↔ binary)

| field | value |
|-------|-------|
| git commit | `c2c207a408493271f672a6e38744e00d1ad9f3eb` (tree DIRTY — P10 research files uncommitted) |
| build timestamp | 2026-07-29 23:25:24 |
| main.exe SHA256 | `de9defaee5ec688c6de15d0c057f1bb04d88bccd3cec7cf8280d9111e4d0fd19` |
| JJFB_Launcher.exe SHA256 | `452b68f8218d508c54233e0c98d7a4e429d0720e11779d2b8736713b8050a27b` |
| SkipBuild on matrix cells | YES (full `RUN_BUILD` + `RUN_BUILD_VMRP` before matrix; no SkipBuild there) |
| `RUN_TESTS.ps1` | anti-drift audit FAIL (pre-existing 121 findings); product unit tests PASS separately |
| product default resume_mode | `direct_lr` (unchanged) |
| identity file | `reports/p10_epilogue_build_identity.txt` |

## Preconditions completed

1. `p9_disasm_304bf0_epilogue.py` — exit 0; prints `prologue_total=224`, `epilogue_total=224`, `resume_pc=0x304C4B`
2. `RUN_P10_VERIFY_EPILOGUE.ps1` — PASS → `reports/p10_epilogue_stack_verify.txt`
3. Full rebuild without SkipBuild — `RUN_BUILD.ps1`, `RUN_BUILD_VMRP.ps1` (Mode=Gwy)
4. Matrix runner — `research/runners/RUN_P10_EPILOGUE_AB_MATRIX.ps1`

## Matrix (2×150s per group, this build)

| group | resume_mode | EPILOGUE_R0 | Layer-1 | resources/run | SP delta ok | alive@150s | 6th | frame SHA |
|-------|-------------|-------------|---------|---------------|-------------|------------|-----|-----------|
| A | direct_lr | n/a | 2/2 PASS | 5,5 | 5/5,5/5 | YES,YES | NO | `c789a129…bffda` |
| B | epilogue | 0 | 2/2 PASS | 5,5 | 5/5,5/5 | YES,YES | NO | `c789a129…bffda` (identical) |
| C | epilogue | handle | 0/2 FAIL | 3,3 | 4/4,4/4 | YES,YES | NO | missing |

Raw rows: `reports/p10_epilogue_ab_matrix.csv`  
Per-cell archives: `out/p10_epilogue_ab/`

### Observed resume lines (vm stdout)

- A: `resume=direct_lr` ×5 (all five BMPs)
- B: `resume=epilogue` ×5, PC=`0x304C4A`, SP contract held (delta 0)
- C: `resume=epilogue` ×3 then stall — R0=handle is **not** a safe drop-in for `direct_lr`'s R0=0

### Fault timing (vm stdout)

- A/B: late `P3_FAULT` **after** 5 resources + first frame (Layer-1 still PASS). Example A: `pc=0x2D960E addr=0x1E205`.
- C: early `P3_FAULT` **after** 3 resources, **before** first frame: `pc=0x30736A addr=0x8`. Reproduced on both C reps.

### Other signals (all groups)

- `0x11F00` / `0x12340`: present
- family `0x1E209` / case 9: present
- invalid free / alloc storm: not observed as blockers
- `FIRST_POST_UI_PC`: `0x0` in flushed P6 verdict for these holds (no 6th resource path)
- Product default left at `direct_lr`; no family ABI / Event15 / E6C changes in this pass

## Reading

1. Independent Unicorn proof ≠ product acceptance.
2. `epilogue`+R0=0 ≈ `direct_lr` on the acceptance surface that matters (Layer-1, 5 BMP, SP delta 0, same BMP SHA). That is **equivalence**, not superiority.
3. `epilogue`+R0=handle is a **regression** on this binary — do not use as product or as “successor” hope.
4. Because A/B never produce a 6th natural resource under a verified stack contract, the open Successor Gate is probably **downstream of** `0x304BF0` resume (e.g. family case 9 runtime behavior) — out of scope for this P10-only pass.

## Cell detail

### Group A rep 1–2 (`direct_lr`)

- Layer-1 PASS, DrawFP YES, members loadingbar/bar/textbar/topleft/topright, SP deltas all 0, alive@150s YES

### Group B rep 1–2 (`epilogue`, R0=0)

- Same as A on Layer-1 / members / SP / frame SHA; actual mode confirmed `epilogue`

### Group C rep 1–2 (`epilogue`, R0=handle)

- Layer-1 FAIL (`no_drawfp`, `bmp_missing`); only 3 members; early P3 fault; alive@150s YES but UI incomplete
