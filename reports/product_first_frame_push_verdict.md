# Product First-Frame Push Verdict

- **run_id:** ffp_event_20260724_155025_4016
- **mode:** Event
- **verdict:** EVENT_QUEUE_CONSUMER_REACHED
- **runtime:** Gwy+stubs
- **seconds:** 90
- **process_exit:** killed
- **apply_abi:** yes
- **ok_callback_returns:** 3

## Provenance

- **git_commit:** d3c99d3225c64384f213ac17524d0d6722825070
- **git_tree:** f1f2b117fc6820313637ede984839e1e089557dd
- **git_dirty:** yes
- **runner_path:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\RUN_PRODUCT_FIRST_FRAME_PUSH.ps1
- **runner_sha256:** aac32efa1e4277d69d467f97e8ccfd92c558003922b74dc4e7630f589965245e
- **main_exe:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\main.exe
- **main_exe_sha256:** fdf964de2a580230cb01e58d5b463313ae766e7f412882cd1b19fa197946011a
- **gwy_launcher_sha256:** bbbe41e0354e399849d6b8672410beff9b0ece27352a50e9b0bf2ca3fb16f6ce
- **stdout:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\product_ffp_stdout.txt
- **stdout_sha256:** b174e5ff877947b7d4000db56e5f57e10b6755dd9f335b9a3357214e01d749b5
- **stderr:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\product_ffp_stderr.txt
- **stderr_sha256:** c209a4d83d6bb94fa4cb3a0d9786f3f1cd25cc9296f7209e9f1adc1cf4f2ba19
- **hashes_sidecar:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_hashes_ffp_event_20260724_155025_4016.txt
- **verdict_sha256:** d49939730f8de34e585803b8281e69c40452fc92e4b3695922040659a41c8bc3

## Farthest natural milestone

- **farthest:** event_node_consumed
- **last_successful_transaction:** EVENT_NODE_CONSUMED
- **first_unmet_platform_contract:** post-drain gates 15D=0 B71=0 134D=0 block 2DADC4->2FC418 (POST_DRAIN_GATE_15D_B71_134D)
- **note:** `EVENT_PATH_A_ENQUEUE_COMPLETE` is an independent marker; if node linked/consumed, prefer consumer milestones over that marker.

## Post-Drain Gate (successor, not protocol ACK)

- **successor_status:** POST_DRAIN_SUCCESSOR_BLOCKED
- **successor_blocker:** POST_DRAIN_GATE_15D_B71_134D
- **gate_sample:** 15D=0 B71=0 134D=0 C76=0
- **EVENT_POST_DRAIN_GATE_OK:** no
- **UI_writer_2FC418:** no
- **legacy_alias:** former `Ack path` / `ack_done` == post-drain successor reachability
- **PDGT enter 30CBBC:** no
- **PDGT enter 2E2520:** yes
- **PDGT enter 2DC4D8:** no
- **PDGT store 15D:** no
- **PDGT store B71:** no

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

- **manifest:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_manifest_ffp_event_20260724_155025_4016.txt
- **csv_requests:** missing
- **csv_10165:** missing
- **csv_samples:** missing
- **csv_mem:** missing
- **abi_manifest:** missing
- **pdgt_watch_csv:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_post_drain_gate_watch.csv sha256=350ce3d5fe94546a9d74907b871245548cf0c9d65a1d059b3ab90eedbb3cc9ac
- **pdgt_timeline:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_post_drain_gate_timeline.md sha256=2a287a12907954aadee2a51426cbdd1c613d4af33692d4a3e82bf4d8db20da62
- **forbidden_hits:** none

## Discipline

- Event Round A: collect identity + 10165 object + handler ABI (no one-shot default)
- Event Round B: `-ApplyAbi` once after provenance
- Resource/Validate auto-continue when state advances / display predecessor reached
- Forbidden: fixed PC patches, Robotol flag writes, fabricated 10165, forced DispUpEx, E9/E10A
- Post-drain gate: observe-only writers/watchpoints; no forced 15D/B71/UI_MODE

