# Stage: Platform 0x10138 ABI + Nested Path-A

**Date:** 2026-07-25  
**Entry:** JJFB product path (`gwy/jjfb.mrp` / robotol)  
**Primary verdict:** `NEXT_PLATFORM_CONTRACT_IDENTIFIED`  
**Closed this round:** platform `0x10138` multi-out memory contract + companion `0x10132` size-malloc ABI

## Core answer

`0x10138` is **not** an allocator and **ret=0 is MR_SUCCESS**.

| Question | Answer |
|---|---|
| What API is `0x10138`? | Multi-out **query** via six guest `*out` pointers (R1/R2/R3/SP[0]/SP[4]/SP[8]) |
| Why did product previously “return 0”? | Classify fell through `default_status` → ret 0 **without writing outs / ED8 gates** — ABI incomplete, not “wrong success code” |
| Does ret=0 become R4? | **No.** Heap helper keeps R4=`ER_RW+0xED8`; after call it `ldr [r4]` (capacity) and compares free-out vs size, then may `BL 0x10132(size)` |
| R4=0 at `0x312A78`? | **Not reproduced** after fix. Nested `0x312A60` push builds nodes with live entry pointers |
| Fix module? | `platform_send_app_event` classify + `gwy_ext_obs_sendappevent_dispatch` MULTI_OUT executor (slot28 already bound) |
| Nested code=5 infinite? | One nested publish observed during handler; not a proven unbounded loop in this window |
| `0x2F68E4` clean return? | **Not yet** (PAH insn budget while still inside helper) |
| `0x2E4066` / `0x2DADC4`? | **Not entered** this round |

Next boundary (unchanged product goal, new gap):

> Finish **inside** `0x2F68E4` stream/work so fallthrough `0x2E4066 → 0x2DADC4` happens naturally.

---

## 1. Proven ABI (legacy + live)

### Binding

```text
API 0x10138
→ robotol 0x2D9A6E / 0x30D010
→ wrapper 0x304558
→ extChunk+0x28 (slot28)
→ gwy_ext_obs_sendappevent_dispatch
```

Slot28 binding is correct; R0 return path already writes guest R0.

### Semantics (legacy bridge + robotol `0x2D99AC`)

| Site LR | Mode | Outs | Side effects |
|---|---|---|---|
| ≈`0x2D9A6A` | heap | `*out5 = free (0x200000)` | `ER_RW+ED8=0`, bytes `7DC/7DD/7D9/11=1` |
| ≈`0x30D010` | metrics | `*out0=W`, `*out5=H` (240×320) | none |

`SET_RET_V(MR_SUCCESS)` → **R0=0**.

### Causation chain (live, contract ON)

```text
0x10138 heap outs+gates
→ 0x10132(R1=size) malloc block base (header = size-4)
→ caller uses block+4
→ Path-A list/node publish
→ 0x2E2520 word0=5 → 0x2E4040 → BL 0x2F68E4
```

First B run with only `0x10138` (before `0x10132` size fix) faulted immediately:

```text
0x10132 R1=0x8 → default_status ret=0 → DSM memcpy @0x94E40
```

So the **true companion contract** after `0x10138` is **`0x10132` size-malloc**, not strdup.

---

## 2. Product implementation

### `0x10138` → `GWY_PLAT_KIND_MULTI_OUT`

- Writes six outs; heap vs metrics by `site_lr` at `SP+0x24`
- Robotol ER_RW gate poke via R9 (not B71/15D/UI_MODE)
- Opt-out: `JJFB_PLATFORM_10138_CONTRACT=0` (Variant A)
- Trace: `JJFB_PLATFORM_10138_TRACE=1` (`product_platform_10138_trace`)

### `0x10132` dual ABI

| R1 | Kind |
|---|---|
| `4 .. 0x1FFFFF` | size malloc + header word |
| `≥ 0x200000` | strdup (Path-A name publish) |

---

## 3. A / B / C

| | A `10138_CONTRACT=0` + diag | B contract=1 + diag + traces | C default (contract on, quiet) |
|---|---|---|---|
| Multi-out / gates | off | **on** (`gates=1`, `out5=0x200000`) | on (quiet) |
| Early `0x10132(size)` | may still fault/identify `@0x94E40` then alternate paths | **malloc ok** (`ret=0x2829E4`…) | malloc ok |
| Call1 / `0x2E4040` | yes | yes | (expected when drained) |
| Nested `0x312A60` | — | **yes** (`nested_path_a_published`) | — |
| `0x2F68E4` return / `0x2E4066` / `0x2DADC4` | no | no (budget) | no |
| Fake B71/15D/UI / DispUp | no | no | no |

Logs: `out/p10138_task6/{A,B,C}/`.

---

## 4. Milestones (B)

Observed:

- `platform_10138_entered` / `platform_10138_completed` (heap)
- `path_a_valid_dispatch` / `path_a_handler_entered`
- `nested_path_a_published`

Not observed:

- `path_a_helper_returned`
- `lifecycle_successor_entered`

Status label maps `platform_10138_entered` → **“Completing platform 0x10138”**.

---

## 5. Hard bans (respected)

No forced nonzero `0x10138` ret, no skip of `0x2F68E4`, no forced PC to `0x2E4066`/`0x2DADC4`, no B71/15D/UI_MODE writes, no fake resources/DispUpEx.

---

## 6. What the user should see

With Diagnostic: milestone **Completing platform 0x10138**, then Path-A handler enter. Window may stay white — first frame is still downstream of `0x2DADC4`.

Default Launcher: same platform contract without dense traces.
