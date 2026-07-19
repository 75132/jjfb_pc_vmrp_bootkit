# D5b — mr_entry / launch param consumption

## Static strings in gamelist.ext

| needle | count | notes |
|--------|------:|-------|
| `napptype` | 3 | format / field names |
| `nmrpname` | 2 | |
| `gwyblink` | 3 | |
| `mr_entry` / `_mr_entry` | **0** | not present as literal in gamelist.ext |
| `gwy/cfg.bin` | 2 | |

## DOCUMENTED (mythroad)

- `mr_start_dsm` copies entry → `mr_entry[]`
- Lua global `_mr_entry`
- `_mr_c_function_table[144]` → `mr_entry` pointer

## Runtime (45s quiet boot) — TARGET_OBSERVED

| Probe | Result |
|-------|--------|
| `[JJFB_PARAM_MAP]` | **2×** (va=`0x2829FC`, then `0x682ACC`; len=79; full cfg36 string) |
| `[JJFB_PARAM_READ]` | **0** |
| `[JJFB_PARAM_PARSE_HINT]` | **0** |
| `[JJFB_MR_ENTRY_*]` | not emitted (no guest literal / table[144] hit observed) |

### Answers (D5b Step 5)

1. **Does gamelist.ext read runtime param?** Not observed under code-hook reg scan (r0–r3) during init + 4 timer fires.  
2. **Does param enter command dispatcher?** No — cmd_disp hits = 0.  
3. **Is table[144] a command entry?** Unproven; gamelist has no `mr_entry` string; no observe hit.  
4. **Missing MR_ENTRY/foreground/resume?** Plausible HYPOTHESIS for D5c — only if DOCUMENTED/CROSS_TARGET that such event must be delivered after `start_dsm`; do not inject merely for screen advance.

### Classification

```text
PARAM_NOT_CONSUMED
```
