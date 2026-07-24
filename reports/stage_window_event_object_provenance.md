# Stage: Window-mode calibration + Event Object Provenance

**Date:** 2026-07-24  
**Entry:** `JJFB_Launcher.exe --debug --diagnostic` (not FFP headless runner)  
**Run id:** `launcher_20260724_221234_95515` (consume proof) / `eot_20260724_221442` (writer chain log)

## Verdict

```text
WINDOW_MODE_REACHES_event_node_consumed = YES
0x2E2520_enters                    = YES (call_id 1,2 proven; call 3 also seen)
0x2DC4D8_entered                   = NO
gate                               = 15D=0 B71=0 134D=0 C76=0
first_contract_deviation           = producer layout: value 3 lives at entry+8; switch reads +0
```

## Window-mode calibration

| Check | Result |
|-------|--------|
| Status + SDL windows | yes (HoldSeconds run; main_alive=True) |
| profile | `profiles/jjfb.json` |
| target | `gwy/jjfb.mrp` cfg36 |
| progress IPC | `out/vmrp_run/runtime_progress.jsonl` |
| farthest milestone | **event_node_consumed** |
| child pid in progress | matches main.exe |

Milestone sequence (child-written):

```text
runtime_spawned
→ guest_entry_called
→ waiting_for_first_frame
→ event_path_a_seen
→ event_node_linked
→ event_dispatch_2e2520 (call 1)
→ event_node_consumed
→ event_dispatch_2e2520 (call 2)
→ event_node_consumed
```

**Env gap closed:** without `JJFB_PRODUCT_FFP_APPLY_ABI=1`, Path-A blocked on `B54==0` (`EVENT_PATH_A_BLOCKED_NULL_LIST_HEAD`). Launcher now enables the same platform list-head recovery as FFP Event TraceQueueConsumer (publishes 8-byte list control via `PlatformEventQueue`; does **not** write B71/15D/UI_MODE).

## Call 1 (event_object_call_id=1)

| Field | Value |
|-------|-------|
| R0 | `0x2A8374` |
| LR | `0x2DC8D9` (drain caller) |
| word0 / switch code | **0** |
| word4 | `0x2A8364` (inner buffer) |
| word8 | **3** |
| wordC | `0x12` |
| dispatch | BCS @ `0x2E2532` → default `BL 0x2E4194` |
| reached 0x2DC4D8 | no |

### Provenance chain (Call 1)

```text
Path-A payload @0x30D2F9  r0=ctx 0x6AD11C  r1=sib 0x69EF14
→ 0x101AB fill (empty Path-A body; with_rec=0)
→ framing @0x2E4EAE  BL heap helper  arg0=0x3
→ framing @0x2E4EB6  BL heap helper  arg0=0xC
→ DSM helper @0x2E4ECA  r0=0x2A8364 (inner)  r1=0x6AD127
→ push @0x2E4EEE→0x312A60  list=0x2829D4  entry=0x2A8374
     entry already: [0]=0  [4]=0x2A8364  [8]=3  [C]=0x12
→ node ctor @0x312A72  STR entry into node+8  (node=0x2A838C)
→ drain get_item / 0x2DC82E  r0=entry 0x2A8374
→ caller LR=0x2DC8D9  passes entry as R0
→ 0x2E2520  ldr r0,[r0,#0] → 0 → default 0x2E4194
```

### Answers for +0/+8

1. **Who wrote +8=3?** Not observed as a live `STR` after object track (track starts at push). Correlated producer site: **`PC≈0x2E4EAE`** passes **`arg0=0x3`** into the heap helper immediately before the entry is pushed; Call2 passes **`0xFFFFFFFD`** at the same PC and later shows **`entry+8=0xFFFFFFFD`**. Therefore `+8` carries the Path-A framing size/arg, not a Mythroad `event_code` store into `+0`.
2. **Source of 3:** Path-A framing argument at `0x2E4EAE` (same numeric family as `MR_MOUSE_UP=3`, but **wrong field** for the `0x2E2520` switch).
3. **Why +0 stays 0:** At first observation (`0x312A60`) `+0` is already 0; no later write to `entry+0` was seen. Event code was never placed at `+0`.
4. **Other object with +0=3?** Not on Call1/Call2. Call3 later showed `word0=0x5` (different object).
5. **Struct copy moved field to +8?** Node ctor correctly stores **pointer** `0x2A8374` at `node+8`. It does **not** move event code. The `3` is already on the **entry** object before push.
6. **Wrapper vs event object?** Consumer passes the **queue entry / item pointer** (`node+8` content). Call1 R0 equals that entry. Not an accidental outer wrapper at the BL site. Inner `*[R0+4]` is a data buffer (`0x2A8364`), not the switch key.
7. **Why this R0?** Drain path `0x2DC80C → get_item → … → LR=0x2DC8D9` passes the popped item pointer by contract.

## Call 2 (event_object_call_id=2)

| Field | Value |
|-------|-------|
| R0 | `0x2A83C4` |
| word0 | 0 |
| word8 | `0xFFFFFFFD` |
| classification | **not** a mouse-event twin of Call1 |

Same framing PC `0x2E4EAE` used `arg0=0xFFFFFFFD` (absurd size / payload marker). Treat as alternate Path-A framed object / invalid size residue — **do not** expect mouse-case dispatch.

## First proven contract deviation

```text
Path-A framing builds entry A at ~0x2E4EAE..0x2E4EEE with code-like value in +8
PC=0x312A60 pushes A (already [0]=0, [8]=3)
PC=0x312A72 node stores A at node+8
consumer PC≈0x312AB4/0x2DC82E retrieves A
caller LR=0x2DC8D9 passes A as R0 to 0x2E2520
0x2E2520 reads word0 as event_code
therefore first proven deviation = producer/layout: Mythroad event_code expected at +0,
but Path-A framed object presents related value at +8 while +0 remains 0
```

**Repair design (not implemented this round):** fix platform/Path-A object construction so the dispatcher-facing record places a valid Mythroad `event_code` at **`+0`** (or change which pointer is queued if an inner record already has the correct layout). Do **not** patch R0 to R0+8 at `0x2E2520`, and do not force `0x2E379E` / B71 stores.

## Diagnostic vs normal

| Mode | PDGT/B71/EOT | FFP+list-head APPLY | Expected consume |
|------|--------------|---------------------|------------------|
| `--diagnostic` | on | on | observed |
| default launcher | off | on | same Path-A path (observe quieter) |

Traces are observe-only for dispatch/object fields; list-head recovery is required for Path-A to run (same as prior FFP Event runner).

## Artifacts

- `out/vmrp_run/runtime_progress.jsonl`
- `reports/product_b71_dispatch_{calls,reads}.csv`
- `reports/product_event_object_stores.csv` (node ctor stores)
- `logs/launcher_eot_stdout.txt` (full EOT_STAGE / Path-A framing)

## Next (Task 4 design only until approved)

1. Disassemble `0x2E4EAE..0x2E4EEE` and name the STR that writes entry+8.
2. Decide correct platform boundary: Path-A payload encode vs node content vs consumer unwrap.
3. Implement observe-proven fix; verify `word0` becomes valid and `0x2E379E`/`0x2DC4D8` can occur naturally with trace off.

**Task 4 done:** see `reports/stage_valid_event_comparator.md` — `EVENT_CONTRACT_REPAIRED` via `platform_event_queue_ensure_path_a_framing` (hdr gate). Call1 `word0=5` → jump `0x2E4040`.
