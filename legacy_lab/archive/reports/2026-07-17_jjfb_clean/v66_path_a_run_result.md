# v66 Path A 2E4040→2DADC4 运行结果

- 日志：`C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\v66_path_a_stdout.txt`

## 1. 目标

- 证伪：body 以非 0 BE u32 开头会让 `2F68E4` 死循环。
- 补齐：body = BE `0` 终止 → `2F68E4` 立即返回 → `2E4066` → `2DADC4`。
- 禁止 FORCE ui_mode / C0 inject / host UI。

## 2. 计数

| 探针 | 次数 |
|---|---:|
| V66 101AB fill | 1 |
| 312A60 from 2E4D6C | 1 |
| EVENT code=5 | 142 |
| enter 2E4040 | 71 |
| enter 2F68E4 | 71 |
| leave 2F68E4 | 71 |
| caller_2E4066 | 71 |
| gate_init_2DADC4 | 71 |
| uimode_writer | 0 |
| B58 spam from 2F694F | 0 |
| FORCE ui | 1 |

## 3. 关键日志

```text
[JJFB_V66_101AB] fill buf=0x6AEE6C out=0x27FD08 type=2 payload_len=14 hdr_be=5 body_sz=6 u16=5 body=BE(-1) (2F68E4 terminate → 2E4066 → 2DADC4)
[JJFB_V66_PATH_A] contract=2F68E4_body_BE0_terminate_to_2E4066 no_FORCE no_C0_inject
[JJFB_V66_PATH_A] enter_2E4040 #1 r0=0x2 r4_ev=0x6BD768 B5C=0x0 B60=0x0 B58=0x6BD708 lr=0x2DC8D9
[JJFB_V66_PATH_A] enter_2F68E4 #1 list=0x6BD708 ev=0x6BD768 B5C=0x6BD758 B60=0x0 lr=0x2E4063
[JJFB_V66_PATH_A] leave_2F68E4 #1 count_r5=0 lr=0x2F68FF (expect → 2E4062 → 2E4066 → 2DADC4)
[JJFB_V66_PATH_A] enter_2E4040 #2 r0=0x2 r4_ev=0x6BD768 B5C=0x6BD7A8 B60=0x4 B58=0x6BD708 lr=0x2DC8D9
[JJFB_V66_PATH_A] enter_2F68E4 #2 list=0x6BD708 ev=0x6BD768 B5C=0x6BD758 B60=0x0 lr=0x2E4063
[JJFB_V66_PATH_A] leave_2F68E4 #2 count_r5=0 lr=0x2F68FF (expect → 2E4062 → 2E4066 → 2DADC4)
[JJFB_V66_PATH_A] enter_2E4040 #3 r0=0x2 r4_ev=0x6BD768 B5C=0x6BD7A8 B60=0x4 B58=0x6BD708 lr=0x2DC8D9
[JJFB_V56_EVENT] #1 site=0x2DC8D4 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x312ABB TARGETS_2DADC4
[JJFB_V56_EVENT] #2 site=0x2E2520 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x2DC8D9 TARGETS_2DADC4
[JJFB_V56_EVENT] #3 site=0x2DC8D4 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x312ABB TARGETS_2DADC4
[JJFB_V56_EVENT] #4 site=0x2E2520 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x2DC8D9 TARGETS_2DADC4
[JJFB_V56_EVENT] #5 site=0x2DC8D4 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x312ABB TARGETS_2DADC4
[JJFB_V56_EVENT] #6 site=0x2E2520 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x2DC8D9 TARGETS_2DADC4
[JJFB_V56_EVENT] #7 site=0x2DC8D4 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x312ABB TARGETS_2DADC4
[JJFB_V56_EVENT] #8 site=0x2E2520 ptr=0x6BD768 code=5(0x5) p0=0x6BD758 p1=0x4 p2=0x0 p3=0x10 lr=0x2DC8D9 TARGETS_2DADC4
caller_2E4066 #1 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
caller_2E4066 #2 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
caller_2E4066 #3 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
caller_2E4066 #4 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
caller_2E4066 #5 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
caller_2E4066 #6 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
caller_2E4066 #7 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
caller_2E4066 #8 lr=0x2F68FF -> 2DADC4 B58=0x6BD708
gate_init_2DADC4 #1 lr=0x2E406B ui_mode=0x0 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
gate_init_2DADC4 #2 lr=0x2E406B ui_mode=0x3 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
gate_init_2DADC4 #3 lr=0x2E406B ui_mode=0x3 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
gate_init_2DADC4 #4 lr=0x2E406B ui_mode=0x3 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
gate_init_2DADC4 #5 lr=0x2E406B ui_mode=0x3 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
gate_init_2DADC4 #6 lr=0x2E406B ui_mode=0x3 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
gate_init_2DADC4 #7 lr=0x2E406B ui_mode=0x3 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
gate_init_2DADC4 #8 lr=0x2E406B ui_mode=0x3 B70=0 B58=0x6BD708 DB0=0x0 f134d=0
```

## 4. 结论

- 已进 gate_init_2DADC4；下一 blocker 在 B70/writer。

## 5. blocker / 下一步

- next: 看 gate_B70 / uimode_writer。
