# Cursor 下一阶段说明：Phase 6L — MRPGCMAP Entry ABI / Init Cluster Reachability Audit

> Phase 6K 已经证明：`image_base+8` 的 documented entry 可以被命中，并且 entry 在 callback continuation 前执行。  
> 但 `P+0x0C / mrc_extChunk` 仍没有自然写入，entry 正常返回后发生 `mythroad exit`。  
> 因此 6K 的“修 entry order”只完成了第一半：**entry 顺序对了，但 entry 语义/ABI/可达路径仍不对。**

---

## 0. Phase 6K 结论复盘

已确认：

```text
MRPGCMAP_ENTRY_HIT = yes
entry_pc = image_base + 8 = 0x2EB7E8
entry EMU_OK = yes
entry_called before callback_continuation = yes
natural P+0xC write = no
classification = ENTRY_OK_BUT_NO_EXTCHUNK
6K-C/D skipped
```

重要变化：

```text
之前是 WRONG_ENTRY_SELECTION；
现在入口顺序已经修正；
但 publication 仍没有发生。
```

所以不要回退到：

```text
WRONG_ENTRY_SELECTION
```

现在的新问题是：

```text
为什么 documented entry 执行并返回，却没有走到任何 +0xC init cluster？
```

---

## 1. Phase 6L 唯一目标

```text
解释 documented entry 为什么返回但没有发布 P+0xC。
```

具体要查：

```text
1. entry ABI 参数是否错误？
2. entry 调用时 R0/R1/R2/R3/SP/LR/R9 是否符合原平台约定？
3. entry 是否因为 mode/arg/state 不对走了早退路径？
4. emu stop rule 是否太早，导致 entry 后续 init cluster 未跑？
5. 当前 watch 的 P 是否是 entry 真实要初始化的 P？
6. +0xC init clusters 是否在 entry 可达路径上？
7. publication 是否不是 entry 内部完成，而是由 cfunction/reg.ext primary 或第二阶段 callback 完成？
```

---

## 2. 仍然禁止

Phase 6L 只做观测和最小 ABI 实验。

禁止：

```text
fake P+0xC
手动写 mrc_extChunk
直接跳到 +0xC cluster
skip fault
R9 promotion
force ui_mode
写 AC8/progress
修改 jjfb/robotol 游戏逻辑
```

允许：

```text
记录 entry 寄存器
记录 entry coverage
记录 branch trace
延长 emu stop window
调整合法 entry ABI 参数进行对照实验
比较 gbrwcore/jjfb/wxjwq
```

---

## 3. 第一优先：entry 调用 ABI 快照

6K 只证明了 entry hit，不足以证明 entry ABI 正确。  
6L 必须在 entry 前后记录完整寄存器：

```text
R0 R1 R2 R3 R4 R5 R6 R7 R8 R9 R10 R11 R12 SP LR PC CPSR/Thumb
```

日志：

```text
[JJFB_ENTRY_ABI_PRE] module=gbrwcore.ext pc=0x2EB7E8 r0=... r1=... r2=... r3=... r9=... sp=... lr=...
[JJFB_ENTRY_ABI_RET] module=gbrwcore.ext ret_pc=... r0=... r1=... r2=... r3=... r9=... sp=... reason=...
```

重点判断：

```text
R0 是否是 expected P？
R1 是否是 moduleP / ext context？
R2/R3 是否是 MR_LOAD_C_FUNCTION / init reason？
R9 是否应为 DSM、ER_RW，还是 shell module ER_RW？
LR 是否合理？
SP 是否属于合法 stack？
```

---

## 4. 第二优先：entry coverage 和早退路径

必须生成 entry 执行覆盖：

```text
entry_start = image_base + 8
entry_end_reason = return / mr_exit / unmapped / stop_at_base / timeout
executed_basic_blocks
executed_file_offsets
last_50_pcs
branch decisions
```

输出：

```text
reports/phase6l_entry_coverage.md
```

关键问题：

```text
entry 到底执行了多少指令？
是立即返回，还是跑了一段初始化？
有没有到达 0x3CB6 / 0x9E8A / 0xA7F6 / 0xE6D6 / 0x21BB8 这些 +0xC clusters？
如果没到，最近的分叉是哪条？
```

日志：

```text
[JJFB_ENTRY_COVERAGE] module=gbrwcore.ext insns=... unique_blocks=... end_reason=...
[JJFB_ENTRY_BRANCH] pc=... taken=... target=... reason=...
[JJFB_ENTRY_LASTPCS] ...
```

---

## 5. 第三优先：+0xC init cluster 可达性

6K 已列出 gbrwcore.ext 的候选 clusters：

```text
file_off=0x3CB6  VA=0x2EF496
file_off=0x9E8A  VA=0x2F566A
file_off=0xA7F6  VA=0x2F5FD6
file_off=0xE6D6  VA=0x2F9EB6
file_off=0x21BB8 VA=0x30D398
```

6L 要为每个 cluster 判断：

```text
1. 是否在 entry coverage 内？
2. 如果不在，entry 到它是否有静态路径？
3. 前置条件是什么？
4. 它写的是不是当前 P=0x2AC8DC？
5. 它是否初始化 mrc_extChunk 还是其他结构？
```

输出：

```text
reports/phase6l_init_cluster_reachability.md
```

表格：

| cluster | VA | reached? | nearest branch | base reg | writes current P? | hypothesis |
|---|---:|---|---|---|---|---|

---

## 6. 第四优先：当前 P 是否是正确 publication target

6K 的关键现象：

```text
nested P 的 +0/+4/+8 被写
+0xC 不写
```

但这不能自动证明：

```text
entry 的 +0xC cluster 应该写同一个 P
```

6L 必须区分：

```text
launcher/global P
nested CFN P
module-local P
shell ext P
game ext P
```

记录所有疑似 P：

```text
[JJFB_P_CANDIDATE] kind=nested_cfn addr=...
[JJFB_P_CANDIDATE] kind=module_local addr=...
[JJFB_P_CANDIDATE] kind=global_platform addr=...
```

watch 所有 candidate 的：

```text
+0x00 +0x04 +0x08 +0x0C +0x10
```

避免只盯 `0x2AC8DC`。

---

## 7. 第五优先：stop rule / mythroad exit 分析

6K 出现：

```text
entry_returned
mythroad exit
no fault_pc
```

6L 必须解释这个 exit：

```text
1. 是 entry 自己调用 mr_exit？
2. 是 host stop rule 把 entry_return 当成功后停止？
3. 是 LR 返回到了 mythroad exit thunk？
4. 是缺少下一阶段 callback/event？
5. 是缺合法 argv / init reason 导致直接退出？
```

输出：

```text
reports/phase6l_entry_return_exit_reason.md
```

日志：

```text
[JJFB_ENTRY_EXIT] reason=... pc=... lr=... r0=... caller=...
[JJFB_MYTHROAD_EXIT] source=entry_return/lr/mr_exit/api_stub/host_stop
```

---

## 8. 合法 ABI 对照实验

只允许小范围对照，不许乱猜。

### 8.1 ABI variants

新增环境：

```powershell
$env:JJFB_ENTRY_ABI_VARIANT="baseline"
$env:JJFB_ENTRY_ABI_VARIANT="r0_p"
$env:JJFB_ENTRY_ABI_VARIANT="r0_1_r1_p"
$env:JJFB_ENTRY_ABI_VARIANT="r0_p_r1_param"
$env:JJFB_ENTRY_ABI_VARIANT="mirror_callback_regs"
```

每个 variant 必须记录：

```text
entry hit?
entry insn count
P+0xC write?
reached cluster?
exit reason?
new fault?
```

禁止：

```text
variant 直接写 P+0xC
variant 直接改 guest code
variant 直接跳 cluster
```

### 8.2 对照原则

如果某个 ABI variant 让 entry coverage 扩大，或到达 +0xC cluster：

```text
记录为 ABI_CANDIDATE
```

如果某个 ABI variant 只是假装成功但没有 natural writer：

```text
不算成功
```

---

## 9. wxjwq / jjfb 对照仍保留，但暂不扩大

6L 只保留最小对照：

```text
gbrwcore_jjfb
gbrwcore_wxjwq
```

跑同一 ABI variant，看是否一致。  
不要现在扩到所有游戏，避免分支太多。

---

## 10. 成功标准

### 最低成功

```text
解释 entry 为什么返回：早退、host stop、mr_exit、ABI 不匹配、还是缺二阶段 callback。
```

### 中级成功

```text
确认 +0xC init cluster 为什么没执行，定位最近前置条件或 branch。
```

### 高级成功

```text
找到合法 ABI / init order，使 entry 自然到达 +0xC writer，或明确下一阶段唯一修复方向。
```

### 可视化前兆

```text
P+0xC natural write 出现；
_strCom 601/800/801 开始出现；
shell/gamelist/runapp 才能继续推进。
```

---

## 11. 新增脚本

```powershell
.\RUN_PHASE6L_ENTRY_ABI_CLUSTER_AUDIT.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_guest_native_runapp"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER="gbrwcore_only"
$env:JJFB_ENTRY_ABI_AUDIT="1"
$env:JJFB_ENTRY_COVERAGE_TRACE="1"
$env:JJFB_PUBLICATION_AUDIT="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 12. 产物

```text
logs/phase6l_entry_abi_cluster_stdout.txt
logs/phase6l_entry_abi_cluster_report.txt

reports/phase6l_entry_abi_snapshot.md
reports/phase6l_entry_coverage.md
reports/phase6l_init_cluster_reachability.md
reports/phase6l_p_candidate_map.md
reports/phase6l_entry_return_exit_reason.md
reports/phase6l_abi_variant_compare.md
reports/phase6l_verdict.md

packages/JJFB_phase6l_entry_abi_cluster_audit_pack_*.zip
```

---

## 13. 给 Cursor 的一句话

**Phase 6K 已确认 `gbrwcore.ext` 的 MRPGCMAP documented entry `image_base+8` 被命中且 `EMU_OK`，并且 entry 在 callback continuation 前执行，但仍没有 `P+0x0C` 自然写入，分类为 `ENTRY_OK_BUT_NO_EXTCHUNK`。Phase 6L 请不要 fake chunk，也不要回退追 runapp/UI；改做 entry ABI 与 init cluster 可达性审计：记录 entry 前后完整寄存器、coverage/branch/last PC、解释 mythroad exit，判断 `+0xC` clusters 为什么未执行，确认当前 P 是否为正确 target，并用少量合法 ABI variants 做对照。目标是找出 entry 返回但 publication 不发生的真实原因。**
