# v57 Lifecycle Source Coverage 实施报告

## 1. 目标

承接本机 v56 结果，只追两个缺口：

1. 谁应向已注册的 family handler 输入 `app=0xC0`；
2. 为什么 `0x3054A4` 从未注册 callback `0x2F5405`。

本轮只覆盖，不注入任何 lifecycle 命令。

## 2. 关键静态修正

`0x10140` 注册的函数指针为 `0x30630D`，真实代码入口 `0x30630C`。`0x303E14` 不是 tick handler，而是 robotol EXT method 1 输入的 lifecycle-command dispatcher。

## 3. callback 注册的两个自然生产者

```text
robotol EXT method 1
  -> 0x304B30
  -> payload[0] command
  -> 0x303E14
  -> command=10002
  -> 0x304418
  -> 0x2F5390
  -> 0x3054A4(callback=0x2F5405)

robotol EXT method 5
  -> 0x304B5A
  -> 0x3053B8
  -> 0x2F5390
  -> 0x3054A4(callback=0x2F5405)
```

## 4. family C0 的输入边界

`0x30D300` 没有直接 BL caller，而通过 `0x10102` 注册为 family handler。v57 对每次 guest `sendAppEvent(0x1E209, app, ...)` 记录 `app`、wrapper LR 与真实 `site_lr`，并把 host 对 family handler 的调用标记为 `source=guest_deferred_1e209`。

因此本轮可以区分：

- guest 自己发出的 family 消息；
- 平台本应主动送入、但当前 host 未实现的 lifecycle family 消息。

## 5. 动态探针

- `0x304914/0x304932/0x304998`：平台注册契约；
- `0x304AEC/0x304B02`：robotol EXT dispatcher；
- `0x304B30/0x303E14/0x304406/0x304418`：method 1 + command 10002；
- `0x304B5A/0x3053B8/0x3053BA`：method 5；
- `0x30630C/0x306344`：真实 10140 tick handler；
- guest family source 与 site LR；
- 既有 family/callback/2DADC4/writer 探针全部保留。

## 6. 运行

```powershell
.\RUN_V57_LIFECYCLE_SOURCE_COVERAGE.ps1 -Seconds 25
```

报告：`reports57_lifecycle_source_run_result.md`。
