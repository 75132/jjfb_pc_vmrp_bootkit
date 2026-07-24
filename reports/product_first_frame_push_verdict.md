# Product First-Frame Push Verdict

- **run_id:** ffp_event_20260724_200514_39822
- **mode:** Event
- **verdict:** EVENT_QUEUE_CONSUMER_REACHED
- **runtime:** Gwy+stubs
- **seconds:** 50
- **process_exit:** killed
- **apply_abi:** yes
- **ok_callback_returns:** 2

## Provenance

- **git_commit:** 4e020b3249b3cd8d25004988b81e1414c2145a99
- **git_tree:** 670edcb4750d2d0fa310b17a3bce9699a35c1143
- **git_dirty:** yes
- **runner_path:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\RUN_PRODUCT_FIRST_FRAME_PUSH.ps1
- **runner_sha256:** 6a22e3c409752ab94f4269d75e907d1baa95e3752fef08d360feecdb2e442021
- **main_exe:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\main.exe
- **main_exe_sha256:** 3406cf4a8d2eeededb24427c7372186befde714a2ef319266cd96c5ef83d3915
- **gwy_launcher_sha256:** 450d04e2f1a821698c971fc6aac52e6b607ce49ca87983eacd37dc07f91c7d99
- **stdout:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\product_ffp_stdout.txt
- **stdout_sha256:** 758c90caee0758cdd11165c807697f6df9044e62433916b00db35e741d4e26d1
- **stderr:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\logs\product_ffp_stderr.txt
- **stderr_sha256:** 85d874072e2bc7387eb1fe9690f1d42eb98b6717a1f7518d7e473cb112d7738c
- **hashes_sidecar:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_hashes_ffp_event_20260724_200514_39822.txt
- **verdict_sha256:** d8d1d282cc807b3e59169ba26a597571b95ee67462f2cc8c74659e21fc94cecc

## Farthest natural milestone

- **farthest:** event_node_consumed
- **last_successful_transaction:** EVENT_NODE_CONSUMED
- **first_unmet_platform_contract:** request identity not classified across samples
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
- **B71_dispatch_trace:** no
- **b71_dispatch_timeline:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_timeline.md sha256=32e03694878843168c1f819cc416187772f026f1f638fbcf78c228f55cb7c890
- **b71_dispatch_calls:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_calls.csv sha256=514817fc8c33c6371691a570eed0159da2bbc1d0f8e8fa0c6c5b3b52b7a463a6
- **b71_dispatch_branches:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_branches.csv sha256=a689ef3748e18a6597dd826776bce6fd72e0b03d4dee6f4981b6956c1217be0c
- **b71_dispatch_reads:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_reads.csv sha256=3af75a2233d48287b4c31efc8a7f12518114c60a9483c873a335fb7bbe1e2059

## Event / ABI

- **guest request samples:** 2
- **EVENT_TXN ACCEPT:** 2
- **FAMILY DELIVER:** 3
- **SUPPRESS:** 0
- **identity_class:** UNKNOWN
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
- **EVENT_TRANSACTION_IDENTITY_CONFIRMED:** no
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

- **manifest:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_manifest_ffp_event_20260724_200514_39822.txt
- **csv_requests:** missing
- **csv_10165:** missing
- **csv_samples:** missing
- **csv_mem:** missing
- **abi_manifest:** missing
- **pdgt_watch_csv:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_post_drain_gate_watch.csv sha256=c70bedf4d5ffa9becd4de8cba29b2363637b20535ff318f0398274acff3944bc
- **pdgt_timeline:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_post_drain_gate_timeline.md sha256=683b75c6cc52b8a141e62467dfa5966cded61d0e8cb0135e08ace4b315301dc5
- **b71_dispatch_timeline:** C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_timeline.md sha256=32e03694878843168c1f819cc416187772f026f1f638fbcf78c228f55cb7c890
- **forbidden_hits:** none

## Discipline

- Event Round A: collect identity + 10165 object + handler ABI (no one-shot default)
- Event Round B: `-ApplyAbi` once after provenance
- Resource/Validate auto-continue when state advances / display predecessor reached
- Forbidden: fixed PC patches, Robotol flag writes, fabricated 10165, forced DispUpEx, E9/E10A
- Post-drain gate: observe-only writers/watchpoints; no forced 15D/B71/UI_MODE

