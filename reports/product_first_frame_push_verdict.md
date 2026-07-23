# Product First-Frame Push Verdict

- **run_id:** ffp_validate_20260724_024141_66224
- **mode:** Validate
- **verdict:** EVENT_QUEUE_BOOTSTRAP_FIXED_NEW_BLOCKER
- **runtime:** Gwy+stubs
- **seconds:** 150
- **process_exit:** killed
- **apply_abi:** yes

## Farthest natural milestone

- **farthest:** event_path_a_enqueue_ok
- **last_successful_transaction:** EVENT_PATH_A_ENQUEUE_OK
- **first_unmet_platform_contract:** Path A enqueued after B54 recover; new blocker DSM/cfunction fault @0x94E40 (UI_MODE still 0)

## Event Queue Bootstrap

- **EVENT_QUEUE_OWNER_STORE_DIFFERS_FROM_B54:** yes (`0x2B215C` vs `0x2B23A8`)
- **EVENT_LIST_HEAD_INITIALIZED:** yes (`0x2829D4` via PlatformEventQueue Case E)
- **EVENT_PATH_A_ENQUEUE_OK:** yes (`0x30D2F9` -> `0x101AB` -> `0x2E4D6C` -> `0x312A60`)
- **UI_MODE:** still 0
- **ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION:** no
- **new blocker:** DSM/cfunction mem_fault @0x94E40 r0=0 ENTRY_ARGUMENT

## Event / ABI

- **identity_class:** SAME_UNFINISHED_REQUEST
- **EVENT_TRANSACTION_IDENTITY_CONFIRMED:** yes
- **EVENT_CONTEXT_OBJECT_IDENTIFIED:** yes
- **FAMILY_EVENT_ABI_CONFIRMED:** yes

## Resource / display

- **resource request:** markers wired; auto Resource phase requires state advance (not reached)
- **framebuffer / _DispUpEx / first frame:** no

## Artifacts

- reports/product_event_queue_contexts.csv
- reports/product_event_queue_address_identity.csv
- reports/product_event_queue_write_history.csv
- reports/product_event_queue_object_history.csv
- reports/product_10162_10165_contract.csv
- reports/product_10162_10165_storage_flow.csv
- reports/product_event_list_access.csv
- reports/product_event_list_contract.json
- reports/product_event_queue_initializer_xrefs.csv
- reports/product_event_queue_bootstrap_verdict.md
- reports/product_event_queue_validate.csv
- out/product_event/312a60_list_path_annotated.txt
- out/product_event/312a60_list_path_cfg.dot
- reports/product_event_ack_contract.json
