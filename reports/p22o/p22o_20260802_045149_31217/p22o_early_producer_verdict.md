# P22O-CLEAN early command-buffer producer verdict

## Bottom line

cmd buffer early producer: class=dynamic_early_writes write14=1@+0x128F8 write1b=1@+0x128F8 staging1b=1; no UI/init record after 0x14/0x1B

## PASS answers

```
0x2AF8F8 first writer：OBSERVED pc=0x928F8 off=+0x128F8
0x2AF904 first writer：OBSERVED pc=0x928F8 off=+0x128F8
op1b staging：OBSERVED addr=0x28065C pc=0x92CE0
writer_class：dynamic_early_writes
opcode stream：0x05,0x01,0x01,0x19,0x22,0x07,0x01,0x07,0x22,0x07,0x22,0x07,0x22,0x07,0x22,0x07,0x05,0x19,0x07,0x05,0x19,0x05,0x01,0x01,0x19
append_n：2
skip_field_Y：object+0x30_or_cmp_regs
skip_actual_A：f30=0x100 r0=0x27FC84 r1=0x0
expected_W：UNKNOWN_natural_platform_contract_for_UI_init_enqueue

+0x10740：NO
+0x7B6C：NO
真实 cfg open：NO
真实游戏画面：NO

是否修改 Guest：NO
当前唯一门锁：cmd buffer early producer: class=dynamic_early_writes write14=1@+0x128F8 write1b=1@+0x128F8 staging1b=1; no UI/init record after 0x14/0x1B
下一处最小通用修复：trace staging@0x28065C (+0x12CE0) to seed publish; skip Y=object+0x30_or_cmp_regs A=f30=0x100 r0=0x27FC84 r1=0x0; find contract W for UI/init enqueue
```

## Identity

- run_id=p22o_20260802_045149_31217
- cfunction_runtime_sha256=2e6bea16b462f1bb141644fbb6c0a7088cc88f969648535b730659ca505eecf5
- cf_base=0x80000 cf_end=0xD1154
- stop=early_producer_closed_idle
