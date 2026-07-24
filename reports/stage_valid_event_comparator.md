# Stage: Valid Event Comparator + Path-A Contract Repair

**Date:** 2026-07-24  
**Entry:** `JJFB_Launcher.exe` (window)  
**Verdict:** `EVENT_CONTRACT_REPAIRED`

## Why Call3 had word0=5 while Call1/2 had word0=0

Same Path-A constructor (`0x2E4D6C` framing loop → `0x312A60` push). Difference is **hdr pre-consume gate**:

| State | `ER_RW+0x15C` | `A90+4` | Stream into loop | First entry |
|-------|---------------|---------|------------------|-------------|
| Cold (Call1/2) | 0 | n/a | `[hdr=5][body_size=6][code=5][marker]` | `+0=0` (BE u16 from body_size hi), `+8=3` (hdr−2) |
| Warm (old Call3) | 1 | 0 | hdr stripped first; loop sees body | `+0=5`, `+8=4` (body_size−2) |

Call3 was not a different object type — it was the **second (or later) Path-A publish** after the guest set `0x15C=1`.

## Call3 provenance (pre-fix baseline)

| Field | Value |
|-------|-------|
| R0 | `0x2A83F4` |
| word0 / word4 / word8 / wordC | `5` / inner / `4` / adjacent |
| `+0` writer | `0x2E4ED8` `STR r1,[r5]` (same as Call1 path) |
| source | `0x308D28` BE u16 read → event code **5** from `platform_101ab_fill_path_a` |
| producer | Path-A framing after hdr pre-consume |
| dispatch | would take jump-table index `5−3=2` → **`0x2E4040`** |

## Call1 vs Call3 (constructor)

| Item | Call1 (cold, contract off) | Call3 / Call1 (contract on) |
|------|----------------------------|-----------------------------|
| producer | `30D2F9→101AB→2E4D6C` | same |
| event family | Path-A | Path-A |
| allocation | `2D99AC` size=`r6−2`; entry `malloc(0xC)` | same |
| `+0` writer | `2E4ED8` writes **0** | `2E4ED8` writes **5** |
| `+8` writer | `2E4EE6` = framing size | `2E4EE6` = framing size |
| value `3` | **framing size** (`hdr−2`), not Mythroad event code | n/a (`+8=4`) |
| dispatch | default `0x2E4194` | **`0x2E4040`** (table index 2) |

## Path-A framing naming (`0x2E4E92..0x2E4EEE`)

See `out/product_event/path_a_framing_named.txt`.

- `0x308D98` — read BE u32 (length / hdr)
- `0x308D28` — read BE u16 (event code → `entry+0`)
- `0x2E4EAE` `BL 0x2D99AC` — **malloc(size)** with `size=r6−2` (not type; `3` is size; `0xFFFFFFFD` is absurd size from misaligned `0xFFFFFFFF` marker)
- `0x2E4EB6` `BL 0x2D99AC` — **malloc(0xC)** entry
- `0x2E4ED8` — `entry+0 = event_code`
- `0x2E4EE0` — `entry+4 = inner`
- `0x2E4EE6` — `entry+8 = size`
- `0x2E4EEE` — `BL 0x312A60` push

## Call1 correct event_code

**5** — BE u16 in Path-A payload (`platform_101ab_fill_path_a`), Path-A / terminate family (legacy `2E4040/2E4066`).  
Value **3** is framing length residue, **not** `MR_MOUSE_UP` at `+0`.

## Repair (Scheme A / gate arming)

**Module:** `src/platform/platform_event_queue.c`  
`platform_event_queue_ensure_path_a_framing` — before `0x101AB` fill (`gwy_ext_obs.c`):

- poke byte `ER_RW+0x15C = 1` (hdr-consume flag; **not** `0x15D`)
- poke `ER_RW+0xA90+4 = 0`

Env: `JJFB_PATH_A_EVENT_CONTRACT=0|1` (default **1**).

Forbidden paths unused: no `+8→+0` copy, no `0x2E2520` patch, no B71/15D/UI_MODE writes.

## Acceptance

| Mode | Call1 word0 | Dispatch |
|------|-------------|----------|
| Diagnostic + contract=1 | **5** (`+8=4`) | table → **0x2E4040** (not `0x2E4194`) |
| Diagnostic + contract=0 | **0** (`+8=3`) | default **0x2E4194** |
| Default launcher (contract on) | (trace quiet) | still reaches `event_node_consumed` |

Live store proof (contract on):  
`product_event_object_stores.csv` — `pc=0x2E4ED8` `new=0x5` `src_reg=1` stage=`framing_store_plus0`.

Natural milestone after repair: Call1 is a **valid Path-A event** (`word0=5`) selecting **`0x2E4040`** (legacy terminate / Path-A handler), instead of default sink.

Gate still `15D=B71=0` at enter (expected; not forced). First frame / resource load not yet claimed this round.

## Classification

```text
EVENT_CONTRACT_REPAIRED
```

(also explains prior cold-start class `PATH_A_MISSING_EVENT_CODE_FIELD` was misnamed: `+0` was written, but with wrong stream alignment until hdr gate armed.)
