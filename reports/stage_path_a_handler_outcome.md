# Stage: Valid Path-A Handler Outcome Closure

**Date:** 2026-07-24  
**Entry:** JJFB product path (`gwy/jjfb.mrp` / robotol)  
**Primary verdict:** `NEXT_PLATFORM_CONTRACT_IDENTIFIED`  
**Secondary:** `PATH_A_HANDLER_SCHEDULED_NEXT_EVENT`

## Core answer

`0x2E4040` is **not** a terminate-only stub that immediately writes B71 or draws. On a valid Call1 (`word0=5`) it:

1. Treats **R4** as the event entry (`+0=5`, `+4=inner`, `+8=4`)
2. Writes a few ER_RW queue/lifecycle slots (`B5C`, `B60`, `AC8`)
3. **`BL 0x2F68E4`** — Path-A stream/helper work (BE reads, malloc, repeated platform **`0x10138`**)
4. While still inside that helper, **re-enters Path-A framing** and **`0x312A60` push**, linking **additional code-5 nodes**
5. Static fallthrough **`0x2E4066 → BL 0x2DADC4`** was **not reached** in the live window

So the first real post-dispatch boundary is:

> **Complete `0x2F68E4` (incl. platform `0x10138` / nested Path-A publish ABI) so the handler can reach `0x2E4066` → `0x2DADC4`.**

B71/15D stay 0 because those writers sit **downstream of `0x2DADC4` / post-drain succession**, not inside the `0x2E4040` prologue.

---

## 1. Path-A framing hardening

`platform_event_queue_ensure_path_a_framing(uc, er_rw, module_id, generation, module_name)`:

| Requirement | Evidence |
|---|---|
| Once before first `0x101AB` fill | `PATH_A_CONTRACT_ARM … reason=FIRST_PATH_A_FILL` |
| Same generation idempotent | later `reason=ALREADY` (no re-poke) |
| Reset/new generation re-arms | arm state cleared in `platform_event_queue_reset()` |
| JJFB/Robotol only | skips non-robotol module names |
| `contract=0` restores cold path | Variant B: no ARM log; Call1 `word0=0` |
| Default + Diagnostic same contract | Variant C still emits `FIRST_PATH_A_FILL` |
| Non–Path-A untouched | only arms on `0x101AB` fill site |
| No MRP hash poke | observe-only env + 15C/A94 bytes |

---

## 2. A/B/C

| | A contract=1 + Diagnostic + PAH | B contract=0 + Diagnostic + PAH | C default (no PAH/EOT/PDGT) |
|---|---|---|---|
| Call1 `word0` | **5** (`+8=4`) | **0** (`+8=3`) | (quiet; contract armed) |
| Dispatch | **`0x2E4040`** | **`0x2E4194`** | (expected `0x2E4040` when drained) |
| Handler tree | enter + `BL 0x2F68E4` + nested Path-A | no `0x2E4040` | n/a |
| Nested events | yes (`push_312A60` / linked code=5) | default-sink consumes | — |
| Resource / DispUp / frame | no | no | no |
| Gate 15D/B71 | 0/0 | 0/0 | — |
| Extra host advance from contract | **framing only** | cold framing | framing only |

Contract-on does **not** invent B71/15D/UI; it only repairs Path-A hdr pre-consume.

Logs: `out/pah_task5/{A,B,C}/`.

---

## 3. What `0x2E4040` actually did (Variant A)

### Enter

```text
R4=entry=0x2A8374  +0=5  +4=0x2A8364  +8=4
LR=0x2DC8D9 (return into drain/dispatch)
```

### Key calls

| depth | src → tgt | role |
|---|---|---|
| 1 | `0x2E405E → 0x2F68E4` | Path-A helper (required before static `0x2E4066`) |
| 2+ | `0x308D98`, `0x30A0CC`, `0x2D99AC` | BE read / work / malloc |
| 4+ | `0x304558` plat **`0x10138`** → slot28 | platform API under helper |
| later | `0x2E4EFx` framing → `0x312A60` | **nested Path-A publish** |

**Not seen:** `0x2E4066`, `0x2DADC4`, clean ret to `LR=0x2DC8D9` before hold kill.

### Guest state edits (handler-active)

- Queue/lifecycle ER_RW offsets (`B5C`, `B60`, `AC8`) — not B71/15D
- Additional list nodes with **code 5**
- No UI_MODE / framebuffer / DispUpEx

### Code 5 semantics (dynamic)

Classify as **Path-A response / stream-control processing** (not UI event, not resource-ready, not “terminate stub only”).

In the JJFB boot chain, Call1 code 5 means: *run the Path-A case body that processes the Path-A buffer via `0x2F68E4`, may enqueue further Path-A nodes, and only then is allowed to fall into `0x2E4066→0x2DADC4`.*

---

## 4. Why B71 / 15D still 0

| Claim | Result |
|---|---|
| Legal handler directly writes B71/15D | **Disproved** (no stores; gate samples unchanged) |
| Legal handler unrelated to gate forever | **Too strong** — static path still ends at `2DADC4`, which is on the successor graph |
| Handler missing completion / platform API so writers never scheduled | **Supported** — stuck/busy in `0x2F68E4`+`0x10138`+nested publish; `2E4066` not reached |

Post-drain gate at `0x305EC2` still reads `15D=0 B71=0` (Variant B explicit sample). That is expected until a later natural writer runs.

---

## 5. Window / UI

Status milestones advanced through:

`guest_entry_called` → `event_path_a_seen` → `path_a_valid_dispatch` → `path_a_handler_entered` → `post_dispatch_event_seen` (nested push)

No natural first frame / DispUpEx. Window remains pre-frame (`waiting_for_first_frame`).

---

## 6. Classification

```text
NEXT_PLATFORM_CONTRACT_IDENTIFIED
```

Supporting tag:

```text
PATH_A_HANDLER_SCHEDULED_NEXT_EVENT
```

(nested code-5 publishes from inside `0x2F68E4`)

Not claimed: first frame, resource product load, post-drain gate pass, natural B71/15D.

---

## 7. Next minimal platform capability

**One concrete next contract:**

> Make **`0x2F68E4`’s use of platform `0x10138` (and any nested Path-A `0x312A60` push ABI)** complete successfully under natural drain so execution returns to **`0x2E4066` → `0x2DADC4`**.

Do **not** force-call `0x2DC4D8`, do **not** poke B71/15D, do **not** fake DispUpEx.

Observe-only questions for the next round:

1. What is plat `0x10138` supposed to allocate/return here?
2. Why does nested `0x312A60` sometimes fault with `r4=0` at `0x312A78`?
3. After a clean `0x2DADC4` enter, do B71/15D writers appear on later ticks?

---

## Success checklist

| Item | Status |
|---|---|
| Framing hardened + logged | done |
| `0x2E4040` natural side effects mapped | done |
| Gate re-checked read-only | done (still 0; explained) |
| Code 5 semantics | Path-A stream/control (not terminate-only) |
| A/B/C | done |
| First post-dispatch boundary | **`0x2F68E4` / `0x10138` completion before `0x2DADC4`** |
