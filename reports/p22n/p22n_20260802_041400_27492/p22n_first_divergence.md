# P22N first divergence

## P22M correction

- `0x9C41C BEQ 0x9C56C` = **NORMAL_OPCODE_DISPATCH** (not a functional lock)
- Do not set `[object+0x30] |= 0x0C` as a fix

## Observed opcode sequence

`0x14,0x1B`

## Divergence

after natural 6→0→1, interpreter only saw opcodes [0x14,0x1B] then method=2; no UI/init opcode record observed

- divergence_pc=0x9C40C
- actual=opcodes=0x14,0x1B fire2=1
- producer=UNKNOWN_UI_INIT_RECORD_PRODUCER_not_in_observed_stream
- stream_kind=mixed_or_dynamic
- exit_how=natural_601_complete_no_post_progress
- method2_source=FIRE_EXT_method2_after_6_0_1 (timer/platform)
- fire2_n=1
