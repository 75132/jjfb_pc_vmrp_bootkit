# Native GWY/JJFB 真启动长任务总计划与资料包

## 0. 目标边界

这份计划的目标是把项目从当前 Phase 6Q 状态，一次性推进到：

```text
原始 GWY 网游 shell 自然加载目标游戏 jjfb
```

这里的“真启动”不是：

```text
host-side 模拟 runapp
host overlay 假画面
force UI state
修改 jjfb.mrp / robotol.ext 游戏逻辑
```

而是：

```text
gbrwcore/gamelist/gbrwshell 原始 native shell 链
→ cfg36 / no_update / native runapp
→ 原始 gwy/jjfb.mrp
→ start.mr / mrc_loader.ext / robotol.ext
→ mrc_init
→ 原始资源自然请求
→ 自然可视化
```

如 jjfb 特有 robotol 后续卡住，允许同步启动 `gwy/wxjwq.mrp` 作为 mrc_loader-class positive control，因为文件层证明它和 jjfb 的 `start.mr` 与 `mrc_loader.ext` 完全一致。

---

## 1. 当前 Phase 6Q 状态

Phase 6Q 上传包的结论：

```text
verdict = MID_SUCCESS
class = GAMELIST_PRIMARY_HELPER_CLEARED
gamelist member_view = True
0x8CC00 / ENTRY_ARGUMENT cleared or reclassed = True
cfg36 / post_update / export = False
SLOT_CALL = 0
GAMELIST_STARTED = True
```

关键解释：

- `gamelist.mrp` 的 member_view / primary 已经修到能生成；
- 之前 `dsm:cfunction.ext 0x8CC00 ENTRY_ARGUMENT` 已清除或合法化；
- 但还没有进入 `gamelist.ext` 完整 platform context；
- 还没有 `cfg36 build / post_update / native runapp`；
- `SLOT_CALL=0`，因此不要先补 slot matrix。

Phase 6Q 仍显示：

```text
EXTCHUNK gamelist = 0
ER_RW_BIND gamelist = 0
R9_SWITCH_OK gamelist = 0
SHELL_CORE init_ok = 0
```

所以当前真正 blocker 是：

```text
gamelist 已被 shell chain 启动，但 gamelist.ext 还没有像 gbrwcore.ext 一样完成 primary module context：EXT_REGISTER → P+0x0C → ER_RW_BIND → R9_SWITCH_OK → init_ok。
```

---

## 2. 文件层资料总览

### 2.1 核心 MRP 包

| basename      |   size |   entry_count | class              | reg_primary_guess   |   start_len | has_mrc_loader_ext   |   asset_count | ext_members                                                                                                                                                                                                                                                                                                                                                           |
|:--------------|-------:|--------------:|:-------------------|:--------------------|------------:|:---------------------|--------------:|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| gamelist.mrp  |  72620 |            26 | gwy_shell_core     | gamelist.ext        |        2490 | False                |            23 | gamelist.ext|reg.ext                                                                                                                                                                                                                                                                                                                                                  |
| gbrwcore.mrp  | 100292 |             3 | gwy_shell_core     | gbrwcore.ext        |        2490 | False                |             0 | reg.ext|gbrwcore.ext                                                                                                                                                                                                                                                                                                                                                  |
| gbrwshell.mrp |  33152 |             9 | gwy_shell_core     | gbrwshell.ext       |        2240 | False                |             6 | reg.ext|gbrwshell.ext                                                                                                                                                                                                                                                                                                                                                 |
| jjfb.mrp      | 414602 |            50 | mrc_loader_game    | robotol.ext         |        3787 | True                 |            54 | mrc_loader.ext|bigworldmapmodule.ext|mainmenumodule.ext|mailmodule.ext|moduletest.ext|chatmodule.ext|gezimodule.ext|taobaomodule.ext|gameattackmodule.ext|monsterstatemodule.ext|itembagshopmodule.ext|viewmanmodule.ext|betmodule.ext|monstermodule.ext|leitaimodule.ext|reg.ext|lianmengmodule.ext|othermodule.ext|robotol.ext|shopmodule.ext|itemhechengmodule.ext |
| roomlist.mrp  |  13627 |             3 | native_ext_package | roomlist.ext        |        2490 | False                |             0 | reg.ext|roomlist.ext                                                                                                                                                                                                                                                                                                                                                  |
| sanguo.mrp    | 638474 |           275 | native_ext_package | sanguo.ext          |      559903 | False                |           271 | reg.ext|sanguo.ext                                                                                                                                                                                                                                                                                                                                                    |
| spacetime.mrp | 488893 |           422 | native_ext_package | spacetime.ext       |        3435 | False                |           282 | reg.ext|spacetime.ext                                                                                                                                                                                                                                                                                                                                                 |
| tlbb.mrp      | 307238 |            18 | native_ext_package | dream.ext           |        3435 | False                |            11 | reg.ext|dream.ext                                                                                                                                                                                                                                                                                                                                                     |
| vdload.mrp    |  11586 |             3 | native_ext_package | vdload.ext          |        2490 | False                |             0 | reg.ext|vdload.ext                                                                                                                                                                                                                                                                                                                                                    |
| wxjwq.mrp     | 299067 |            21 | mrc_loader_game    | mmochat.ext         |        3787 | True                 |            17 | mrc_loader.ext|reg.ext|mmochat.ext                                                                                                                                                                                                                                                                                                                                    |

### 2.2 cfg.bin 中 jjfb 记录

| cfg                             |   path_off | path         | pre16_hex                        | post48_hex                                                                                                               |   pre_minus8_u32le_a |   pre_minus4_u32le_b |
|:--------------------------------|-----------:|:-------------|:---------------------------------|:-------------------------------------------------------------------------------------------------------------------------|---------------------:|---------------------:|
| 冒泡游戏320480/网游/gwy/cfg.bin |      10943 | gwy/jjfb.mrp | 0000000001e200000000020000000001 | 6777792f6a6a66622e6d7270000000000000000000000000000000000000000000000000000000000305ffffffffffffffffffffffffffffffffffff |               131072 |             16777216 |

### 2.3 mrc_loader 类对照

```text
jjfb.mrp 与 wxjwq.mrp：
- start.mr SHA 完全一致
- mrc_loader.ext SHA 完全一致
```

`mrc_loader_hash_groups.csv`：

| sha256                                                           |   count | members            |
|:-----------------------------------------------------------------|--------:|:-------------------|
| d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938 |       2 | jjfb.mrp|wxjwq.mrp |

### 2.4 shell core 文件证据

- `gbrwcore.mrp`：primary=`gbrwcore.ext`
- `gbrwshell.mrp`：primary=`gbrwshell.ext`
- `gamelist.mrp`：primary=`gamelist.ext`
- `gamelist.ext` 内含 `gwy/cfg.bin`、`gwy/cfg.bin.td`、`napptype...nmrpname...gwyblink` 参数格式；
- `gbrwcore.ext` 内含 `lib.startGame`、`lib.runapp`、`lib.download`、`lib.getClientInfo` 等 shell API 名称；
- `jjfb.mrp` 内含 `mrc_loader.ext`、`robotol.ext` 与多模块资源；
- `wxjwq.mrp` 内含同一 `mrc_loader.ext` 与 `mmochat.ext`。

---

## 3. 总体路线图

### Stage A：修正 package-scoped c_load / member-view 语义

当前 6Q 虽然修了 `gamelist.mrp` member_view，但还没有证明 `gamelist.ext` 完整 platform context。  
最关键的是：当 guest 在 generated `gwy/gamelist.mrp` 内请求 `cfunction.ext` 或 primary c ext 时，不能再落到 global `dsm:cfunction.ext` 的 helper 事件路径；必须在 package scope 内解析到 `gamelist.ext`。

最低目标：

```text
[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=init_ok
[JJFB_EXTCHUNK_PUBLISH] module=gamelist.ext ...
[JJFB_ER_RW_BIND] module=gamelist.ext ...
[JJFB_R9_SWITCH_OK] module=gamelist.ext ...
```

### Stage B：泛化 Phase 6N/6O 的 module context provider

已经对 `gbrwcore.ext` 证明有效的机制：

```text
P+0x0C / mrc_extChunk publication
ER_RW metadata bind
R9_SWITCH_OK
```

必须泛化到：

```text
gbrwcore.ext
gbrwshell.ext
gamelist.ext
mrc_loader.ext
robotol.ext
mmochat.ext
```

但按真实路径懒加载，不要预加载所有游戏模块。

### Stage C：让 gamelist.ext 进入 cfg36/no_update/post_update

文件层证明 gamelist.ext 有：

```text
gwy/cfg.bin
gwy/cfg.bin.td
napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink
gwy/%s.mrp
```

因此，目标是让原始 gamelist native code 自己完成：

```text
read cfg.bin
match cfg36
build param
no_update branch
call startGame/runapp
```

目标日志：

```text
[JJFB_GAMELIST_CFG36_BUILD]
[JJFB_GAMELIST_POST_UPDATE] result=no_update
```

### Stage D：恢复 native export/dispatcher

`lib.startGame` / `lib.runapp` 只可作为导出名/字符串证据，不能直接把 string VA 当函数入口。  
必须找到 native dispatcher/export call。

目标日志：

```text
[JJFB_SHELL_EXPORT_RESOLVE]
[JJFB_SHELL_EXPORT_CALL] name=lib.runapp/lib.startGame
[JJFB_RUNAPP] source=native_shell target=gwy/jjfb.mrp
```

### Stage E：真正打开 jjfb.mrp

只有当 source 是 native shell runapp，才算进入目标。

禁止：

```text
bridge_dsm_mr_start_dsm 直接 host 打开 jjfb.mrp 并当成功
host_runapp_equivalent_after_no_update
```

允许：

```text
native shell runapp 调用底层 mr_start 执行器
```

但日志必须保留 native source chain。

### Stage F：mrc_loader 游戏启动链

JJFB 预期链：

```text
gwy/jjfb.mrp natural open
start.mr executes
_mr_c_load / _strCom 601/800/801
mrc_loader.ext loaded
robotol.ext loaded
mrc_init(0) ret=0
```

WXJWQ positive control：

```text
gwy/wxjwq.mrp natural open
same start.mr + same mrc_loader.ext
mmochat.ext loaded
mrc_init/equivalent reached
```

### Stage G：自然可视化

可视化不是 force 出来的。必须由 game code 自然触发：

```text
resource request
bitmap load
draw/blit/refresh
```

最低自然可视化候选：

```text
loadingbar / bar / textbar / slogo
server select
login/update pre-screen
main menu robot resources
```

---

## 4. 一次性长任务执行方案

给 Cursor 的完整任务文档见：

```text
02_CURSOR_ONE_SHOT_IMPLEMENTATION_SPEC.md
```

最核心原则：

```text
不要再分散小阶段；用一个 RUN_NATIVE_GWY_JJFB_FULL_BOOT.ps1 总脚本跑完整链。
每一段都要有 gate 和 report；但实现上一次性把链路铺完。
```

---

## 5. 总脚本建议

```powershell
.\RUN_NATIVE_GWY_JJFB_FULL_BOOT.ps1
```

建议环境：

```powershell
$env:JJFB_NATIVE_BOOT_FULL="1"
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_native_full_shell"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_MEMBER_VIEW_PRIMARY="all_shell_and_game"
$env:JJFB_PACKAGE_SCOPED_CLOAD="1"
$env:JJFB_EXTCHUNK_PROVIDER="shell_and_game"
$env:JJFB_ER_RW_BIND_RESTORE="shell_and_game"
$env:JJFB_GWY_UPDATE_STUB="no_update_native_branch"
$env:JJFB_RUNAPP_NATIVE_ONLY="1"
$env:JJFB_PRIMARY_TARGET="gwy/jjfb.mrp"
$env:JJFB_CONTROL_TARGET="gwy/wxjwq.mrp"
$env:JJFB_EXTCHUNK_SLOT_TRACE="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

---

## 6. 成功等级

### 6.1 Shell-native 成功

```text
gamelist cfg36 branch
post_update/no_update
native lib.runapp
```

### 6.2 JJFB 真实启动成功

```text
native_shell runapp opens gwy/jjfb.mrp
jjfb start.mr executes
mrc_loader.ext loads
robotol.ext loads
mrc_init appears
```

### 6.3 自然可视化成功

```text
jjfb original resources requested naturally
draw/blit/refresh naturally triggered
visible output from original game path
```

### 6.4 WXJWQ control 成功

```text
native_shell runapp opens gwy/wxjwq.mrp
same mrc_loader path loads mmochat.ext
```

---

## 7. 关键禁止清单

禁止：

```text
修改 jjfb.mrp / robotol.ext / wxjwq.mrp / mmochat.ext
hardcode P+0xC
skip fault
force ui_mode / progress / AC8
host overlay UI
host_runapp_equivalent pretending native
string_va pretending function entry
```

允许：

```text
平台 shim 恢复
GWY shell update no_update 平台返回
package scoped c_load
member_view primary resolve
extChunk/ER_RW/R9 context restore
slot API only after real slot call
```

---

## 8. 后续判断表

| 结果 | 解释 | 处理 |
|---|---|---|
| gamelist member_view ok，但 gamelist.ext 无 init_ok | package scoped c_load/primary handoff 不完整 | 修 c_load scope |
| gamelist.ext init_ok，但 cfg36 无 | gamelist branch/update/event 未推进 | 查 no_update/event/timer |
| cfg36 出现，但 no runapp | export dispatcher 未恢复 | 查 lib.startGame/lib.runapp dispatcher |
| native runapp 出现，但 jjfb 不 open | runapp args/path/VFS 问题 | 查 nmrpname/path/root |
| jjfb open，但 mrc_loader 不进 | cfunction.ext alias/mrc_loader class问题 | 对照 wxjwq |
| robotol/mmochat 进，但无 mrc_init | platform ABI/slot/API 缺 | slot-specific audit |
| mrc_init 后无画面 | resource/draw/refresh/timer | 图形自然链 |

---

## 9. 文件资料索引

本包包含：

```text
01_NATIVE_GWY_JJFB_FULL_BOOT_MASTER_PLAN.md
02_CURSOR_ONE_SHOT_IMPLEMENTATION_SPEC.md
03_FILE_LAYER_DOSSIER.md
04_VALIDATION_CHECKLIST_AND_REPORTS.md
05_FORBIDDEN_SHORTCUTS_AND_SAFETY.md
data/*.csv
data/phase6q_digest.md
phase6q_reports/*
reference/Original_GWY_Full_File_Deep_Analysis.md
reference/Original_GWY_Full_File_Deep_Analysis_Pack.zip
reference/JJFB_phase6q_gamelist_native_primary_abi_pack_20260719_065259.zip
```


---

# Cursor One-Shot Task Prompt：Native GWY/JJFB Full Boot

你现在的任务不是继续做小阶段观察，而是把现有 Phase 6Q 以后的一整条链打通到“原始 GWY 网游启动链自然加载 jjfb”。

## 总目标

在不修改 `gwy/jjfb.mrp`、不伪造游戏 UI、不 host-side 模拟 runapp 的前提下，恢复平台/GWY shell 启动契约，使：

```text
gwy/gbrwcore.mrp
→ gwy/gamelist.mrp / gwy/gbrwshell.mrp
→ cfg36/no_update/post_update native branch
→ native lib.startGame/lib.runapp
→ gwy/jjfb.mrp natural open
→ jjfb start.mr
→ mrc_loader.ext
→ robotol.ext
→ mrc_init
→ jjfb 原始资源自然请求
→ 首屏/检查更新/登录前自然可视化
```

允许用 `gwy/wxjwq.mrp` 做 mrc_loader-class positive control，因为它和 jjfb 的 `start.mr`、`mrc_loader.ext` 完全相同。

## 当前状态

Phase 6Q 已经达成：

```text
GAMELIST_PRIMARY_HELPER_CLEARED
gamelist member_view/generated primary fixed
dsm:cfunction.ext 0x8CC00 ENTRY_ARGUMENT cleared/reclassed
SLOT_CALL = 0
GAMELIST_STARTED = true
CFG36 / POST_UPDATE / EXPORT / RUNAPP = false
gamelist.ext platform context = not yet complete
```

不要补 slot matrix，因为 slot 还没有被调用。下一步先把 gamelist 真正变成 native shell module，而不是只看到 “started”。

## 必须一次性推进的实现链

### A. 修正 shell package-scoped c_load / member view 语义

1. 对每个 generated member_view 包维护 package scope：
   - `gwy/gbrwcore.mrp -> primary=gbrwcore.ext`
   - `gwy/gbrwshell.mrp -> primary=gbrwshell.ext`
   - `gwy/gamelist.mrp -> primary=gamelist.ext`
   - `gwy/jjfb.mrp -> primary=robotol.ext, cfunction.ext alias/mrc_loader.ext path`
   - `gwy/wxjwq.mrp -> primary=mmochat.ext, cfunction.ext alias/mrc_loader.ext path`

2. 当 guest 在某个 package scope 内请求 `_mr_c_load("cfunction.ext")` 或等价 primary：
   - 不能落回 global `dsm:cfunction.ext`；
   - 必须解析到当前 package 的 generated primary/member；
   - 必须出现 `EXT_REGISTER`、`P+0x0C publication`、`ER_RW_BIND`、`R9_SWITCH_OK` for that primary module。

3. 对 gamelist 的最低成功日志：

```text
[JJFB_SHELL_CORE_MODULE] module=gamelist.ext stage=init_ok
[JJFB_EXTCHUNK_PUBLISH] module=gamelist.ext ...
[JJFB_ER_RW_BIND] module=gamelist.ext ...
[JJFB_R9_SWITCH_OK] module=gamelist.ext ...
```

### B. 统一 module context provider

把 Phase 6N/6O 对 gbrwcore 的 extChunk + ER_RW/R9 恢复泛化为 `shell_and_game`：

```text
gbrwcore.ext
gbrwshell.ext
gamelist.ext
mrc_loader.ext
robotol.ext
mmochat.ext
```

但仍按真实启动路径触发，不要预加载乱跑。

需要支持顺序不确定：

```text
P 先出现、extChunk 后出现
extChunk 先创建、P 后绑定
ER_RW metadata 先写 P、后同步 registry
registry 先有 module、后补 P
```

### C. 让 gamelist native branch 自己读 cfg.bin 并走 no_update/post_update

1. 保证 gamelist.ext 真正 native init 后进入自己的后续流程。
2. 保证 `gwy/cfg.bin`、`gwy/cfg.bin.td`、`gwy/gifs/`、gamelist bitmap 资源能从 `game_files/mythroad/240x320/gwy` 打开。
3. GWY update stub 只返回“无更新/成功继续”的平台结果，不 host-side 模拟 runapp。
4. 目标日志：

```text
[JJFB_GAMELIST_CFG36_BUILD]
[JJFB_GAMELIST_POST_UPDATE] result=no_update
```

参数必须匹配：

```text
napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink
```

### D. 恢复 native export/dispatcher 调用

`lib.startGame` / `lib.runapp` 的字符串 VA 不能当入口。必须找到 dispatcher/export call。

目标日志：

```text
[JJFB_SHELL_EXPORT_RESOLVE] module=gbrwcore.ext name=lib.runapp ...
[JJFB_SHELL_EXPORT_CALL] name=lib.runapp pc=... args=...
[JJFB_RUNAPP] source=native_shell target=gwy/jjfb.mrp
```

禁止：

```text
host_runapp_equivalent_after_no_update
bridge_dsm_mr_start_dsm 直接打开 jjfb
```

除非只是底层 `mr_start` 执行器被 native shell runapp 调用，日志必须证明 source=native_shell。

### E. 进入 jjfb / wxjwq mrc_loader-class 游戏

先用 jjfb；如 jjfb 卡在 robotol 专属逻辑，同步跑 wxjwq 做 positive control。

JJFB 预期：

```text
gwy/jjfb.mrp natural open
start.mr executes
_mr_c_load / _strCom 601/800/801
mrc_loader.ext loaded
robotol.ext loaded
mrc_init(0) ret=0
```

WXJWQ 预期：

```text
gwy/wxjwq.mrp natural open
same start.mr and same mrc_loader.ext
mmochat.ext loaded
mrc_init or equivalent reached
```

### F. 只在真实 slot call 出现后补 slot API

当出现：

```text
[JJFB_EXTCHUNK_SLOT_CALL] off=...
```

才开 slot-specific handler。不要提前铺 matrix。

slot handler 原则：

```text
observe args first
return conservative platform status
document slot meaning
never fake business/game result
```

### G. 自然可视化标准

能算“看到部分自然呈现”的最低标准：

```text
jjfb 原始资源由 game code 自然请求
bitmap/object/display API 由 game code 触发
不是 force ui_mode，不是 host overlay
```

最终首批可视化候选包括：

```text
loadingbar / bar / textbar / slogo
mainmenu robots
server select / update / login pre-screen resources
```

## 禁止清单

绝对禁止：

```text
修改 jjfb.mrp / robotol.ext / wxjwq.mrp / mmochat.ext 游戏逻辑
hardcode P+0xC 到某个猜测地址
skip fault 当成功
force ui_mode / AC8 / progress
host overlay 模拟 UI
host_runapp_equivalent 伪装成 native runapp
把 string_va 当函数入口
```

## 总运行脚本

新增一个总脚本：

```powershell
.\RUN_NATIVE_GWY_JJFB_FULL_BOOT.ps1
```

环境建议：

```powershell
$env:JJFB_NATIVE_BOOT_FULL="1"
$env:JJFB_GWY_LAUNCHER_MODE="1"
$env:JJFB_LAUNCH_PATH="gwy_native_full_shell"
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT="1"
$env:JJFB_MEMBER_VIEW_PRIMARY="all_shell_and_game"
$env:JJFB_PACKAGE_SCOPED_CLOAD="1"
$env:JJFB_EXTCHUNK_PROVIDER="shell_and_game"
$env:JJFB_ER_RW_BIND_RESTORE="shell_and_game"
$env:JJFB_GWY_UPDATE_STUB="no_update_native_branch"
$env:JJFB_RUNAPP_NATIVE_ONLY="1"
$env:JJFB_PRIMARY_TARGET="gwy/jjfb.mrp"
$env:JJFB_CONTROL_TARGET="gwy/wxjwq.mrp"
$env:JJFB_EXTCHUNK_SLOT_TRACE="1"
$env:JJFB_GAME_SELF_PATCH="0"
```

## 必须输出报告

```text
reports/fullboot_00_environment.md
reports/fullboot_01_package_scope_member_view.md
reports/fullboot_02_shell_module_context.md
reports/fullboot_03_gamelist_cfg36_no_update.md
reports/fullboot_04_native_export_runapp.md
reports/fullboot_05_jjfb_mrc_loader_robotol.md
reports/fullboot_06_wxjwq_control.md
reports/fullboot_07_slot_calls_if_any.md
reports/fullboot_08_fileopen_resource_chain.md
reports/fullboot_09_visual_natural_chain.md
reports/fullboot_10_final_verdict.md
```

## 最终成功判定

### 最低完整启动成功

```text
native_shell runapp opens gwy/jjfb.mrp
```

### 中级完整启动成功

```text
jjfb mrc_loader.ext + robotol.ext + mrc_init reached
```

### 高级完整启动成功

```text
jjfb 原始资源自然请求，并出现自然首屏/检查更新/登录前画面
```

### wxjwq control 成功

```text
wxjwq same mrc_loader path can load mmochat.ext and reach equivalent init
```


---

# File-layer Dossier：GWY / JJFB / WXJWQ 原始文件资料

## 1. 核心包一览

| basename      |   size |   entry_count | class              | reg_primary_guess   |   start_len | has_mrc_loader_ext   |   asset_count | ext_members                                                                                                                                                                                                                                                                                                                                                           |
|:--------------|-------:|--------------:|:-------------------|:--------------------|------------:|:---------------------|--------------:|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| gamelist.mrp  |  72620 |            26 | gwy_shell_core     | gamelist.ext        |        2490 | False                |            23 | gamelist.ext|reg.ext                                                                                                                                                                                                                                                                                                                                                  |
| gbrwcore.mrp  | 100292 |             3 | gwy_shell_core     | gbrwcore.ext        |        2490 | False                |             0 | reg.ext|gbrwcore.ext                                                                                                                                                                                                                                                                                                                                                  |
| gbrwshell.mrp |  33152 |             9 | gwy_shell_core     | gbrwshell.ext       |        2240 | False                |             6 | reg.ext|gbrwshell.ext                                                                                                                                                                                                                                                                                                                                                 |
| jjfb.mrp      | 414602 |            50 | mrc_loader_game    | robotol.ext         |        3787 | True                 |            54 | mrc_loader.ext|bigworldmapmodule.ext|mainmenumodule.ext|mailmodule.ext|moduletest.ext|chatmodule.ext|gezimodule.ext|taobaomodule.ext|gameattackmodule.ext|monsterstatemodule.ext|itembagshopmodule.ext|viewmanmodule.ext|betmodule.ext|monstermodule.ext|leitaimodule.ext|reg.ext|lianmengmodule.ext|othermodule.ext|robotol.ext|shopmodule.ext|itemhechengmodule.ext |
| roomlist.mrp  |  13627 |             3 | native_ext_package | roomlist.ext        |        2490 | False                |             0 | reg.ext|roomlist.ext                                                                                                                                                                                                                                                                                                                                                  |
| sanguo.mrp    | 638474 |           275 | native_ext_package | sanguo.ext          |      559903 | False                |           271 | reg.ext|sanguo.ext                                                                                                                                                                                                                                                                                                                                                    |
| spacetime.mrp | 488893 |           422 | native_ext_package | spacetime.ext       |        3435 | False                |           282 | reg.ext|spacetime.ext                                                                                                                                                                                                                                                                                                                                                 |
| tlbb.mrp      | 307238 |            18 | native_ext_package | dream.ext           |        3435 | False                |            11 | reg.ext|dream.ext                                                                                                                                                                                                                                                                                                                                                     |
| vdload.mrp    |  11586 |             3 | native_ext_package | vdload.ext          |        2490 | False                |             0 | reg.ext|vdload.ext                                                                                                                                                                                                                                                                                                                                                    |
| wxjwq.mrp     | 299067 |            21 | mrc_loader_game    | mmochat.ext         |        3787 | True                 |            17 | mrc_loader.ext|reg.ext|mmochat.ext                                                                                                                                                                                                                                                                                                                                    |

## 2. Shell key strings：gamelist.mrp

| member       |   off | s                                                                    |
|:-------------|------:|:---------------------------------------------------------------------|
| gamelist.ext | 81128 | gwy/gifs/                                                            |
| gamelist.ext | 81408 | logo.bmp                                                             |
| gamelist.ext | 81440 | gwy/                                                                 |
| gamelist.ext | 81580 | gwy/                                                                 |
| gamelist.ext | 81600 | gwy/cfg.bin.td                                                       |
| gamelist.ext | 81616 | gwy/cfg.bin                                                          |
| gamelist.ext | 81656 | gamelist.bmp                                                         |
| gamelist.ext | 81678 | new.bmp                                                              |
| gamelist.ext | 81700 | exit.bmp                                                             |
| gamelist.ext | 81722 | dtiele.bmp                                                           |
| gamelist.ext | 81744 | border.bmp                                                           |
| gamelist.ext | 81766 | dload.bmp                                                            |
| gamelist.ext | 81788 | dload1.bmp                                                           |
| gamelist.ext | 81810 | head.bmp                                                             |
| gamelist.ext | 81832 | dl_list.bmp                                                          |
| gamelist.ext | 81854 | dbhead.bmp                                                           |
| gamelist.ext | 81876 | dbbot.bmp                                                            |
| gamelist.ext | 81898 | dbboard.bmp                                                          |
| gamelist.ext | 81920 | dbtitle.bmp                                                          |
| gamelist.ext | 81942 | 1.bmp                                                                |
| gamelist.ext | 81964 | 2.bmp                                                                |
| gamelist.ext | 81986 | 3.bmp                                                                |
| gamelist.ext | 82008 | 4.bmp                                                                |
| gamelist.ext | 82030 | 5.bmp                                                                |
| gamelist.ext | 82052 | 6.bmp                                                                |
| gamelist.ext | 82100 | gwy/font.mrp                                                         |
| gamelist.ext | 82116 | gwy/gbrwcore.mrp                                                     |
| gamelist.ext | 82136 | gwy/gbrwshell.mrp                                                    |
| gamelist.ext | 82188 | napptype=%d_nurl=%s_gwyblink                                         |
| gamelist.ext | 82220 | napptype=%d_nextid=%d_ncode=%d_narg=%d_narg1=%d_nmrpname=%s_gwyblink |
| gamelist.ext | 82300 | cfunction.ext                                                        |
| gamelist.ext | 82352 | gwyblink                                                             |
| gamelist.ext | 82364 | napptype                                                             |
| gamelist.ext | 82384 | nextid                                                               |
| gamelist.ext | 82392 | ncode                                                                |
| gamelist.ext | 82416 | nmrpname                                                             |
| gamelist.ext | 82428 | gwy/%s.mrp                                                           |
| gamelist.ext | 82448 | %s.mrp                                                               |
| gamelist.ext | 82528 | gwy.mrp                                                              |
| gamelist.ext | 82536 | reglogin.mrp                                                         |
| gamelist.ext | 82552 | resmng.mrp                                                           |
| gamelist.ext | 82564 | dload.mrp                                                            |
| gamelist.ext | 82576 | gamelist.mrp                                                         |
| gamelist.ext | 82592 | gui.mrp                                                              |
| gamelist.ext | 82624 | gwy/                                                                 |
| gamelist.ext | 82632 | gwy\wizard.mrp                                                       |
| gamelist.ext | 82648 | gwy\hotchat.mrp                                                      |
| gamelist.ext | 82684 | gwy/sound/                                                           |
| gamelist.ext | 90044 | Content-Type: application/x-www-form-urlencoded;charset=UTF-8        |
| gamelist.ext | 90136 | mrc_freesky.51mrp.com                                                |
| reg.ext      |   848 | gamelist.ext                                                         |

## 3. Shell key strings：gbrwcore.mrp

| member       |    off | s                                                             |
|:-------------|-------:|:--------------------------------------------------------------|
| reg.ext      |    664 | gbrwcore.ext                                                  |
| gbrwcore.ext | 140020 | lib.clearlogininfo                                            |
| gbrwcore.ext | 140040 | lib.savelogininfo                                             |
| gbrwcore.ext | 140244 | lib.startGame                                                 |
| gbrwcore.ext | 140272 | lib.reportresult                                              |
| gbrwcore.ext | 140372 | lib.isFileOnServerNewer                                       |
| gbrwcore.ext | 140452 | lib.download                                                  |
| gbrwcore.ext | 140480 | lib.runapp                                                    |
| gbrwcore.ext | 140600 | C:/Download                                                   |
| gbrwcore.ext | 140620 | :/Download                                                    |
| gbrwcore.ext | 140632 | C:/Download/                                                  |
| gbrwcore.ext | 140648 | :/Download/                                                   |
| gbrwcore.ext | 141168 | .bmp                                                          |
| gbrwcore.ext | 142048 | .bmp                                                          |
| gbrwcore.ext | 142388 | sky_download:                                                 |
| gbrwcore.ext | 142812 | cfunction.ext                                                 |
| gbrwcore.ext | 142828 | logo.ext                                                      |
| gbrwcore.ext | 143286 | .bmp                                                          |
| gbrwcore.ext | 143605 | .mrp                                                          |
| gbrwcore.ext | 145700 | download                                                      |
| gbrwcore.ext | 145720 | download/dwnlist.dat                                          |
| gbrwcore.ext | 146728 | Content-Type: application/x-www-form-urlencoded;charset=UTF-8 |
| gbrwcore.ext | 146820 | mrc_freesky.51mrp.com                                         |

## 4. Shell key strings：gbrwshell.mrp

| member        |   off | s                                                                                                                                                             |
|:--------------|------:|:--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| reg.ext       |   608 | gbrwshell.ext                                                                                                                                                 |
| gbrwshell.ext | 39888 | cfunction.ext                                                                                                                                                 |
| gbrwshell.ext | 39904 | logo.ext                                                                                                                                                      |
| gbrwshell.ext | 41568 | wait.bmp                                                                                                                                                      |
| gbrwshell.ext | 41590 | extred.bmp                                                                                                                                                    |
| gbrwshell.ext | 41612 | dlfail.bmp                                                                                                                                                    |
| gbrwshell.ext | 41634 | dled.bmp                                                                                                                                                      |
| gbrwshell.ext | 41656 | paused.bmp                                                                                                                                                    |
| gbrwshell.ext | 41678 | expnded.bmp                                                                                                                                                   |
| gbrwshell.ext | 41700 | dling.bmp                                                                                                                                                     |
| gbrwshell.ext | 41722 | fmimg.bmp                                                                                                                                                     |
| gbrwshell.ext | 41744 | fmfolder.bmp                                                                                                                                                  |
| gbrwshell.ext | 41766 | fmcmmn.bmp                                                                                                                                                    |
| gbrwshell.ext | 41788 | fmback.bmp                                                                                                                                                    |
| gbrwshell.ext | 41810 | listf.bmp                                                                                                                                                     |
| gbrwshell.ext | 41832 | progbar.bmp                                                                                                                                                   |
| gbrwshell.ext | 41984 | .bmp                                                                                                                                                          |
| gbrwshell.ext | 42346 | .bmp                                                                                                                                                          |
| gbrwshell.ext | 42665 | .mrp                                                                                                                                                          |
| gbrwshell.ext | 43908 | http://dmrp.wapproxy.sky-mobi.com/sd?type=2&ver=0&appid=480006&shortname=wmplay&extid=6&mrpname=wmplay.mrp&extname=wmplay.ext&path=/plugins&func=1&param=NULL |
| gbrwshell.ext | 44068 | /plugins/wmplay.mrp                                                                                                                                           |
| gbrwshell.ext | 44108 | gbrw/download                                                                                                                                                 |
| gbrwshell.ext | 44128 | %s/DOWNLOADLIST%d                                                                                                                                             |
| gbrwshell.ext | 44148 | gbrw/download                                                                                                                                                 |
| gbrwshell.ext | 44164 | %s/TMPDOWNLOADLIST%d                                                                                                                                          |

## 5. JJFB key strings / resources / modules

| member                |    off | s                            |
|:----------------------|-------:|:-----------------------------|
| mrc_loader.ext        |    212 | cfunction.ext                |
| bigworldmapmodule.ext |  16308 | mapcity!150!30.bmp           |
| bigworldmapmodule.ext |  16328 | mapload!23!15.bmp            |
| mainmenumodule.ext    |  26752 | serversel                    |
| mainmenumodule.ext    |  27144 | m8!160!120@bmm8.bmp          |
| mainmenumodule.ext    |  27164 | m7!160!120@bmm7.bmp          |
| mainmenumodule.ext    |  27184 | m6!160!120@bmm6.bmp          |
| mainmenumodule.ext    |  27204 | m5!160!120@bmm5.bmp          |
| mainmenumodule.ext    |  27224 | m4!160!120@bmm4.bmp          |
| mainmenumodule.ext    |  27244 | m3!160!120@bmm3.bmp          |
| mainmenumodule.ext    |  27264 | m2!160!120@bmm2.bmp          |
| mainmenumodule.ext    |  27284 | m1!160!120@bmm1.bmp          |
| mainmenumodule.ext    |  27304 | m4!120!160@mainmenu4.bmp     |
| mainmenumodule.ext    |  27332 | m3!120!160@mainmenu3.bmp     |
| mainmenumodule.ext    |  27360 | m2!120!160@mainmenu2.bmp     |
| mainmenumodule.ext    |  27388 | m1!120!160@mainmenu1.bmp     |
| mainmenumodule.ext    |  27424 | top!76!28.bmp                |
| mainmenumodule.ext    |  27676 | robot2!165!115.bmp           |
| mainmenumodule.ext    |  27696 | robot1!191!104.bmp           |
| mainmenumodule.ext    |  27716 | menutext!76!80.bmp           |
| mailmodule.ext        |  25744 | fujian!15!15.bmp             |
| mailmodule.ext        |  25764 | money!13!14.bmp              |
| moduletest.ext        |  13984 | @serverDo=true               |
| chatmodule.ext        |  13472 | face!120!12.bmp              |
| gameattackmodule.ext  |  45184 | wy_baoji!120!14.bmp          |
| gameattackmodule.ext  |  45204 | wy_shanbi!44!14.bmp          |
| gameattackmodule.ext  |  45636 | wy_jiaxue!110!12.bmp         |
| gameattackmodule.ext  |  45660 | wy_shanghai!100!12.bmp       |
| gameattackmodule.ext  |  45708 | effect!49!30.bmp             |
| gameattackmodule.ext  |  46276 | shandow!49!17@attack.bmp     |
| itembagshopmodule.ext |  42028 | bag_hand!17!16.bmp           |
| itembagshopmodule.ext |  42048 | money!13!14.bmp              |
| viewmanmodule.ext     |  18328 | bag_hand!17!16.bmp           |
| reg.ext               |   2104 | robotol.ext                  |
| reg.ext               |   2116 | moduletest.ext               |
| reg.ext               |   2132 | mainmenumodule.ext           |
| reg.ext               |   2152 | bigworldmapmodule.ext        |
| reg.ext               |   2176 | gameattackmodule.ext         |
| reg.ext               |   2200 | taobaomodule.ext             |
| reg.ext               |   2220 | gezimodule.ext               |
| reg.ext               |   2236 | mailmodule.ext               |
| reg.ext               |   2252 | chatmodule.ext               |
| reg.ext               |   2268 | monsterstatemodule.ext       |
| reg.ext               |   2292 | viewmanmodule.ext            |
| reg.ext               |   2312 | itembagshopmodule.ext        |
| reg.ext               |   2336 | leitaimodule.ext             |
| reg.ext               |   2356 | monstermodule.ext            |
| reg.ext               |   2376 | lianmengmodule.ext           |
| reg.ext               |   2396 | othermodule.ext              |
| reg.ext               |   2412 | itemhechengmodule.ext        |
| reg.ext               |   2436 | shopmodule.ext               |
| reg.ext               |   2452 | betmodule.ext                |
| robotol.ext           | 239004 | .mrp                         |
| robotol.ext           | 239048 | default.mrp                  |
| robotol.ext           | 239060 | default2.mrp                 |
| robotol.ext           | 239076 | default3.mrp                 |
| robotol.ext           | 239092 | jiantou_x!12!10.bmp          |
| robotol.ext           | 239112 | jiantou_t!12!10.bmp          |
| robotol.ext           | 239132 | jiantou!12!15.bmp            |
| robotol.ext           | 239152 | wy_jiao5!11!11.bmp           |
| robotol.ext           | 239172 | wy_xian3!15!5.bmp            |
| robotol.ext           | 239192 | wy_jiao32!10!10.bmp          |
| robotol.ext           | 239212 | wy_jiao3!10!10.bmp           |
| robotol.ext           | 239232 | wy_xian2!15!5.bmp            |
| robotol.ext           | 239252 | wy_jiao22!10!10.bmp          |
| robotol.ext           | 239272 | wy_jiao2!10!10.bmp           |
| robotol.ext           | 239292 | wy_xian0!15!6.bmp            |
| robotol.ext           | 239312 | wy_jiao02!10!10.bmp          |
| robotol.ext           | 239332 | wy_jiao0!10!10.bmp           |
| robotol.ext           | 239352 | wy_xian1!15!7.bmp            |
| robotol.ext           | 239372 | wy_jiao12!11!11.bmp          |
| robotol.ext           | 239392 | wy_jiao1!11!11.bmp           |
| robotol.ext           | 239420 | face!120!12.bmp              |
| robotol.ext           | 239520 | boygirl!40!15.bmp            |
| robotol.ext           | 239540 | star!8!8.bmp                 |
| robotol.ext           | 239556 | keypress!28!21.bmp           |
| robotol.ext           | 239576 | e!7!9.bmp                    |
| robotol.ext           | 239588 | finger!14!12.bmp             |
| robotol.ext           | 239608 | vip!34!12.bmp                |
| robotol.ext           | 239624 | downArrow!14!9.bmp           |
| robotol.ext           | 239644 | updown!28!8.bmp              |
| robotol.ext           | 239660 | upArrow!14!9.bmp             |
| robotol.ext           | 239680 | money!5!10.bmp               |
| robotol.ext           | 239696 | lrarrow!16!14.bmp            |
| robotol.ext           | 239716 | chilunbar!23!23.bmp          |
| robotol.ext           | 239736 | icon1!100!25.bmp             |
| robotol.ext           | 239756 | buttons!36!12.bmp            |
| robotol.ext           | 239776 | bighand!25!25.bmp            |
| robotol.ext           | 239796 | topright!12!4.bmp            |
| robotol.ext           | 239816 | topleft!15!5.bmp             |
| robotol.ext           | 239852 | textbar!120!30.bmp           |
| robotol.ext           | 239872 | bar!16!18.bmp                |
| robotol.ext           | 239888 | loadingbar!201!29.bmp        |
| robotol.ext           | 239912 | gunModel!47!28.bmp           |
| robotol.ext           | 239932 | wingModel!72!41.bmp          |
| robotol.ext           | 239952 | weaponModel!41!42.bmp        |
| robotol.ext           | 239976 | dongModel!28!48.bmp          |
| robotol.ext           | 240704 | dir!100!100@vmimage.bmp      |
| robotol.ext           | 240728 | vmleft!57!36@vmimage.bmp     |
| robotol.ext           | 240756 | vmright!57!36@vmimage.bmp    |
| robotol.ext           | 240784 | taskbutton!57!36@vmimage.bmp |
| robotol.ext           | 240904 | gwy/jjfbol/                  |
| robotol.ext           | 240916 | buttons!36!12.bmp            |
| robotol.ext           | 240936 | keypress!28!21.bmp           |
| robotol.ext           | 242888 | .bmp                         |
| robotol.ext           | 242928 | slogo!157!58.bmp             |
| robotol.ext           | 242948 | textbar!120!30.bmp           |
| robotol.ext           | 242968 | bar!16!18.bmp                |
| robotol.ext           | 242984 | loadingbar!201!29.bmp        |
| robotol.ext           | 243540 | zuobiao!60!8.bmp             |
| robotol.ext           | 243560 | dirarrow!13!12.bmp           |
| robotol.ext           | 243592 | monstersheji!48!16.bmp       |
| robotol.ext           | 243616 | monsterquanneng!48!16.bmp    |
| robotol.ext           | 243644 | monstergedou!48!16.bmp       |
| robotol.ext           | 243696 | fasticon!112!16.bmp          |
| robotol.ext           | 243716 | cheng!8!8.bmp                |
| robotol.ext           | 243732 | smallicon!140!20.bmp         |
| robotol.ext           | 243756 | text2!36!12.bmp              |
| robotol.ext           | 243772 | upArrow!14!9.bmp             |
| robotol.ext           | 243792 | downArrow!14!9.bmp           |
| robotol.ext           | 243812 | exp!14!9.bmp                 |
| robotol.ext           | 243828 | frame!16!16.bmp              |
| robotol.ext           | 243844 | fuhao!40!16.bmp              |
| robotol.ext           | 243860 | lchars!96!9.bmp              |
| robotol.ext           | 243876 | shine!27!9.bmp               |
| robotol.ext           | 245720 | chilun!123!35.bmp            |
| robotol.ext           | 245912 | explight!102!15.bmp          |
| robotol.ext           | 245932 | moneylight!102!15.bmp        |
| robotol.ext           | 245956 | powerlight!138!29.bmp        |
| robotol.ext           | 245980 | teclight!102!15.bmp          |
| robotol.ext           | 246000 | defendlight!132!23.bmp       |
| robotol.ext           | 246024 | speedlight!102!15.bmp        |
| robotol.ext           | 246048 | lucklight!102!15.bmp         |
| robotol.ext           | 247436 | jt!30!16.bmp                 |
| robotol.ext           | 249724 | face!120!12.bmp              |

## 6. WXJWQ key strings / resources / modules

| member         |    off | s                                       |
|:---------------|-------:|:----------------------------------------|
| mrc_loader.ext |    212 | cfunction.ext                           |
| reg.ext        |    688 | mmochat.ext                             |
| mmochat.ext    | 305956 | gwy/xjwq/loginserver.sys                |
| mmochat.ext    | 305984 | src\mmochat_serverlist.c                |
| mmochat.ext    | 306012 | gwy/xjwq/                               |
| mmochat.ext    | 306620 | gwy/xjwq/res/mrp/mmochat_mapBaseInfo.rs |
| mmochat.ext    | 306796 | gwy/xjwq/xjwq.res                       |
| mmochat.ext    | 309748 | mmochat.ext                             |
| mmochat.ext    | 309760 | gwy/rollscr.mrp                         |
| mmochat.ext    | 311892 | src\mmochat_update.c                    |
| mmochat.ext    | 311924 | gwy/xjwq/res/                           |
| mmochat.ext    | 311948 | gwy/xjwq/res/mrp/                       |
| mmochat.ext    | 311984 | gwy/xjwq/xjwq.res                       |
| mmochat.ext    | 312004 | %smmochat_res%d.mrp.rs                  |
| mmochat.ext    | 312028 | face1.bmp                               |
| mmochat.ext    | 312050 | face2.bmp                               |
| mmochat.ext    | 312072 | face3.bmp                               |
| mmochat.ext    | 312094 | face4.bmp                               |
| mmochat.ext    | 312116 | face5.bmp                               |
| mmochat.ext    | 312138 | hill1.bmp                               |
| mmochat.ext    | 312160 | hill2.bmp                               |
| mmochat.ext    | 312182 | name1.bmp                               |
| mmochat.ext    | 312204 | name2.bmp                               |
| mmochat.ext    | 312226 | name3.bmp                               |
| mmochat.ext    | 312248 | load1.bmp                               |
| mmochat.ext    | 312270 | load2.bmp                               |
| mmochat.ext    | 312292 | load3.bmp                               |
| mmochat.ext    | 312314 | zhang.bmp                               |
| mmochat.ext    | 313216 | gwy/xjwq/sfc.bin                        |
| mmochat.ext    | 313248 | gwy/xjwq/                               |
| mmochat.ext    | 313312 | line1.bmp                               |
| mmochat.ext    | 313334 | line2.bmp                               |
| mmochat.ext    | 313356 | loghead.bmp                             |
| mmochat.ext    | 313378 | logfra1.bmp                             |
| mmochat.ext    | 313400 | logfra2.bmp                             |
| mmochat.ext    | 313422 | logfra3.bmp                             |
| mmochat.ext    | 313444 | logfra4.bmp                             |
| mmochat.ext    | 313466 | logbutt.bmp                             |
| mmochat.ext    | 313488 | logbutt2.bmp                            |
| mmochat.ext    | 313510 | serbg1.bmp                              |
| mmochat.ext    | 313532 | serbg2.bmp                              |
| mmochat.ext    | 313554 | logser.bmp                              |
| mmochat.ext    | 313576 | light.bmp                               |
| mmochat.ext    | 313598 | loguser.bmp                             |
| mmochat.ext    | 313620 | light2.bmp                              |
| mmochat.ext    | 313642 | rolebg1.bmp                             |
| mmochat.ext    | 313664 | rolebg2.bmp                             |
| mmochat.ext    | 313686 | rolebg3.bmp                             |
| mmochat.ext    | 313708 | rolebg4.bmp                             |
| mmochat.ext    | 313730 | pkbg.bmp                                |
| mmochat.ext    | 313752 | boy1_1.bmp                              |
| mmochat.ext    | 313774 | boy1_2.bmp                              |
| mmochat.ext    | 313796 | boy1_3.bmp                              |
| mmochat.ext    | 313818 | girl1_1.bmp                             |
| mmochat.ext    | 313840 | girl1_2.bmp                             |
| mmochat.ext    | 313862 | girl1_3.bmp                             |
| mmochat.ext    | 313884 | boy2_1.bmp                              |
| mmochat.ext    | 313906 | boy2_2.bmp                              |
| mmochat.ext    | 313928 | boy2_3.bmp                              |
| mmochat.ext    | 313950 | girl2_1.bmp                             |
| mmochat.ext    | 313972 | girl2_2.bmp                             |
| mmochat.ext    | 313994 | girl2_3.bmp                             |
| mmochat.ext    | 314016 | boy3_1.bmp                              |
| mmochat.ext    | 314038 | boy3_2.bmp                              |
| mmochat.ext    | 314060 | boy3_3.bmp                              |
| mmochat.ext    | 314082 | girl3_1.bmp                             |
| mmochat.ext    | 314104 | girl3_2.bmp                             |
| mmochat.ext    | 314126 | girl3_3.bmp                             |
| mmochat.ext    | 314148 | frame.bmp                               |
| mmochat.ext    | 314170 | list.bmp                                |
| mmochat.ext    | 314192 | arrow.bmp                               |
| mmochat.ext    | 314214 | arrow2.bmp                              |
| mmochat.ext    | 314236 | arrow3.bmp                              |
| mmochat.ext    | 314258 | m1.bmp                                  |
| mmochat.ext    | 314280 | m1_s.bmp                                |
| mmochat.ext    | 314302 | m2.bmp                                  |
| mmochat.ext    | 314324 | m2_s.bmp                                |
| mmochat.ext    | 314346 | m3.bmp                                  |
| mmochat.ext    | 314368 | m4.bmp                                  |
| mmochat.ext    | 314390 | m4_s.bmp                                |
| mmochat.ext    | 314412 | 1.bmp                                   |
| mmochat.ext    | 314434 | 2.bmp                                   |
| mmochat.ext    | 314456 | 3.bmp                                   |
| mmochat.ext    | 314478 | 4.bmp                                   |
| mmochat.ext    | 314500 | 5.bmp                                   |
| mmochat.ext    | 314522 | 6.bmp                                   |
| mmochat.ext    | 314544 | 7.bmp                                   |
| mmochat.ext    | 314566 | 8.bmp                                   |
| mmochat.ext    | 314588 | 9.bmp                                   |
| mmochat.ext    | 314610 | 10.bmp                                  |
| mmochat.ext    | 314632 | 11.bmp                                  |
| mmochat.ext    | 314654 | 13.bmp                                  |
| mmochat.ext    | 314676 | 14.bmp                                  |
| mmochat.ext    | 314698 | 15.bmp                                  |
| mmochat.ext    | 314720 | 16.bmp                                  |
| mmochat.ext    | 314742 | 17.bmp                                  |
| mmochat.ext    | 314764 | 18.bmp                                  |
| mmochat.ext    | 314786 | 19.bmp                                  |
| mmochat.ext    | 314808 | 20.bmp                                  |
| mmochat.ext    | 314830 | 21.bmp                                  |
| mmochat.ext    | 314852 | 22.bmp                                  |
| mmochat.ext    | 314874 | 30.bmp                                  |
| mmochat.ext    | 314896 | 31.bmp                                  |
| mmochat.ext    | 314918 | 32.bmp                                  |
| mmochat.ext    | 314940 | 33.bmp                                  |
| mmochat.ext    | 314962 | 34.bmp                                  |
| mmochat.ext    | 314984 | 35.bmp                                  |
| mmochat.ext    | 315006 | 37.bmp                                  |
| mmochat.ext    | 315028 | 39.bmp                                  |
| mmochat.ext    | 315050 | 40.bmp                                  |
| mmochat.ext    | 315072 | 42.bmp                                  |
| mmochat.ext    | 315094 | 43.bmp                                  |
| mmochat.ext    | 315116 | 45.bmp                                  |
| mmochat.ext    | 315138 | 46.bmp                                  |
| mmochat.ext    | 315160 | 48.bmp                                  |
| mmochat.ext    | 315182 | 49.bmp                                  |
| mmochat.ext    | 315204 | entry.bmp                               |
| mmochat.ext    | 315226 | entry2.bmp                              |
| mmochat.ext    | 315248 | 1_s.bmp                                 |
| mmochat.ext    | 315270 | 11_s.bmp                                |
| mmochat.ext    | 315292 | 13_s.bmp                                |
| mmochat.ext    | 315314 | 16_s.bmp                                |
| mmochat.ext    | 315336 | 49_s.bmp                                |
| mmochat.ext    | 315358 | entry_s.bmp                             |
| mmochat.ext    | 315380 | roletag.bmp                             |
| mmochat.ext    | 315402 | npctag.bmp                              |
| mmochat.ext    | 315424 | guai1.bmp                               |
| mmochat.ext    | 315446 | guai2.bmp                               |
| mmochat.ext    | 315468 | guai3.bmp                               |
| mmochat.ext    | 315490 | guai4.bmp                               |
| mmochat.ext    | 315512 | guai5.bmp                               |
| mmochat.ext    | 315534 | guai5_2.bmp                             |
| mmochat.ext    | 315556 | guai6.bmp                               |
| mmochat.ext    | 315578 | guai7.bmp                               |
| mmochat.ext    | 315600 | guai8.bmp                               |
| mmochat.ext    | 315622 | guai9.bmp                               |
| mmochat.ext    | 315644 | guai10.bmp                              |
| mmochat.ext    | 315666 | guai11.bmp                              |
| mmochat.ext    | 315688 | guai12.bmp                              |
| mmochat.ext    | 315710 | guai13.bmp                              |

## 7. JJFB member manifest

| member                 |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:-----------------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr               |          1514 |            3787 | True         | .mr   | c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05 |
| betmodule.ext          |          8327 |           14512 | True         | .ext  | 2bd3cee91fb99173522b6c83f33cdd1b885fb349874bfa0f8a4ce7b3e2bcb60e |
| bigworldmapmodule.ext  |          8741 |           17360 | True         | .ext  | baf01ab991598919473c35dc2e96c4574b10d62bf3c166c976a0437eda0ea3de |
| chatmodule.ext         |          8857 |           15228 | True         | .ext  | b1f7f8bc1846f164e7b7273931806118505086e20aedb29aab18714cbe455401 |
| gameattackmodule.ext   |         26089 |           47368 | True         | .ext  | 6f3afe7462a83168142f205a83da9fb22057c930c10feef8a652953d29f43b18 |
| gezimodule.ext         |          4261 |            7356 | True         | .ext  | 59d39d323fe634f0aeb65076672fe501a557c473a86f02f3ad7aa69f2525d490 |
| itembagshopmodule.ext  |         20439 |           43420 | True         | .ext  | de9a51caaf5fa084fae2bbf45579c3a3cbcdf5fad3f0032b59fa8fec8dd06887 |
| itemhechengmodule.ext  |         12957 |           25136 | True         | .ext  | fa4ffb041f016c82ac21745ea35e1e00bb93a218e0ca625f85b2b5c7e9504a06 |
| leitaimodule.ext       |         11387 |           21248 | True         | .ext  | b0a9ac472cc958acf06ecb6a55a6a6dc59942d2943a45d5918ee1e6f1c9c2885 |
| lianmengmodule.ext     |         17529 |           36216 | True         | .ext  | 3129f23b7b1038ab09a8017805ec662fbab9ae05fa65bcc0ccb8e2efa6ead93b |
| mailmodule.ext         |         14778 |           27692 | True         | .ext  | faad4e9f796c26658e8d7a3a97b5f38f3161b202e264499bb4dec3bb9cb1b640 |
| mainmenumodule.ext     |         15300 |           28712 | True         | .ext  | ecad68139c8f94b379a8c88c7facf42da5b6778ea50cd5e8a99718a3b769b4cd |
| moduletest.ext         |          7200 |           15152 | True         | .ext  | 120fdea40196e29b9cbfb355ba460ac568c3d26e3f008da6ad7b838975bc7e92 |
| monstermodule.ext      |          8870 |           19256 | True         | .ext  | 071e0a52f716500cac606c35d673c9fb6caead2e535f2cf2feb5aeb0415891ab |
| monsterstatemodule.ext |         10663 |           18712 | True         | .ext  | 39e73ba119de641eee0ea2b738b26ef297d784ecad4137ceba9784403901e5fb |
| mrc_loader.ext         |           219 |             232 | True         | .ext  | d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938 |
| othermodule.ext        |         24412 |           46708 | True         | .ext  | 060159526b5a84d11e7eaa9cb2b9c030d40319dd0b812bd0edacc0e280f15d70 |
| reg.ext                |          1381 |            2472 | True         | .ext  | 6aa5e6be29634f0ecddd27a142ff9a55f922a9e51f06b0ef2bf029995c02fb9d |
| robotol.ext            |        161178 |          253420 | True         | .ext  | 55f66f1cca810ae7febe31af7e2dba8d00e6296a31aa7613f4aaf9ce5bb2fe82 |
| shopmodule.ext         |          8820 |           14836 | True         | .ext  | 96736090d5f8b987df218dfbfe0e8f00f35c13f440b281ad3f468fb979409b9c |
| taobaomodule.ext       |          9352 |           16504 | True         | .ext  | 175a7268ae625ed9473b980ff59369159e6f1147fbb21df19fba4ac41c0a705b |
| viewmanmodule.ext      |         10976 |           19436 | True         | .ext  | 9cc25245d323d87f3f9de9a3a091e3eb127ea940030ce7c1e72a8e91bed6897c |
| bar!16!18.bmp          |            35 |             576 | True         | .bmp  | 3dfc15b75d265a3593991b6d076e41b678d341c681721628baac21cd081886eb |
| dirarrow!13!12.bmp     |            95 |             312 | True         | .bmp  | b74167de275cc489e573714b3c5a7af4d0df86a31e31cf6c353fde4accb0f866 |
| jiantou!12!15.bmp      |           108 |             360 | True         | .bmp  | 26cd05339d7819be0b6cff6781cf666f5fd614a55fe943594d606d021bf6f7c2 |
| jiantou_t!12!10.bmp    |            47 |             240 | True         | .bmp  | b5be03d78f1fc1c7b688b3e2c76e6c2239c1c2b2e533e72f207c380ea5981c94 |
| jiantou_x!12!10.bmp    |            45 |             240 | True         | .bmp  | 2983645fa9322a3339df218d9ffe615aac39c764f5caf6ab60889139f026b609 |
| jiao41!12!12.bmp       |            90 |             288 | True         | .bmp  | 77d895f0cc9fa2a9ce2f46c90269ca7e5dc7088280102b747fe850d8c291ae78 |
| jiao42!11!6.bmp        |            48 |             132 | True         | .bmp  | a252997e7ea6abb542f300257d4459eed5217c8616692da92afa473762396013 |
| jt!30!16.bmp           |           148 |             960 | True         | .bmp  | c65d7cdd7edb99447ff33268a7e56658bbddf7b502c0a08877a1c2bc897dc601 |
| listicon.bmp           |           184 |             800 | True         | .bmp  | 2828abd633027d0d3a890438149f574eb1b626262689f92027ae3b21e24c75f2 |
| loadingbar!201!29.bmp  |          1385 |           11658 | True         | .bmp  | 6ab17ba601918ab21e1ec7d802ed70027586f2ce231e5b11732e0f97a5d6435d |
| slogo!157!58.bmp       |         11897 |           18212 | True         | .bmp  | 919d94d541ad0863fd1e7b630475401cde0097801ccdc8a516ef48cb84773fa9 |
| target!65!25.bmp       |           351 |            3250 | True         | .bmp  | 636bacb7634ff7eda8c69ae9816decc99f62a9356c1986cdbe97b887255cba7a |
| textbar!120!30.bmp     |          1544 |            7200 | True         | .bmp  | 1ec9f3d46f79243d4c961afc2b61a241b213a22ef35f43999fd6f2a1adb13bbc |
| top!76!28.bmp          |          1321 |            4256 | True         | .bmp  | b2dbe87f1b037af5cb892bac0c0469f2d7d5415f9df7df4c1d2bb9f5f8d9da0a |
| wy_jiao0!10!10.bmp     |            89 |             200 | True         | .bmp  | de492a542e5aec02cee34d3508c0c728c09d5404ed24007211f44d74f90a0f64 |
| wy_jiao02!10!10.bmp    |            91 |             200 | True         | .bmp  | 57298e740114d972d8c14c4c9b9075ec5119be13acc3888512b7cf658f6e2786 |
| wy_jiao1!11!11.bmp     |            90 |             242 | True         | .bmp  | edfe428dfb2daa8deea599915b7c5d4db75b6bfbfe78671cecd33e4ca4662a13 |
| wy_jiao12!11!11.bmp    |            51 |             242 | True         | .bmp  | a998e920d576ab639cdffad5b47f8f088431c8fa12f44747b68ebbb45f78b6d8 |
| wy_jiao2!10!10.bmp     |            67 |             200 | True         | .bmp  | e44746d176272d4165daa2e5733c7ea188c82762c8bba8fe8f65618d5613415c |
| wy_jiao22!10!10.bmp    |            48 |             200 | True         | .bmp  | 455e422004ed8dbfa1e822e656034aaa60d5c7fd77dd8409483f832680b66881 |
| wy_jiao3!10!10.bmp     |            67 |             200 | True         | .bmp  | 6c75abc1bb592f55c075d4681769205d25f65d11744b95155e967d3f3bd8878c |
| wy_jiao32!10!10.bmp    |            48 |             200 | True         | .bmp  | d0fa471ca30cf10edf1c18b0af25868b411870380e06d90499c9151532580361 |
| wy_jiao5!11!11.bmp     |            73 |             242 | True         | .bmp  | 8c32b77a50ba7638952ccdbfbd160b2c9ba7f618157222b1c563151ef52bcdc6 |
| wy_xian0!15!6.bmp      |            42 |             180 | True         | .bmp  | e581348ffc815e467b5314bdfc9bb887dc3ae02ded019975403af80037572899 |
| wy_xian1!15!7.bmp      |            36 |             210 | True         | .bmp  | 84d53fabd01318e635c5f87138f21d2eb3271d280f56ca9c0dd8b01f1554968d |
| wy_xian2!15!5.bmp      |            35 |             150 | True         | .bmp  | d38fb926de56c4243ea0145f5454b52ccfd19d683ebe4cb758743efabc4a5485 |
| wy_xian3!15!5.bmp      |            36 |             150 | True         | .bmp  | 872cd8cab86510eb19b578fec2ffa07af1792940c95835644cb59618bb73ce92 |
| target.ani             |           113 |             125 | True         | .ani  | b35ad4317927a0c190731b4fd15361d7c644653b9fe642852f51c3912702d83b |

## 8. WXJWQ member manifest

| member         |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:---------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr       |          1514 |            3787 | True         | .mr   | c8d664aa7034d044ded428158f9cca1d49a3781242421eb1b1de7f201949ff05 |
| mmochat.ext    |        206148 |          320292 | True         | .ext  | b7cf57219e8eedaba3c71129df2ed4b03b9918d9038d977191fa14f78199f0fd |
| mrc_loader.ext |           219 |             232 | True         | .ext  | d36151ee3c119717305afe4b1f0ba47f0f0154f8ba6f2c5081d6402c8eddd938 |
| reg.ext        |           658 |             704 | True         | .ext  | fe21e8b00e67a42779aad07a0a5adc264395b88065888644d2a0351e85cc7204 |
| sfcidx.bin     |          1978 |            2020 | True         | .bin  | e19edf8805d7982bad464ce1f0c0541596d974753c992e55e96ef77f8fb28b9c |
| face1.bmp      |          4411 |           30720 | True         | .bmp  | f169a959f47843c83c964071b4983ce2760f078318c35fbc0f21ad9587d80731 |
| face2.bmp      |          5858 |           30720 | True         | .bmp  | 46454b24cfd087b92f2b4ebebc19f3a8c9ee7d2d5ccd192910d721bc6fe8ac33 |
| face3.bmp      |         10582 |           30720 | True         | .bmp  | dbfc11c586dcfb6bb83a42aae10d94c202214038918d4458607fdf749d75015e |
| face4.bmp      |         11632 |           30720 | True         | .bmp  | 4abc18479faebb44ac8114ba98c67d956df7bc0098f3653f120ff95c108e900e |
| face5.bmp      |          9314 |           30720 | True         | .bmp  | ebb2f58d620178bde8c3ef9dee8331af8c16d43ccff0a4c3ca5bf388c928721e |
| hill1.bmp      |         16861 |           35960 | True         | .bmp  | 812fdcf98e717e6891b094e5c347a8f07d3f87f04ce36eeba5ec2c6923d591fb |
| hill2.bmp      |         17987 |           42390 | True         | .bmp  | 58f04c1d9cc2ce19c3a00e07f40a515572cad87194eb608c5c06ea6e3b5bfca2 |
| load1.bmp      |           465 |            6154 | True         | .bmp  | c695a68743794db03540f809170ad13947e34572f4b305984cffb2153aa09a3e |
| load2.bmp      |           321 |            3006 | True         | .bmp  | 77620a4db7a9ee7040e6aabefebb24346a338393a9060bce6ca2abd0b612dd3c |
| load3.bmp      |           112 |             364 | True         | .bmp  | 71d989c1a2f248e0ea7a702e6b1c5182bc97f19ce2b54502e6659ecb5b58be22 |
| name1.bmp      |          3410 |           23940 | True         | .bmp  | 4e9df47fcedaf4fdbb00f529f972cddbe872d4a1948b0910677343c7ed50ef97 |
| name2.bmp      |           159 |             224 | True         | .bmp  | 98ca09c1f8f7385efa59e796c7867ae5602b4278861f8b299a7f869bfac85bed |
| name3.bmp      |           161 |             696 | True         | .bmp  | 6bee9896666d73c57e2e17665ee8bfdfe3e0eccc4f28fd2b890ec9304278cefc |
| zhang.bmp      |           691 |             684 | True         | .bmp  | 20c8026cb56ef9127699209745a35f016c6587d012b37b5579a795b3c1fc7ec6 |
| text.res       |          4359 |            8454 | True         | .res  | 6fb1df673795d20d47160949bd84fd8ad2661073f117dfefc7e9e38c2868d502 |
| world.path     |          1053 |            1694 | True         | .path | 597456b8468e98cb640319f83581dc309a3b6879ef28a76a5f7e86cda6c06f17 |

## 9. Shell package member manifests

### gbrwcore.mrp

| member       |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:-------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr     |          1018 |            2490 | True         | .mr   | ff67eea7e6eed10e3871bd465953e7137001e2495b1eb13309c56db8659bfc7c |
| gbrwcore.ext |         98264 |          147196 | True         | .ext  | bad9ffb88d41d26a7db81ae41a449e2476ea615947ab06819a6ac13a55c26270 |
| reg.ext      |           638 |             684 | True         | .ext  | 8cd1a97b5cb27f9cd6ab6e67d496b9acf3b0cd48386efe17c345b36341dd8d0c |

### gbrwshell.mrp

| member        |   stored_size |   unpacked_size | compressed   | ext   | sha256                                                           |
|:--------------|--------------:|----------------:|:-------------|:------|:-----------------------------------------------------------------|
| start.mr      |           917 |            2240 | True         | .mr   | 7fc8d5412ac5d932196ebd2b1f22b042965442057f604371f3025d0

[File dossier truncated in single master file; full dossier is in the zip pack.]
