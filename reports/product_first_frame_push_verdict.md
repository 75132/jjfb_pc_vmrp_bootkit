# Product First-Frame Push Verdict

- **run_id:** ffp_event_20260801_041844_51205
- **mode:** Event
- **verdict:** P6_EVENT_PARTIAL
- **runtime:** Gwy+stubs
- **seconds:** 50
- **process_exit:** 
- **apply_abi:** no
- **ok_callback_returns:** 0

## Provenance

- **git_commit:** 70e35adabed1ccb80f8692b5f8bdf888aaf59dd5
- **git_tree:** 86f0fb56b0bcb9782b612d57882abaf96acb5418
- **git_dirty:** yes
- **runner_path:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\RUN_PRODUCT_FIRST_FRAME_PUSH.ps1
- **runner_sha256:** 6a22e3c409752ab94f4269d75e907d1baa95e3752fef08d360feecdb2e442021
- **main_exe:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\main.exe
- **main_exe_sha256:** cc9153ef48b1fb049e206464784939bec460e037b57ed74fd3aa08a9dad402ff
- **gwy_launcher_sha256:** dbaa7975bd7a2b1108c374c2d3c509f0a00cea9606df425ae41dc7ec0bee1784
- **stdout:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\logs\product_ffp_stdout.txt
- **stdout_sha256:** d2463f6ad65f52c6a01c2f1a8f31205a83915dc8a756392d3396c3bb14f0ba8c
- **stderr:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\logs\product_ffp_stderr.txt
- **stderr_sha256:** 747feaf3fc8c102dd1863a5bc5522da68a2aa15ad3529ab81842f16e4d35873d
- **hashes_sidecar:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_hashes_ffp_event_20260801_041844_51205.txt
- **verdict_sha256:** c4951cb6f5fb31152b1a5d3f923ee7e6840f295fa75b3d5ff8179a60b05edbd7

## Farthest natural milestone

- **farthest:** family_handler_delivered
- **last_successful_transaction:** EVENT_CONTEXT_OWNER_CONFIRMED
- **first_unmet_platform_contract:** request identity not classified across samples
- **note:** `EVENT_PATH_A_ENQUEUE_COMPLETE` is an independent marker; if node linked/consumed, prefer consumer milestones over that marker.

## Post-Drain Gate (successor, not protocol ACK)

- **successor_status:** POST_DRAIN_SUCCESSOR_BLOCKED
- **successor_blocker:** none
- **gate_sample:** not_sampled_in_log
- **EVENT_POST_DRAIN_GATE_OK:** no
- **UI_writer_2FC418:** no
- **legacy_alias:** former `Ack path` / `ack_done` == post-drain successor reachability
- **PDGT enter 30CBBC:** yes
- **PDGT enter 2E2520:** no
- **PDGT enter 2DC4D8:** no
- **PDGT store 15D:** no
- **PDGT store B71:** no
- **B71_dispatch_trace:** no
- **b71_dispatch_timeline:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_timeline.md sha256=ce70d930b118296454af7a20a97aeaa82c4d004a8a9d66fc906a64182f88625d
- **b71_dispatch_calls:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_calls.csv sha256=514817fc8c33c6371691a570eed0159da2bbc1d0f8e8fa0c6c5b3b52b7a463a6
- **b71_dispatch_branches:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_branches.csv sha256=a689ef3748e18a6597dd826776bce6fd72e0b03d4dee6f4981b6956c1217be0c
- **b71_dispatch_reads:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_reads.csv sha256=3af75a2233d48287b4c31efc8a7f12518114c60a9483c873a335fb7bbe1e2059

## Event / ABI

- **guest request samples:** 1
- **EVENT_TXN ACCEPT:** 1
- **FAMILY DELIVER:** 1
- **SUPPRESS:** 0
- **identity_class:** UNKNOWN
- **EVENT_LIST_HEAD_INITIALIZED:** no
- **EVENT_PATH_A_ENQUEUE_OK:** no
- **NODE_94E40_FUNCTION_IDENTIFIED:** no
- **NODE_FIRST_CAUSAL_ZERO_FOUND:** no
- **EVENT_LIST_NODE_LINKED:** no
- **EVENT_LIST_COUNT_CHANGED:** no
- **EVENT_QUEUE_NONEMPTY_VISIBLE:** no
- **EVENT_QUEUE_CONSUMER_TRIGGER:** no
- **EVENT_QUEUE_CONSUMER_ENTER:** no
- **EVENT_NODE_CONSUMED:** no
- **NODE_ALLOCATION_RETURN_VALID:** no
- **EVENT_PATH_A_ENQUEUE_COMPLETE:** no
- **fault_at_0x94E40:** no
- **EVENT_TRANSACTION_IDENTITY_CONFIRMED:** no
- **EVENT_CONTEXT_OBJECT_IDENTIFIED:** yes
- **EVENT_CONTEXT_OWNER_CONFIRMED:** yes
- **EVENT_CONTEXT_LIFETIME_CONFIRMED:** no
- **FAMILY_EVENT_ABI_CONFIRMED:** no
- **FAMILY_HANDLER_OUTPUT_WRITES_OBSERVED:** no
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
| EVENT samples / identity | no |
| FAMILY DELIVER | yes |
| ROBOTOL_STATE_ADVANCED | no |
| FIRST_NATURAL_REFRESH | no |
| FRAMEBUFFER_NONEMPTY | no |
| HWND_VISIBLE | no |

## Artifacts

- **manifest:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_ffp_manifest_ffp_event_20260801_041844_51205.txt
- **csv_requests:** missing
- **csv_10165:** missing
- **csv_samples:** missing
- **csv_mem:** missing
- **abi_manifest:** missing
- **pdgt_watch_csv:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_post_drain_gate_watch.csv sha256=2f7e6f428c373693c04d6bda8ebd02d825828cfd8b54e6a781f501bfcc84b05b
- **pdgt_timeline:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_post_drain_gate_timeline.md sha256=2c12a842e62377b1c1393a53ea4a1e8d9aaf27f01b84ccd66f652f2a264270b3
- **b71_dispatch_timeline:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_b71_dispatch_timeline.md sha256=ce70d930b118296454af7a20a97aeaa82c4d004a8a9d66fc906a64182f88625d
- **forbidden_hits:** none

## Discipline

- Event Round A: collect identity + 10165 object + handler ABI (no one-shot default)
- Event Round B: `-ApplyAbi` once after provenance
- Resource/Validate auto-continue when state advances / display predecessor reached
- Forbidden: fixed PC patches, Robotol flag writes, fabricated 10165, forced DispUpEx, E9/E10A
- Post-drain gate: observe-only writers/watchpoints; no forced 15D/B71/UI_MODE

