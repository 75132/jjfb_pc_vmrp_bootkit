# P8 callsite stack contract

reason: incremental

## Resume mode

JJFB_304BF0_RESUME_MODE = callsite

## Hit counts

| marker | count |
|--------|-------|
| LOOKUP_CONTINUATION_HIT | 2 |
| DISPATCH_EPILOGUE_HIT | 0 |
| CALLER_CONTINUATION_HIT | 1 |
| resources completed | 2 |
| 6th natural resource | NO |

## SP invariant (DISPATCH_ENTER vs CALLER_CONTINUATION)

| call_id | member | enter_sp | caller_delta | ok |
| 1 | bar!16!18.bmp | 0x0 | 0 | pending |

CALLER_SP_DELTA_OK=0 CALLER_SP_DELTA_BAD=0

## Acceptance (callsite mode)

LOOKUP_CONTINUATION_HIT=5, DISPATCH_EPILOGUE_HIT=5, CALLER_CONTINUATION_HIT=5, CALLER_SP_DELTA=0 for all five.

## Verdict

P8_STACK_CONTRACT = PARTIAL (callsite hits or SP delta incomplete)
