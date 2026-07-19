# Cursor 下一阶段说明：Phase 6P — Shell Core Continue After Init（快进恢复 gamelist/runapp）

> Phase 6O 已经完成关键中段：`P+0x0C` 已发布，gbrwcore `ER_RW` 已绑定，`R9_SWITCH_OK` 已出现。  
> 现在没有 `NULL+0x28` fault，也没有 terminal `CALLEE_ER_RW_NOT_AVAILABLE`。  
> 当前停在 `mythroad exit`，且 `SLOT_CALL=0`、`_strCom=0`、`gamelist=0`。  
> 这说明现在不该补 slot matrix，而应从 `gbrwcore_only` 调试模式推进到 **shell core 继续执行**：让 gbrwcore 初始化成功后继续到 gamelist/gbrwshell/runapp。

---

## 0. Phase 6O 结论

已确认：

```text
verdict = MID_SUCCESS
class = ER_RW_BOUND_R9_SWITCH_OK
P+0x0C publication = restored in Phase 6N
gbrwcore registry ER_RW = nonzero
R9_SWITCH_OK = yes
post-bind CALLEE_ER_RW_NOT_AVAILABLE = false
SLOT_CALL = 0
_strCom 601/800/801 = 0
gamelist/runapp = 0
stop = mr_exit
```

关键日志：

```text
[JJFB_ER_RW_BIND] module=gbrwcore.ext module_id=3 P=0x2AC8DC p_base=0x2B0D18 p_len=0x19A8 registry_base=0x2B0D18 registry_len=0x19A8 reason=platform_er_rw_publication_restore
[JJFB_R9_SWITCH_OK] module=gbrwcore.ext module_id=3 r9=0x2B0D18 er_rw_len=0x19A8
[JJFB_6O_SUMMARY] mode=gbrwcore_only bound=yes ... stop=mr_exit
```

这代表：

```text
gbrwcore native init 已经不再因 extChunk / ER_RW/R9 死掉。
```

---

## 1. 现在不要做什么

暂时不要：

```text
全量 slot API matrix
补 +0x28 slot 业务语义
继续 fake/猜 extChunk
回到 UI/图形
继续只跑 gbrwcore_only
继续扩大所有游戏
```

原因：

```text
SLOT_CALL count = 0；
slot 还没被调用。
当前 gbrwcore_only 成功后直接 mr_exit，很可能是 harness / chain 没继续到 shell core。
```

---

## 2. Phase 6P 唯一目标

```text
从 gbrwcore_only 推进到 shell_core_continue：
gbrwcore init 成功后，不要停在 mythroad exit；
继续加载/执行 gamelist/gbrwshell，并进入 native post-update → cfg36 → runapp/startGame 路径。
```

也就是把当前流程从：

```text
gbrwcore init OK
→ mr_exit
```

推进为：

```text
gbrwcore init OK
→ gamelist/gbrwshell native start
→ post-update no_update branch
→ cfg36 param build
→ lib.startGame/lib.runapp export/dispatcher
→ gwy/jjfb.mrp natural start
```

---

## 3. 关键判断

6O 的 `stop=mr_exit` 不应立刻被当成 guest fatal error。  
需要先判别：

```text
1. 是 gbrwcore entry 正常 return 后，host harness 结束？
2. 是 shell shim 的 gbrwcore_only 模式主动停止？
3. 是 LR 返回到了 mr_exit thunk？
4. 是没有启动 gamelist/gbrwshell 后续包？
5. 是 shell no_update branch 未继续？
```

输出：

```text
reports/phase6p_mr_exit_source.md
```

日志：

```text
[JJFB_6P_EXIT_SOURCE] source=harness_stop/gbrwcore_only/mr_exit_api/lr_return/guest_call pc=... lr=... mode=...
```

如果 source 是 `gbrwcore_only` 或 `harness_stop`：

```text
直接切换到 shell_core_continue，不要继续修 guest。
```

---

## 4. 运行模式升级

新增/使用：

```powershell
$env:JJFB_EXTCHUNK_PROVIDER="shell_core"
$env:JJFB_ER_RW_BIND_RESTORE="shell_core"
$env:JJFB_SHELL_CHAIN_MODE="continue_after_gbrwcore_init"
```

或等价模式：

```text
JJFB_LAUNCH_PATH=gwy_shell_core_continue
```

语义：

```text
1. gbrwcore.ext 仍走合法 extChunk + ER_RW/R9；
2. gbrwcore init 返回后，不退出；
3. 继续执行/启动 gamelist.ext / gbrwshell.ext；
4. 继续追 cfg36 参数构建和 native runapp。
```

---

## 5. shell_core 扩展范围

现在可以从 `gbrwcore_only` 扩展到：

```text
gbrwcore.ext
gbrwshell.ext
gamelist.ext
```

但仍不要扩到所有游戏模块。

对每个 shell module 都启用：

```text
extChunk provider
P+0x0C publication
ER_RW bind restore
R9_SWITCH_OK trace
entry/continuation trace
```

日志：

```text
[JJFB_SHELL_CORE_MODULE] module=gbrwcore.ext stage=init_ok
[JJFB_SHELL_CORE_MODULE] module=gbrwshell.ext stage=init_ok
[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=init_ok
[JJFB_EXTCHUNK_PUBLISH] module=gamelist.ext ...
[JJFB_ER_RW_BIND] module=gamelist.ext ...
[JJFB_R9_SWITCH_OK] module=gamelist.ext ...
```

---

## 6. gamelist / cfg36 快速目标

Phase 6P 要尽快打到以下任一日志：

```text
[JJFB_GAMELIST_STARTED]
[JJFB_GAMELIST_CFG36_BUILD]
[JJFB_GAMELIST_POST_UPDATE] result=no_update
[JJFB_STARTGAME] source=gamelist/gbrwcore
[JJFB_RUNAPP] source=gamelist/gbrwcore
```

cfg36 参数仍然是：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

重点是：

```text
不能再是 host_runapp_equivalent；
必须是 shell native branch / dispatcher / export call。
```

---

## 7. export/dispatcher 重新启用

Phase 6H 之前卡在 P+0xC；现在这个 blocker 已过。  
Phase 6P 可以重新追：

```text
lib.startGame
lib.runapp
gamelist post-update branch
```

但要保留判别：

```text
string_va_not_entry 仍不能当函数入口；
必须找 dispatcher/export call。
```

日志：

```text
[JJFB_SHELL_EXPORT_RESOLVE] module=gbrwcore.ext name=lib.runapp string_va=... dispatcher=... status=...
[JJFB_SHELL_EXPORT_CALL] name=lib.runapp pc=... args=...
```

---

## 8. slot API 触发条件

仍然只在出现真实 slot call 后才进入 slot API：

```text
[JJFB_EXTCHUNK_SLOT_CALL] off=...
```

若 6P 中出现 `+0x24` 或 `+0x28` slot call：

```text
1. 先记录 args/ret；
2. observe stub 可返回保守 status；
3. 不要假装业务成功；
4. 若卡在某个 slot，再开 Phase 6Q slot-specific audit。
```

---

## 9. 资源路径同步检查

shell core 继续执行后，必须继续记录 file open：

```text
gwy/gbrwcore.mrp
gwy/gbrwshell.mrp
gwy/gamelist.mrp
gwy/jjfb.mrp
gwy/gifs/...
gwy/jjfbol/...
gwy/save/...
```

特别注意：

```text
不要回到 jjfb_alias.mrp
```

---

## 10. 成功标准

### 最低成功

```text
不再停在 gbrwcore_only 的 mr_exit；
gbrwshell 或 gamelist 至少一个 shell module native init/entry 被观察到。
```

### 中级成功

出现任一：

```text
[JJFB_GAMELIST_STARTED]
[JJFB_GAMELIST_CFG36_BUILD]
[JJFB_GAMELIST_POST_UPDATE]
```

### 高级成功

出现任一：

```text
[JJFB_SHELL_EXPORT_CALL] name=lib.runapp/lib.startGame
[JJFB_RUNAPP] source=native_shell
gwy/jjfb.mrp natural open after shell branch
_strCom 601/800/801
mrc_loader.ext loaded
```

### 可视化前兆

```text
jjfb 原始资源链自然请求；
进入游戏自身启动/检查更新/登录前流程。
```

---

## 11. 新脚本

```powershell
.\RUN_PHASE6P_SHELL_CORE_CONTINUE.ps1
```

环境：

```powershell
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_shell_core_continue"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_EXTCHUNK_PROVIDER="shell_core"
$env:JJFB_ER_RW_BIND_RESTORE="shell_core"
$env:JJFB_SHELL_CHAIN_MODE="continue_after_gbrwcore_init"
$env:JJFB_GWY_UPDATE_STUB="no_update"
$env:JJFB_EXTCHUNK_SLOT_TRACE="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 12. 产物

```text
logs/phase6p_shell_core_continue_stdout.txt
logs/phase6p_shell_core_continue_report.txt

reports/phase6p_mr_exit_source.md
reports/phase6p_shell_core_module_init.md
reports/phase6p_gamelist_cfg36_branch.md
reports/phase6p_export_dispatcher_result.md
reports/phase6p_fileopen_after_shell_continue.md
reports/phase6p_slot_trigger_result.md
reports/phase6p_next_fault_classification.md
reports/phase6p_verdict.md

packages/JJFB_phase6p_shell_core_continue_pack_*.zip
```

---

## 13. 快速判定表

| 结果 | 解释 | 下一步 |
|---|---|---|
| 仍 stop=mr_exit 且 exit_source=harness/gbrwcore_only | harness 未继续 | 修 shell chain mode |
| gamelist/gbrwshell init fault | 对该 module 启用 extChunk/ER_RW bind | 继续 shell_core |
| cfg36 build 出现 | shell branch 过半 | 追 runapp/export |
| native runapp 出现 | GWY 启动链恢复 | 观察 jjfb 自然加载 |
| slot call 出现 | 进入具体平台 API | Phase 6Q slot audit |
| jjfb.mrp 自然 open | 重大突破 | 转 loader/mrc_init |

---

## 14. 给 Cursor 的一句话

**Phase 6O 已达 `ER_RW_BOUND_R9_SWITCH_OK`：`P+0x0C` 已恢复，gbrwcore ER_RW 已绑定，`R9_SWITCH_OK` 出现，`NULL+0x28` 和 terminal `CALLEE_ER_RW_NOT_AVAILABLE` 已消失；但 `SLOT_CALL=0`、`_strCom=0`、`gamelist=0`，当前停在 `gbrwcore_only stop=mr_exit`。Phase 6P 不要补全 slot matrix；请把模式从 `gbrwcore_only` 升级为 `shell_core_continue`：gbrwcore init 成功后继续执行 gbrwshell/gamelist，给 shell modules 同样启用 extChunk provider + ER_RW/R9 bind，追 `gamelist cfg36 build → post-update no_update → native lib.startGame/lib.runapp`。成功标准是至少出现 gamelist/gbrwshell native init、cfg36 branch、native runapp 或 jjfb.mrp natural open。**
