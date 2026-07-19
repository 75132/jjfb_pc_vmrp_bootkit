# v61 Callback Nest-Fix 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v61_callback_nest_fix_stdout.txt`

## 1. 目标

- v60：`entry_2F5404` 出现 1 次后卡在 `family app=7`（嵌套 emu）。
- v61：`ext_call` 期间 defer `1E209`，返回后再 flush。
- 禁止 FORCE ui_mode / 注入 C0 / host 画 UI。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| V61 nest coverage | 1 |
| defer 1E209 | 863 |
| flush deferred | 431 |
| family app=2 | 1 |
| ERW+0x8C4 OK | 1 |
| mrc_resume | 1 |
| timer err:1000 | 0 |
| ext_call code=2 | 453 |
| ext_call code=2 ret | 11 |
| entry_2F5404 | 431 |
| tail_call_305EB8 | 431 |
| entry_305EB8 | 431 |
| gate_init_2DADC4 | 0 |
| uimode_writer | 0 |
| family C0 | 0 |
| FORCE | 0 |

## 3. 关键日志

```text
[JJFB_V61_NEST] contract=defer_1E209_during_ext_call_then_flush no_FORCE no_C0_inject
[JJFB_V61_NEST] defer_1E209_during_ext_call (fix nested emu hang on first timer/app7)
[JJFB_V61_NEST] defer 1E209 app=3 code=0x0 (avoid nested emu during guest)
[JJFB_V61_NEST] defer 1E209 app=7 code=0x0 (avoid nested emu during guest)
[JJFB_V61_NEST] defer 1E209 app=5 code=0x0 (avoid nested emu during guest)
[JJFB_V61_NEST] flush deferred 1E200 after ext_call code=2
entry_2F5404 #1 lr=0x3056D5 r0-r3=0x0,0x2F5405,0x0,0x0
entry_2F5404 #2 lr=0x3056D5 r0-r3=0x0,0x2F5405,0x0,0x0
entry_2F5404 #3 lr=0x3056D5 r0-r3=0x0,0x2F5405,0x0,0x0
entry_2F5404 #4 lr=0x3056D5 r0-r3=0x0,0x2F5405,0x0,0x0
entry_2F5404 #5 lr=0x3056D5 r0-r3=0x0,0x2F5405,0x0,0x0
entry_2F5404 #6 lr=0x3056D5 r0-r3=0x0,0x2F5405,0x0,0x0
tail_call_305EB8 #1 lr=0x304589
tail_call_305EB8 #2 lr=0x304589
tail_call_305EB8 #3 lr=0x304589
tail_call_305EB8 #4 lr=0x304589
tail_call_305EB8 #5 lr=0x304589
tail_call_305EB8 #6 lr=0x304589
entry_305EB8 #1 lr=0x2F5739 f134d=0 B70=0 B58=0x6BD708 DB0=0x0
entry_305EB8 #2 lr=0x2F5739 f134d=0 B70=0 B58=0x6BD708 DB0=0x0
entry_305EB8 #3 lr=0x2F5739 f134d=0 B70=0 B58=0x6BD708 DB0=0x0
entry_305EB8 #4 lr=0x2F5739 f134d=0 B70=0 B58=0x6BD708 DB0=0x0
entry_305EB8 #5 lr=0x2F5739 f134d=0 B70=0 B58=0x6BD708 DB0=0x0
entry_305EB8 #6 lr=0x2F5739 f134d=0 B70=0 B58=0x6BD708 DB0=0x0
ext_call code=2 input=0x0 len=0 P=0x2AC8DC erw=0x2B1858 helper=0x304AED (timer)
ext_call code=2 ret=0 out_len=0
ext_call code=2
ext_call code=2
ext_call code=2
ext_call code=2
```

## 4. 结论

- 2F5404 已走到 2F5734/305EB8；尚未 gate/writer。

## 5. blocker / 下一步

- next: 追 305EB8 → 305EF4 → 2DADC4 条件。
