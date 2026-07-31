# Stage E10A-3.1n post-range provenance

- **Mode**: `apply_napptype`
- **Primary verdict**: `METHOD0_NAPPTYPE_APPLY_STILL_2E3FBA`

## E10A-3.1o apply_napptype (via N runner)

### Helpers
- `0xAC374` = strlen
- `0xAC4A4` = strstr(haystack, needle); needle from failfn r3 = `napptype`
- `@0x355` = copy length for `@0x377` C-string haystack

### AB
| case | ret | armed | 2E3FBA | apply |
|------|-----|-------|--------|-------|
| A |  | True | True | no |
| B |  | True | True | True |

- strstr milestone B: True
- `reports/e10a31o_requirement_ab_compare.csv`
- `out/e10a31o/helpers_ac374_ac4a4_annotated.txt`
- original_default_recovered=false

## Notes

- `original_default_recovered=false`
- int16@0x355=1 is compatibility/diagnostic (range 1..434); not claimed as original default.
- Profile content validity is separate from bootstrap application timing.
- `cfg_validate` remains disabled until method0 returns 0 AND safe bootstrap is proven.
