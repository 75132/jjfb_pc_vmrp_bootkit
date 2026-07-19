# Phase 6L — verdict

## Bottom line

**Classification: `POST_ENTRY_MR_EXIT`**

Phase 6K proved documented entry order; Phase 6L asks why entry returns without P+0xC publication.

## Primary (baseline) facts

- ENTRY_HIT: `yes`
- ABI PRE/RET present: `yes` / `yes`
- cluster reached: `no`
- natural P+0xC nonzero: `no`
- end_reason: `stop_at_base`
- mythroad exit: `yes`

## Interpretation

Documented entry returned (EMU_OK / stop) without publishing P+0xC; `mr_exit` ran **after** entry (POST_ENTRY_MR_EXIT). Publication is not completed inside the current entry ABI/path — likely wrong ABI early-return, missing second-stage callback, or wrong P target.

## Forbidden

Do not invent P+0xC, jump to cluster, force UI, or expand to gamelist chase yet.

## Best ABI_CANDIDATE

`none`

