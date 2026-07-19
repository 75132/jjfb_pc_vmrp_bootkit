# Phase 6L — verdict

## Bottom line

**Classification: `EARLY_RETURN_ABI`**

Phase 6K proved documented entry order; Phase 6L asks why entry returns without P+0xC publication.

## Primary (baseline) facts

- ENTRY_HIT: `yes`
- ABI PRE/RET present: `yes` / `yes`
- cluster reached: `no`
- natural P+0xC nonzero: `no`
- end_reason: `stop_at_base`
- mythroad exit: `yes`

## Interpretation

Documented entry hit `stop_at_base` after only `98` instructions; none of the five +0xC init clusters were reached. All five ABI variants and wxjwq baseline behaved the same. `mr_exit` is **post-entry** (not the entry killer). Leading cause: entry control-flow early-out / incomplete init semantics — not wrong entry PC selection (already fixed in 6K).

## Forbidden

Do not invent P+0xC, jump to cluster, force UI, or expand to gamelist chase yet.

## Best ABI_CANDIDATE

`none`

