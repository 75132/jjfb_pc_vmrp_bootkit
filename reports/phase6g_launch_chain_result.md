# Phase 6G — launch chain result

- class: `SHELL_RUNAPP_CHAINED`
- CFG36: `yes`
- STARTGAME: `yes`
- RUNAPP: `yes`
- shell_summary: `[JJFB_GWY_SHELL_SUMMARY] class=SHELL_RUNAPP_CHAINED shell_open=yes gbrwcore=yes gamelist=yes gbrwshell=no jjfb_real=no jjfb_alias=no update_stub=yes runapp_chained=yes stop=shell_context_ready_before_jjfb_dsm evidence=TARGET_OBSERVED`

## Success ladder

- min (not SHELL_BYPASSED + shell open + no alias): `PASS`
- mid (startGame/runapp tags): `PASS`
- mid+ (_strCom 601/800/801): `PENDING`
- high (natural P+0xC + mrc_init): `PENDING`
