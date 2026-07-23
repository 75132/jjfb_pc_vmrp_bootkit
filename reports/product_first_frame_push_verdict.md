# Product First-Frame Push Verdict

- **run_id (Round B):** ffp_event_20260724_010820_81327
- **run_id (Round A):** ffp_event_20260724_010631_8498
- **mode:** Event (P6 complete — two live runs)
- **verdict:** P6_EVENT_PROVENANCE_MAPPED_ABI_APPLIED_NO_ADVANCE
- **runtime:** Gwy+stubs
- **apply_abi Round B:** yes

## Farthest natural milestone

- **farthest:** event_identity_confirmed + stack ABI delivered
- **last_successful_transaction:** EVENT_TRANSACTION_IDENTITY_CONFIRMED / FAMILY_HANDLER_ACK_CONTRACT
- **first_unmet_platform_contract:** case-9 callee does not consume 10165 context → Robotol state unchanged

## P6 Round A (observe)

| Finding | Result |
|---------|--------|
| 8 guest `sendAppEvent(0x1E209,9)` samples | yes — identical regs/stack/ctx/digest |
| Identity class | **SAME_UNFINISHED_REQUEST** |
| 10165 object | `0x6AD11C` size `0xE200`, handler stub `0x30D2F9` |
| Owner store | `ER_RW+…` @ `0x2B215C` → **EVENT_CONTEXT_OWNER_CONFIRMED** |
| Baseline delivery | `r0=9 r1=0x1E209 r2=r3=0`; stack leftovers `0x248/0x280004` |
| State change | no |

**Fork decision:** same object/token + unchanged ER_RW → guest is polling one unfinished request. One-shot is *semantically* reasonable as a diagnostic, but the missing piece is completion/ABI consumption — not “66 independent requests”.

## P6 Round B (one ApplyAbi merge)

Delivered:

```text
r0=9  r1=0x1E209  r2=0x6AD11C  r3=0
stack0=0x6AD11C  stack1=0x30D2F9
```

Observed at handler prologue loads:

```text
sp+32 = 0x6AD11C   sp+36 = 0x30D2F9
```

| Gate | Status |
|------|--------|
| EVENT_TRANSACTION_IDENTITY_CONFIRMED | yes |
| EVENT_CONTEXT_OBJECT_IDENTIFIED | yes |
| EVENT_CONTEXT_OWNER_CONFIRMED | yes |
| FAMILY_HANDLER_ACK_CONTRACT (stack reads) | yes |
| FAMILY_EVENT_ABI_CONFIRMED (ctx touched) | **no** — `ctx_touch=0` |
| EVENT_CONTEXT_WRITES / LIFETIME | **no** |
| ROBOTOL_STATE_DIGEST_CHANGED | **no** |
| ROBOTOL_CALLBACK_SIGNATURE_CHANGED | **no** |

## Blocker migration (do not open P6c)

```text
Platform now supplies recovered context + stack ABI
→ case 9 still returns 0 without reading/writing 0x6AD11C
→ ER_RW digest stays 0x25D6A3D6
→ guest reissues identical unfinished request
```

Next causal target (still P6-class, before P7 resources):

```text
0x30D301 case 9 @ 0x30E1A0
→ BL 0x305E09 (observed earlier with r0=event)
→ what does 0x305E09 require beyond pointer presence?
→ platform write-into 10165 object / ack field / queue slot?
```

Hypothesis retained (not proven): platform must **publish completion bytes into the 10165 object** (or a sibling 10162 object @ `0x69EF14` / store `0x2B2158`), not only invoke the family switch with the pointer.

## Resource / display

- resource request: no
- framebuffer / `_DispUpEx` / first frame: no
- HWND: correctly deferred

## Gates

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | yes |
| ROBOTOL_INIT_RETURN_ZERO | yes |
| EVENT samples / identity | yes |
| FAMILY DELIVER with ApplyAbi | yes |
| ROBOTOL_STATE_ADVANCED | no |
| FIRST_NATURAL_REFRESH | no |

## Artifacts

- Runner: `RUN_PRODUCT_FIRST_FRAME_PUSH.ps1` (`Event` / `Resource` / `Validate`)
- Service: `src/platform/platform_event_service.c`
- Probe: `src/runtime/product_first_frame_push.c`
- CSV: `reports/product_ffp_event_requests.csv`, `product_ffp_10165_objects.csv`, `product_ffp_guest_request_samples.csv`
- ABI JSON: `reports/product_ffp_family_abi_manifest.json`
- Logs: `logs/product_ffp_stdout.txt`

## Discipline note

P6 live budget used: **2/2**. Round B did not clear the strong gate (`ROBOTOL_STATE_*`). Per plan: do **not** invent P6c — next work must prove `0x305E09` / 10165 **write-ack** contract from data flow, then one more Apply if evidence warrants (or fold into Validate once state moves and auto-enter Resource).
