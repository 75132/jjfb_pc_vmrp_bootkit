# Event 15 / E6C provenance (Task 14)

## Status

**No natural event 15 / no `0x2E5E60` / no `ER_RW+E6C` store** in Task 14 A/B/C.

Prerequisite for the productive chain was:

```
leave_2FC26C @ 0x2FC3E6
 → nested drain / 1E201
 → event code 15
 → 0x2E4020 → BL 0x2E5E60
 → calloc + STR [R9+0xE6C]
```

`LEAVE_2FC26C` was not reached, so this report records **absence** and the known producer map (static + prior tasks).

## Known productive path (static / prior evidence)

| Step | Address / API | Note |
|------|----------------|------|
| Event code | 15 | Robotol family dispatch |
| Entry | `0x2E4020` | event-15 handler |
| Alloc | `0x2E5E60` | int16 table / E6C object |
| Heap | via `0x305E30` calloc | guest allocator |
| Store | `STR` to `R9+0xE6C` | only observed BL caller of `2E5E60` is `0x2E4022` |

Forbidden this task: host enqueue 15, direct call `2E5E60`, hardwrite E6C, FAST assists.

## Variant C (record-first) C0 fault

```
B71_NATURALLY_WRITTEN @ 0x30ED7A → ER_RW+0xB71
CALL_FAMILY_C0 → 0x30D301 → 0x30DC44 → 0x2FEBBC
at_2FEC3C: R9+E6C=0 → LDRSH unmapped @0
```

So E6C remains the Case-5 blocker on the product default path; DrawFP publish did not regress that chain and did not invent E6C.

## Empty-first (Variant B)

Reached `2FC26C` with `E6C=0`, `B71=0`. Did not leave. Therefore:

- `PATH_A_SECOND` arm-after-leave did not fire a second 101AB record
- No `LRT_EVENT15` / `LRT_E6C_ALLOC`

## Next when leave is achieved

Record at minimum:

1. Producer PC/LR of event 15 enqueue  
2. Event object / code / payload  
3. Dispatch to `0x2E4020`  
4. `0x2E5E60` enter + returned guest ptr  
5. First non-zero `*(ER_RW+0xE6C)` writer PC  
6. Then re-run record-first C0 for `2FEC3C` / B70 / UI_MODE=0x45  

Mark only when natural: `EVENT15_NATURAL`, `E6C_NATURAL_STORE`.
