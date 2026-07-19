# Cursor 下一阶段说明：Stage E — PRODUCT_DESCRIPTOR_DIRECT → robotol.ext ENTRY_CALLED + mrc_init

> D6 已证明产品轨可以从 cfg36 直接启动原始 `gwy/jjfb.mrp`，source 标记为 `descriptor_launcher`，不再依赖 gbrwcore/gamelist。  
> 现在不要回到 gamelist/native shell，不要补 UI，不要追 slot matrix。  
> 下一步的最小目标非常明确：**在产品轨上把 `mrc_loader.ext → robotol.ext` 这一段打通到真实 `robotol.ext ENTRY_CALLED`，再推进到 `[JJFB_MRC_INIT]`。**

---

## 0. D6 已证明的事

D6 产品轨已经通过：

```text
source=descriptor_launcher
cfg36 → DSM target gwy/jjfb.mrp
no gbrwcore
no gamelist
no host_runapp_equiv
mrc_loader.ext EXTRACTED
mrc_loader.ext ENTRY_CALLED
cfunction.ext → robotol.ext via reg_primary
jjfb sha256 unchanged
audit ok
```

这意味着：

```text
启动目标选择问题已经解决；
产品轨已经可以诚实进入原始 jjfb.mrp；
下一步不需要再修 gamelist/cfg36/native shell。
```

---

## 1. 当前未完成

仍未证明：

```text
robotol.ext MODULE ENTRY_CALLED
[JJFB_MRC_INIT]
natural DRAW / REFRESH
```

尤其注意：

```text
BOOTSTRAP_SEQ event=ROBOTOL_ENTER
HELPER_ABI stage=ROBOTOL_ENTER
```

这些日志当前会套在 `mrc_loader` / DSM helper 上，不能当作 `robotol.ext` 真实进入。

所以后续必须用更严格的模块身份判定：

```text
module_name == robotol.ext
package == gwy/jjfb.mrp
module_base belongs to extracted robotol.ext
entry_pc belongs to robotol.ext image
state == ENTRY_CALLED
```

---

## 2. Stage E 唯一目标

```text
保持 source=descriptor_launcher，
清空 shell/gamelist 相关环境，
在产品轨上打通：

gwy/jjfb.mrp
→ start.mr
→ mrc_loader.ext ENTRY_CALLED
→ package-scoped cfunction.ext resolves robotol.ext
→ robotol.ext MODULE ENTRY_CALLED
→ guest mrc_init
```

成功日志最低要求：

```text
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=robotol.ext state=ENTRY_CALLED
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext pc=...
```

中级要求：

```text
[JJFB_MRC_INIT] module=robotol.ext ret=...
```

高级要求：

```text
[JJFB_RESOURCE_REQUEST] package=gwy/jjfb.mrp ...
[JJFB_DRAW] natural=1
[JJFB_REFRESH] natural=1
```

---

## 3. 禁止回退方向

Stage E 不要做：

```text
gbrwcore/gamelist/native shell
cfg36/post_update/runapp
slot API matrix
UI force
AC8/progress/ui_mode
host overlay
host_runapp_equiv
fake robotol entry
fake mrc_init
```

D6 已经证明产品轨可以直接进 jjfb。  
现在的路线应该更短：

```text
descriptor_launcher → jjfb → mrc_loader → robotol → mrc_init
```

---

## 4. 环境要求

新增/使用：

```powershell
.\RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1 -Seconds 60
```

推荐环境：

```powershell
$env:JJFB_PRODUCT_DESCRIPTOR_DIRECT="1"
$env:JJFB_LAUNCH_SOURCE="descriptor_launcher"
$env:JJFB_PRIMARY_TARGET="gwy/jjfb.mrp"

# 明确禁用 shell 轨
$env:JJFB_GWY_LAUNCHER_MODE="0"
$env:JJFB_LAUNCH_PATH="descriptor_direct"
$env:JJFB_SHELL_CHAIN_MODE=""
$env:JJFB_GWY_UPDATE_STUB=""
$env:JJFB_RUNAPP_NATIVE_ONLY="0"

# 游戏包 scoped loader
$env:JJFB_PACKAGE_SCOPED_CLOAD="1"
$env:JJFB_MEMBER_VIEW_PRIMARY="game_package"
$env:JJFB_EXTCHUNK_PROVIDER="game_package"
$env:JJFB_ER_RW_BIND_RESTORE="game_package"

# 观测
$env:JJFB_MODULE_REGISTRY_TRACE="1"
$env:JJFB_ROBOTOL_ENTRY_TRACE="1"
$env:JJFB_MRC_INIT_TRACE="1"
$env:JJFB_EXTCHUNK_SLOT_TRACE="1"

# 不改游戏
$env:JJFB_GAME_SELF_PATCH="0"
```

注意：

```text
shell 相关 env 必须清干净，否则会污染产品轨判断。
```

---

## 5. 必须先修日志身份污染

D6 里 `ROBOTOL_ENTER` 可能套在 mrc_loader/DSM 上。  
Stage E 第一项是把日志改成强身份判定。

新增报告：

```text
reports/stage_e_robotol_identity_audit.md
```

新增日志：

```text
[JJFB_MODULE_IDENTITY]
package=gwy/jjfb.mrp
module=robotol.ext
module_id=...
image_base=...
image_size=...
entry_pc=...
entry_in_range=yes/no
sha256=...
source=descriptor_launcher
```

`ROBOTOL_ENTER` 只有满足以下全部条件才允许打印：

```text
package == gwy/jjfb.mrp
module == robotol.ext
entry_pc in robotol.ext mapped range
state transition: REGISTERED/EXTRACTED → ENTRY_CALLED
```

否则打印：

```text
[JJFB_ROBOTOL_ENTER_REJECT]
reason=not_robotol_module / wrong_package / pc_out_of_range / loader_context / dsm_context
module=...
pc=...
```

---

## 6. Stage E-A：重建 jjfb package-scoped cfunction / reg_primary

D6 已证明：

```text
cfunction.ext → robotol.ext via reg_primary
```

但还没有证明 `robotol.ext ENTRY_CALLED`。  
所以要检查：

```text
1. start.mr 请求 cfunction.ext 时，当前 package 是否是 gwy/jjfb.mrp；
2. cfunction.ext alias 是否落到 jjfb 包内的 reg_primary；
3. reg.ext primary 是否明确指向 robotol.ext；
4. robotol.ext 是否被 extract/map/register；
5. robotol.ext 是否获得自己的 P / extChunk / ER_RW / R9；
6. 是否真正执行 robotol.ext entry，而不是只执行 mrc_loader helper。
```

必需日志：

```text
[JJFB_CLOAD_SCOPE]
request=cfunction.ext
package=gwy/jjfb.mrp
resolved=robotol.ext
reason=package_reg_primary

[MRP_MEMBER_VIEW]
package=gwy/jjfb.mrp
primary=robotol.ext
status=installed

[MODULE_REGISTRY]
package=gwy/jjfb.mrp
module=robotol.ext
state=EXTRACTED/MAPPED/REGISTERED/ENTRY_CALLED
```

---

## 7. Stage E-B：给 robotol.ext 启用通用 module context provider

前面 gbrwcore 已经证明：

```text
P+0x0C / mrc_extChunk publication
ER_RW_BIND
R9_SWITCH_OK
```

这次同样要给 `robotol.ext` 做，不能只给 mrc_loader 做。

必需日志：

```text
[JJFB_EXTCHUNK_PUBLISH]
package=gwy/jjfb.mrp
module=robotol.ext
P=...
old=0x0
new=...

[JJFB_ER_RW_BIND]
package=gwy/jjfb.mrp
module=robotol.ext
p_base=...
p_len=...
registry_base=...
registry_len=...

[JJFB_R9_SWITCH_OK]
package=gwy/jjfb.mrp
module=robotol.ext
r9=...
```

如果出现：

```text
R9_SWITCH_BLOCKED reason=CALLEE_ER_RW_NOT_AVAILABLE module=robotol.ext
```

按前面 6O 的方式修：

```text
P+0/+4 → registry ER_RW
extChunk var_buf/var_len 同步
```

但必须是 generic game_package provider，不要写死 robotol。

---

## 8. Stage E-C：mrc_loader → robotol entry handoff

D6 已经有 mrc_loader ENTRY_CALLED。现在重点是它如何把控制权交给 robotol。

必须输出：

```text
reports/stage_e_mrc_loader_to_robotol_handoff.md
```

要记录：

```text
mrc_loader.ext image_base/entry_pc
mrc_loader helper calls
_mr_c_load("cfunction.ext") / _strCom 601/800/801
reg_primary result
robotol.ext extract/map/register
robotol entry candidate
robotol actual entry pc
entry call args r0-r3/r9/sp/lr
return reason
first fault if any
```

关键判定：

```text
mrc_loader ENTRY_CALLED ≠ robotol ENTRY_CALLED
cfunction.ext reg_primary resolved ≠ robotol executed
robotol mapped/register ≠ robotol entry called
```

要强制区分这四个 gate。

---

## 9. Stage E-D：mrc_init trace

如果 robotol ENTRY_CALLED 后仍没有 `[JJFB_MRC_INIT]`，需要区分：

```text
1. robotol entry 早退；
2. robotol entry 调了 mrc_init 但日志没抓；
3. mrc_init 是通过 function pointer / extChunk slot / mr_table 调；
4. mrc_init 名称不显式存在，但等价 init routine 已执行；
5. 卡在缺平台 API / slot / file open / timer。
```

新增报告：

```text
reports/stage_e_mrc_init_route.md
```

必需日志：

```text
[JJFB_MRC_INIT_ATTEMPT]
module=robotol.ext
pc=...
args=...
route=direct/funcptr/mr_table/extchunk/unknown

[JJFB_MRC_INIT]
module=robotol.ext
ret=...
```

如果没有 mrc_init，打印：

```text
[JJFB_MRC_INIT_ABSENT]
reason=entry_not_called / entry_returned / platform_fault / file_missing / api_missing / timeout
last_pc=...
last_module=...
```

---

## 10. Stage E-E：wxjwq positive control

必要时直接跑：

```powershell
.\RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1 -Target gwy/wxjwq.mrp -Seconds 60
```

WXJWQ 目的不是替代 jjfb，而是验证 mrc_loader-class 链。

文件层已确认：

```text
jjfb.mrp start.mr == wxjwq.mrp start.mr
jjfb.mrp mrc_loader.ext == wxjwq.mrp mrc_loader.ext
```

预期：

```text
gwy/wxjwq.mrp
→ start.mr
→ same mrc_loader.ext
→ cfunction.ext / reg_primary
→ mmochat.ext
→ mrc_init/equivalent
```

如果 wxjwq 成功而 jjfb 不成功：

```text
问题在 robotol.ext 特有逻辑。
```

如果 wxjwq 也卡在同一位置：

```text
问题在 mrc_loader-class 平台 provider / cfunction alias / module context。
```

必需报告：

```text
reports/stage_e_wxjwq_control.md
```

---

## 11. Stage E 成功标准

### E1 最低成功

```text
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=robotol.ext state=ENTRY_CALLED
```

### E2 中级成功

```text
[JJFB_MRC_INIT] module=robotol.ext ret=...
```

### E3 高级成功

```text
robotol.ext 后续自然请求 jjfb 资源；
出现自然 draw / refresh。
```

### E4 对照成功

```text
gwy/wxjwq.mrp 同一 runner 可进入 mmochat.ext ENTRY_CALLED / mrc_init equivalent。
```

---

## 12. 如果 Stage E 仍卡住，按这个分叉处理

| 现象 | 判断 | 下一步 |
|---|---|---|
| robotol.ext 没有 EXTRACTED | member_view / c_load scope 错 | 修 game_package scoped c_load |
| robotol.ext EXTRACTED 但无 MAPPED | MRP member extraction/map bug | 修 ext map |
| robotol.ext MAPPED 但无 REGISTERED | module registry/primary bug | 修 register |
| robotol.ext REGISTERED 但无 ENTRY_CALLED | entry selection / handoff bug | 修 entry invoke |
| robotol ENTRY_CALLED 但 R9 blocked | ER_RW publication 缺 | 套用 6O generic provider |
| robotol ENTRY_CALLED 后 fault | 新平台 API/slot 缺 | 只做 slot-specific |
| robotol ENTRY_CALLED 后正常 return 无 mrc_init | init route/entry args 错 | trace mrc_init route |
| wxjwq 成功 jjfb 失败 | robotol 特有逻辑 | 对 robotol 做专门 API/file audit |
| wxjwq 同样失败 | mrc_loader/common platform bug | 修 mrc_loader-class provider |

---

## 13. 下一阶段报告包

输出：

```text
logs/stage_e_product_robotol_mrcinit_stdout.txt
logs/stage_e_wxjwq_control_stdout.txt

reports/stage_e_robotol_identity_audit.md
reports/stage_e_game_package_cfunction_scope.md
reports/stage_e_mrc_loader_to_robotol_handoff.md
reports/stage_e_robotol_context_provider.md
reports/stage_e_mrc_init_route.md
reports/stage_e_wxjwq_control.md
reports/stage_e_next_fault_classification.md
reports/stage_e_verdict.md

packages/JJFB_stage_e_product_robotol_mrcinit_pack_*.zip
```

---

## 14. 给 Cursor 的一句话

**D6 已证明产品轨 `source=descriptor_launcher` 可以直接从 cfg36 启动原始 `gwy/jjfb.mrp`，并到达 `mrc_loader.ext ENTRY_CALLED`，且 `cfunction.ext → robotol.ext` 的 reg_primary 已成立；但 `ROBOTOL_ENTER` 日志目前会套在 mrc_loader/DSM 上，不能当作 robotol 真进入。Stage E 请保持产品轨、清空 shell/gamelist env，先修日志身份判定，然后打通 game-package-scoped c_load：`gwy/jjfb.mrp → mrc_loader.ext → cfunction.ext resolves robotol.ext → robotol.ext EXTRACTED/MAPPED/REGISTERED/ENTRY_CALLED → mrc_init`。如果 robotol 卡住，同一 runner 跑 `gwy/wxjwq.mrp` 做 positive control；若 wxjwq 也卡，修 mrc_loader-class 平台 provider，若 wxjwq 成功，则专查 robotol 特有逻辑。禁止回退 gamelist/native shell、禁止 fake robotol entry、禁止 UI force。**
