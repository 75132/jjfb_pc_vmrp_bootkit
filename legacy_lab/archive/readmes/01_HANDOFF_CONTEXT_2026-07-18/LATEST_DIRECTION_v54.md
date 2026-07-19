# 最新路线：v54 Natural Game-Self

## 已锁定事实

- v51：canonical `sdk_key.dat`。
- v52/v53：`br_log` → `ext_base+0xD4` 别名 `cfunction.ext→robotol.ext`；`MR_IGNORE` 仅在 alias+robotol 后置条件下恢复 host `6→8→0`；timer RUNNING。
- v53 跑通后仍默认 FORCE `ui_mode=0x45`（环境变量未设时 C 默认开启）——这不是正式方案。
- v54：GWY Launcher Mode 默认 NO FORCE；本机确认 `natural_mode=1` 且 handoff 仍完整。

## 本机证据

```text
[JJFB_GAME_SELF] natural_mode=1 gwy=1 no_force_ui_mode state=0x0 tick=10
[JJFB_FIRST_SCREEN] NO FORCE ui_mode (natural) state=0x0 tick=10
ui_mode 全程 0x0
connecting xref hits=0
```

## v55 唯一任务

找出游戏自然推进所需的平台契约（事件 / `_strCom` / 网络），让 guest **自己**写状态：

```text
不要 FORCE ui_mode=0x45
不要 AC8 / progress driver
不要 host overlay UI
```

优先：

1. `0x306344` 在 ui_mode=0 + event=0x13 的自然分支；
2. 谁写 `ERW+0x8D0`；
3. connecting / 网络 / `_strCom` 是否被平台挡住。

## 禁止回退

- 不回 UI 动画打磨；
- 不覆盖 canonical `jjfb.mrp`；
- 不再把 FORCE 0x45 当成果。
