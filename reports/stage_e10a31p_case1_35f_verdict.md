# Stage E10A-3.1p case1 / 0x35F

- **Primary verdict**: `METHOD0_CASE1_N_KEYS_PASSED_NEXT_BAPPTYPE`
- **Mode**: `apply_case1`
- `original_default_recovered=false`

## What was proven (live B: run_id 1784735556145)

### Apply (method0-enter DIAGNOSTIC only)
- `@0x355` = len(77) + `@0x377` =
  `napptype=1_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink`
- `@0x35F` int32 LE = 1 → guest BNE skips optional `entry` strstr

### strstr needle chain (compare RHS + call-tree r0_out)
| needle | result |
|--------|--------|
| napptype | PASS (r0≠0) |
| entry | **skipped** (no call; `@0x35F` bypass) |
| nextid | PASS |
| ncode | PASS |
| narg | PASS |
| narg1 | PASS |
| nmrpname | PASS |
| **bapptype** | **FAIL** → MVNS `@0x2E3FBA` |

So case1 **n*** parse completed; next causal gate is **`bapptype`** (then likely `bextid`/`bcode`/`barg`/`barg1`/`bmrpname`/`burl` from gamelist.ext rodata).

### Static key pool (`out/tmp_gamelist_disasm/gamelist.ext`)
`napptype, entry, nextid, ncode, narg, narg1, nmrpname, nappid, nurl, bapptype, bextid, bcode, barg, barg1, bmrpname, burl`

### Format evidence (other packages; not claimed as this SMSCFG default)
- `napptype=%d_nurl=%s_bapptype=%d_bextid=%d_bcode=%d_barg=%d_barg1=%d_bmrpname=%s`
- reglogin example shape: `napptype=3_..._bapptype=1_bextid=%d`

## AB summary

| case | armed | apply | skip@0x35F | fail needle | notes |
|------|-------|-------|------------|-------------|-------|
| A launch12 (O) | no | no | — | — | `MEM_GET_HOST_CRASH` before method0 (flake) |
| B case1+0x35F (P) | yes | yes | yes | **bapptype** | n* all PASS |

Artifacts:
- `reports/e10a31p_requirement_ab_compare.csv`
- `reports/e10a31n_post_range_compare_chain.csv`
- `reports/e10a31n_return_provenance.csv`
- `logs/e10a31n_p_case_b_case1_stdout.txt`

## Constraints (unchanged)
- No product bootstrap of `@0x355` (early host crash)
- No cfg_validate until method0==0 and safe bootstrap
- Do not invent arbitrary `bapptype=` values; next step may only append b* keys as DIAGNOSTIC using **mirrored launch n* field values** + format evidence, still `original_default_recovered=false`

## Next (E10A-3.1q candidate)
Append b* fields to the case1 haystack (parallel to launch n*):
`..._nmrpname=..._bapptype=12_bextid=482_bcode=512_barg=0_barg1=1_bmrpname=gwy/jjfb.mrp_gwyblink`
and re-run AB watching the next fail needle.
