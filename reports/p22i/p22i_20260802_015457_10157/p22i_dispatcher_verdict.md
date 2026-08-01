# P22I-CLEAN cfunction dispatcher verdict

## Bottom line

**Class: D**

```text
natural 6/0/1 used non-child R9 context (possible wrong ERW writes)
→ method8_status=METHOD8_REQUIREMENT_UNPROVEN
→ method8_class=PRESENT_BUT_UNREACHABLE
→ hist_680=SUPERSEDED_BY_NATURAL_601
```

## PASS answers

```
cfunction runtime base/end：0x80000 / 0xD1154
LR 0x89BF0 对应 offset：0x9BF0
真实 callsite：0x89BEC
调用指令：BLX r12
target register：r12
R12 producer：LDR r12,[r7,#0x70]! @0x89BD8
caller function：0x80000 .. 0x89DEC (entry heuristic weak; callsite cluster solid)

自然 method 序列：6→0→1
method6 call/return：YES / YES (r0=0)
method0 call/return：YES / YES (r0=0)
method1 call/return：YES / YES (r0=0)
是否出现 method8：NO

method8 静态分支是否存在：MAYBE/YES
method8 是否属于当前 init：UNPROVEN
6→8→0 历史假设裁决：SUPERSEDED_BY_NATURAL_601

method6 返回消费分支：see p22i_return_branch_chain.md
method0 返回消费分支：see p22i_return_branch_chain.md
method1 返回消费分支：see p22i_return_branch_chain.md

调用时实际 R9：see p22i_r9_owner_timeline.csv
R9 owner：see timeline
gamelist ERW：0x682B8C
cfunction ERW：0x280400
是否需要 thunk 内部切换：YES_investigate
ABI 是否正确：LIKELY_WRONG_R9

method1 后是否发布 callback：NO
+0x10740 是否自然进入：NO
+0x7B6C 是否自然进入：NO
是否出现真实 cfg open：NO

是否 Host 调用 init method：NO
是否修改 Guest：NO
是否注入事件：NO
是否启用 FAST：NO
当前唯一门锁：natural 6/0/1 used non-child R9 context (possible wrong ERW writes)
下一处最小通用修复：fix generic parent→child helper call frame R9 (no gamelist-specific call)
stop_reason：natural_601_complete_no_post_progress
fire_ext_n：1
guest_insn_n：2359
```
