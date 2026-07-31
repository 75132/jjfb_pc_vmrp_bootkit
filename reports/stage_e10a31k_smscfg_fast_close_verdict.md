# Stage E10A-3.1k SMSCFG fast close

- **Mode**: `method0_validate` (after causal A/B)
- **Primary verdict**: `SMSCFG_GATE_PASSED_NEXT_PRECONDITION_FOUND`
- **Prior A/B verdict**: `SMSCFG_GPT_GATE_CAUSAL_CONFIRMED`

## Headline

DSM compatibility bootstrap publishes `GPT` at `mr_sms_cfg_buf+0x349` via `COMPAT_PROFILE`. method0's GPT gate passes; failure moves to the next guest precondition (strcmp involving `gwy`).

## Bootstrap

```
[SMSCFG_BOOTSTRAP] profile=mythroad_mini_2011_smscfg_0x10E0_gpt349_anyhash
mr_version=2011 cfg_len=0x10E0 source=COMPAT_PROFILE
original_default_recovered=false cfg_guest=0x280CD4
[SMSCFG_BOOTSTRAP_COMPLETE] gpt_hex=475054
```

## method0

| Check | Result |
|-------|--------|
| helper6 | 0 (prior A/B + same path) |
| helper8 | 0 |
| GPT @ 0x349 | `475054` / `SMS_CFG_349_IS_GPT` |
| GPT strcmp | passes |
| helper0 | -1 |
| next fail | strcmp vs `gwy\0GPT\0` → TRUE_FAIL @ `0xAC2E8` |

## Stop / next

- SMSCFG writer research: **stopped**
- Do **not** invent a full `dsm.cfg`
- Next product work: identify the post-GPT sms_cfg / path precondition (`gwy`), then resume cfg-gate only after method0 returns 0

## Notes

- `original_default_recovered=false`
- Platform compatibility reconstruction, not recovered original default template
