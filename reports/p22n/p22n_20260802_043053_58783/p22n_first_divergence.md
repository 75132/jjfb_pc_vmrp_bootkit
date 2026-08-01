# P22N first divergence

## P22M correction

- `0x9C41C BEQ 0x9C56C` = **NORMAL_OPCODE_DISPATCH** (not a functional lock)
- Do not set `[object+0x30] |= 0x0C` as a fix

## Observed opcode sequence

`0x14,0x1B`

## Divergence

post-6→0→1 opcode stream [0x14,0x1B] only; 0x1B LDMFD-exits interpreter; no UI/init record; index(LSR#24)=0 for raw 0x800054

- divergence_pc=0x9C40C
- actual=opcodes=0x14,0x1B fire2=1
- producer=UNKNOWN_writer_of_cmd_buffer_before_+0x1C40C_or_post_exit_producer
- stream_kind=preexisting_command_buffer_no_record_writer_in_
- exit_how=op1b_exit_then_fire2_n=1_opcodes=2
- method2_source=FIRE_EXT_method2_after_6_0_1 (timer/platform)
- fire2_n=1
