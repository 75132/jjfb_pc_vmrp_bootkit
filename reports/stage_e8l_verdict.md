# Stage E8L Verdict

**Verdict:** `CASE_156_REACHED_NEXT_GAP`

Evidence class: observe-only structured probes — **not** product success.

## Scope

- Derive R0–R3 / stack ABI of `0x30D300`
- Structured case-156 / case-310 probes with R1 payloads
- No 10165 spray, no force `R9+0x8D0` / C44/C9D/CF5, no SVC `#0xAB`, no fake DRAW

## Static ABI (TARGET_OBSERVED)

| Reg / slot | Role |
| --- | --- |
| R0 | switch case index (`CMP r0, #0x157`) |
| R1 | saved to `r4`; case arms `MOV r0,r4; BL target` → **callee R0 = delivery R1** |
| R2 | saved to `r5` (not used by case 156/310 arms) |
| R3 | moved to `r1` then clobbered as bound `0x157` |
| stack arg4/5 | `LDR r6/r2` from `[sp,#0x20]/[sp,#0x24]` after prologue |

Case table (from E8K, confirmed live):

| case | hex | arm | callee |
| --- | --- | --- | --- |
| 156 | `0x9C` | `0x30DDF4` | `BL 0x300158` (parent) |
| 310 | `0x136` | `0x30D72E` | `BL 0x2DFC3C` (hot) |

### `0x2DFC3C` (case 310) — corrected vs early E8L hyp

- Incoming R0 (delivery R1) saved to `r4` at `0x2DFC40`
- Early gate is **`[R9+0xE6C]+0x7C == 0` → `0x2DFCAC`**, not “R1 null”
- Both arms still forward `r4` into later calls
- Does **not** call `0x300158`

### `0x300158` (case 156)

- Saves incoming R0 → `r4`; later restores via `MOV r0,r4`
- In-module BL callers use const `#18` / `#20` as R0 (event codes)

Full static dump: `out/e8l_tmp/dispatch_abi.md`

## Live probe matrix (observe-only)

| Probe | R0,R1,R2,R3 | fire | `0x30D300` | arm | `0x300158` | `0x300714` | hot / E6C-null | state | DRAW |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A | 156,0,0,0 | yes | HIT | `0x30DDF4` | **HIT** `r0=0` | NEVER | — | 0 | no |
| B | 156,18,0,0 | yes | HIT | `0x30DDF4` | **HIT** `r0=0x12` | NEVER | — | 0 | no |
| C | 156,20,0,0 | yes | HIT | `0x30DDF4` | **HIT** `r0=0x14` | NEVER | — | 0 | no |
| D | 310,0,0,0 | yes | HIT | `0x30D730` | NEVER | NEVER | **HIT** `0x2DFC3C` + **`0x2DFCAC`** | 0 | no |

After every probe: `C44=C9D=CF5=B7D=0`, `FE8=0`, `queue_depth=0`, `R9_state=0`.

### Key live lines

**Case 156 B (R1=18):**

```text
[JJFB_E8L_10102_FIRE] case_r0=0x9C r1=0x12 ... handler=0x30D301
[JJFB_E8I_PARENT_HIT] tag=p300158 pc=0x300158 r0=0x12 lr=0x30DDFB
[JJFB_E8L_10102_FIRE_DONE] ... ok=1 end=stop_at_base
```

**Case 310 D:**

```text
tag=e30D300 / e30D730 / e2DFC3C / e2DFCAC  (HIT)
no p300158 / p300714
```

## What this means

1. **ABI forward path is real:** `0x10102` → `0x30D301`/`0x30D300` → case 156 → **`0x300158`** with callee R0 = delivery R1.
2. **Case 156 does not require non-zero R1 to enter parent** (A also hits). R1=18/20 is still the right *semantic* hyp for deeper arms, but parent entry alone is not the remaining wall.
3. **Next gap inside / after parent:** none of A/B/C reach `0x300714` / state / DRAW. Parent returns with idle/queue still cold (`FE8=0`, depth=0). Likely needs prior queue/object setup (7D8 / FE8 / other init), not just case+event-code.
4. **Case 310** reconfirmed: enters hot, takes **E6C-absent** arm (`0x2DFCAC`); not the parent bridge.
5. **Product gap unchanged:** host still only **REGISTER**s `0x10102`; natural delivery remains missing. Probes are counterfactual diagnostics.

## Verdict selection

| Candidate | Result |
| --- | --- |
| `CASE_156_REACHED_NEXT_GAP` | **ACCEPTED** — parent HIT; dispatcher/state/DRAW still cold |
| `CASE_156_REQUIRES_PAYLOAD` | rejected for *entry* (R1=0 still enters); payload may still matter past parent |
| `CASE_310_REQUIRES_PAYLOAD` | secondary — also needs `R9+0xE6C` object (live took null arm) |
| `MISSING_10102_APP_INIT_EVENT` | still true for **product** path; not the E8L probe verdict |
| `DRAW_REACHED` | no |
| `EVENT_SWITCH_ABI_STILL_UNKNOWN` | rejected — ABI for R0/R1 forward is TARGET_OBSERVED |

## Ranking after E8L

1. **Why `0x300158` does not call `0x300714`** with case-156 delivery (queue / 7D8 / object prerequisites)
2. **Natural `0x10102` delivery source** (product still REGISTER-only; docs/06 = register only)
3. Case-310 / `R9+0xE6C` prep (secondary)
4. B7D drain (secondary)

## Natural delivery notes

- `docs/06_PLATFORM_ABI_AND_SCHEDULER.md`: `0x10102` = family **register** (family id + handler), not delivery schema
- Host lifecycle drains `0x10140`; idle-watch marks `host_drain=no` for `0x10102`
- No APP_START / RESUME / RESOURCE mapping claimed this stage

## Forbidden (held)

- no product force of `R9+0x8D0` / C44/C9D/CF5
- no blind SVC `#0xAB`
- no random 10165/10102 spray
- probes marked observe-only / not product success

## Artifacts

- `out/e8l_tmp/dispatch_abi.md` / `dispatch_abi.json` / `e8l_bp_spec.txt`
- `logs/stage_e8l_case156_*_stdout.txt` / `logs/stage_e8l_case310_D_null_stdout.txt`
- `tools/e8l_10102_dispatch_abi.py`
- `RUN_E8L_DISPATCH_ABI.ps1`
- runtime: `JJFB_E8L_10102_REGS` / `R1`/`R2`/`R3` in `robotol_flag_writer_trace.c`
