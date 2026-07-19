# v64 10165 Enqueue 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v64_enqueue_stdout.txt`

## 1. 目标

- 证伪：helper `mrc_event(5)` 不入 B54（v63）。
- 补齐：host 保存并调用 `0x10165` 注册的 `0x30D2F9`（→`30D24C`→`2E4D6C`→B54）。
- 期望：timer drain `2DC80C` → `2E2520` → Path A `2DADC4`。
- 禁止 FORCE ui_mode / C0 inject / host UI。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| register 10165 | 1 |
| ENQ PROBE once | 1 |
| PROBE skipped | 0 |
| PROBE ret | 1 |
| site 30D2F8 | 1 |
| site 30D24C | 1 |
| site 2E4D6C | 1 |
| drain 2DC80C | 414 |
| 2E2520 EVENT | 0 |
| Path A code 5/12 | 0 |
| caller_2E4066 | 0 |
| gate_init_2DADC4 | 0 |
| uimode_writer | 0 |
| FORCE ui | 1 |

## 3. 关键日志

```text
[JJFB_V64_ENQ] contract=PROBE_10165_enqueue_once_Path_A no_FORCE no_C0_inject
[JJFB_V64_ENQ] contract=opt_in_10165_enqueue_once via JJFB_V64_ENQUEUE_ONCE=1 (30D2F8→B54; not FORCE)
[JJFB_V64_ENQ] note 0x10162 code=0x30D249 size=0xE200 (sibling alloc)
[JJFB_V64_ENQ] register 0x10165 enqueue_handler=0x30D2F9 size=0xE200 (targets 30D24C→2E4D6C→B54→2DC80C→2E2520 Path A)
[JJFB_V64_ENQ] PROBE once handler=0x30D2F9 r0=0x6AEE6C r1=0x6A0C64 tick=12 15D=1 (30D2F8→2E4D6C→B54; not FORCE ui_mode / not C0 / not mrc_event)
[JJFB_V64_ENQ] site=0x30D2F8 #1 r0-r3=0x6AEE6C,0x6A0C64,0x0,0xE7C lr=0x80000
[JJFB_V64_ENQ] site=0x30D24C #2 r0-r3=0x6AEE6C,0x6A0C64,0x0,0xE7C lr=0x30D2FF
[JJFB_V64_ENQ] site=0x2E4D6C #3 r0-r3=0x6AEE72,0x0,0x0,0x30D2D3 lr=0x30D2DD
```

## 4. 结论

- 2E4D6C 入队已跑，但后续 drain 未把事件送到 2E2520。

## 5. blocker / 下一步

- next: 查 2E4D6C 返回值 / B54 头 / 2DC80C 过滤分支。
