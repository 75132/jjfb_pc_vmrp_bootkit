# v57 C0 / Callback Source Coverage 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v57_c0_callback_source_stdout.txt`

## 1. 目标

- 只观察谁应产生 family `app=0xC0`、谁应 BL `3054A4` 注册 `2F5405`。
- 禁止 FORCE / 注入 C0 / 注入 event 5·12 / host 画 UI。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| V57_SRC installed | 1 |
| call 2F5390 prep | 0 |
| BL 3054A4 | 0 |
| BL 3054A4 + CALLBACK_2F5404 | 0 |
| CMP r0,#0xC0 | 0 |
| CMP 时 r0==0xC0 | 0 |
| MOVS #0xC0 | 0 |
| family dispatch (max #) | 400 |
| family log lines | 16 |
| family app=0xC0 | 0 |
| callback register | 0 |
| CALLBACK_2F5404 标记 | 0 |
| 2F5404 entry | 0 |
| 2DADC4 / writer | 0 / 0 |
| FORCE | 0 |
| natural_mode | 1 |

## 3. 关键日志

```text
[JJFB_V57_SRC] contract=observe_2F5390_BL3054A4_C0_no_FORCE_no_inject
[JJFB_V57_SRC] coverage installed (call_2F5390/BL_3054A4/cmp_C0/movs_C0) NO_INJECT NO_FORCE
[JJFB_V56_FAMILY] dispatch #1 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #2 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #3 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #4 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #5 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #6 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #7 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #8 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #9 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
[JJFB_V56_FAMILY] dispatch #10 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #11 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #12 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #100 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #200 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #300 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
[JJFB_V56_FAMILY] dispatch #400 app=0x9 code=0x0 p0=0x0 p1=0x0 lr=0x80000
```

## 4. 结论

- 回调注册上游（call 2F5390 / BL 3054A4）整段未执行；不是“已注册未调度”，而是注册链从未启动。

## 5. blocker / 下一步

- next: 静态追 0x304418 / 0x3053BA 的 caller（谁应启动注册）。
