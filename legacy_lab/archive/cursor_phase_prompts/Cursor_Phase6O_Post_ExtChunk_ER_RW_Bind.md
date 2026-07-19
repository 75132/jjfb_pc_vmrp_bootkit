# Cursor 下一阶段说明：Phase 6O — Post-ExtChunk Continuation / ER_RW Metadata Restore（快进，不做全 slot matrix）

> Phase 6N 已经成功恢复 `P+0x0C / mrc_extChunk` publication。  
> `0x30CCF8 / NULL+0x28` 已消失，说明 `P+0x0C` 这一关过了。  
> 但当前还没有进入 `_strCom 601/800/801`、`gamelist/runapp` 或 slot 调用；日志里仍出现 `R9_SWITCH_BLOCKED reason=CALLEE_ER_RW_NOT_AVAILABLE`，随后 `mythroad exit`。  
> 因此下一步不要立刻展开完整 slot API matrix；先做 **post-extChunk 后续执行 / callee ER_RW metadata 恢复**。

---

## 0. Phase 6N 结论

已确认：

```text
class = EXTCHUNK_PUBLICATION_RESTORED
P = 0x2AC8DC
P+0x0C old=0 new=0x682A5C
reason = mr_c_function_new_contract / platform_publication_restore
0x30CCF8 NULL+0x28 = gone
fault_addr=0x28 = gone
jjfb hash unchanged
wxjwq cross-target covered
```

已填充 extChunk：

```text
mrc_extChunk @ 0x682A5C
+0x04 init_func    = 0x2EB7E8
+0x08 event_helper = 0x30CFE9
+0x28 sendAppEvent = 0x280058
```

当前未发生：

```text
[JJFB_EXTCHUNK_SLOT_CALL] off=0x28
_strCom 601/800/801
mrc_init
gamelist/runapp
```

---

## 1. 当前最可能的新 blocker

6N stdout 里仍有：

```text
[R9_SWITCH_BLOCKED] reason=CALLEE_ER_RW_NOT_AVAILABLE module=gbrwcore.ext call_kind=BOOTSTRAP_ENTRY
[R9_SWITCH_BLOCKED_OUTCOME] entry_executed=yes effective_r9=0x280400 caller_r9=0x280400 callee_er_rw=0x0
```

这说明：

```text
extChunk 发布已经恢复；
但 gbrwcore.ext 的 callee ER_RW metadata 仍没有发布/绑定；
entry 虽然执行，但仍用 caller R9 / DSM-like context，
很可能导致 entry 后早退或 mythroad exit。
```

所以 6O 不应先铺完整 slot matrix，而应先查：

```text
为什么 extChunk 已有，但 callee ER_RW 仍为 0？
```

---

## 2. Phase 6O 唯一目标

```text
恢复/验证 gbrwcore.ext 的 callee ER_RW metadata 与 R9 module binding，
让 entry 后能继续进入 shell native 后续流程，而不是 mythroad exit。
```

目标是推进到：

```text
gbrwcore entry after extChunk
→ correct ER_RW/R9
→ no immediate mythroad exit
→ shell continuation
→ possible slot call / _strCom / gamelist/runapp
```

---

## 3. 不要做的事

暂时不要：

```text
全量 slot API matrix
UI/图形
gamelist/runapp 深追
继续 fake/猜 extChunk slot
扩大所有游戏样本
```

原因：

```text
+0x28 SLOT_CALL count = 0；
slot 还没被调用，说明还没进入需要 slot matrix 的阶段。
```

仍禁止：

```text
fake P+0x0C
R9 promotion 无证据强推
skip mythroad exit
force UI
AC8/progress
改 jjfb/robotol 游戏逻辑
```

允许：

```text
恢复 callee ER_RW metadata；
修 module registry 的 ER_RW binding；
让 R9 切换使用已记录的 callee ER_RW；
补 documented cfunction load ER_RW publication；
观察后续新 fault。
```

---

## 4. 关键问题清单

Phase 6O 必须回答：

```text
1. extChunk provider 是否已经知道 gbrwcore.ext 的 ER_RW base/len？
2. module registry 里 module_id=3 是否有 ER_RW base/len？
3. 为什么 R9_SWITCH 时 callee_er_rw=0？
4. gbrwcore.ext 的 P+0/+4/+8 写入值与 registry 里的 ER_RW 是否一致？
5. `_mr_c_function_new` / `mr_c_function_load` 是否本应把 P+0/+4 写入 module registry？
6. 当前是不是只把 extChunk 发布到 P+0x0C，却没有把 P+0/+4 反向同步到 module registry？
7. entry 返回 `mythroad exit` 是不是因为 R9 仍是 caller/DSM context？
```

---

## 5. 6O-A：ER_RW 时间线重建

输出：

```text
reports/phase6o_er_rw_timeline.md
```

必须记录：

```text
module_id
module_name
P addr
P+0x00 start_of_ER_RW
P+0x04 ER_RW_Length
registry_er_rw_base
registry_er_rw_len
extChunk+0x14 var_buf
extChunk+0x18 var_len
R9 before entry
R9 during entry
R9 after entry
```

日志：

```text
[JJFB_ER_RW_BIND] module=gbrwcore.ext module_id=3 P=0x2AC8DC p_base=... p_len=... registry_base=... registry_len=...
[JJFB_R9_SWITCH_ATTEMPT] module=gbrwcore.ext callee_er_rw=... caller_r9=...
[JJFB_R9_SWITCH_OK] module=gbrwcore.ext r9=...
[JJFB_R9_SWITCH_BLOCKED] ...
```

---

## 6. 6O-B：把 P+0/+4 合法同步到 module registry

如果观察到：

```text
P+0x00 非零
P+0x04 非零
module registry ER_RW = 0
```

则允许恢复：

```text
registry[module_id].er_rw_base = P+0x00
registry[module_id].er_rw_len  = P+0x04
extChunk+0x14 = P+0x00
extChunk+0x18 = P+0x04
```

触发点：

```text
after P metadata write
after extChunk publication restore
before MRPGCMAP entry / callback continuation
```

日志 reason 必须是：

```text
reason=mr_c_function_st_metadata_bind
reason=platform_er_rw_publication_restore
```

禁止：

```text
reason=fake_r9
reason=force_r9
reason=guess
```

---

## 7. 6O-C：entry with callee R9

在 registry ER_RW 非零后，允许：

```text
entry / continuation 使用 callee ER_RW R9
```

验收：

```text
[R9_SWITCH_OK] module=gbrwcore.ext r9=<callee_er_rw>
```

然后观察：

```text
是否不再 mythroad exit；
是否出现 SLOT_CALL；
是否出现 _strCom；
是否进入 gamelist/runapp；
是否出现新 fault。
```

如果新 fault 出现，先分类，不要马上修。

---

## 8. 6O-D：slot matrix 触发条件

只有当出现：

```text
[JJFB_EXTCHUNK_SLOT_CALL] off=...
```

才进入 slot matrix。

如果仍然没有 slot call：

```text
继续 ER_RW / continuation / event flow，不做 API matrix。
```

如果 +0x28 被调用：

```text
记录 args/ret；
先 observe stub；
不要伪造业务成功；
下一阶段才补 slot API。
```

---

## 9. 成功标准

### 最低成功

```text
module registry 中 gbrwcore.ext 的 ER_RW base/len 不再为 0；
R9_SWITCH_BLOCKED reason=CALLEE_ER_RW_NOT_AVAILABLE 消失。
```

### 中级成功

```text
entry / callback continuation 使用 callee ER_RW；
不再立即 mythroad exit；
继续推进到后续 shell native path。
```

### 高级成功

出现任意一个：

```text
[JJFB_EXTCHUNK_SLOT_CALL]
[JJFB_STRCOM] code=601/800/801
gamelist started
lib.runapp/export_called
mrc_loader.ext loaded
```

### 可视化前兆

```text
jjfb/gwy 原始资源请求继续增加；
出现游戏自身 check/update/login 前资源链。
```

---

## 10. 脚本

新增：

```powershell
.\RUN_PHASE6O_POST_EXTCHUNK_ER_RW_BIND.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_guest_native_runapp"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER="gbrwcore_only"
$env:JJFB_EXTCHUNK_PROVIDER="gbrwcore_only"
$env:JJFB_ER_RW_BIND_RESTORE="gbrwcore_only"
$env:JJFB_EXTCHUNK_SLOT_TRACE="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 11. 产物

```text
logs/phase6o_post_extchunk_er_rw_stdout.txt
logs/phase6o_post_extchunk_er_rw_report.txt

reports/phase6o_er_rw_timeline.md
reports/phase6o_registry_bind_result.md
reports/phase6o_r9_switch_result.md
reports/phase6o_post_entry_continuation.md
reports/phase6o_slot_trigger_result.md
reports/phase6o_next_fault_classification.md
reports/phase6o_verdict.md

packages/JJFB_phase6o_post_extchunk_er_rw_bind_pack_*.zip
```

---

## 12. 快速判定表

| 结果 | 解释 | 下一步 |
|---|---|---|
| registry ER_RW 仍 0 | 没绑定到正确 module/P | 修 registry key / module_id |
| R9_SWITCH_OK 但仍 mythroad exit | entry 后续条件/return path 缺 | trace continuation / event |
| 出现 SLOT_CALL +0x28 | extChunk 生效，进入 slot API | Phase 6P slot-specific |
| 出现 _strCom | 重大突破 | 继续 loader/mrc_init |
| gamelist/runapp 出现 | shell 链恢复 | 回到 6H mid/high ladder |

---

## 13. 给 Cursor 的一句话

**Phase 6N 已恢复 `P+0x0C/mrc_extChunk` publication，`NULL+0x28` fault 消失；但 `SLOT_CALL +0x28 count=0`，且 stdout 仍显示 `R9_SWITCH_BLOCKED reason=CALLEE_ER_RW_NOT_AVAILABLE module=gbrwcore.ext`，随后 `mythroad exit`。Phase 6O 不要直接铺完整 slot API matrix；先做 post-extChunk ER_RW metadata restore：重建 gbrwcore 的 `P+0/+4`、module registry ER_RW、extChunk `var_buf/var_len`、entry R9 时间线；若 P metadata 非零但 registry 为 0，则按 `mr_c_function_st_metadata_bind` 合法同步 registry 和 extChunk var fields，再让 entry/continuation 使用 callee ER_RW。成功标准是 `R9_SWITCH_OK`、不再立即 exit，并推进到 SLOT_CALL/_strCom/gamelist/runapp 任一新门槛。**
