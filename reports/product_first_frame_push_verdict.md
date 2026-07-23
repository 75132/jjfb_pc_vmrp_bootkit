# Product First-Frame Push Verdict

- **run_id:** ffp_validate_20260724_033705_36171
- **mode:** Validate
- **verdict:** EVENT_NODE_CONSTRUCTION_COMPLETE
- **runtime:** Gwy+stubs
- **seconds:** 150
- **process_exit:** killed
- **apply_abi:** yes
- **ok_callback_returns:** 8

## Farthest natural milestone

- **farthest:** resource_request
- **last_successful_transaction:** EVENT_PATH_A_ENQUEUE_OK
- **first_unmet_platform_contract:** Path A node construction complete; UI_MODE still 0 / state not advanced

## Event / ABI

- **guest request samples:** 8
- **EVENT_TXN ACCEPT:** 8
- **FAMILY DELIVER:** 16
- **SUPPRESS:** 0
- **identity_class:** SAME_UNFINISHED_REQUEST
- **EVENT_LIST_HEAD_INITIALIZED:** yes
- **EVENT_PATH_A_ENQUEUE_OK:** yes
- **NODE_94E40_FUNCTION_IDENTIFIED:** yes
- **NODE_FIRST_CAUSAL_ZERO_FOUND:** yes
- **EVENT_LIST_NODE_LINKED:** yes
- **NODE_ALLOCATION_RETURN_VALID:** yes
- **EVENT_PATH_A_ENQUEUE_COMPLETE:** yes
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

- **resource request:** yes
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

- **manifest:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_manifest_ffp_validate_20260724_033705_36171.txt
- **csv_requests:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_event_requests.csv
- **csv_10165:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_10165_objects.csv
- **csv_samples:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_guest_request_samples.csv
- **csv_mem:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_handler_mem.csv
- **abi_manifest:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_family_abi_manifest.json
- **forbidden_hits:** none

## Discipline

- Event Round A: collect identity + 10165 object + handler ABI (no one-shot default)
- Event Round B: `-ApplyAbi` once after provenance
- Resource/Validate auto-continue when state advances / display predecessor reached
- Forbidden: fixed PC, Robotol flag writes, fabricated 10165, forced DispUpEx, E9/E10A
