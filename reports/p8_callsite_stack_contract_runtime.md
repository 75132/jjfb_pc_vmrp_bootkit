# P8 callsite stack contract

reason: atexit

## Resume mode

JJFB_304BF0_RESUME_MODE = direct_lr

## Hit counts

| marker | count |
|--------|-------|
| LOOKUP_CONTINUATION_HIT | 0 |
| DISPATCH_EPILOGUE_HIT | 0 |
| CALLER_CONTINUATION_HIT | 0 |
| resources completed | 0 |
| 6th natural resource | NO |

## SP invariant (DISPATCH_ENTER vs CALLER_CONTINUATION)

| call_id | member | enter_sp | caller_delta | ok |

CALLER_SP_DELTA_OK=0 CALLER_SP_DELTA_BAD=0

## Acceptance (callsite mode)

LOOKUP_CONTINUATION_HIT=5, DISPATCH_EPILOGUE_HIT=5, CALLER_CONTINUATION_HIT=5, CALLER_SP_DELTA=0 for all five.

## Verdict

P8_BASELINE = direct_lr (continuation hits expected 0; compare with callsite A/B)
