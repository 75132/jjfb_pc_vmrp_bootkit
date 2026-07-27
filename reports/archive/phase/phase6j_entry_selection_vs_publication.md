# Phase 6J — entry selection vs publication

## Live numbers (TARGET_OBSERVED)

- header_entry_candidate: `0x2EB7E8` (DOCUMENTED image+8)
- observed_first_pc: `0x30CA96`
- entry_class: `WRONG_ENTRY_SELECTION`
- DISPATCH_CLASS nature: `guest_callback_continuation`
- callback_continuation evidence: `yes`
- fault function_start: `0x30CCD0`
- fault_pc / fault_addr: `0x30CCF8` / `0x28`

## Publication chain

- P: `0x2AC8DC`
- wrote +0/+4/+8/+0xC: `yes` / `yes` / `yes` / `no`
- nearest +0/+4/+8 writer: pc=`0x30CADE` module=`gbrwcore.ext`
- missing P+0x0C tag: `yes`

## Interpretation

1. `first_pc` is **not** header image+8 → `WRONG_ENTRY_SELECTION` (TARGET_OBSERVED).
2. If DISPATCH_CLASS / CALLBACK_CONTINUATION says guest callback continuation, DSM entered mid-function after nested `_mr_c_function_new`, not the module init entry that would publish mrc_extChunk (**HYPOTHESIS** favoring conclusion B, but +0/+4/+8 still got written on this path — so publication of +0xC is not solely “never entered any init”).
3. +0/+4/+8 written while +0xC stays 0 on the same P means the live init path fills ER_RW metadata but **never** stores mrc_extChunk (TARGET_OBSERVED → supports A or C more than “no writer exists”).
