# v65 101AB Fill → B54 Enqueue 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v65_101ab_stdout.txt`

## 1. 目标

- 补齐 plat `0x101AB`：给 `30D24C` 填 `>c>i` 可 unpack 的缓冲。
- 让 `2E4D6C(r0, r1)` 的 `r1!=0`，真正 `312A60` push 到 B54。
- 期望：drain → `2E2520` → Path A `2DADC4`。
- 禁止 FORCE ui_mode / C0 inject / host UI。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| register 10165 | 1 |
| ENQ PROBE once | 1 |
| 101AB fill | 1 |
| 101AB unhandled | 0 |
| site 30D24C | 1 |
| site 2E4D6C | 1 |
| 2E4D6C r1==0 | 0 |
| 2E4D6C r1!=0 | 1 |
| 2E4D6C peek | 1 |
| 312A60 push | 34 |
| 312A60 from 2E4D6C | 1 |
| 10132 alloc FAIL | 0 |
| UNMAP crash | 0 |
| drain 2DC80C | 13 |
| 2E2520 EVENT | 2 |
| Path A TARGETS_2DADC4 | 2 |
| caller_2E4066 | 0 |
| gate_init_2DADC4 | 0 |
| uimode_writer | 0 |
| FORCE ui | 1 |

## 3. 关键日志

```text
[JJFB_V65_101AB] contract=fill_buf_for_30D24C_unpack_gt_c_i no_FORCE no_C0_inject
[JJFB_V65_101AB] fill buf=0x6AEE6C out=0x27FD08 type=2 payload_len=20 hdr_be=5 body_be=12 ev_be=5 (ret=0 → unpack@buf → 2E4D6C; 15C cursor-aware)
[JJFB_V65_ENQ] 2E4D6C buf=0x6AEE71 len/r1=0x14 B54_head=0x2A8394
[JJFB_V65_ENQ] 2E4D6C peek 00000005 0000000C 00050000 00050000
[JJFB_V65_ENQ] 312A60 push #1 list=0x2A8394 item=0x6BD770 lr=0x2E4EF3
[JJFB_V65_ENQ] 312A60 push #2 list=0x6BD708 item=0x6BD7B0 lr=0x2F694F
[JJFB_V65_ENQ] 312A60 push #3 list=0x6BD708 item=0x6BD7E0 lr=0x2F694F
[JJFB_V65_ENQ] 312A60 push #4 list=0x6BD708 item=0x6BD810 lr=0x2F694F
[JJFB_V65_ENQ] 312A60 push #5 list=0x6BD708 item=0x6C6120 lr=0x2F694F
[JJFB_V65_ENQ] 312A60 push #6 list=0x6BD708 item=0x6D39A0 lr=0x2F694F
[JJFB_V65_ENQ] 312A60 push #7 list=0x6BD708 item=0x6D39D0 lr=0x2F694F
[JJFB_V65_ENQ] 312A60 push #8 list=0x6BD708 item=0x6D4B38 lr=0x2F694F
[JJFB_V64_ENQ] contract=PROBE_10165_enqueue_once_Path_A no_FORCE no_C0_inject
[JJFB_V64_ENQ] contract=opt_in_10165_enqueue_once via JJFB_V64_ENQUEUE_ONCE=1 (30D2F8→B54; not FORCE)
[JJFB_V64_ENQ] note 0x10162 code=0x30D249 size=0xE200 (sibling alloc)
[JJFB_V64_ENQ] register 0x10165 enqueue_handler=0x30D2F9 size=0xE200 (targets 30D24C→2E4D6C→B54→2DC80C→2E2520 Path A)
[JJFB_V64_ENQ] PROBE once handler=0x30D2F9 r0=0x6AEE6C r1=0x6A0C64 tick=12 15D=1 (30D2F8→2E4D6C→B54; not FORCE ui_mode / not C0 / not mrc_event)
[JJFB_V64_ENQ] site=0x30D2F8 #1 r0-r3=0x6AEE6C,0x6A0C64,0x0,0xE7C lr=0x80000
[JJFB_V64_ENQ] site=0x30D24C #2 r0-r3=0x6AEE6C,0x6A0C64,0x0,0xE7C lr=0x30D2FF
[JJFB_V64_ENQ] site=0x2E4D6C #3 r0-r3=0x6AEE71,0x14,0x0,0x30D2D3 lr=0x30D2DD
[JJFB_V64_ENQ] PROBE ret=0 (expect next 2DC80C to reach 2DC8D4/2E2520)
[JJFB_V56_EVENT] #1 site=0x2DC8D4 ptr=0x6BD770 code=5(0x5) p0=0x6BD758 p1=0xA p2=0x0 p3=0x10 lr=0x312ABB TARGETS_2DADC4
[JJFB_V56_EVENT] #2 site=0x2E2520 ptr=0x6BD770 code=5(0x5) p0=0x6BD758 p1=0xA p2=0x0 p3=0x10 lr=0x2DC8D9 TARGETS_2DADC4
```

## 4. 结论

- B54 已有事件进 2E2520；未到 gate_init。

## 5. blocker / 下一步

- next: 查 2E4040 前置（B58 等）。
