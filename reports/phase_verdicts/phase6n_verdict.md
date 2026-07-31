# Phase 6N Verdict

- **verdict:** `MID_SUCCESS`
- **class:** `EXTCHUNK_PUBLICATION_RESTORED`
- **gbrwcore PUBLISH nonzero:** `True`
- **fault_addr=0x28 / NULL+0x28:** `False`
- **legacy 0x30CCF8 seen:** `False`
- **SLOT_CALL +0x28 count:** `0`
- **REPUBLISH count:** `1`
- **ALLOC count:** `3`
- **advanced tags noted (not chased):** JJFB_SHELL_NATIVE

## Evidence

- Layout/magic/sendAppEvent: **DOCUMENTED**
- Platform publish after zero-init: **DOCUMENTED** + **TARGET_OBSERVED**
- Tags use `*_contract` / `platform_publication_restore` only

## Publish events

- module=gbrwcore.ext P=0x2AC8DC old=0x0 new=0x682A5C reason=mr_c_function_new_contract
- module=gbrwcore.ext P=0x2AC8DC old=0x682A5C new=0x682A5C reason=platform_publication_restore
- module=gbrwcore.ext P=0x2AC8DC old=0x0 new=0x682A5C reason=platform_publication_restore

## 6N summary lines

- `[JJFB_6N_SUMMARY] mode=gbrwcore_only published=yes last_chunk=0x682A5C stop=mr_exit evidence=TARGET_OBSERVED`
