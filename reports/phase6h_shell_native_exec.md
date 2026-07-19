# Phase 6H — shell native exec gate

- shell_native_exec_gate: `open`
- native_class: `EXEC_GATE_OPEN`
- shell_package_open / SHELL_EXEC: `yes`
- shell_ext_loaded: `yes`
- shell_export_registered: `yes`
- shell_export_called: `no`
- shell_guest_pc_hit: `yes`
- host_runapp_equivalent present: `no` (must be no for mid success)
- guest_native via tag: `no`

## Ladder

- min (gate open / PC+EXT+export): `PASS`
- mid (guest-native runapp/startGame): `PENDING`
- high (_strCom + P+0xC + mrc_init): `PENDING`
