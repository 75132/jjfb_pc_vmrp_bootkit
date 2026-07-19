# v56 Upstream Trigger Coverage 实施报告

## 目标

只追 v55 已锁定的 `0x2DADC4` 三条上游，动态回答哪一种自然触发源缺失。

```text
禁止 FORCE ui_mode=0x45
禁止 AC8 / progress driver
禁止人为注入 event 5/12 或 family app=0xC0
```

## 动态覆盖

1. **事件路径**：`2DC80C/2DC8D4/2E7B7C/2E7B9E/2E2520`，记录完整 event struct，并标记 code 5/12。
2. **启动/重置路径**：`2FEBBC` 及 14 个直接 callsite；额外记录 `30D300` family app/code，标记 app=0xC0。
3. **回调路径**：`2F5390/2F53AC/30D128/3054A4` 注册点，`2F5404` 回调 entry，`2F5734` tail，`305EB8` gate entry。
4. 保留 v55 的 `2DADC4/B70/2FC03C/2FC418` 最终探针。

## 纯自然运行

v56 设置：

```text
JJFB_FORCE_UI_MODE=0
JJFB_FORCE_SPLASH_NUDGE=0
JJFB_DISABLE_MRC_EVENT0_INJECT=1
```

旧的 tick=5 synthetic `mrc_event(0,0,0)` 被禁用，避免污染覆盖结果。v56 不主动生成任何触发事件。

## 预期分流

- `callback registration target > 0` 且 `2F5404 entry = 0`：下一 blocker 是 host callback scheduler/dispatch。
- family 活跃但 `app=0xC0 = 0`：启动 family 命令未被产生/转发。
- event 分派活跃但 code 5/12 = 0：自然事件源未出现；`0x13` 已被排除。
- 命中 `2DADC4` 但不写 0x45：上游完成，下一轮回到 B70/B58/DB0 内部门。

## 运行

```powershell
.\RUN_V56_UPSTREAM_TRIGGER_COVERAGE.ps1 -Seconds 25
```

结果：

```text
reports56_upstream_trigger_run_result.md
logs56_upstream_trigger_stdout.txt
```
