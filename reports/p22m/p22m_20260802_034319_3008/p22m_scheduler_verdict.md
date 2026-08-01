# P22M-CLEAN cfunction queue/scheduler verdict

## Bottom line

did not reach +0x1D098 in this run

## PASS answers

```
cfunction runtime image SHA：NOT_EXPORTED
+0x17970 真实函数作用：index→node: R0=slot_index, [r4+8]=array_base, return base-(R0<<3)
+0x1D098 所属函数：entry=0x0 — validate derived node then call +0x174C8(object, ?, node)
+0x174C8 真实作用：container unlink/pop on object list fields (+0x0C/+0x14/+0x30 flags)

派生节点地址：0x0
节点类型/tag：derived_node_index=0
节点自然生产者：wrapper_final_R0=2_selects_preexisting_slot; slot table filled earlier in cfunction init (see provenance writes)

object +0x08：before=0x0 after=0x0
object +0x0C：before=0x0 after=0x0
object +0x14：before=0x0 after=0x0
object +0x30：before=0x0 after=0x0
调用前后变化：+0x0C same/unknown; +0x14 same/unknown; +0x30 same/unknown

+0x174C8 返回值：0x0
返回 continuation：0x1D0E0 (+0x1D0E0)
+0x1D098 所属函数最终返回：NO pc=0x0 r0=0x0
上层调用者：0x0

节点消费后队列状态：unknown
下一条调度记录是否存在：NO
callback/helper/event 是否存在：callback=NO helper_reenter=NO
为什么没有继续初始化：did not reach +0x1D098 in this run

第一条真实阻断分支：pc=0x0 NONE
实际操作数：lhs=0x0 rhs=0x0
调度目标路径：no_UI_callback_path
字段最后写入者：see_writes_csv
自然生产者：wrapper_final_R0=2_selects_preexisting_slot; slot table filled earlier in cfunction init (see provenance writes)

+0x10740 是否进入：NO
+0x7B6C 是否进入：NO
真实 cfg open：NO
真实游戏画面：NO

是否修改 Guest：NO
当前唯一门锁：did not reach +0x1D098 in this run
下一处最小通用修复：verify m1 wrapper return arming
```

## Evidence

- run_id=p22m_20260802_034319_3008 cf_base=0x0 cf_end=0x0 gen=0
- call_174c8 r0=0x0 r1=0x0 r2=0x0 r3=0x0
- slice_n=1471 parent_n=1 writes=0 cf_insn=0
- entered F670=0 8CDC=0 D978=0 10740=0 7B6C=0
- stop_reason=natural_601_complete_no_post_progress
