# P22L-CLEAN parent return / R0=2 consumer verdict

## Bottom line

**Class: D**

stack_decoded_pc!=unicorn next_pc (stack=0xABD44 actual=0x86E5C)

## PASS answers

```
E8BD8DF0 实际寄存器列表：{r4,r5,r6,r7,r8,r10,r11,pc}
pre-LDM SP：0x27F948
PC 栈槽地址：0x280564
栈中返回地址：0xABD44
Unicorn 下一条实际 PC：0x86E5C
二者是否一致：NO (prefer unicorn)

真实父级 module：cfunction.ext
真实父级 offset：0x6E5C
父级 callsite：0x89C2C
wrapper 返回 R0：2
R0 第一消费指令：NONE @ 0x0
比较操作数：lhs=0x0 rhs=0x0
实际分支：NONE
目标分支：NONE

method 6 真实返回：0 (0x0)
method 0 真实返回：-1 (0xFFFFFFFF)
method 1 真实返回：1 (0x1)
wrapper 最终返回：2

callback 是否发布：NO
+0x10740 是否进入：NO
+0x7B6C 是否进入：NO
真实 cfg open 是否出现：NO

当前唯一门锁：stack_decoded_pc!=unicorn next_pc (stack=0xABD44 actual=0x86E5C)
下一处最小通用修复：audit thumb bit / exception return / SP sample timing; prefer unicorn PC
```

## Evidence

- run_id=p22l_20260802_031007_7326
- cont=0x89BF0 ldm_rows=3 next_rows=3 slice_n=1
- entered F670=0 8CDC=0 D978=0 10740=0 10814=0 7B6C=0
- stop_reason=parent_returned
- r0_meaning=?
