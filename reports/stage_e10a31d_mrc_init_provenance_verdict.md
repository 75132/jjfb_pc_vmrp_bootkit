# Stage E10A-3.1d mrc_init Failure Provenance Verdict

- **Mode**: final synthesis (history + method0_trace + appinfo)
- **Label**: FAST_REAL_GAMELIST_INIT_SEQUENCE / NOT_PRODUCT
- **Primary**: `MRC_INIT_INTERNAL_PRECONDITION_FAILED` / `RETURN_NEG1_IMMEDIATE`
- **Secondary**: `APPINFO_ID_VERSION_MISMATCH` (ver=12 vs MRP 1006)

## Decision order results

| Priority | Verdict | Result |
|----------|---------|--------|
| 1 | GAMELIST_METHOD0_DUPLICATE_CALL | **NO** — first method0 is FAST_REAL only |
| 1b | GAMELIST_NO_NATURAL_METHOD0_OBSERVED | **YES** |
| 1c | GAMELIST_METHOD0_FIRST_CALL | **YES** (host_fast_real) |
| 2 | METHOD0_FAST_CALL_ABI_WRONG | open — input=0 vs mythroad filebuf; dispatcher may ignore R2 |
| 3 | APPINFO_ID_VERSION_ZERO | **NO** (id=400101) |
| 3b | APPINFO_ID_VERSION_MISMATCH | **YES** ver=12 env vs pkg appver=1006 |
| 4 | MR_TABLE / platform slot | **NOT causal** — 0 platform APIs before first -1 |
| 5 | MRC_INIT_FIRST_FAILURE_FOUND | **YES** at `0x2E1C24` |

## Lane A — helper history

Only gamelist FAST_REAL sequence after first PC:

| source | method | helper | ret |
|--------|--------|--------|-----|
| host_fast_real | 6 | 0x2E3089 | 0 |
| host_fast_real | 8 | 0x2E3089 | 0 |
| host_fast_real | 0 | 0x2E3089 | **-1** |
| timer | 2 | 0x2E3089 | (after init tx) |

**`-1` is a genuine first init failure, not duplicate init.**

## Lane B — helper dispatch

- Entry `0x2E3088` (thumb helper `0x2E3089`)
- Switch on method via jump table
- method0 → `BL 0x2DB045` (mrc_init candidate) then `BL 0x2E2529`
- method6 → `STR r7,[r9+0x900+0x1C]` = **R9+0x91C** ← MR_VERSION
- method8 → `STR r6,[r9+0x900+0x20]` = **R9+0x920** ← appInfo*

**mythroad `ERW+0x20/+0x24` does not apply to gamelist.** Live proof:

- `R9+0x91C = 0x7DB` (2011) after code6
- `R9+0x920 = appinfo` after code8

## Lane C — R0=-1 provenance

- budget used: ~518 insns, 11 BLs, **0** platform APIs
- class: `RETURN_NEG1_IMMEDIATE`
- failure_pc: `0x2E1C24` (`MVN`-family → r0=-1)
- enclosing path: `0x2E30C0 → 0x2DB045 → … → 0x2E2529 → 0x2E1BBD → 0x2E1C24`
- verdicts: `MRC_INIT_FIRST_FAILURE_FOUND`, `MRC_INIT_INTERNAL_PRECONDITION_FAILED`, `MRC_INIT_RETURN_PROVENANCE_COMPLETE`

## Lane D — appInfo contract

| Field | Value | Expected (gamelist.mrp) |
|-------|-------|-------------------------|
| id | 400101 | 400101 |
| ver | **12** (from `GWY_PACKAGE_APPVER`) | **1006** |
| sidName | NULL | unknown / unused until proven |
| ram | 0 | unknown / unused until proven |

No `APPINFO_READ` mem-hook hit yet — method0 may read via `R9+0x920` indirection rather than the raw 16-byte guest blob address we hooked. Binding package metadata is still the top fix candidate once read-site is confirmed (or as controlled experiment).

## Recommended next single fix (Lane E / J Case 1)

1. Bind `appInfo.ver` (and id) from **active package MRP header** (gamelist → 1006), not jjfb env `12`.
2. Do **not** invent sidName/ram yet.
3. Do **not** force method0 return / patch `0x2E1C24`.
4. Re-run method0_trace; expect either `GAMELIST_METHOD0_RETURN_ZERO` or a new first-failure PC.

## Artifacts

- `reports/e10a31d_helper_call_history.csv`
- `reports/e10a31d_method0_instruction_trace.csv`
- `reports/e10a31d_method0_call_tree.csv`
- `reports/e10a31d_method0_return_provenance.csv`
- `reports/e10a31d_appinfo_contract.csv`
- `out/e10a31d/gamelist_helper_dispatch_annotated.txt`
- `out/e10a31d/gamelist_method0_cfg.dot`
- `RUN_E10A31D_MRC_INIT_PROVENANCE.ps1`
