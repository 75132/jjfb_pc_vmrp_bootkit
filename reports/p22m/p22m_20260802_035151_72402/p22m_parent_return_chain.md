# P22M parent return chain

- fn_1d098_entry=0x9D098 sp=0x27F9A0
- call_174c8 @ +0x1D0DC r0=0x2A8264 r1=0x0 r2=0x2ACA04 r3=0xC r9=0x280400 sp=0x27F9A0 lr/cont=0x972CC
- ret_174c8 r0=0x2AC9E4 continuation=+0x1D0E0 seen=1
- fn_1d098_returned=1 ret_pc=0x9D0E0 ret_r0=0x2AC9E4
- parent_after=0x9D0E0 parent_returned=1 helper_reenter=0

## Timeline

1. `begin_dense` pc=0x86E5C off=0x6E5C r0=0x2 r2=0x2ACA0C — post_wrapper_m1
2. `hit_17970` pc=0x97970 off=0x17970 r0=0x2 r2=0x2ACA0C — index_scale_entry
3. `fn_1d098_at_cmp` pc=0x9D098 off=0x1D098 r0=0x2ACA04 r2=0x2ACA0C — derived_ptr_check
4. `call_174c8` pc=0x9D0DC off=0x1D0DC r0=0x2A8264 r2=0x2ACA04 — pre_remove
5. `enter_174c8` pc=0x974C8 off=0x174C8 r0=0x2A8264 r2=0x2ACA04 — container_ops
6. `fn_1d098_return` pc=0x9D0E0 off=0x1D0E0 r0=0x2AC9E4 r2=0xFFFFFFFF — containing_fn_returned
7. `ret_174c8` pc=0x9D0E0 off=0x1D0E0 r0=0x2AC9E4 r2=0xFFFFFFFF — post_remove
8. `sched_branch` pc=0x9C41C off=0x1C41C r0=0x0 r2=0xFFFFFFFF — TAKEN ->0x9C56C
9. `enter_174c8` pc=0x974C8 off=0x174C8 r0=0x2A8264 r2=0x2AC9D4 — container_ops
10. `parent_frame_return` pc=0x972CC off=0x172CC r0=0x2AC9CC r2=0xFFFFFFFE — upper_caller
11. `end_dense` pc=0x0 off=0x0 r0=0x0 r2=0x0 — parent_returned_after_consume
