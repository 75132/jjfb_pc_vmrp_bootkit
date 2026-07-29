# P8 callsite stack contract

reason: incremental

## Resume mode

JJFB_304BF0_RESUME_MODE = epilogue

## Hit counts

| marker | count |
|--------|-------|
| LOOKUP_CONTINUATION_HIT | 0 |
| DISPATCH_EPILOGUE_HIT | 0 |
| CALLER_CONTINUATION_HIT | 1 |
| resources completed | 2 |
| 6th natural resource | NO |

## SP invariant (DISPATCH_ENTER vs CALLER_CONTINUATION)

| call_id | member | enter_sp | caller_delta | ok |
| 1 | loadingbar!201!29.bmp | 0x27FE80 | 0 | YES |
| 2 | bar!16!18.bmp | 0x27FE80 | 0 | pending |

CALLER_SP_DELTA_OK=1 CALLER_SP_DELTA_BAD=0

## Acceptance (callsite mode)

LOOKUP_CONTINUATION_HIT=5, DISPATCH_EPILOGUE_HIT=5, CALLER_CONTINUATION_HIT=5, CALLER_SP_DELTA=0 for all five.

## Verdict

P8_BASELINE = direct_lr (continuation hits expected 0; compare with callsite A/B)
