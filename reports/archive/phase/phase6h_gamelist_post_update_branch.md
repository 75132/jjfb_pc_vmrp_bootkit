# Phase 6H — gamelist post-update success branch

Evidence: **TARGET_OBSERVED** static strings + live tags.

## Static map (`gamelist.ext`)

- size: `91532`

| needle | file_offset | next_step_hypothesis |
|---|---|---|
| `napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink` | `0x1412C` | build cfg36 then call gbrwcore lib.startGame/lib.runapp or dispatcher (HYPOTHESIS) |
| `gwyblink` | `0x14120` | unknown |
| `cfunction.ext` | `0x1417C` | unknown |

## Live observation

- CFG36 build tag: `no`
- post-update branch tag: `no`
- guest-native call evidence: `no`
- host_runapp_equivalent still present: `no` (must be **no** for 6H mid success)

## Required branch (target)

```text
gamelist.ext
  -> sprintf cfg36 (napptype/nextid/ncode/narg/narg1/nmrpname/gwyblink)
  -> update check => no_update / update_ok
  -> gbrwcore lib.startGame / lib.runapp (guest-native)
  -> start gwy/jjfb.mrp
```
