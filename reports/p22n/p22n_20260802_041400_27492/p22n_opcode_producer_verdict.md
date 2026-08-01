# P22N-CLEAN opcode stream / UI-init producer verdict

## Bottom line

natural 6→0→1 then cfunction opcode stream [0x14,0x1B] (0x9C41C=NORMAL_OPCODE_DISPATCH); returns to helper method=2; UI/init record producer not observed

## P22M correction

`0x9C41C BEQ → 0x9C56C` = **NORMAL_OPCODE_DISPATCH**, not a lock.

## PASS answers

```
完整自然 opcode 序列：0x14,0x1B
opcode 0x14 的作用：cursor transform via literals/masks; writes [sp,#0xC] then B +0x1C408
opcode 0x1B 的作用：updates object cursor/list fields; may BL +0x174C8; can LDMFD-return
每条 record 的地址：见 p22n_opcode_stream.csv
每条 record 的写入者：see p22n_record_provenance.csv

命令流是静态还是动态：mixed_or_dynamic
解释器最终如何退出：natural_601_complete_no_post_progress
method=2 由哪个 opcode/事件产生：FIRE_EXT_method2_after_6_0_1 (timer/platform)

是否存在 UI/init opcode：NO_CLEAR_UI_INIT_OPCODE_IN_STREAM
若存在，为何未执行：n/a
若不存在，本应由谁产生：UNKNOWN_UI_INIT_RECORD_PRODUCER_not_in_observed_stream

第一处真实分歧：after natural 6→0→1, interpreter only saw opcodes [0x14,0x1B] then method=2; no UI/init opcode record observed
分歧 PC：0x9C40C
实际字段/返回值：opcodes=0x14,0x1B fire2=1
自然生产者：UNKNOWN_UI_INIT_RECORD_PRODUCER_not_in_observed_stream

+0x10740：NO
+0x7B6C：NO
真实 cfg open：NO
真实游戏画面：NO

是否修改 Guest：NO
当前唯一门锁：natural 6→0→1 then cfunction opcode stream [0x14,0x1B] (0x9C41C=NORMAL_OPCODE_DISPATCH); returns to helper method=2; UI/init record producer not observed
下一处最小通用修复：locate writer of missing UI/init opcode record (not flag 0x0C); compare producer callers upstream of +0x1C40C buffer fill
```

## Identity

- run_id=p22n_20260802_041400_27492
- cfunction_runtime_sha256=2e6bea16b462f1bb141644fbb6c0a7088cc88f969648535b730659ca505eecf5
- cf_base=0x80000 cf_end=0xD1154
- stream_n=2 fire2_n=1 stop=natural_601_complete_no_post_progress
