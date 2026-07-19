# v60 Family app=2 Timer + Resume 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v60_family_app2_timer_stdout.txt`

## 1. 目标

- 参照 `JJFB_v57_LIFECYCLE_SOURCE_COVERAGE_COMPLETE`：
  method5/resume 注册前必须先有 timer 对象。
- 平台契约：`family app=2` → `30CBBC` → `mrc_timerCreate` → `ERW+0x8C4`，
  再 `mrc_resume(5)` 注册 `2F5405`。
- 禁止 FORCE ui_mode / 注入 C0 / host 画 UI。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| family app=2 | 1 |
| fn_30CBBC / timerCreate | 1 / 2 |
| ERW+0x8C4 OK | 1 |
| ERW+0x8C4 NULL | 0 |
| mrc_resume | 1 |
| timer err:1000 | 0 |
| BL_3054A4 / CALLBACK | 3 / 4 |
| entry_2F5404 | 1 |
| C0 / gate / writer | 0 / 0 / 0 |
| FORCE | 0 |

## 3. 关键日志

```text
[JJFB_V60_LIFECYCLE] contract=family_app2_timerCreate_then_mrc_resume no_FORCE no_C0_inject
[JJFB_V59_LIFECYCLE] contract=mrc_resume_code5_once_after_init
[JJFB_V60_TIMER] coverage installed (30CBBC/timerCreate/ERW8C4) family_app2_before_resume
[JJFB_V60_LIFECYCLE] family app=2 via handler=0x30D301 (app2→30E15E→30CBBC timerCreate; not C0)
[JJFB_V60_TIMER] fn_30CBBC_init #1 pc=0x30CBBC lr=0x30E163 r0-r3=0x2,0x0,0x0,0xE3C
[JJFB_V60_TIMER] mrc_timerCreate #2 pc=0x305404 lr=0x30D08B r0-r3=0x0,0x2802A4,0x24,0x0
[JJFB_V60_TIMER] mrc_timerCreate #3 pc=0x305404 lr=0x30D091 r0-r3=0x2A8340,0x0,0x959A0,0x0
[JJFB_V56_CALLBACK] register #1 pc=0x30D128 lr=0x30D113 r0-r3=0x2A8340,0x0,0x1,0x27FCE0 ERW8C4=0x2A8340
[JJFB_V57_SRC] BL_3054A4 #1 pc=0x30D136 lr=0x30D113 r0-r3=0x2A8340,0x50,0x0,0x2F5405 CALLBACK_2F5404
[JJFB_V56_CALLBACK] register #2 pc=0x3054A4 lr=0x30D13B r0-r3=0x2A8340,0x50,0x0,0x2F5405 ERW8C4=0x2A8340 CALLBACK_2F5404
[JJFB_V60_LIFECYCLE] after family app=2 ERW+0x8C4 timer=0x2A8340 OK
[JJFB_V56_CALLBACK] register #3 pc=0x2F5390 lr=0x3053BF r0-r3=0x5,0x2B1858,0x0,0x48 ERW8C4=0x2A8340
[JJFB_V56_CALLBACK] register #4 pc=0x2F53AC lr=0x303CA7 r0-r3=0x0,0x0,0x1,0x140 ERW8C4=0x2A8340
[JJFB_V57_SRC] BL_3054A4 #2 pc=0x2F53BC lr=0x303CA7 r0-r3=0x2A8340,0x50,0x0,0x2F5405 CALLBACK_2F5404
[JJFB_V56_CALLBACK] register #5 pc=0x3054A4 lr=0x2F53C1 r0-r3=0x2A8340,0x50,0x0,0x2F5405 ERW8C4=0x2A8340 CALLBACK_2F5404
[JJFB_V59_LIFECYCLE] mrc_resume(5) after init ret=0 (targets robotol 304B5A→2F5390 register path)
[JJFB_V56_CALLBACK] register #6 pc=0x3054A4 lr=0x3056CB r0-r3=0x2A8340,0x0,0x0,0x0 ERW8C4=0x2A8340
[JJFB_V56_CALLBACK] entry_2F5404 #1 lr=0x3056D5 r0-r3=0x0,0x2F5405,0x0,0x0
```

## 4. 结论

- entry_2F5404 已命中且无 timer err:1000；尚未到 2DADC4/writer — 回调调度已开但上游未跑完。

## 5. blocker / 下一步

- next: 追 2F5404 → 305EB8 → 2DADC4；查为何只调度 1 次。
