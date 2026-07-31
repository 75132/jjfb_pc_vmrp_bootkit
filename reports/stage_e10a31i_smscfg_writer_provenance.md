# Stage E10A-3.1i sms_cfg writer provenance

- **Mode**: static + existing live traces
- **Scope**: who can write `mr_sms_cfg_buf[0x349..0x34B]` before method0 GPT check

## Confirmed read side

- Method0 fail path reads `pos=0x349 len=3` via `BL 0x2E3180`.
- `0x2E3180` and `0x2E334C` are both `_mr_smsGetBytes`-style readers:
  - `range_limit = 0x10E0` (`MR_SMS_CFG_BUF_LEN`)
  - source from `*(mr_table + 0x1C0)` (mapped to `mr_sms_cfg_buf`)
  - copy via `*(mr_table + 0xC)` (`memcpy`)

## Candidate write APIs from source

From `mythroad.c` / `mythroad_mini.c`, writes to sms cfg happen through `_mr_smsSetBytes(pos,p,len)` only.

Direct reachable entry points:

1. TestCom path (`_mr_TestCom`, code `502`)
   - `_mr_smsSetBytes(input1, buf, len)`
2. SMS chunk path (`_mr_smsIndiaction`, chunk `code 6`)
   - `_mr_smsSetBytes((chunk[1]<<8)+chunk[2], chunk+4, chunk[3])`

Both can write arbitrary offsets, including `0x349`.

## Negative runtime evidence (current runs)

- In E10A31H live trace, method0 reaches `_mr_smsGetBytes(0x349, ..., 3)` and sees NULs.
- No observed helper-call evidence for TestCom sms opcodes `500/501/502/503/504` before method0 fail in current provenance CSV/log set.
- Current traces therefore show **read without prior observed writer** on the causal path.

## Interim verdict

- `METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG` remains valid.
- Most probable missing publication is upstream of `_mr_smsGetBytes`: either
  - no write ever happened to `0x349`, or
  - write happened in an unobserved path/time window not covered by current method0-focused hooks.

## Next step to close

- Add long-window write watch (from gamelist ext-first-PC to method0 fail) for:
  - writes to `cfg_base+0x349..+0x34B`
  - calls matching `_mr_smsSetBytes` ABI (`pos,len,p`) with `pos==0x349`.

