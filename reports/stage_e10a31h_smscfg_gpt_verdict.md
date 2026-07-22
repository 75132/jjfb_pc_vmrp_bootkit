# Stage E10A-3.1h sms_cfg / GPT-tag Verdict

- **Mode**: `live`
- **run_id**: `1784698222100`
- **Primary verdict**: `METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG`

## Headline

method0 `strcmp(empty,"GPT")` is **`_mr_smsGetBytes(0x349, dst, 3)`** against guest `mr_sms_cfg_buf`, which is empty.

Not primary: appInfo, `0x2E1C24` sentinel, method0 `filebuf` ABI.

## Identity proof

| Fact | Value |
|------|-------|
| range_limit in `0x2E3180` | `0x10E0` = `MR_SMS_CFG_BUF_LEN` (120*36) |
| `mr_table` (r4) | `0x281EFC` (inside cfunction ERW) |
| `*(mr_table+0x0)` | live `0x80160` (mr_malloc stub) |
| `*(mr_table+0xC)` | live `0x94E94` memcpy (matches BLX r3) |
| `*(mr_table+0x1C0)` | `0x280CD4` = `mr_sms_cfg_buf` (bridge MAP_DATA offset) |
| cfg @ `+0x349` | **NUL NUL NUL** (expect `GPT`) |
| `dsm.cfg` in tree | **absent** |
| bridge `MAP_DATA` initFn for `mr_sms_cfg_buf` | **NULL** (`hooks_init` skips DATA) |

## Causal chain

```
smsGetBytes(0x349, sp+0x30, 3)
  = memcpy(dst, mr_sms_cfg_buf + 0x349, 3)   ; buf @ ERW+0x8D4, empty
strcmp(dst, "GPT") @ 0xAC2D0 鈫?-1 @ 0xAC2E8
```

## Milestones

- `FIELD_READ_IS_SMSGETBYTES`
- `MR_TABLE_SLOT_1C0_IS_SMS_CFG_BUF`
- `SMS_CFG_349_IS_NUL`
- `SMS_CFG_BUF_IN_CFUNCTION_ERW_8D4`
- `BRIDGE_MAP_DATA_SMS_CFG_NO_INITFN`
- `DSM_CFG_FILE_ABSENT_IN_TREE`
- `METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG`

## Flags

- saw_sms=True empty_349=True table_ok=True
- elapsed=82.1s exit=0 observeStop=OBSERVE_STOP_PROVENANCE_COMPLETE

## Next (still no cfg.bin / no force ret0)

1. Find who should `smsSetBytes(0x349, "GPT", 3)` (gwy pack registration?).
2. Decide guest mapping for `mr_sms_cfg_buf`: publish real buffer + run `_mr_load_sms_cfg`, or sync host `dsm.cfg`.
3. method0 `input=filebuf` remains **secondary** (orthogonal to sms_cfg GPT check).

## Artifacts

- `out/e10a31h/gamelist_2e3180_ptrchain.txt`
- `reports/e10a31h_smscfg_trace.csv`
- `reports/e10a31g_strcmp_arg_trace.csv`
- `RUN_E10A31H_SMSCFG_GPT.ps1`
