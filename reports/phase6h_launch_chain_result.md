# Phase 6H — launch chain result

- gwy_shell_class: `SHELL_LOADED_BUT_NO_EXTCHUNK`
- native_class: `EXEC_GATE_OPEN`
- shell_native_exec_gate: `open`
- stop: `shell_ext_fault_in_guest_pc`
- _strCom 601/800/801: `no/no/no`
- mrc_init: `no`
- P+0xC writes: `0`
- host_runapp_equivalent: `no`
- guest_native runapp evidence: `no`
- fault: `pc=0x30CCF8 addr=0x28` (gbrwcore.ext)

## Success ladder

- min: `PASS`
- mid: `BLOCKED` (shell fault before runapp; see `phase6h_blocker_mid_ladder.md`)
- mid+ (_strCom): `PENDING`
- high (P+0xC + mrc_init): `PENDING`
