# Stage E5 — next decision

**branch:** `PLATFORM_RET0_CAUSE`

| Signal | Value |
|--------|-------|
| arm (timer API) | no |
| arm_absent | yes |
| post_loop | yes |
| fire | no |
| delivered | no |
| draw | no |
| ret0ok | no (`ret0=-1`) |

E5 falsified “host never polls after start_dsm”. Host polls with `timers_active=0`. Guest never called `timerStart`/`mr_timerStart`.

**Next:** PLATFORM_RET0_CAUSE — why `mrc_init` returns `-1` under DOCUMENTED 6/8/0 args.
