# v57 Lifecycle Source Coverage 运行结果

- 日志：`/mnt/data/JJFB_v57_LIFECYCLE_SOURCE_COVERAGE_COMPLETE/reports/v57_simulated_missing_sources.log`

## 覆盖计数

| 探针 | 次数 |
|---|---:|
| 0x10102 family 注册 | 1 |
| guest family 发送源 | 1 |
| guest app=0xC0 源 | 0 |
| guest-deferred family 调用 | 0 |
| family dispatcher | 1 |
| family app=0xC0 | 0 |
| 0x10140 tick 注册 | 1 |
| host periodic tick 调用 | 1 |
| 真实 0x30630C entry | 1 |
| robotol EXT dispatcher entry | 0 |
| EXT method 1 | 0 |
| EXT method 5 | 0 |
| 0x303E14 lifecycle command | 0 |
| command 10002 | 0 |
| 10002 -> 2F5390 branch | 0 |
| method5 -> 2F5390 | 0 |
| 注册 callback=2F5405 | 0 |
| callback 2F5404 entry | 0 |
| 2DADC4 gate | 0 |
| ui_mode writer | 0 |
| host EXT code1 | 0 |
| host EXT code5 | 0 |

## guest family app 分布

| app | 次数 |
|---|---:|
| `0x9` | 1 |

## guest family site_lr 分布

| site_lr | 次数 |
|---|---:|
| `0x302C0F` | 1 |

## robotol EXT method 分布

（无）

## lifecycle command 分布

（无）

## 自动判定

- **Callback registration producers are absent: robotol received neither EXT method 1 with command 10002 nor EXT method 5. Separately, family app=0xC0 ingress is absent.**

## 关键解释

- `0x10140` 的真实 handler 是 `0x30630D`（代码 `0x30630C`）；它与 `0x303E14` lifecycle-command dispatcher 不是同一函数。
- callback `0x2F5405` 的自然注册生产者有两条：EXT method 1 + command 10002，或 EXT method 5。
- family app=0xC0 没有在本轮被注入；guest app 分布用于证明现有 family 流量来自哪些真实 guest callsite。
- 本报告不把 periodic `r0=0,r1=0` tick 误判成 lifecycle command 10002。
