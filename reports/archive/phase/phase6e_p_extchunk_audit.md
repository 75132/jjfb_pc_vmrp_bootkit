# Phase 6E — P.mrc_extChunk Provider Audit

## Summary

- extchunk_class: `EXTCHUNK_GWY_CONTEXT_HYPOTHESIS`
- writes_seen: `0`
- reads_seen: `1`
- null_at_use: `yes`
- P: `0x2AC8DC`
- function_start: `0x304558`
- memory_access_pc: `0x304580`
- next_allowed_fix: `GWY_STARTGAME_RUNAPP_CONTEXT_AUDIT`
- P owner: `p=0x2AC8DC reg=r3 pc=0x304570`

## DOCUMENTED layout (mr_helper.h)

```
mr_c_function_st:
  +0x00 start_of_ER_RW
  +0x04 ER_RW_Length
  +0x08 ext_type
  +0x0C mrc_extChunk*
  +0x10 stack
mrc_extChunk_st:
  +0x28 sendAppEvent
  +0x2c extMrTable
```

Evidence: **DOCUMENTED**. Live NULL / fault: **TARGET_OBSERVED** (6D-B/6E).

## Phase timeline (P+0xC)

| phase | P | P+0xC |
|---|---|---|
| `after_cfunction_p_alloc` | `0x2803E4` | `0x0` |
| `after_robotol_load` | `0x2AC8DC` | `0x0` |
| `pre_continuation` | `0x2AC8DC` | `0x0` |
| `post_continuation` | `0x2AC8DC` | `0x0` |
| `pre_fault` | `0x2AC8DC` | `0x0` |
| `mrc_init_not_observed` | `0x2AC8DC` | `0x0` |

## P+0xC reads/writes

- EXTCHUNK_READ count (logged): 1
- EXTCHUNK_WRITE count (logged): 0

- READ value=0x0 pc=0x304580

## P.mrc_extChunk 与 GWY launcher context 的关系假设

Evidence level: **HYPOTHESIS** until a discriminating startGame/runapp experiment.

- Direct `gwy/jjfb.mrp` launch with `_gwyblink` may skip shell steps that
  plugin/`mrc_extLoad` / startGame/runapp use to publish `mrc_extChunk`.
- Legacy CROSS_TARGET comment: Mythroad 800 load never sets mrc_extChunk;
  only plugin load paths do — not proven for this clean launcher.
- Fault `LDR [r0,#0x28]` with r0 from `P+0xC` matches DOCUMENTED `sendAppEvent`
  offset; NULL chunk alone explains the fault without R9 promotion.
- cfg36 / resource root / napptype: not mutated in Phase 6E; observe-only.

## Candidate next fix (NOT implemented)

- Selected next_allowed_fix: `GWY_STARTGAME_RUNAPP_CONTEXT_AUDIT`
- Candidate A: restore missing GWY startGame/runapp context that initializes P.mrc_extChunk
- Candidate B: call missing platform extChunk publication routine before continuation
- Candidate C: fix file/path/launcher root so registry publication occurs naturally

Forbidden: fake P+0xC, force ER_RW, skip fault, R9 promotion.

