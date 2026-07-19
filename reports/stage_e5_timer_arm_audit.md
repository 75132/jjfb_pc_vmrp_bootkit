# Stage E5 鈥?timer arm audit (gwy/jjfb.mrp)

| Gate | Result |
|------|--------|
| TIMER_ARM_ATTEMPT | yes |
| TIMER_ARM | no |
| TIMER_ARM_ABSENT | yes |
| START_DSM_RETURN | yes |

## Samples
```
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x303B92 method=0 r0=0x0 r9=0x280400 route=bootstrap_first_pc evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_INIT_SEQ] queued=1 reason=retry_after_context_restore evidence=DOCUMENTED source=mythroad.c:mr_doExt/start.mr
[JJFB_TIMER_ARM_ATTEMPT] module=? pc=0x280058 r0=0x1 r1=0x64 r2=0x0 r3=0x0 r4=0x1 delay_ms=100 period_ms=100 route=sendAppEvent kind=0 name=default_status evidence=OBSERVED
[JJFB_TIMER_ARM_ATTEMPT] module=? pc=0x280058 r0=0x10120 r1=0x4 r2=0x682A5C r3=0x0 r4=0x10120 delay_ms=4 period_ms=4 route=sendAppEvent kind=0 name=default_status evidence=OBSERVED
[JJFB_INIT_SEQ] action=deliver_after_code_1 helper=0x304AED version=2011 appinfo=0x682AA4 evidence=DOCUMENTED source=mythroad_mini.c:mr_doExt
[JJFB_INIT_SEQ] helper=0x304AED code=6 P=0x2AC8DC input=0x0 len=2011 er_rw=0x2B1858 evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x304AEC method=6 r0=0x2AC8DC r9=0x2B1858 route=bootstrap_first_pc evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_INIT_SEQ] helper=0x304AED code=8 P=0x2AC8DC input=0x682AA4 len=16 er_rw=0x2B1858 evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x304AEC method=8 r0=0x2AC8DC r9=0x2B1858 route=bootstrap_first_pc evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_INIT_SEQ] helper=0x304AED code=0 P=0x2AC8DC input=0x0 len=2011 er_rw=0x2B1858 evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x304AEC method=0 r0=0x2AC8DC r9=0x2B1858 route=helper_code0_candidate evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_TIMER_ARM_ATTEMPT] module=? pc=0x280058 r0=0x1 r1=0x64 r2=0x0 r3=0x0 r4=0x1 delay_ms=100 period_ms=100 route=sendAppEvent kind=0 name=default_status evidence=OBSERVED
[JJFB_TIMER_ARM_ATTEMPT] module=? pc=0x280058 r0=0x10120 r1=0x4 r2=0x682A5C r3=0x0 r4=0x10120 delay_ms=4 period_ms=4 route=sendAppEvent kind=0 name=default_status evidence=OBSERVED
[JJFB_MRC_INIT] module=robotol.ext helper=0x304AED method=0 ret=-1 route=mr_extHelper evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_INIT_SEQ] delivered ret6=0 ret8=0 ret0=-1 evidence=OBSERVED
[PLATFORM_TIMER] op=START_DSM_RETURN filename=gwy/jjfb.mrp ret=0 evidence=DOCUMENTED
[JJFB_TIMER_ARM_ABSENT] window=mrc_init_to_start_dsm_return reason=no_timer_api_call_seen arm_count=0 evidence=TARGET_OBSERVED
[JJFB_POST_START_LOOP] t_ms=26955 loop_iter=1 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=26955 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=27979 loop_iter=23 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=27979 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=28980 loop_iter=41 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=28980 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=30029 loop_iter=60 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=30029 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=31049 loop_iter=76 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=31049 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=32052 loop_iter=94 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=32052 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=33098 loop_iter=113 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=33098 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=34098 loop_iter=131 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=34098 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=35147 loop_iter=150 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=35147 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=36193 loop_iter=169 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=36193 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=37244 loop_iter=188 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=37244 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=38247 loop_iter=206 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=38247 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=39254 loop_iter=224 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=39254 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=40303 loop_iter=243 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=40303 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=41349 loop_iter=262 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=41349 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=42394 loop_iter=281 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=42394 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=43440 loop_iter=300 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=43440 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=44487 loop_iter=319 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=44487 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=45538 loop_iter=338 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=45538 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=46590 loop_iter=357 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=46590 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=47642 loop_iter=376 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=47642 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=48645 loop_iter=394 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=48645 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=49652 loop_iter=412 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=49652 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=50702 loop_iter=431 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=50702 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=51703 loop_iter=449 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=51703 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=52756 loop_iter=468 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=52756 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=53806 loop_iter=487 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=53806 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=54857 loop_iter=506 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=54857 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=55905 loop_iter=525 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=55905 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=56905 loop_iter=543 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=56905 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=57948 loop_iter=562 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
[JJFB_TIMER_POLL] t_ms=57948 active=0 due=0 fired=0 reason=no_active period_ms=0 evidence=OBSERVED
[JJFB_POST_START_LOOP] t_ms=58992 loop_iter=581 timers_active=0 next_due_ms=0 events_pending=0 last_module=? draw_count=0 refresh_count=0 arm_seen=0 evidence=OBSERVED
```
