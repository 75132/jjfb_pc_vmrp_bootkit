# Stage E10A-3.1e Package-scoped appInfo Metadata Binding Verdict

- **Mode**: `method0_validate` (+ `ab_compare`, `read_proof`)
- **Primary verdict**: `APPINFO_VERSION_MISMATCH_NOT_CAUSAL`
- **Outcome**: **3** — appInfo bound to active gamelist MRP header (`400101/1006`), method0 still returns `-1` at exactly `0x2E1C24` with the same ~518-insn path

## Product fix landed

| Item | Result |
|------|--------|
| Source | `MRP_HEADER` (active package), not env |
| Singleton removed | binding keyed by `package_id + package_generation` |
| gamelist bind | `id=400101 ver=1006 sidName=0 ram=0` |
| Owner | `APPINFO_OWNER_MATCH` (gamelist.mrp / gamelist.ext / helper `0x2E3089`) |
| R9+0x91C | `2011` (`GAMELIST_MR_VERSION_PUBLISHED`) |
| R9+0x920 | package-scoped appInfo* (`GAMELIST_APPINFO_PUBLISHED`) |
| Env | `GWY_PACKAGE_APPVER=12` still present but **not** used unless `JJFB_E10A31E_FORCE_ENV_APPINFO=1` |

## A/B compare

| Case | appInfo.ver | ret6/ret8/ret0 | fail PC | class |
|------|-------------|----------------|---------|-------|
| A legacy (`FORCE_ENV`) | **12** | 0 / 0 / **-1** | `0x2E1C24` | `RETURN_NEG1_IMMEDIATE` |
| B package metadata | **1006** | 0 / 0 / **-1** | `0x2E1C24` | `RETURN_NEG1_IMMEDIATE` |

Changing ver `12 → 1006` does **not** move the failure PC or change the return. Version mismatch was a real launcher defect, but **not** the direct cause of `0x2E1C24`.

## Read proof (Lane H)

- Hook armed on method0: R9+0x920 + appInfo 16B
- Observed: **`METHOD0_DOES_NOT_READ_APPINFO_BEFORE_FAILURE`** (zero matching reads before first failure)
- CSV: `reports/e10a31e_appinfo_read_proof.csv` (header only)

## Milestones

- `ACTIVE_PACKAGE_METADATA_RESOLVED`
- `APPINFO_BOUND_TO_ACTIVE_PACKAGE_METADATA`
- `APPINFO_OWNER_MATCH`
- `GAMELIST_MR_VERSION_PUBLISHED`
- `GAMELIST_APPINFO_PUBLISHED`
- `APPINFO_PACKAGE_METADATA_MATCH`
- `APPINFO_VERSION_MISMATCH_NOT_CAUSAL`
- `MRC_INIT_INTERNAL_PRECONDITION_STILL_AT_2E1C24`
- `METHOD0_DOES_NOT_READ_APPINFO_BEFORE_FAILURE`

## Next (Outcome 3 follow-ups — do not start cfg)

1. Annotate predecessor branches into `0x2E1C24` (`0x2E1BBD → 0x2E1C24`)
2. Validate method0 `input` / `input_len` ABI vs genuine root bootstrap
3. Trace why appInfo fields are never read before failure (R9+0x920 indirection timing / different base)
4. Compare gwy.mrp root bootstrap side effects

## Artifacts

- `reports/e10a31e_package_metadata_trace.csv`
- `reports/e10a31e_appinfo_binding_trace.csv`
- `reports/e10a31e_appinfo_owner_validation.csv`
- `reports/e10a31e_gamelist_globals.csv`
- `reports/e10a31e_appinfo_ab_compare.csv`
- `reports/e10a31e_appinfo_read_proof.csv`
- `logs/e10a31e_package_appinfo_stdout.txt`
- `RUN_E10A31E_PACKAGE_APPINFO.ps1`
