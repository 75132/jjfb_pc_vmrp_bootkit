# Phase 6K — summary (stop rule)

## Verdict

**Entry-order fix (6K-B) landed; mid ladder not cleared.**

| check | result |
|---|---|
| observe mismatch (`intended=base+8` vs old first_pc) | proven in observe pass |
| `JJFB_MRPGCMAP_ENTRY_HIT` gbrwcore `pc==base+8` | yes |
| entry `EMU_OK` | yes |
| `entry_called` before `callback_continuation` | yes (host inject order) |
| natural `P+0xC` nonzero write | **no** |
| 6K-C shell / 6K-D wxjwq | skipped (mid success gate) |

Evidence tags: ENTRY path **DOCUMENTED** (`guest_code_base+8`); live publication gap **TARGET_OBSERVED**.

## What changed

- `JJFB_FIX_MRPGCMAP_ENTRY_ORDER=observe|gbrwcore_only|shell`
- Nested CFN path runs documented image+8 before continuation resume
- No fake `P+0xC`, no R9 promotion, no UI force, no MRP patch

## Live class

`ENTRY_OK_BUT_NO_EXTCHUNK`

During entry emu, gbrwcore wrote nested `P+0/+4/+8` near continuation file offsets; `+0xC` stayed 0. Session ended with `mythroad exit` after `entry_returned` (no `fault_pc=0x30CCF8` in this pass).

## Forbidden next steps

- invent `P+0xC` / fake extChunk
- skip publisher PC
- force ui_mode / progress / login

## Next discriminating experiment (not this phase)

Trace why documented entry returns without executing a `+0xC` init cluster (ABI args / R0=1 / stop-at-base / wrong P object), still without inventing chunk.
