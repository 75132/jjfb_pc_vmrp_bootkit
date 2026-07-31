# Stage E10A-3.1o strstr / napptype

- **Primary verdict**: `METHOD0_NAPPTYPE_STRSTR_GATE_PASSED`
- Also: `HELPER_0xAC4A4_IS_STRSTR`, `HELPER_0xAC374_IS_STRLEN`, `SMSCFG_355_IS_COPY_LENGTH_FOR_0x377`

## Helpers (static + live)

| PC | Role | Evidence |
|----|------|----------|
| `0xAC374` | **strlen** | live r0_out=78 on haystack; 8 on `"napptype"` |
| `0xAC4A4` | **strstr(haystack, needle)** | ARM strlen+slide memcmp; NULL → failfn MVNS |
| `0x2E3F85` | key/value extractor | strstr → skip key+`=` → value until `_` → atoi |

Needle comes from caller **r3** (not a strcmp rhs). First call: `"napptype"` @ `0x2E8510`.

## SMSCFG pairing

- `@0x355` int16_le = **copy length** for the C-string at `@0x377` (range still 1..434)
- `@0x377` = haystack for failfn keys (`napptype`, then `entry`, …)
- Compat `0x355=1` only copies 1 byte → strstr(`""`,`napptype`) fails (case A)

## AB (method0-enter diagnostic)

| case | apply | napptype strstr | fail @0x2E3FBA needle |
|------|-------|-----------------|------------------------|
| A len1 | int16=1 only | FAIL | `napptype` |
| B paired | len=78 + `GWY_LAUNCH_PARAM` | **PASS** (ptr≠0); atoi→12 | **`entry`** |

Provenance B:
`lhs='napptype=12_nextid=482_ncode=512…' needle='entry'` → strstr NULL → MVNS.

Call tree B: first `BLX_strstr` r0_out≠0; second (key entry) r0_out=0.

## What is proven / not

**Proven**
- Helper identities
- `@0x355` length semantics for `@0x377`
- Launch-param-shaped haystack satisfies **napptype** gate (candidate; `original_default_recovered=false`)

**Not proven (do not invent)**
- Exact original `@0x377` bytes
- Required `entry` value (failfn later switches on atoi∈{1,2,3}); launch param has no `entry=`
- `@0x35F` 4-byte field: if nonzero, skips string `entry` parse — expected value unknown

## Next

1. Recover `entry` expectation from guest (string key and/or `@0x35F`) without brute force
2. Keep method0-enter apply diagnostic-only; bootstrap `@0x355` still deferred/unsafe early
3. `cfg_validate` remains disabled

## Artifacts

- `out/e10a31o/helpers_ac374_ac4a4_annotated.txt`
- `reports/e10a31o_requirement_ab_compare.csv`
- `reports/e10a31n_return_provenance.csv` (B run)
- `reports/e10a31n_call_tree.csv` / `e10a31n_post_range_compare_chain.csv`
- `logs/e10a31n_o_case_a_len1_stdout.txt`
- `logs/e10a31n_o_case_b_napptype_stdout.txt`
