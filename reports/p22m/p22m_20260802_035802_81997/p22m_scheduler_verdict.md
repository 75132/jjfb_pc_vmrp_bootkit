# P22M-CLEAN cfunction queue/scheduler verdict

## Bottom line

init node 0x2ACA04 unlinked via +0x174C8; dispatcher branch @0x9C41C TAKEN ->0x9C56C (lhs=0x0); continues draining object list but never enqueues callback/UI/+0x10740 after natural 6→0→1

## PASS answers

```
cfunction runtime image SHA：2e6bea16b462f1bb141644fbb6c0a7088cc88f969648535b730659ca505eecf5
+0x17970 真实函数作用：reads [object+8]=base, returns base-(R0<<3); R0 was wrapper index=2; result=node/slot ptr 0x2ACA04
+0x1D098 所属函数：entry=0x9D098 — in fn_entry=0x9D098; CMP derived_node; bounds vs [object+8]; BL +0x174C8 to consume/unlink node
+0x174C8 真实作用：object=0x2A8264 node=0x2ACA04; mutates +0x0C/+0x14 (list unlink); ret_r0=0x2AC9E4; continuation=+0x1D0E0

派生节点地址：0x2ACA04
节点类型/tag：slot_index=2_temp_stack_or_array_node_from_wrapper_R0
节点自然生产者：wrapper_final_R0=2_selects_preexisting_slot; slot table filled earlier in cfunction init (see provenance writes)

object +0x08：before=0x2ACA14 after=0x2AC9E4
object +0x0C：before=0x2AC9EC after=0x2AC9D4
object +0x14：before=0x2A84E4 after=0x2A84CC
object +0x30：before=0x100 after=0x100
调用前后变化：+0x0C CHANGED; +0x14 CHANGED; +0x30 same/unknown

+0x174C8 返回值：0x2AC9E4
返回 continuation：0x9D0E0 (+0x1D0E0)
+0x1D098 所属函数最终返回：NO pc=0x0 r0=0x0
上层调用者：0x0

节点消费后队列状态：unknown
下一条调度记录是否存在：NO
callback/helper/event 是否存在：callback=NO helper_reenter=YES
为什么没有继续初始化：init node 0x2ACA04 unlinked via +0x174C8; dispatcher branch @0x9C41C TAKEN ->0x9C56C (lhs=0x0); continues draining object list but never enqueues callback/UI/+0x10740 after natural 6→0→1

第一条真实阻断分支：pc=0x9C41C TAKEN ->0x9C56C
实际操作数：lhs=0x0 rhs=0x2AC9D4
调度目标路径：TAKEN ->0x9C56C
字段最后写入者：pc=0x97548 off=0x17548 +0x14
自然生产者：missing_UI_init_enqueuer_after_helper_6_0_1

+0x10740 是否进入：NO
+0x7B6C 是否进入：NO
真实 cfg open：NO
真实游戏画面：NO

是否修改 Guest：NO
当前唯一门锁：init node 0x2ACA04 unlinked via +0x174C8; dispatcher branch @0x9C41C TAKEN ->0x9C56C (lhs=0x0); continues draining object list but never enqueues callback/UI/+0x10740 after natural 6→0→1
下一处最小通用修复：find natural producer that sets object flag bits (TST #0xC path) or enqueues UI/init record after helper 6/0/1; do not inject
```

## Evidence

- run_id=p22m_20260802_035802_81997 cf_base=0x80000 cf_end=0xD1154 gen=0
- call_174c8 r0=0x2A8264 r1=0x0 r2=0x2ACA04 r3=0xC
- slice_n=1370 parent_n=10 writes=8 cf_insn=1369
- entered F670=0 8CDC=0 D978=0 10740=0 7B6C=0
- stop_reason=helper_reenter
