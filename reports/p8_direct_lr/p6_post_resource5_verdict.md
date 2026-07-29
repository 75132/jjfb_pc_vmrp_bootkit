# P6 post-resource5 verdict

reason: incremental

## Answers

1. Initial UI builder return: YES/CANDIDATE (marker UI_BUILDER_LEAVE)
2. First stable loop PC: 0x304558 (hits=6)
3. Resources completed: 5; 6th natural: NO
4. FIRST_POST_UI_PC: 0x2D93D0
5. NOTE_PIXELS_LEGACY_CALLS: 0
6. entry_complete skips: 5
7. family 0x1E209 notes: 2; 0x10102: 2; 0x10140: 2

## RAW_BLOB status

RAW_BLOB_CONTRACT = HOST_PROBE_CANDIDATE
RAW_BLOB_NATURAL_FLOW_VALIDATED = NO
(ani/txt natural requests still 0 in prior census)

## Hypothesis

All early resources share caller_lr=0x2D93D1. After resource 5, no new 0x304BF0 request implies boot successor (family event ABI and/or native 304BF0 side effects) is incomplete — not missing ANI whitelist.
