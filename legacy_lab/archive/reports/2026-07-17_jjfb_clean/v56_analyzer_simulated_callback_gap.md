# v56 Upstream Trigger Coverage 运行结果

- 日志：`/tmp/v56_sim.log`

## 覆盖计数

| 路径/探针 | 次数 |
|---|---:|
| 事件队列入口/调用 | 0 |
| 事件分派 | 0 |
| 目标事件 5/12 | 0 |
| family dispatcher | 1 |
| family app=0xC0 | 0 |
| 2FEBBC entry | 0 |
| 2FEBBC direct calls | 0 |
| callback registration | 1 |
| callback=2F5404 registration | 1 |
| 2F5404 callback entry | 0 |
| 2F5734 -> 305EB8 | 0 |
| 305EB8 entry | 0 |
| 2DADC4 gate | 0 |
| ui_mode writer | 0 |
| ui_mode store | 0 |

## 自动判定

- **Callback 0x2F5404 was registered but never invoked: host callback scheduler/dispatch is the leading blocker.**

## 解释规则

- 注册了 `0x2F5404` 但 entry 为 0：优先补 host callback scheduler。
- family 一直只有 app=9、没有 app=0xC0：优先追启动 family 命令来源。
- event dispatcher 有活动但没有 5/12：不要再把 `0x13` 当作自然 writer 触发源。
- 一旦命中 `0x2DADC4`，v56 上游任务完成，下一轮只查内部 ERW 门。
