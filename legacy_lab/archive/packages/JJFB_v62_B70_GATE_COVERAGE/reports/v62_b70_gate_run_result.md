# v62 B70/B71/15D Gate 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v62_b70_gate_stdout.txt`

## 1. 目标

- 静态确认：`305EB8` 门控是 `15D==1` / `B71!=0` / `134D==0`（不是旧探针的 B70）。
- 动态观察谁写这些标志；禁止 FORCE ui_mode / C0 inject / host UI。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| V62 coverage | 1 |
| mem watch | 1 |
| flag writer sites | 2 |
| flag mem_write | 2 |
| 30CCF4 15D=1 | 1 |
| 2FE854 B71=0 | 1 |
| 30ED7A B71=1 | 0 |
| 2DC572 B71=1 | 0 |
| 2DC576 15D=1 | 0 |
| 2FEC9A B70 | 0 |
| after app=2 dump | 1 |
| entry_305EB8 | 16 |
| fail_15D | 0 |
| fail_B71 | 16 |
| fail_134D | 0 |
| ok_to_2DADC4 | 0 |
| caller_305EF4 | 0 |
| gate_init_2DADC4 | 0 |
| uimode_writer | 0 |
| FORCE | 1 |

## 3. 关键日志

```text
after family app=2 15D=1 B71=0 B70=0 134D=0
entry_305EB8 #1 15D=1 B71=0 f134d=0 B70=0 B58
entry_305EB8 #2 15D=1 B71=0 f134d=0 B70=0 B58
entry_305EB8 #3 15D=1 B71=0 f134d=0 B70=0 B58
entry_305EB8 #4 15D=1 B71=0 f134d=0 B70=0 B58
entry_305EB8 #5 15D=1 B71=0 f134d=0 B70=0 B58
entry_305EB8 #6 15D=1 B71=0 f134d=0 B70=0 B58
[JJFB_V62_FLAG] writer #1 pc=0x30CCF4 lr=0x305E55 strb_15D=1_in_30CBBC r0-r3=0x2B19B5,0x88,0x6BD0D0,0x2B1868 before 15D=0 B71=0 B70=0
[JJFB_V62_FLAG] writer #2 pc=0x2FE854 lr=0x30D0B1 strb_B71=0_in_2FE82C r0-r3=0x2B23C9,0x2B19B4,0xFFFFFFFF,0x0 before 15D=1 B71=0 B70=0
[JJFB_V62_FLAG] mem_write ERW+0x15D size=1 new=0x1 pc=0x30CCF4 lr=0x305E55
[JJFB_V62_FLAG] mem_write ERW+0xB71 size=1 new=0x0 pc=0x2FE854 lr=0x30D0B1
```

## 4. 结论

- 15D 已满足，blocker 是 B71 恒 0（无人置位）。

## 5. blocker / 下一步

- next: 追 2E2520→2DC4D8 / 30ED2C 谁应写 B71=1（事件或平台契约）。
