# v54 Post-Handoff Natural Progression Audit

- log: `C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v53_start_handoff_recovery_stdout.txt`
- lines: 1617
- handoff_ok (alias+801+timer): yes
- FORCE 0x45 present: yes
- FILEOPEN_MISS: 0

## Tag counts (top 25)

- 681 `[JJFB_SEND]`
- 61 `[JJFB_2EF86C_COV]`
- 52 `[JJFB_TIMER_DISPATCH]`
- 51 `[JJFB_2EFC_DISASM]`
- 45 `[JJFB_UI]`
- 43 `[JJFB_FIRST_SCREEN]`
- 39 `[JJFB_SLOGO_XREF]`
- 38 `[JJFB_AC8TAIL_DISASM]`
- 35 `[JJFB_801]`
- 32 `[JJFB_UI_DISPATCH]`
- 32 `[JJFB_HANDLER_306344]`
- 31 `[JJFB_OBJ_BIND_PC]`
- 31 `[JJFB_GATE_DISASM]`
- 30 `[JJFB_DEEP_DISASM]`
- 25 `[JJFB_STARTUP_PHASE]`
- 24 `[JJFB_FILEOPEN]`
- 23 `[JJFB_FILE]`
- 22 `[JJFB_12340]`
- 20 `[JJFB_DIM]`
- 20 `[JJFB_303D94]`
- 19 `[JJFB_10134]`
- 16 `[JJFB_2EF9F4]`
- 13 `[JJFB_DRAW]`
- 12 `[JJFB_2EFC_GATE]`
- 9 `[JJFB_CTX]`

## ui_mode / state

- ui_mode: {'0x0': 19, '0x45': 197}
- state: {'0': 2, '69': 36}

## FORCE samples (must disable for natural game-self)

```text
[JJFB_FIRST_SCREEN] FORCE state/ui_mode 0x0 -> 0x45 (splash path; slogo/loadingbar @0x2EF86C)
```

## FILEOPEN guests (top 30)

- 22 `mythroad/gwy/jjfb.mrp`
- 1 `mythroad/system/gb16.uc2`
- 1 `mythroad/sdk_key.dat`

## Network / update hints

- (none matched)

## _strCom hints

- (none matched)

## Verdict

- Current blocker: post-handoff path still uses FORCE ui_mode=0x45 / splash probes. v54 must disable these in GWY launcher mode and re-observe natural game-self/events/network.
