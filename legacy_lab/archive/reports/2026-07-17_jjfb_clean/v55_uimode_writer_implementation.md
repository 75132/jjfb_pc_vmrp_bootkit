# v55 ui_mode Writer Coverage 实施报告

## 1. 本轮目标

v54 证明：关掉 FORCE 后 `ui_mode` 停在 `0`，guest 从不写 `ERW+0x8D0`。

v55 只做一件事：找出**自然**写 `ui_mode=0x45` 的 guest 代码，并动态确认是否执行。

```text
不要 FORCE ui_mode
不要 AC8 / progress driver
```

## 2. 静态结论（已锁定）

自然 writer：

```text
0x2FC418:
  LDR r5, =0x8D0; ADD r5, ERW
  ... BA0 相关清理 ...
  MOVS r0, #0x45
  STR r0, [r5, #0]   @ 0x2FC448
```

调用链：

```text
0x2FC03C ──BL──► 0x2FC418 (writer)
     ▲
0x2DAE24 (仅当 ERW+0xB70 有符号字节 != 0)
     ▲
0x2DADC4 (门控 init；还读 B58/DB0)
     ▲
callers: 0x2FECA2 / 0x2E4066 / 0x305EF4

备选：0x30EE50 也可 BL writer（条件 r5/r4）
```

`0x306344` 在 `ui_mode==0` 时走 `0x306640`（清屏/plat 调用），**不会**调用 writer。

## 3. 修改文件

| 文件 | 变更 |
|------|------|
| `bridge.c` | `jjfb_install_uimode_writer_hooks`；mrc_init 前安装；`[JJFB_GAME_SELF]` 覆盖日志 |
| `RUN_V55_UIMODE_WRITER_COVERAGE.ps1` | 自然模式运行 |
| `scripts/v55_*.py` | 静态 map + 日志分析 |
| `reports/v55_*.md` | 本报告与运行结果 |

## 4. 运行命令

```powershell
.\RUN_V55_UIMODE_WRITER_COVERAGE.ps1 -Seconds 25
```

## 5. 关键动态日志

```text
[JJFB_START_HANDOFF] ... action=run_host_801_recovery
[JJFB_GAME_SELF] uimode_writer coverage installed ...
[JJFB_801] host mrc_init(0) ret=0
[JJFB_SEND] ARM robotol timer ... RUNNING=1
[JJFB_GAME_SELF] natural_mode=1 ... state=0x0 tick=10
```

覆盖计数（25s）：

| 探针 | 次数 |
|------|------|
| writer ENTER/STORE | 0 |
| 2DADC4 / B70 / 2FC03C | 0 |
| callers 2FECA2/2E4066/305EF4 | 0 |
| alt 30EE50 | 0 |

## 6. 结论

1. **谁写 ui_mode=0x45**：guest `0x2FC418`（静态锁定）。
2. **当前为何不写**：整条上游（含 `2DADC4` 三处 caller）在 handoff 后 25s **从未执行**。
3. 不是 B70 门挡住（门都没走到），而是 **根本没人调用门控 init**。

## 7. 被证伪

- 「timer/`event=0x13`/`0x306344` 会自然写 ui_mode」——否；mode0 分支不写。
- 「mrc_init 会调用 writer」——否；hooks 在 init 前装好，init 期间无命中。

## 8. 当前 blocker

```text
no caller of 2DADC4 / 2FC418 in natural post-handoff run
```

## 9. 下一步（v56）

追谁应调用 `0x2DADC4` 的上游：

1. `0x2FECA2` / `0x2E4066` / `0x305EF4` 所在函数的调用者；
2. 这些调用是否依赖网络 / `_strCom` / 特定 `sendAppEvent` / ERW 门（如 `0x134D`）；
3. 仍禁止 FORCE `0x45`。
