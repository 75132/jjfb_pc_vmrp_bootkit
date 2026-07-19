# v67 drain / B70 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v67_drain_b70_stdout.txt`

## 1. 目标

- 解释 code=5 节点为何反复 drain。
- 区分：`2E2520 ret==1`（故意留队）vs `312C0C` 出队失败。
- 观察 B70==0 时的 `2FC26C` alt（非 FORCE / 非 C0）。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| 101AB fill | 1 |
| enter 2E4040 | 67 |
| gate_init_2DADC4 | 67 |
| gate_B70_check | 67 |
| BL 2FC26C site | 67 |
| enter 2FC26C | 67 |
| after_2E2520 | 0 |
| ret==1 KEEP | 0 |
| ret!=1 EXPECT_DEQUEUE | 0 |
| 312C0C remove | 0 |
| 312C0C from drain | 0 |
| V67 bad drawFP skip | 62 |
| strb B70 in 2FEBBC | 0 |
| 2FC03C | 0 |
| uimode_writer | 0 |
| FORCE ui | 1 |

## 3. 返回值样本

```text
rets(hex)=[]
B54_count_after=[]
```

## 4. 关键日志

```text
[JJFB_V67_DRAIN] contract=trace_2E2520_ret_312C0C_2FC26C no_FORCE no_C0_inject
[JJFB_V67_DRAIN] contract=trace_2E2520_ret_and_312C0C_dequeue +2FC26C_alt_path +bad_drawFP150C_skip_BLX NO_FORCE NO_INJECT
[JJFB_V67_GATE] BL_2FC26C_site #1 lr=0x304589 B70=0 ui_mode=0x0 (alt when B70==0; not writer)
[JJFB_V67_GATE] enter_2FC26C #1 lr=0x2DAE1F r0-r3=0x0,0x6BD7A4,0x0,0x0 ui_mode=0x0 B70=0
[JJFB_V67_GATE] BL_2FC26C_site #2 lr=0x304589 B70=0 ui_mode=0x3 (alt when B70==0; not writer)
[JJFB_V67_GATE] enter_2FC26C #2 lr=0x2DAE1F r0-r3=0x0,0x6BD7A4,0x0,0x0 ui_mode=0x3 B70=0
[JJFB_V67_GATE] BL_2FC26C_site #3 lr=0x304589 B70=0 ui_mode=0x3 (alt when B70==0; not writer)
[JJFB_V67_GATE] enter_2FC26C #3 lr=0x2DAE1F r0-r3=0x0,0x6BD7A4,0x0,0x0 ui_mode=0x3 B70=0
[JJFB_V67_GATE] BL_2FC26C_site #4 lr=0x304589 B70=0 ui_mode=0x3 (alt when B70==0; not writer)
[JJFB_V67_GATE] enter_2FC26C #4 lr=0x2DAE1F r0-r3=0x0,0x6BD7A4,0x0,0x0 ui_mode=0x3 B70=0
[JJFB_V67_GATE] BL_2FC26C_site #5 lr=0x304589 B70=0 ui_mode=0x3 (alt when B70==0; not writer)
[JJFB_V67_GATE] enter_2FC26C #5 lr=0x2DAE1F r0-r3=0x0,0x6BD7A4,0x0,0x0 ui_mode=0x3 B70=0
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6BD7D8 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6BD7D8 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6BD7D8 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6C0588 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6BD7D8 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6C0588 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6BD7D8 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6C0588 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6C0588 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
[JJFB_V67_DRAWFP] bad fp150C=0x270F obj=0x6BD7D8 -> skip BLX ret lr (unblock 2FC26C/2E2520 dequeue)
gate_B70_check #1 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #2 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #3 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #4 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #5 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #6 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #7 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #8 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #9 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
gate_B70_check #10 lr=0x2DADD1 B70=0 (nonzero -> BL 2FC03C)
[JJFB_V66_101AB] fill buf=0x6AEE6C out=0x27FD08 type=2 payload_len=14 hdr_be=5 body_sz=6 u16=5 body=BE(-1) (2F68E4 terminate → 2E4066 → 2DADC4)
```

## 5. 结论

- 已 skip 坏 drawFP，但仍未见 after_2E2520。

## 6. blocker / 下一步

- next: 确认 2EC6B0 入口总是命中；或在 2EC71A 再加 guard。
