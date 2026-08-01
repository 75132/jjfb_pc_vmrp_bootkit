# P22H-CLEAN helper handoff provenance verdict

## Bottom line

**Class: E**

```text
Guest path hit helper with partial init methods (saw_m6=1 saw_m8=0 saw_m0=1); full natural 6→8→0 not confirmed
→ missing_contract=method8_or_full_680_sequence
→ 6→8→0 grade=NATURAL_EQUIVALENT_FOUND (source=HISTORICAL_HOST_RECONSTRUCTION unless CONFIRMED)
```

## PASS answers

```
gamelist registered_helper：0x2E3099
helper首次自然调用方法：1
method1 caller module：gamelist.ext
method1 caller PC：0x2E3098
调用指令：UNKNOWN_NOT_EXPOSED
helper指针来源地址：0x0
helper指针写入者：LOG_PARSE
method1 producer/event：UNKNOWN_NOT_EXPOSED

method1 entry R0-R3：0x2AC8EC 0x1 0x2A835C 0xC
stack args：0x0 0x0 0x0 0x0
R9：0x280400
ERW：0x2AF58
P：0x2AC8EC
return：0
return consumer：HOST_return

同一dispatcher是否支持init：YES_OBSERVED
自然init method/event序列：see matrix
6→8→0证据等级：NATURAL_EQUIVALENT_FOUND

第一条阻断分支：Guest helper entry; LR→cfunction caller
实际操作数：UNKNOWN_NOT_EXPOSED
init目标路径：Guest BLX/run into helper (not Host bridge_deliver_ext_init_seq)
字段最后写入者：UNKNOWN_NOT_EXPOSED
自然生产者：gamelist.ext

缺失合同属于：method8_or_full_680_sequence

是否Host调用helper：NO
是否修改Guest：NO
是否注入事件：NO
是否启用FAST：NO
当前唯一门锁：Guest path hit helper with partial init methods (saw_m6=1 saw_m8=0 saw_m0=1); full natural 6→8→0 not confirmed
下一处最小通用修复：disasm Guest caller LR/continuation for missing method=8 producer
stop_reason：method1_and_fire
fire_ext_n：1
```
