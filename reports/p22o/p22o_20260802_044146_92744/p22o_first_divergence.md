# P22O first divergence

## Early producer window

- mem_armed_at=cfunction_map_or_bind_uc
- seed14=0x2AF8F8 seed1b=0x2AF904
- first_write14=1 pc=0x927A8 off=+0x0
- first_write1b=1 pc=0x927A8 off=+0x0
- seed_preexisting14=0 seed_preexisting1b=0
- writer_class=dynamic_early_writes

## Divergence

producer wrote 0x14@+0x0 and/or 0x1B@+0x0 then no UI/init append; stream=[0x14,0x1B]

- skip_field_Y=UNKNOWN
- skip_actual_A=UNKNOWN
- expected_contract_W=UNKNOWN
- opcodes=0x14,0x1B
- fire2_n=1
