# P20 Nested JJFB Result

- nested_jjfb: **NO**
- startGame fn: `0x2AAD84`
- opcode300: NO
- shell continue after gbrwcore init_ok: YES → `gwy/gamelist.mrp`
- parent code15: not closed (gate9 incomplete)
- first screen vs direct_boot: N/A until gate9
- timer_fire: YES
- post-continue stop: `shell_ext_fault_in_guest_pc` (`0x30D5D2` / `0xD09C6C91`) with gamelist↔gbrwcore P collision
