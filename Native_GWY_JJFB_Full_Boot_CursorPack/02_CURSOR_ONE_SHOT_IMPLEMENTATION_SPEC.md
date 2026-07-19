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
