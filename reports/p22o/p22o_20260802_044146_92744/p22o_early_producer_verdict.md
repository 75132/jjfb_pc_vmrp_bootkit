# P22O-CLEAN early command-buffer producer verdict

## Bottom line

cmd buffer early producer: class=dynamic_early_writes write14=1@+0x0 write1b=1@+0x0; no UI/init record after 0x14/0x1B

## PASS answers

```
0x2AF8F8 first writer：OBSERVED pc=0x927A8 off=+0x0
0x2AF904 first writer：OBSERVED pc=0x927A8 off=+0x0
writer_class：dynamic_early_writes
opcode stream：0x14,0x1B
append_n：23879
skip_field_Y：UNKNOWN
skip_actual_A：UNKNOWN
expected_W：UNKNOWN

+0x10740：NO
+0x7B6C：NO
真实 cfg open：NO
真实游戏画面：NO

是否修改 Guest：NO
当前唯一门锁：cmd buffer early producer: class=dynamic_early_writes write14=1@+0x0 write1b=1@+0x0; no UI/init record after 0x14/0x1B
下一处最小通用修复：inspect producer +0x0/+0x0 skip predicate (field Y=UNKNOWN actual=UNKNOWN); find natural contract W that should enqueue UI/init
```

## Identity

- run_id=p22o_20260802_044146_92744
- cfunction_runtime_sha256=2e6bea16b462f1bb141644fbb6c0a7088cc88f969648535b730659ca505eecf5
- cf_base=0x80000 cf_end=0xD1154
- stop=early_producer_closed_idle
