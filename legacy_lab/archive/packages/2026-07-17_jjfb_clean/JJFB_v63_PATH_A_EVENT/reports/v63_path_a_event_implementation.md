# v63 Path A Event Probe — Implementation

## 1. goal

v62：`305EB8` 卡在 `B71==0`；候选 `MOUSE_UP→2DC4D8` 可疑。

本轮：

```text
1) 静态确认 Path A / MOUSE_UP 语义
2) PROBE: 单次 helper mrc_event(5) 是否能进 2E2520→2DADC4
```

禁止 FORCE ui_mode / C0 inject / host UI / 事件码盲扫。

## 2. modified files

| 文件 | 作用 |
|------|------|
| `bridge.c` | `JJFB_PATH_A_EVENT_ONCE` PROBE；`jjfb_robotol_inject_mrc_event`；V63 日志 |
| `scripts/v63_static_b71_event_path.py` | 静态图 |
| `scripts/v63_analyze_path_a_event_log.py` | 分析 |
| `RUN_V63_PATH_A_EVENT.ps1` | 跑测 |
| `reports/v63_b71_event_path_static_map.md` | 静态结论 |

## 3. new env vars

```text
JJFB_PATH_A_EVENT_ONCE=5|12|0   # PROBE only; 0=off
```

## 4. run command

```powershell
.\RUN_V63_PATH_A_EVENT.ps1 -Seconds 25 -SkipResourceCopy
```

## 5. key logs

```text
[JJFB_V63_PATH_A] PROBE once code=5 (MR_MENU_RETURN) tick=12 15D=1
[JJFB_V63_PATH_A] mrc_event(5) ret=0
drain_2DC80C ×441
V56_EVENT / caller_2E4066 / gate_init_2DADC4 = 0
```

## 6. conclusion

1. **MOUSE_UP 证伪**：`15D==1` 时写 `B71=1` **且** `134D=2`，与 `305EB8` 要求 `134D==0` 冲突。
2. **Path A 静态成立**：`MENU_RETURN(5)` / `MOUSE_MOVE(12)` → `2E4040` → `BL 2DADC4`（绕过 B71）。
3. **投递契约未通**：`helper code=1` + event struct **ret=0 但不进** `2E2520`；队列 drain 空转。  
   `code=1` 实际走到 `303E14`，不是 Mythroad 事件分发进 `2E2520` 的路径。

## 7. disproved assumptions

- ~~用 `MR_MOUSE_UP` 置 B71 即可开 Path C~~
- ~~robotol `helper code=1` = 投递 MR 事件进 `2E2520` 队列~~

## 8. current blocker

Path A 目标地址已知，但 **host→robotol 事件入队契约** 未知（`2DC8D4` 无直接 BL 调用者，或为函数指针）。

## 9. next minimal task

v64：弄清谁入队 / 谁调用 `2DC8D4`/`2E7B9E`→`2E2520`（DSM `mr_event`、sendAppEvent、函数指针表）；用正确契约投递一次 Path A。仍禁止 FORCE / C0 / host UI / 盲扫。
