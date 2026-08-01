# P22L parent branch chain

## Wrapper epilogue

- LDM `0xE8BD8DF0` reglist=`{r4,r5,r6,r7,r8,r10,r11,pc}` (mask=0x8DF0)
- pre_ldm_sp=0x27F948 pc_index=775 pc_addr=0x280564 stack_pc=0xABD44
- unicorn next_pc=0x86E5C module=cfunction.ext off=0x6E5C match=0 r0=0x2

## R0=2 consumption

- consume_pc=0x0 insn=`NONE`
- cmp_lhs=0x0 cmp_rhs=0x0
- branch=NONE target=NONE
- meaning=UNKNOWN

## Slice summary (first 32)

```
1 0x86E5C cfunction.ext+0x6E5C LDMFD sp!,{r3,r4,r5,pc} r0=0x2 parent_ldm_return
```
