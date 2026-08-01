# P22K post-m1 path verdict

## Bottom line

**Class: K1**

dispatcher function epilogues with r0=2 after m1; no further init in this fn

## Evidence

- continuation=0x89BF0
- saw_continuation=1
- m1_sp+0x64=0x0 m1_sp+0x68=0x0
- beq_taken(sp64==0)=1 fallthrough=0
- saw_mov_r0_2=1 saw_ldmfd=1 ldmfd_return_pc=0xC
- bl_hits A5690=0 A5724=0 A5704=6
- insn_rows=48
- stop_reason=natural_601_complete_no_post_progress

## Next fix

trace parent resume after LDMFD return_pc=0xC (who consumes r0=2)
