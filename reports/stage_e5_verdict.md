# Stage E5 — verdict

- **target:** `gwy/jjfb.mrp`
- **seconds:** 90
- **runner_exit:** 0
- **decision:** `PLATFORM_RET0_CAUSE`

| Gate | Result |
|------|--------|
| E5-min POST_START_LOOP | yes |
| E5-mid TIMER_ARM/FIRE/DELIVER | arm=no fire=no delivered=no |
| E5-high DRAW/REFRESH | no |
| MRC_INIT | yes (`ret=-1`) |
| ret0=0 | no |

Log: `logs/stage_e5_jjfb_scheduler_stdout.txt`
