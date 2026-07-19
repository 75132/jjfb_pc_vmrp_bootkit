# Cursor 下一阶段说明：Phase 6M — cfunction publication / chunk_field_04 Source Audit

> Phase 6L 已完成。它证明：`gbrwcore.ext` 的 documented entry 确实命中，ABI variants 全部无效，entry 没到 `+0xC` init cluster，最后是 `POST_ENTRY_MR_EXIT / EARLY_RETURN_ABI`。  
> 但 6L stdout 里有一个比报告摘要更关键的发现：  
> **`P+0x0C` 不是完全没人写，而是 `dsm:cfunction.ext @ 0x94F04` 明确写了 `0`。**  
> 所以 6M 不应继续盲试 entry ABI，而要回到 `cfunction.ext / _mr_c_function_new / EXT_REGISTER / chunk_field_04` 的 publication 源头。

---

## 0. Phase 6L 关键结论

### 0.1 已确认

```text
ENTRY_HIT = yes
entry_pc = image_base + 8 = 0x2EB7E8
entry EMU_OK = yes
entry coverage = 98 instructions
cluster_any = no
end_reason = stop_at_base
mythroad_exit = post_entry
ABI variants = none succeeded
wxjwq baseline = same behavior
natural P+0x0C nonzero = no
```

### 0.2 entry ABI variants 全部无效

以下 variants 都一样：

```text
baseline
r0_p
r0_1_r1_p
r0_p_r1_param
mirror_callback_regs
wxjwq_baseline
```

结果均为：

```text
entry_hit=yes
emu_ok=yes
cluster=no
pxc_nz=no
end_reason=stop_at_base
class=EARLY_RETURN_ABI
```

所以不要继续在这些 ABI variants 里打转。

---

## 1. 6L 里的重大发现

stdout 显示：

```text
[JJFB_P_FIELD_WRITE] off=0x0C old=0x0 new=0x0 pc=0x94F04 lr=0x89D5C module=dsm:cfunction.ext P=0x2AC8DC
[JJFB_EXTCHUNK_WRITE] old=0x0 new=0x0 pc=0x94F04 lr=0x89D5C phase=live addr=P+0xC
```

也就是说：

```text
P+0xC 不是完全没有写；
而是 cfunction.ext 的某个例程把它写成了 0。
```

同时附近还有：

```text
[JJFB_P_WRITE] off=0x00 old=... new=0x0 pc=0x94F04
[JJFB_P_WRITE] off=0x04 ... new=0x0 pc=0x94F04
[JJFB_P_WRITE] off=0x08 ... new=0x0 pc=0x94F04
[JJFB_P_WRITE] off=0x10 ... new=0x0 pc=0x94F04
```

这非常像：

```text
cfunction.ext 对 20-byte P struct 做 zero-init / memset，
而不是发布真实 extChunk。
```

但必须通过反汇编和寄存器追踪确认。

---

## 2. 6M 核心判断

当前 blocker 需要重新命名为：

```text
CFUNCTION_PUBLICATION_SOURCE_ZERO
```

或者：

```text
CHUNK_FIELD_04_SOURCE_MISSING
```

不是：

```text
ENTRY_NOT_HIT
```

也不是：

```text
NO_PXC_WRITER
```

更不是：

```text
需要 fake P+0xC
```

---

## 3. Phase 6M 唯一目标

```text
查清 `dsm:cfunction.ext @ 0x94F04` 为什么把 P+0xC 写成 0，
以及真实 mrc_extChunk 源头应该来自哪里。
```

必须回答：

```text
1. 0x94F04 是什么函数？
2. 它是在 memset/zero-init P，还是在发布 extChunk？
3. P+0xC 的写入值来自哪个寄存器/内存源？
4. 该源为什么是 0？
5. EXT_REGISTER 里的 chunk_field_04=0 是不是 P+0xC 的直接来源？
6. chunk_field_04 正常应由谁写？
7. _mr_c_function_new 是否本应分配/返回/注册 extChunk？
8. 当前 host 实现是否只记录 helper，却没有创建 module extChunk？
```

---

## 4. 必须分析的关键链

### 4.1 gbrwcore 调 cfunction_new

日志链：

```text
gbrwcore.ext pc=0x30CA94
→ dsm:cfunction.ext target=0x89CF4
→ _mr_c_function_new
```

参数：

```text
r0 = 0x30CFE9   ; helper / callback candidate
r1 = 0x14       ; P struct length = 20 bytes
r2 = 0x89CF4    ; cfunction function pointer / slot
r3 = stack addr
lr = 0x2EB814
```

记录到：

```text
[CFUNCTION_NEW] module=gbrwcore.ext helper=0x30CFE9 p_len=20 p_guest=0x2AC8DC
```

### 4.2 EXT_REGISTER

目前记录：

```text
[EXT_REGISTER] module_id=3 helper=0x30CFE9 header_entry_candidate=0x2EB7E8 chunk_field_04=0x0 result=OK
[ENTRY_RECONCILE] ... chunk_field_04_writer=NONE_BEFORE_SELECT
```

这说明：

```text
module registry 里 helper 有了；
header entry 有了；
但 chunk_field_04 没有发布。
```

### 4.3 0x94F04 zero writer

后续：

```text
gbrwcore.ext pc=0x30CB1C
→ dsm:cfunction.ext target=0x94F04
→ 写 P+0x00..+0x13 全部为 0
```

这是 6M 的第一观察对象。

---

## 5. 第一任务：反汇编 dsm:cfunction.ext 关键函数

必须输出：

```text
reports/phase6m_cfunction_disasm_89cf4_94f04.md
```

范围：

```text
0x89CF4 .. 0x89D80
0x94E94 .. 0x94F40
0x94F04 .. 0x94F60
```

必须标注：

```text
函数边界
是否 memset / zero loop
写 P+0x00/+0x04/+0x08/+0x0C/+0x10 的指令
写入值的来源寄存器
r0/r1/r2/r3 在入口和写入时的含义
返回值
调用者
```

日志：

```text
[JJFB_CFN_DISASM] func=0x94F04 kind=zero_init/memset/publication/unknown
[JJFB_CFN_PXC_SOURCE] pc=0x94F04 src_reg=... src_value=0x0 reason=...
```

---

## 6. 第二任务：P struct 时间线必须按序列号重建

当前日志里有“写了 +0/+4/+8，又被 0x94F04 清零，再被 gbrwcore 写回”的混合现象。  
6M 必须生成严格时序：

```text
reports/phase6m_p_struct_timeline.md
```

格式：

| seq | pc | module | operation | P | off | old | new | caller | meaning |
|---:|---|---|---|---|---:|---|---|---|---|

必须区分：

```text
zero-init writes
metadata writes
publication writes
final value
```

特别是：

```text
0x94F04 是否先清零；
0x30CACE/0x30CADE 是否之后写回 +0；
0x30CAC0 是否写 +4；
是否存在任何非零 +0xC candidate。
```

---

## 7. 第三任务：chunk_field_04 源头审计

新增报告：

```text
reports/phase6m_chunk_field_04_source.md
```

必须回答：

```text
1. chunk_field_04 在 module registry 哪个结构字段？
2. 它当前为什么是 0？
3. 它是否从 MRPGCMAP header +4 读取？
4. 它是否从 _mr_c_function_new 返回值读取？
5. 它是否从 helper table / extChunk allocation 读取？
6. 它是否应在 EXT_REGISTER 时被设置？
7. 它是否应在 0x94F04 后由另一个函数设置？
```

日志：

```text
[JJFB_CHUNK_FIELD04] module=gbrwcore.ext source=... value=0x0
[JJFB_CHUNK_FIELD04_WRITE] old=... new=... pc=... module=...
[JJFB_CHUNK_FIELD04_MISSING] reason=...
```

---

## 8. 第四任务：_mr_c_function_new 语义审计

当前日志反复说明：

```text
CFUNCTION_NEW_SIDE_EFFECT kind=HOST_GLOBAL_HELPER helper=...
CFUNCTION_NEW_SIDE_EFFECT kind=GUEST_P_STRUCT p_guest=...
CFUNCTION_NEW_SIDE_EFFECT kind=NO_DSM_DISPATCH_WRITE
```

6M 必须确认：

```text
当前 host _mr_c_function_new 是否只做了 helper registration；
是否缺少 mrc_extChunk allocation；
是否缺少把 extChunk 写入 module registry / P+0xC；
是否原平台 _mr_c_function_new 约定返回 status 0，还是返回 P/extChunk/helper；
```

输出：

```text
reports/phase6m_mr_c_function_new_contract.md
```

注意：

```text
不能因为“缺少 allocation”就直接 fake chunk；
只能定义下一阶段合法修复条件。
```

---

## 9. 第五任务：寻找合法 extChunk 结构

不允许伪造，但可以寻找真实结构候选。

候选来源：

```text
DSM cfunction.ext 内部 helper table
module registry
gbrwcore.ext helper 0x30CFE9 附近
0x2B0D18 ER_RW metadata
0x30F6F0 / 0x30F6F8 参数表
mr_helper.h documented structure
已有平台源码中的 c_function / extChunk 结构
```

要求：

```text
找“已有自然结构”，不是 malloc fake。
```

报告：

```text
reports/phase6m_extchunk_candidate_structs.md
```

字段：

```text
candidate addr
source
why candidate
has +0x28 valid?
points to code/table?
created by whom?
can it be proven natural?
```

---

## 10. 不要再做的事

暂时不要：

```text
继续试 entry ABI variants
扩大到所有游戏
追 gamelist/lib.runapp
追 UI/图形
直接跳到 +0xC cluster
```

原因：

```text
entry 已经执行；
ABI variants 无候选；
当前最近证据是 cfunction.ext 把 P+0xC 清零/写零。
```

---

## 11. 成功标准

### 最低成功

```text
确认 0x94F04 是 zero-init/memset 还是 publication writer。
```

### 中级成功

```text
定位 P+0xC 的写零来源：寄存器/结构字段/module registry/chunk_field_04。
```

### 高级成功

```text
定位合法 extChunk publication routine 或 missing allocation/register step，
并给出下一阶段唯一修复方向。
```

下一阶段可能是：

```text
Phase 6N: restore legitimate extChunk allocation/register in _mr_c_function_new
Phase 6N: fix chunk_field_04 publication from module registry
Phase 6N: restore cfunction/reg primary publication flow
```

仍然禁止：

```text
invent P+0xC
hardcode extChunk
skip fault
force UI
```

---

## 12. 新增脚本

```powershell
.\RUN_PHASE6M_CFUNCTION_PUBLICATION_SOURCE_AUDIT.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_guest_native_runapp"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER="gbrwcore_only"
$env:JJFB_CFUNCTION_PUBLICATION_AUDIT="1"
$env:JJFB_CHUNK_FIELD04_AUDIT="1"
$env:JJFB_P_TIMELINE_TRACE="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 13. 产物

```text
logs/phase6m_cfunction_publication_stdout.txt
logs/phase6m_cfunction_publication_report.txt

reports/phase6m_cfunction_disasm_89cf4_94f04.md
reports/phase6m_p_struct_timeline.md
reports/phase6m_chunk_field_04_source.md
reports/phase6m_mr_c_function_new_contract.md
reports/phase6m_extchunk_candidate_structs.md
reports/phase6m_verdict.md

packages/JJFB_phase6m_cfunction_publication_source_pack_*.zip
```

---

## 14. 给 Cursor 的一句话

**Phase 6L 证明 entry 已命中且 ABI variants 都无效，但 stdout 有重大线索：`dsm:cfunction.ext @ 0x94F04` 对 P struct 做了 20-byte 写零，其中包括 `P+0x0C old=0 new=0`；同时 `EXT_REGISTER` 里 `chunk_field_04=0` 且 `chunk_field_04_writer=NONE_BEFORE_SELECT`。Phase 6M 请暂停继续试 entry ABI/runapp/UI，转为 cfunction publication source audit：反汇编 0x89CF4/0x94E94/0x94F04，重建 P struct 写入时间线，追 `chunk_field_04` 和 `_mr_c_function_new` 的真实契约，找合法 extChunk 来源。禁止 invent P+0xC 或硬跳 cluster。**
