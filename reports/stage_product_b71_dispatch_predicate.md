# Stage: B71 Dispatch Predicate Closure

**Date:** 2026-07-24  
**Calibrate run:** `ffp_event_20260724_195827_54034`  
**Control run:** `ffp_event_20260724_200514_39822`  
**Runtime:** Gwy+stubs (observe-only)

## Verdict

```text
POST_DRAIN_SUCCESSOR_BLOCKED
B71_writer_grade = candidate_entered_no_dispatch
15D_writer_grade = candidate_unproven
blocker         = POST_DRAIN_GATE_15D_B71_134D
```

`0x2E2520` is reached from drain (`LR=0x2DC8D9`), but the switch **never** selects the B71 case. Both live calls take the **out-of-range default** `BL 0x2E4194` because word0 of the object passed in `R0` is `0`, not a Mythroad event code in `[3 .. 0x1B0+3]`.

## Provenance

| Field | Calibrate |
|-------|-----------|
| git_commit | 4e020b3249b3cd8d25004988b81e1414c2145a99 |
| runner_sha256 | 6a22e3c409752ab94f4269d75e907d1baa95e3752fef08d360feecdb2e442021 |
| main_exe_sha256 | 3406cf4a8d2eeededb24427c7372186befde714a2ef319266cd96c5ef83d3915 |
| stdout_sha256 | 0114d36b4a47e24ef533a4b368b6ca559ccf847ca2dee4f92c763b7298b59b99 |
| PDGT/dispatch hashes | see `product_ffp_hashes_ffp_event_20260724_195827_54034.txt` |
| farthest | **event_node_consumed** |
| last_successful_transaction | EVENT_NODE_CONSUMED |

Control (no `JJFB_B71_DISPATCH_TRACE`): same farthest, same gate `15D=0 B71=0 134D=0 C76=0`, still enters `0x2E2520`, still no `0x2DC4D8` / B71 store.

## True 0x2E2520 calls

**2** exact-entry calls (Thumb `+2` no longer double-counted).

| call | R0 | R1 | word0 (switch code) | obj+8 | target | block pred |
|-----:|----|----|---------------------|-------|--------|------------|
| 1 | 0x2A8374 | 0 | **0** | 0x3 | 0x2E4194 | BCS_index_out_of_range |
| 2 | 0x2A83C4 | 1 | **0** | 0xFFFFFFFD | 0x2E4194 | BCS_index_out_of_range |

Parameters **differ** (R0/R1 and payload words), but both share switch input `event_code=word0=0`.

## Branch / indirect targets (proven)

1. `ldr r0,[r0,#0]` @ `0x2E2524` → `0`
2. `subs r0,#3` → index `0xFFFFFFFD`
3. `cmp r0, #0x1B0` @ `0x2E252E`
4. **`bcs 0x2E253E` TAKEN** @ `0x2E2532` (unsigned index ≥ max)
5. `BL 0x2E4194` (default) — **not** `ADD PC` into table, **not** `0x2E379E`, **not** `0x2DC4D8`

Direct/indirect dispatch **to** `0x2DC4D8` was **not** observed. The table path exists statically for `event_code==3` → `0x2E379E` → B71 writer; live runs never passed the BCS gate into that case.

## First proven blocking predicate

```text
pc        = 0x2E2532
pred      = BCS_index_out_of_range
CMP       = index(0xFFFFFFFD)  vs  max(0x1B0)
source    = *[R0+0] as event_code  (both calls: 0)
effect    = BL 0x2E4194 default; skip jump-table / MR_MOUSE_UP case
```

Note: call1 `obj+8=0x3` looks like `MR_MOUSE_UP`, but the switch **does not read +8** — only `+0`. That `3` is therefore **not** the live dispatch key.

## Writer grades

| Target | Grade | Evidence |
|--------|-------|----------|
| B71 / 0x2E2520→0x2DC4D8 | **candidate_entered_no_dispatch** | entered dispatcher; no case to 0x2E379E/0x2DC4D8; no ER_RW+B71 store |
| 15D / 0x30CBBC | **candidate_unproven** | never entered; no store |

## Gate field

```text
15D=0  B71=0  134D=0  C76=0
```

Successor remains **POST_DRAIN_SUCCESSOR_BLOCKED** (no 15D=1, no B71≠0, no 0x305EF4 / 0x2DADC4).

## Answers to core questions

1. **Same params each call?** No — distinct R0 objects and R1; both have switch `word0=0`.
2. **Dispatch to 0x2DC4D8?** Not on this path. Static table can; live BCS blocks before table.
3. **Blocking branch?** `bcs` @ `0x2E2532` after `cmp index, #0x1B0`.
4. **Compared values?** `0xFFFFFFFD` vs `0x1B0` (from `event_code=0`).
5. **Value provenance?** First word of the drain-passed object at R0 (queue/work object), **not** a filled Mythroad `event_code` at +0. Inner `*[R0+4]` also is not a small event code.
6. **Actual path?** Default `0x2E4194`, then return toward drain/pop/gate.
7. **Missing piece?** An object whose **`[0] == event_code` with `code >= 3`**, and for B71 specifically **`code == 3` (`MR_MOUSE_UP`)**, or another producer that feeds that case. Current Path-A/drain objects present `word0=0` → treated as out-of-range. This is an **event-record / object-layout / producer contract** gap for the switch, not a missing host patch.

## Last proven BB / first unknown / next minimal observe

| Item | Value |
|------|-------|
| Last proven BB | switch head through `0x2E2532` BCS → `0x2E253E` BL default |
| First non-B71 target | `0x2E4194` (entered; body not densely traced — out of B71 scope) |
| Missing data | Who constructs the `0x2A83xx` objects with `word0=0`; whether Path-A payload code lives at +8 and is never copied to +0 |
| Next minimal observe | At Path-A enqueue / node ctor: record the words written into the object later passed as R0 to `0x2E2520` (especially +0 vs +8). No broad memscan; no state patches. |

## Engineering landed

1. PDGT: exact-entry Thumb dedup; `watch_step`; store-PC-bound writer grades  
2. `JJFB_B71_DISPATCH_TRACE=1`: bounded head CFG + critical reads + case probes  
3. Runner: `-TraceB71Dispatch`, artifact cleanup, hash binding for dispatch CSVs  

## Forbidden (unchanged)

No forced 15D/B71/UI_MODE writes; no forced jump to `0x2DC4D8`; product default remains Gwy+stubs.
