# B71 Dispatch Predicate Timeline

**Primary calibrate run:** `ffp_event_20260724_195827_54034`  
**Control run (trace off):** `ffp_event_20260724_200514_39822`  
**Runtime:** Gwy+stubs · observe-only · `JJFB_B71_DISPATCH_TRACE=1` on calibrate only

## Verdict snapshot

| Field | Calibrate (trace on) | Control (trace off) |
|-------|----------------------|---------------------|
| farthest | event_node_consumed | event_node_consumed |
| successor_status | POST_DRAIN_SUCCESSOR_BLOCKED | POST_DRAIN_SUCCESSOR_BLOCKED |
| gate_sample | 15D=0 B71=0 134D=0 C76=0 | 15D=0 B71=0 134D=0 C76=0 |
| true 0x2E2520 enters | **2** (exact PC; no Thumb+2 dup) | **2** (stdout PDGT_ENTER) |
| enter 0x2DC4D8 | 0 | 0 |
| store ER_RW+B71 | 0 | 0 |
| B71_writer_grade | candidate_entered_no_dispatch | candidate_entered_no_dispatch |
| 15D_writer_grade | candidate_unproven | candidate_unproven |

Trace did **not** change farthest / node consumed / gate / stop reason.

## Switch head (static + live)

Source: `robotol.ext` Thumb at `0x2E2520` (jjfb.mrp):

```text
0x2E2520 PUSH {r4-r7}
0x2E2522 mov  r4, r0          ; save object ptr
0x2E2524 ldr  r0, [r0,#0]     ; word0 used as event_code
0x2E252A subs r0, #3          ; index = code - 3
0x2E252E cmp  r0, r3          ; r3 = 0x1B0 (max index)
0x2E2532 bcs  0x2E253E        ; unsigned >= max → default
… ADD PC table case …
0x2E253E BL   0x2E4194        ; default / out-of-range
```

B71 writer chain is **not** in the default path. Static table: `event_code==3` (`MR_MOUSE_UP`) → `0x2E379E` → `0x2DC4D8`.

## call_id=1

- entry: `pc=0x2E2520 lr=0x2DC8D9 sp=0x27FFD8 r0=0x2A8374 r1=0 r2=0 r3=0 r9=0x2B1854`
- gate@enter: 15D=0 B71=0 134D=0 C76=0
- object words: `[0]=0x0` `[4]=0x2A8364` `[8]=0x3` `[12]=0x12`
- inner via +4 `@0x2A8364`: `[0]=0x627857` `[4]=0x4` `[8]=0x10`
- **switch event_code (ldr @0x2E2524):** `0` → index `0xFFFFFFFD`
- **CMP @0x2E252E:** `a=0xFFFFFFFD b=0x1B0`
- **first blocking predicate:** `BCS_index_out_of_range` @ `0x2E2532`
- **dispatch_target:** `0x2E4194` (default BL) · `reached_2DC4D8=0`
- local_instruction_count=12 · basic_block_count=2

## call_id=2

- entry: `pc=0x2E2520 lr=0x2DC8D9 sp=0x27FFD8 r0=0x2A83C4 r1=0x1 r2=0 r3=0 r9=0x2B1854`
- gate@enter: 15D=0 B71=0 134D=0 C76=0
- object words: `[0]=0x0` `[4]=0x2A83A4` `[8]=0xFFFFFFFD` `[12]=0x6D466468`
- inner via +4 `@0x2A83A4`: `[0]=0x25998` `[4]=0x10` `[8]=0x38303436` ("6048")
- **switch event_code:** `0` → index `0xFFFFFFFD`
- **same blocking predicate:** `BCS_index_out_of_range` @ `0x2E2532` → `BL 0x2E4194`
- `reached_2DC4D8=0`

## Provenance (calibrate)

| Field | Value |
|-------|-------|
| run_id | ffp_event_20260724_195827_54034 |
| git_commit | 4e020b3249b3cd8d25004988b81e1414c2145a99 |
| runner_sha256 | 6a22e3c409752ab94f4269d75e907d1baa95e3752fef08d360feecdb2e442021 |
| main_exe_sha256 | 3406cf4a8d2eeededb24427c7372186befde714a2ef319266cd96c5ef83d3915 |
| stdout_sha256 | 0114d36b4a47e24ef533a4b368b6ca559ccf847ca2dee4f92c763b7298b59b99 |
| b71_dispatch_timeline_sha256 | d322b78bde0350eb30c5ccc2a6422249a4a75a5e39d5ac2c68221e2f6efe3318 |
| b71_dispatch_calls_sha256 | db550b1776b92a9387a4e383a8d6ba067bb53e5f7ea936ae4c886a5b8862e953 |
| b71_dispatch_reads_sha256 | 03badbde093c7c8a057a06fd009c235f64952ecfb730d89b197f8161ace35709 |
| hashes sidecar | `reports/product_ffp_hashes_ffp_event_20260724_195827_54034.txt` |

## Discipline

- Observe-only: no 15D/B71/UI_MODE writes; no forced jump to `0x2DC4D8`.
- Dense CFG limited to switch head `0x2E2520..0x2E2540` (+ exact case probes).
- Writer proof requires `actual_store_pc` on the candidate chain (not met this run).
