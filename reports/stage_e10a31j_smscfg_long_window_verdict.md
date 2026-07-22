# Stage E10A-3.1j SMSCFG long-window writer provenance

- **Mode**: `compare` (full boot window + gamelist-only window)
- **Primary verdict**: `SMSCFG_NO_BOOTSTRAP_WRITER_OBSERVED`
- **Supporting**: `DSM_CFG_LOAD_NOT_CALLED` · `SMSCFG_NEVER_INITIALIZED` · `METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG`

## Headline

From DSM/cfunction ERW publish through method0 fail, `mr_sms_cfg_buf` (`ERW+0x8D4` = `0x280CD4`) stays **entirely zero**. No writer of any kind was observed in either the full boot window or the gamelist-focused window.

## Evidence chain

1. **Pointer lifetime**: host `mr_table+0x1C0` stays `0`; live buffer resolved via proven `cfunction ERW+0x8D4` as soon as ERW publishes (`0x280400+0x8D4=0x280CD4`). Later confirmed by method0 live `*(r4+0x1C0)`.
2. **Write hook**: armed at `DSM_BOOT_ENTER` for full window (before gbrwcore/gamelist). Write CSV has **header only** — zero `UC_MEM_WRITE` / memcpy / memset overlaps.
3. **Checkpoints**: identical SHA256 `7DABD08D…` and `nz=0` / `gpt=000000` from DSM boot → GBRWCORE → GAMELIST → METHOD0_FAIL.
4. **APIs**: no `_mr_load_sms_cfg`, no TestCom 502, no SMS indication code 6, no `smsSetBytes(0x349)`.
5. **dsm.cfg I/O**: no VFS open/read attempt for `dsm.cfg` (IO CSV empty aside from header).
6. **Window compare**:

| window | any_write | never_init | empty_gpt_fail |
|--------|-----------|------------|----------------|
| full | 0 | 1 | 1 |
| gamelist | 0 | 1 | 1 |

→ `SMSCFG_NO_BOOTSTRAP_WRITER_OBSERVED` (not merely “missed because we started at gamelist”).

## Interpretation (case 三)

```
original phone/Mythroad relied on an already-populated persistent dsm.cfg
  OR
current port is missing the DSM default sms_cfg init path (_mr_load_sms_cfg)
```

Product fix direction (not done in this stage):

```
DSM session init
→ _mr_load_sms_cfg
→ load overlay dsm.cfg if present
→ else evidence-backed default init (not hardcode GPT in gamelist)
→ then start MRP
```

Diagnostic A/B (next, optional): empty vs `smsSetBytes(0x349,"GPT",3)` to close causality — still no invented production `dsm.cfg` contents.

## Forbidden checks (this stage)

- no GPT write
- no method0 return patch
- no invented dsm.cfg
- no fixed-address-only hook (resolved via ERW publish + live mr_table)

## Artifacts

- `reports/e10a31j_smscfg_pointer_lifetime.csv`
- `reports/e10a31j_smscfg_long_write_trace.csv` (empty body)
- `reports/e10a31j_smscfg_writer_api_trace.csv`
- `reports/e10a31j_dsm_cfg_io_trace.csv` (empty body)
- `reports/e10a31j_smscfg_checkpoints.csv`
- `reports/e10a31j_window_compare.csv`
- `RUN_E10A31J_SMSCFG_LONG_WINDOW.ps1`
