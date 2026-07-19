# Phase 6P 复核与 Phase 6Q 方案：Gamelist Native Primary + cfunction Helper ABI

> 结论先说：这次是真 6P 包。  
> 6P 已经把流程推进到 `gamelist.mrp`，但还没有到 `cfg36/post_update/runapp`。  
> 当前新 blocker 不是 extChunk、不是 ER_RW/R9，也不是 slot matrix，而是 **gamelist native primary / dsm:cfunction.ext helper-entry ABI**。

---

## 0. Phase 6P 真实结论

以 `phase6p_verdict.md` / `CONCLUSION.md` / stdout 为准：

```text
verdict = MID_SUCCESS
class = GAMELIST_STARTED_AFTER_CONTINUE
shell_chain_continue = True
gamelist started = True
cfg36 / post_update = False
export_call / native runapp / jjfb natural = False
SLOT_CALL = 0
mythroad exit = False
```

注意：`phase6p_shell_core_continue_report.txt` 里出现过 `HIGH_SUCCESS` / `export_call=True`，但它和 `phase6p_export_dispatcher_result.md`、`phase6p_verdict.md`、`CONCLUSION.md` 冲突。  
本轮应按更细报告和 stdout 判定为：

```text
MID_SUCCESS, not HIGH_SUCCESS
```

---

## 1. 6P 已经成功的部分

### 1.1 已经越过 gbrwcore-only

```text
[JJFB_6P_EXIT_SOURCE] source=shell_chain_continue
[JJFB_SHELL_CORE_CONTINUE] from=gbrwcore.mrp to=gwy/gamelist.mrp via=start_dsm
```

说明：

```text
6O 的 gbrwcore_only stop 已经被越过。
```

### 1.2 gamelist 已经 started

```text
[JJFB_GAMELIST_STARTED] filename="gwy/gamelist.mrp"
[JJFB_SHELL_EXEC] package=gwy/gamelist.mrp stage=mr_start entered=yes
[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=entry
```

说明：

```text
shell chain 已经从 gbrwcore 推到 gamelist。
```

### 1.3 仍未到达 runapp

```text
CFG36_BUILD = 0
POST_UPDATE = 0
EXPORT_CALL = 0
RUNAPP = 0
SLOT_CALL = 0
```

所以：

```text
现在不能铺 slot API matrix；
slot 根本没被调用。
```

---

## 2. 新 blocker

当前 fault：

```text
[EXT_FAIL] module_id=5
stage=ENTRY_EXECUTION
access=UC_MEM_READ_UNMAPPED
fault_pc=0x8CC00
module_offset=0xCC00
invalid_address=0xE58F8FE0
base_reg=r0
base_value=0x0
```

相关分类：

```text
[EXT_FAULT_CLASS] root_cause=ENTRY_ARGUMENT
[BOOTSTRAP_CLASSIFY] entry_class=WRONG_HELPER_CALL_MISSING_P
[HELPER_ABI_SUMMARY] call_site_r0=0x1 thunk_r0=0x0 enter_r0=0x2803E4 call_site_origin=LR_PROXY
```

当前问题应该命名为：

```text
GAMELIST_CFUNCTION_HELPER_ENTRY_ABI
```

或者：

```text
DSM_CFUNCTION_ENTRY_ARGUMENT_FAULT
```

不是：

```text
EXTCHUNK_MISSING
ER_RW_MISSING
SLOT_API_MISSING
RUNAPP_MISSING
```

---

## 3. 关键异常一：gamelist member_view / primary resolve 仍不干净

stderr 里有：

```text
[MRP_MEMBER_VIEW] reg_primary failed package=gwy/gamelist.mrp: no reg.ext primary
```

但静态解析确认 `gamelist.mrp` 里实际有：

```text
gamelist.ext
reg.ext
```

并且 `reg.ext` 字符串中有：

```text
gamelist.ext
```

同时当前 fileopen 显示：

```text
gwy/gbrwcore.mrp → overlay/mrp_member_view/shell_gbrwcore_cfunction.mrp
gwy/gbrwshell.mrp → overlay/mrp_member_view/shell_gbrwshell_cfunction.mrp

但：
gwy/gamelist.mrp → game_files/mythroad/240x320/gwy/gamelist.mrp
```

这说明：

```text
gbrwcore / gbrwshell 走 generated member_view；
gamelist 仍在走 canonical 原包；
它虽然 “started”，但 native primary/成员视图解析可能不完整。
```

Phase 6Q 第一件事必须修这个。

---

## 4. 关键异常二：dsm:cfunction.ext 被以错误 helper-entry 方式执行

fault 前关键上下文：

```text
[EXT_ENTRY_CTX]
module_id=5
module=dsm:cfunction.ext
helper=0xA4178
entry_pc=0xA4178
header_entry_candidate=0x80008
chunk_field_04=0x0
entry_class=WRONG_HELPER_CALL_MISSING_P
r0=0x2803E4
r1=0x1
r2=0x2803B4
r3=0xC
r9=0x280400
```

这说明：

```text
当前不是 gamelist 正常跑到 cfg36；
而是又落回了 dsm:cfunction.ext 的 helper/entry ABI 坑。
```

尤其是：

```text
header_entry_candidate = 0x80008
observed entry_pc = helper 0xA4178
```

这和前面阶段的规律很像：

```text
helper 地址被当成 module entry / 或 cfunction primary entry 调用；
调用参数/上下文不满足 helper 的真实 ABI；
于是出现 ENTRY_ARGUMENT fault。
```

---

## 5. Phase 6Q 唯一目标

```text
让 gamelist.mrp 以正确 native primary/member_view 方式运行；
修正 dsm:cfunction.ext helper-entry ABI，不再把 helper/raw entry 错当 module entry；
推进到 gamelist cfg36/post_update/export/runapp。
```

### 不是目标

```text
不是补 slot matrix
不是补 UI
不是继续 extChunk/R9
不是扩大所有游戏
不是强行 runapp
```

---

## 6. Phase 6Q 任务分解

### 6Q-A：修 gamelist member_view / reg_primary

必须新增报告：

```text
reports/phase6q_gamelist_member_view.md
```

要求：

```text
1. 静态确认 gamelist.mrp 包内有 reg.ext 和 gamelist.ext；
2. 解析 reg.ext primary 指向 gamelist.ext；
3. 生成 overlay/mrp_member_view/shell_gamelist_cfunction.mrp；
4. fileopen 中 gamelist 不再走 canonical 原包，而是 generated member_view；
5. 如果 reg.ext primary 解析失败，必须记录失败原因和 fallback 规则。
```

成功日志：

```text
[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/gamelist.mrp primary=gamelist.ext
[JJFB_FILEOPEN] guest="gwy/gamelist.mrp" host=".../overlay/mrp_member_view/shell_gamelist_cfunction.mrp" backend=generated
[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=init_ok
```

---

### 6Q-B：给 gamelist.ext 启用完整 platform context

不要只给 gbrwcore.ext 做 extChunk/ER_RW。

对 `gamelist.ext` 也启用：

```text
extChunk provider
P+0x0C publication
ER_RW bind
R9_SWITCH_OK
entry/continuation trace
```

报告：

```text
reports/phase6q_gamelist_platform_context.md
```

日志：

```text
[JJFB_EXTCHUNK_ALLOC] module=gamelist.ext ...
[JJFB_EXTCHUNK_PUBLISH] module=gamelist.ext ...
[JJFB_ER_RW_BIND] module=gamelist.ext ...
[JJFB_R9_SWITCH_OK] module=gamelist.ext ...
```

---

### 6Q-C：审计 dsm:cfunction.ext helper-entry ABI

新增报告：

```text
reports/phase6q_dsm_cfunction_helper_abi.md
```

必须回答：

```text
1. 为什么 module_id=5 dsm:cfunction.ext 会进入 helper=0xA4178？
2. 0xA4178 是 helper/function，还是 module entry？
3. 它是否应该通过 MR table / callback bridge 调用，而不是当 ext entry 执行？
4. LR_PROXY 为什么导致 call_site_r0=1 → thunk_r0=0 → enter_r0=P？
5. r0=0x2803E4 是否真是它需要的 P？若是，P+0xC 是否已发布？
6. chunk_field_04=0 是否对 dsm:cfunction.ext 仍需 restore？
```

关键日志：

```text
[JJFB_HELPER_ABI_TRACE] module=dsm:cfunction.ext helper=0xA4178 kind=...
[JJFB_HELPER_CALL_ROUTE] route=mr_table/callback_bridge/module_entry/wrong_helper_entry
[JJFB_HELPER_ARG_FLOW] call_site_r0=... thunk_r0=... enter_r0=... lr_proxy=...
```

### 如果结论是 helper 被错当 entry

允许修：

```text
route helper through callback bridge / mr_table dispatcher
```

禁止修：

```text
硬跳过 fault
硬改 r0
直接 fake 成功
```

---

### 6Q-D：gamelist cfg36/post_update 快速追踪

当 6Q-A/B/C 后没有 ENTRY_ARGUMENT fault，立刻追：

```text
[JJFB_GAMELIST_CFG36_BUILD]
[JJFB_GAMELIST_POST_UPDATE] result=no_update
[JJFB_SHELL_EXPORT_CALL] name=lib.runapp/lib.startGame
[JJFB_RUNAPP] source=native_shell
```

重点：

```text
string_va_not_entry 仍不能作为函数入口；
必须找 dispatcher/export call。
```

---

## 7. 新脚本

```powershell
.\RUN_PHASE6Q_GAMELIST_NATIVE_PRIMARY_ABI.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_shell_core_continue"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_EXTCHUNK_PROVIDER="shell_core"
$env:JJFB_ER_RW_BIND_RESTORE="shell_core"
$env:JJFB_SHELL_CHAIN_MODE="continue_after_gbrwcore_init"
$env:JJFB_GAMELIST_MEMBER_VIEW_FIX="1"
$env:JJFB_DSM_CFUNCTION_HELPER_ABI_AUDIT="1"
$env:JJFB_GWY_UPDATE_STUB="no_update"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 8. 成功标准

### 最低成功

```text
gamelist.mrp member_view / primary=gamelist.ext 正确生成；
不再出现 reg_primary failed package=gwy/gamelist.mrp。
```

### 中级成功

```text
dsm:cfunction.ext 0x8CC00 / ENTRY_ARGUMENT fault 消失；
gamelist.ext init_ok；
R9_SWITCH_OK for gamelist.ext。
```

### 高级成功

出现任意一个：

```text
[JJFB_GAMELIST_CFG36_BUILD]
[JJFB_GAMELIST_POST_UPDATE] result=no_update
[JJFB_SHELL_EXPORT_CALL] name=lib.runapp
[JJFB_RUNAPP] source=native_shell
gwy/jjfb.mrp natural open
```

### 可视化前兆

```text
gwy/jjfb.mrp natural open 后；
mrc_loader.ext / robotol.ext / mrc_init 继续出现；
jjfb 原始资源自然请求。
```

---

## 9. 快速判定表

| 结果 | 解释 | 下一步 |
|---|---|---|
| reg_primary 仍失败 | member_view 解析 bug | 修 reg.ext parser / primary fallback |
| gamelist member_view ok 但 0x8CC00 仍 fault | dsm cfunction helper ABI bug | 修 helper route / LR proxy |
| 0x8CC00 消失但 cfg36 仍无 | gamelist branch/event/update 未推进 | 追 post_update/no_update |
| cfg36 出现 | shell 主流程过半 | 追 export/runapp |
| runapp 出现 | GWY native launch 重大突破 | 观察 jjfb natural start |

---

## 10. 给 Cursor 的一句话

**Phase 6P 已达 `GAMELIST_STARTED_AFTER_CONTINUE`，但还没有 cfg36/post_update/export/runapp，且 `SLOT_CALL=0`，所以不要铺 slot matrix。新 blocker 是 `dsm:cfunction.ext` 的 `ENTRY_ARGUMENT` fault：`module_id=5 fault_pc=0x8CC00 invalid_address=0xE58F8FE0 entry_class=WRONG_HELPER_CALL_MISSING_P`。同时 gamelist fileopen 仍走 canonical 原包，stderr 有 `reg_primary failed package=gwy/gamelist.mrp`，但静态包内确有 `reg.ext` 和 `gamelist.ext`。Phase 6Q 请先修 gamelist member_view/primary=gamelist.ext，再给 gamelist.ext 启用 extChunk+ER_RW/R9，最后审计 dsm:cfunction.ext helper-entry ABI / LR_PROXY 参数流，目标是清掉 0x8CC00 并推进到 cfg36/post_update/native runapp。**
