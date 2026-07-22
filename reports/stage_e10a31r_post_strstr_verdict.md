# Stage E10A-3.1r post-strstr (seed)

- **Primary verdict**: `METHOD0_NEXT_FAIL_0xA1B8C_AFTER_STRSTR_CLEAR`
- Upstream: E10A-3.1q `METHOD0_Q_BAPPTYPE_STRSTR_PASSED`
- `original_default_recovered=false`

## Observation (run_id `1784738290521`)
- Q haystack + `@0x35F=1` applied at method0 enter
- All `@0x377` failfn needles through **`bapptype`** PASS (no `0x2E3FBA`)
- method0 returns **−1**
- First true-fail classification: **`pc=0xA1B8C`** `RETURN_NEG1_IMMEDIATE` (cfunction / DSM region; base `0x80000` → file off `0x21B8C`)

## What this is / is not
- **Is**: next gate after SMSCFG strstr key chain
- **Is not**: another missing `strstr` needle in `@0x377`
- Do not invent more b* keys unless a new failfn BL to `0x2E3F85` is observed

## Next
Static+live provenance of `0xA1B8C` (producer, predicate, required platform/SMSCFG state) under observation-first rules.
