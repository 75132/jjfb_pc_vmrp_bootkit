# P22L-CLEAN parent return / R0=2 consumer verdict

## Bottom line

**Class: C**

wrapper returns into cfunction state machine @+0x6E5C (not helper); R0=2 scale_index_r0_2_in_SUB (r0=r1-(r0<<shift)); not boolean status

## PASS answers

```
E8BD8DF0 实际寄存器列表：{r4,r5,r6,r7,r8,r10,r11,pc}
pre-LDM SP：0x27F948
PC 栈槽地址：0x27F964
栈中返回地址：0x86E5C
Unicorn 下一条实际 PC：0x86E5C
二者是否一致：YES

真实父级 module：cfunction.ext
真实父级 offset：0x6E5C
父级 callsite：0x86E58
wrapper 返回 R0：2
R0 第一消费指令：DP(op=2) r0,r1,r0,LSL#3 @ 0x97978
比较操作数：lhs=0x2ACA14 rhs=0x2
实际分支：NOT_TAKEN
目标分支：0x9D0F4 (on_derived_r0)

method 6 真实返回：0 (0x0)
method 0 真实返回：-1 (0xFFFFFFFF)
method 1 真实返回：1 (0x1)
wrapper 最终返回：2

callback 是否发布：NO
+0x10740 是否进入：NO
+0x7B6C 是否进入：NO
真实 cfg open 是否出现：NO

当前唯一门锁：wrapper returns into cfunction state machine @+0x6E5C (not helper); R0=2 scale_index_r0_2_in_SUB (r0=r1-(r0<<shift)); not boolean status
下一处最小通用修复：trace upward from 0x86E5C / consume@0x97978; find schedule into +0xF670/+0x10740
```

## Evidence

- run_id=p22l_20260802_032452_16306
- cont=0x89BF0 ldm_rows=3 next_rows=4 slice_n=32
- entered F670=0 8CDC=0 D978=0 10740=0 10814=0 7B6C=0
- stop_reason=r0_consumed_and_branched
- r0_meaning=scale_index_r0_2_in_SUB (r0=r1-(r0<<shift)); not boolean status
