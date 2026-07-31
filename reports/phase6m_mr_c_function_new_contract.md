# Phase 6M — `_mr_c_function_new` contract

## DOCUMENTED (mythroad.c / fixR9.c)

1. Host allocates `mr_c_function_P` of `len` bytes
2. Host `memset` zeros the P struct
3. Host stores helper (`MR_C_FUNCTION`)
4. Returns `MR_SUCCESS`
5. **Guest/ext** fills `start_of_ER_RW` / `ER_RW_Length` after return
6. Publishing `mrc_extChunk*` into `P+0xC` is a **separate** step (not done by host new)

## LIVE host (bridge.c `br__mr_c_function_new`)

- CFN_NEW_CONTRACT seen: `yes`
- CFUNCTION_NEW_SIDE_EFFECT NO_DSM_DISPATCH: `yes`
- host_memset note: `yes`

### Verdict

Host zero-init of the 20-byte P is **expected** (DOCUMENTED). Guest `dsm:cfunction.ext @ 0x94F04` also zero-stores the P fields (TARGET_OBSERVED). Missing piece is natural **`mrc_extChunk` create/publish** into P+0xC / chunk+4 — not more entry ABI variants.

Next phase direction (do not invent in 6M): restore legitimate extChunk allocation/register path if DOCUMENTED/CROSS_TARGET contract requires it.

