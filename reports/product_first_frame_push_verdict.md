# Product First-Frame Push Verdict

- **run_id:** ffp_event_20260724_042300_25225
- **mode:** Event
- **verdict:** EVENT_QUEUE_CONSUMER_REACHED
- **runtime:** Gwy+stubs
- **seconds:** 90
- **process_exit:** killed
- **apply_abi:** yes
- **ok_callback_returns:** 3

## Farthest natural milestone

- **farthest:** event_list_node_linked
- **last_successful_transaction:** EVENT_PATH_A_ENQUEUE_OK / EVENT_NODE_CONSUMED
- **first_unmet_platform_contract:** post-drain gates 15D=0 and B71=0 block 2DADC4→2FC418 (POST_DRAIN_GATE_15D_B71_134D)

## Ack path (live)

- Drain runs; `C76=0` → alt `0x2DC848` → still frees via `0x30BC40` (destructor, not ack)
- Gate sample at `0x305EC2`: **15D=0 B71=0 134D=0** → no `2DADC4` / `2FC418`
- Next: natural writers for 15D (`0x30CBBC`) and B71 (`0x2DC4D8`/`0x2E2520`) — no flag/UI poke

## Event / ABI

- **guest request samples:** 3
- **EVENT_TXN ACCEPT:** 3
- **FAMILY DELIVER:** 9
- **SUPPRESS:** 0
- **identity_class:** SAME_UNFINISHED_REQUEST
- **EVENT_LIST_HEAD_INITIALIZED:** yes
- **EVENT_PATH_A_ENQUEUE_OK:** yes
- **NODE_94E40_FUNCTION_IDENTIFIED:** yes
- **NODE_FIRST_CAUSAL_ZERO_FOUND:** yes
- **EVENT_LIST_NODE_LINKED:** yes
- **EVENT_LIST_COUNT_CHANGED:** yes
- **EVENT_QUEUE_NONEMPTY_VISIBLE:** yes
- **EVENT_QUEUE_CONSUMER_TRIGGER:** yes
- **EVENT_QUEUE_CONSUMER_ENTER:** yes
- **EVENT_NODE_CONSUMED:** yes
- **NODE_ALLOCATION_RETURN_VALID:** yes
- **EVENT_PATH_A_ENQUEUE_COMPLETE:** no
- **fault_at_0x94E40:** no
- **EVENT_TRANSACTION_IDENTITY_CONFIRMED:** yes
- **EVENT_CONTEXT_OBJECT_IDENTIFIED:** yes
- **EVENT_CONTEXT_OWNER_CONFIRMED:** yes
- **EVENT_CONTEXT_LIFETIME_CONFIRMED:** yes
- **FAMILY_EVENT_ABI_CONFIRMED:** yes
- **FAMILY_HANDLER_OUTPUT_WRITES_OBSERVED:** yes
- **real state change:** no
- **callback signature change:** no

## Resource / display

- **resource request:** no
- **resource read:** no
- **framebuffer modified:** no
- **_DispUpEx called:** no
- **first frame:** no
- **hwnd_visible:** no

## Gates

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | yes |
| ROBOTOL_INIT_RETURN_ZERO | yes |
| EVENT samples / identity | yes |
| FAMILY DELIVER | yes |
| ROBOTOL_STATE_ADVANCED | no |
| FIRST_NATURAL_REFRESH | no |
| FRAMEBUFFER_NONEMPTY | no |
| HWND_VISIBLE | no |

## Artifacts

- **manifest:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_manifest_ffp_event_20260724_042300_25225.txt
- **csv_requests:** missing
- **csv_10165:** missing
- **csv_samples:** missing
- **csv_mem:** missing
- **abi_manifest:** missing
- **forbidden_hits:** none

## Discipline

- Event Round A: collect identity + 10165 object + handler ABI (no one-shot default)
- Event Round B: `-ApplyAbi` once after provenance
- Resource/Validate auto-continue when state advances / display predecessor reached
- Forbidden: fixed PC, Robotol flag writes, fabricated 10165, forced DispUpEx, E9/E10A
