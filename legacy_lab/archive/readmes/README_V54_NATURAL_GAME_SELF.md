# JJFB v54 Natural Game-Self

承接已跑通的 v53 start handoff，关闭默认 FORCE `ui_mode=0x45`，观察原始游戏自然推进。

## 使用

```powershell
.\RUN_V54_NATURAL_GAME_SELF.ps1 -Seconds 25
```

报告：

```text
reports\v54_natural_game_self_run_result.md
reports\v54_natural_game_self_implementation.md
logs\v54_natural_game_self_stdout.txt
```

## 理想日志

```text
mr_get_method(1514/219/161178)
[JJFB_START_HANDOFF] action=run_host_801_recovery
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer ... RUNNING=1
[JJFB_GAME_SELF] natural_mode=1 ...
[JJFB_FIRST_SCREEN] NO FORCE ui_mode (natural)
```

不应再出现：

```text
[JJFB_FIRST_SCREEN] FORCE state/ui_mode ... -> 0x45
```

## 结果分流

- handoff 通 + NO FORCE + ui_mode 仍为 0：v54 目标完成；v55 查自然写状态的事件/网络/`_strCom`。
- 仍出现 FORCE：检查 `bridge.c` GWY skip_force 与 `JJFB_FORCE_UI_MODE=0`。
- handoff 断：回退 v53，不要继续 natural 审计。
