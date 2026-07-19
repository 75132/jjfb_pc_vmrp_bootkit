# Stage E5 鈥?ret0 vs scheduler correlation

| Item | Result |
|------|--------|
| JJFB_MRC_INIT | yes |
| ret0=0 | no |
| TIMER_ARM before/after init | no (ABSENT) |
| POST_START_LOOP after return | yes |

Interpretation: mr_doExt ignores mrc_init ret (DOCUMENTED). Scheduler arm independent of ret0 success.
