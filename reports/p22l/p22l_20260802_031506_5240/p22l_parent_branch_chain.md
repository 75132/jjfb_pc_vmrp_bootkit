# P22L parent branch chain

## Wrapper epilogue

- LDM `0xE8BD8DF0` reglist=`{r4,r5,r6,r7,r8,r10,r11,pc}` (mask=0x8DF0)
- pre_ldm_sp=0x27F948 pc_index=7 pc_addr=0x27F964 stack_pc=0x86E5C
- unicorn next_pc=0x86E5C module=cfunction.ext off=0x6E5C match=1 r0=0x2

## R0=2 consumption

- consume_pc=0x0 insn=`NONE`
- cmp_lhs=0x0 cmp_rhs=0x0
- branch=NONE target=NONE
- meaning=UNKNOWN

## Slice summary (first 32)

```
1 0x86E5C cfunction.ext+0x6E5C LDMFD sp!,{r3,r4,r5,pc} r0=0x2 parent_resume
2 0x97970 cfunction.ext+0x17970 w=0xE5941008 r0=0x2 
3 0x97974 cfunction.ext+0x17974 w=0xE28DD008 r0=0x2 
4 0x97978 cfunction.ext+0x17978 w=0xE0410180 r0=0x2 
5 0x9797C cfunction.ext+0x1797C LDMFD sp!,{r4,r5,r6,r7,r8,r10,r11,pc} r0=0x2ACA04 parent_ldm_return
```
