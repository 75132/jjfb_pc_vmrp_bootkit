# Phase 6J — publication verdict

## Verdict: **B**

WRONG_ENTRY_SELECTION (first_pc ≠ header+8) with +0/+4/+8 written on continuation path and +0xC never published; next form is restore correct module init / entry order so publication routine can run (do not invent P+0xC).

## Legend

- **A**: legitimate publication routine exists but was not called
- **B**: entry selection wrong; skipped init/publication
- **C**: P+0xC should come from cfunction.ext/reg.ext primary publication flow
- **D**: no writer found across samples — keep researching; no fake

## Next phase (not implemented in 6J)

Phase 6K: fix MRPGCMAP entry selection / module init order

## Runs considered

- `gbrwcore_jjfb`
- `gbrwcore_wxjwq`

Forbidden: invent P+0xC, R9 promotion, force UI, host_runapp_equivalent.
