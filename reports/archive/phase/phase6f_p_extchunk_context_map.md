# Phase 6F — P.mrc_extChunk ↔ GWY context map

- live class: `SHELL_BYPASSED_DIRECT_JJFB`
- shell_bypassed: `yes`
- pxc_writes_seen: `0`
- next_allowed_fix: `RESTORE_GWY_STARTGAME_RUNAPP_CONTEXT`

| candidate provider | module | reason | evidence | executed_in_current_launch? | writes_P+0xC? | missing precondition |
|---|---|---|---|---|---|---|
| gbrwcore startGame/runapp | gbrwcore.mrp | shell publishes app context | HYPOTHESIS + string xref | no (bypass) | no | shell not loaded |
| gamelist post-update / launch | gamelist.mrp | cfg36 → runapp | HYPOTHESIS | no | no | shell not loaded |
| gbrwshell bridge | gbrwshell.mrp | shell bridge | HYPOTHESIS | no | no | shell not loaded |
| vdload download path | vdload.mrp | download-then-run | HYPOTHESIS | no | no | not on direct jjfb path |
| cfunction/robotol registration | jjfb.mrp members | plugin mrc_extLoad | CROSS_TARGET legacy comment | partial (robotol runs) | no | plugin publication skipped |
| bridge_dsm_mr_start_dsm | host bridge | lowest-layer start | DOCUMENTED | yes | no | not full startGame |

## Candidate next fix (NOT implemented)

- `RESTORE_GWY_STARTGAME_RUNAPP_CONTEXT`
- restore missing GWY startGame/runapp context
- call missing legitimate platform publication routine
- fix file/root/cfg param so publication occurs naturally

Forbidden: fake extChunk, write P+0xC, R9 promotion, skip fault.

