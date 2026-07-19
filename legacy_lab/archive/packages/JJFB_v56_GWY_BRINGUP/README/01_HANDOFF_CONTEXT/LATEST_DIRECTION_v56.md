# 最新路线：v56 GWY Bring-up（白屏）

## 已锁定

- v53：alias + MR_IGNORE → host 6/8/0 → timer。
- v54：GWY 默认 NO FORCE；自然 ui_mode=0。
- v55：自然 writer = `0x2FC418`；上游无人调用。
- v56：
  - `_DispUpEx` 可 present。
  - bring-up：dims + C44 + `2FC03C`（→0x3）+ **guest `2FC418`（→0x45）** + 暗底。
  - 之后进入 `0x2EF86C` splash 路径；无 FORCE mem-write。

## 仍属探针（非正式主线）

```text
host 主动 call 0x2FC418
暗底 host present
```

正式主线仍是：自然 caller 触发 `2DADC4 → 2FC03C → 2FC418`，或补齐使其被触发的 GWY/平台上下文。

## v57 唯一任务

```text
1) 为何 splash DrawRect 在 x=270（屏外）？修 clip/坐标系，让原资源可见。
2) 并行：2DADC4 自然 caller（事件/_strCom/net/ERW 门）。
```

禁止 FORCE ui_mode / AC8 / progress 作为正式方案。

## 证据

```text
reports/v56_gwy_bringup_implementation.md
reports/v56_gwy_bringup_run_result.md
logs/v56_gwy_bringup_stdout.txt
RUN_V56_GWY_BRINGUP.ps1
```
