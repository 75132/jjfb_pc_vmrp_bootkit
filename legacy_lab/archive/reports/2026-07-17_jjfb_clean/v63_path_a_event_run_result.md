# v63 Path A Event 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v63_path_a_event_stdout.txt`

## 1. 目标

- 证伪：`MR_MOUSE_UP` 不是 Path C 的 B71 启动器（会置 134D=2）。
- 探针：单次 `MR_MENU_RETURN(5)` 或 `MR_MOUSE_MOVE(12)` → `2E4040` → `2DADC4`。
- 禁止 FORCE ui_mode / C0 inject / host UI / 事件码盲扫。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| PATH_A PROBE | 1 |
| mrc_event ret | 1 |
| Path A event at 2E2520 | 0 |
| MOUSE_UP note | 0 |
| caller_2E4066 | 0 |
| gate_init_2DADC4 | 0 |
| gate_B70_check | 0 |
| BL_2FC03C | 0 |
| uimode_writer | 0 |
| ok_to_2DADC4 | 0 |
| fail_B71 (sample) | 16 |
| FORCE ui | 1 |

## 3. 关键日志

```text
[JJFB_V63_PATH_A] contract=PROBE_MENU_RETURN_once_Path_A no_FORCE no_C0_inject
[JJFB_V63_PATH_A] contract=opt_in_MENU_RETURN_or_MOUSE_MOVE_once via JJFB_PATH_A_EVENT_ONCE=5|12 (PROBE; not FORCE ui_mode)
[JJFB_V63_PATH_A] PROBE once code=5 (MR_MENU_RETURN) tick=12 15D=1 (targets 2E4040→2DADC4; not FORCE ui_mode / not C0 / not blind scan)
[JJFB_V63_PATH_A] mrc_event(5) ret=0
```

## 4. 结论

- 已发出 Path A PROBE，但未看到 2E2520/2E4066。

## 5. blocker / 下一步

- next: 查 mrc_event(code=1) 是否进入 robotol 事件队列。
