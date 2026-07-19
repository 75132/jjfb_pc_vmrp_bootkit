# JJFB v53 Start Handoff Recovery

本包承接已在 Windows 本机跑通的 v52：

```text
start.mr(1514) → mrc_loader.ext(219)
→ br_log 在 ext_base+0xD4 完成 cfunction.ext→robotol.ext
→ robotol.ext(161178) → start_dsm raw_ret=0x1
```

v53 只处理最后一段交接：`0x1` 在当前链路中是 `MR_IGNORE`。它不会被全局改成成功；只有以下条件同时成立才执行 host 侧 `6 → 8 → 0`：

1. MRP 成员别名已成功应用；
2. `robotol.ext` 已解出；
3. 新 robotol helper 已注册。

原始 `game_files/mythroad/240x320/gwy/jjfb.mrp` 不修改。

## 使用

把本 ZIP **整体解压覆盖到现有项目根目录**，然后运行：

```powershell
.\RUN_V53_START_HANDOFF_RECOVERY.ps1 -Seconds 25
```

运行报告：

```text
reports\v53_start_handoff_run_result.md
```

## 理想日志

```text
mr_get_method(1514)
mr_get_method(219)
[JJFB_MRP_ALIAS] ... method=ext_base_0xD4
mr_get_method(161178)
[JJFB_ROBOTOL_LOAD] ... source=br_log
bridge_dsm_mr_start_dsm ... 0x1
[JJFB_START_HANDOFF] ... action=run_host_801_recovery
[JJFB_801_GUARD] ... action=run_host_801
[JJFB_801] host version(6) ret=0
[JJFB_801] host appInfo(8) ret=0
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer ... RUNNING=1
```

## 结果分流

- `mrc_init(0)=0` 且 timer RUNNING：v53 目标完成，下一轮只看游戏事件/网络或首屏。
- `version(6)` 或 `appInfo(8)` 非 0：继续补 robotol 的 801 输入契约。
- `mrc_init(0)` 非 0：继续审计 `mrc_extChunk`、ER_RW、P 结构和初始化内部。
- raw `0x1` 但没有 `run_host_801_recovery`：检查 v53 环境变量和三项后置条件。
- 不再出现 `161178`：回退到别名 hook，不能继续解释 `start_dsm`。

不要运行旧的 v49 UI gate，也不要修改或替换 canonical `jjfb.mrp`。
