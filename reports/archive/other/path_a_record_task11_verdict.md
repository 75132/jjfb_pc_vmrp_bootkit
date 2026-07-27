# Task 11 verdict — Path-A lifecycle record

## Minimum success (met)

| Check | Result |
|---|---|
| Default first Path-A fill not empty | yes (profile `path_a_response.initial_record`) |
| `downVersion` record enters Guest | yes (`str1=downVersion field_d=0x3EE`) |
| no `r5=0x7374` | yes |
| no `0x94E40` fault | yes (only `NODE_94E40_FUNCTION_IDENTIFIED`) |
| `0x2F68E4` clean return | yes |
| `0x2DADC4` branch clear | yes → nonempty B58 → `0x30ED2C` |

## Complete success (not yet)

| Check | Result |
|---|---|
| `0x30ED2C` entered | yes |
| local `downVersion` open | yes (`mythroad/downVersion` → HIT, BE `0x3EE`) |
| `0x304AC4` ok | yes (`r0=0x2`) |
| `0x2F6C44` compare finish / natural B71 | **no** — callback hit `insn_limit_or_yield` inside `0x2F6C44` |
| `B71_NATURALLY_WRITTEN` | no |

## Fixes landed

1. Generation-scoped `platform_path_a_response` (env `JJFB_101AB_WITH_RECORD` = A/B override only)
2. Profile `path_a_response.initial_record` in `profiles/jjfb.json`
3. Empty B58 list control (`platform_event_queue_ensure_lifecycle_list`, Guest equiv `0x2FE970`)
4. Observe-only `JJFB_LIFECYCLE_RECORD_TRACE`
5. Local version file at resource-root `downVersion` / `downVersion.v` (BE `00 00 03 EE`, same bytes as `gwy/jjfbol/downVersion`)

## Next convergence (do not jump to MRP resource loader)

```
0x30ED2C → 0x304AC4 HIT → 0x2F6C44 entered
→ interrupted by family/drain insn budget before cmp @0x30ED62 / STRB B71 @0x30ED7A
```

Also still empty: `ER_RW+0x820` name-table (`0x2D96BC` called with list=0). Finish compare/B71 under natural budget (or prove 820 table is required) before UI/resource work.

## Runner

```powershell
.\RUN_PATH_A_RECORD_TASK11.ps1 -Variant B   # WITH_RECORD=1
.\RUN_PATH_A_RECORD_TASK11.ps1 -Variant C   # profile default
```
