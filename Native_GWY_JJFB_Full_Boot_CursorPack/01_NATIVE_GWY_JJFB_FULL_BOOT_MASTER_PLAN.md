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
