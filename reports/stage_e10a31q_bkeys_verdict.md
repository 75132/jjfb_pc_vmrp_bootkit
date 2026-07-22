# Stage E10A-3.1q b* keys

- **Primary verdict**: `METHOD0_Q_BAPPTYPE_STRSTR_PASSED`
- **Mode**: `apply_bkeys`
- `original_default_recovered=false`
- Confirmed run_ids: `1784738027646`, `1784738290521`

## Apply (method0-enter DIAGNOSTIC)
- `@0x355`=len(156) + `@0x377` =
  `napptype=1_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink_bapptype=12_bextid=482_bcode=512_barg=0_barg1=1_bmrpname=gwy/jjfb.mrp_gwyblink`
- `@0x35F`=1 → skip optional `entry`
- b* values mirrored from launch n* — **not** claimed as original SMSCFG default

## Live needle chain (all PASS / skip MVNS)

`napptype > nextid > ncode > narg > narg1 > nmrpname > bapptype`

| needle | result | note |
|--------|--------|------|
| napptype | PASS | case **1** |
| entry | skipped | `@0x35F` |
| nextid…nmrpname | PASS | atoi 482/512/… |
| **bapptype** | **PASS** | atoi **12** @ `0x2E56B0` |

No MVNS `@0x2E3FBA` after Q apply.

## Post-strstr (seed for 3.1r)
- Continues `0x2E5886` → `0x2E240D` (not full else-path `bextid`…)
- method0 **ret=-1**
- D provenance: `MRC_INIT_TRUE_FAILURE_PC_FOUND` **pc=`0xA1B8C`** `RETURN_NEG1_IMMEDIATE`
- So SMSCFG `@0x377` strstr gates through `bapptype` are cleared; next fail is **DSM/cfunction @0xA1B8C**, not another haystack key

## Artifacts
- `reports/e10a31q_requirement_ab_compare.csv`
- `reports/e10a31n_post_range_compare_chain.csv`
- `reports/e10a31n_call_tree.csv`
- `logs/e10a31n_q_case_b_nb_stdout.txt`
