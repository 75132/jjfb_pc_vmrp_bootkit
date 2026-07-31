# P11 — 0x10102 Registration Contract (family 0x1E200 / Case-9)

## Verdict

**Natural registration recovered.** Family `0x1E200` registers handler `0x30D311` (Thumb) under `robotol.ext`, with `er_rw=R9=0x2B1868`. Host deliver uses **R0=app, R1=event** only; `product_ffp_apply_abi=0`.

## Build / ABI lock

| Field | Value |
|-------|-------|
| run_id | `p11_20260801_043903_31840` |
| source_commit | `568ed0f1548525ee83bbf8b13d7de7d33446a747` |
| source_tree_clean | **False** (P11 sources dirty at evidence build) |
| product_default_return_mode | `direct_lr` |
| product_ffp_apply_abi | `0` |
| JJFB_PRODUCT_EVENT_CONTRACT | `0` |

## Registration row (CSV)

Source: `reports/p11_10102_registration_trace.csv`

| Field | Value | Notes |
|-------|-------|-------|
| family | `0x1E200` | register arg R1 |
| handler | `0x30D311` | register arg R2; **not** historical `0x30D301` |
| owner_module | `robotol.ext` | module_id=3, generation=3 |
| er_rw / R9 | `0x2B1868` | owner RW base |
| code_base | `0x2D8E04` | |
| code_size | `0x3DE3C` | |
| caller_pc / LR | `0x304599` | plat wrapper return |
| R0 | `0x10102` | plat opcode |
| R1 | `0x1E200` | family |
| R2 | `0x30D311` | **handler address**, not deliver-time ctx |
| R3 | `0x0` | |
| status_ret | `1` | register accepted |

## Host deliver ABI (observed)

From `logs/p11_natural_case9_stdout.txt`:

```
[PLATFORM_FAMILY_EVENT] op=DELIVER event=0x1E209 app=0x9 handler=0x30D311 r2=0x0 r3=0x0
[EVENT_DELIVER_ENTER] r0=0x9 r1=0x1E209 r2=0x0 r3=0x0 stack0=0x0 stack1=0x0 ctx=0x6AD12C
```

| Slot | Deliver value | Meaning |
|------|---------------|---------|
| R0 | `0x9` | app id (Case-9) |
| R1 | `0x1E209` | family event code |
| R2 | `0x0` | apply_abi off — **not** registration handler |
| R3 | `0x0` | |
| stack0 / stack1 | `0x0` / `0x0` | no host stack-arg apply |
| ctx | `0x6AD12C` | platform event-service object (near historical `0x6AD11C`) |

## Classification

Handler `0x30D311` is a **direct Thumb family dispatcher** in `robotol.ext` (prologue `PUSH {R4-R6,LR}` at `0x30D310`), not a host-side wrapper. Registration R2 is the code entry; deliver R2 stays 0 under current product ABI.
