# Stage E8H — Bootstrap Dispatcher Provenance + SVC #0xAB

## Verdict

`BOOTSTRAP_DISPATCHER_NEVER_ENTERED`

Secondary SVC classify: `SVC_AB_POST_GATE_REQUIRED`

## Gates

| Gate | Result |
|------|--------|
| dispatcher_svc_xref.md | yes |
| dispatcher BP armed | yes (n=8) |
| 0x300714 hit (product) | **no** |
| 0x30103C hit (product) | **no** |
| 0x3020C8 hit (product) | **no** |
| 0x302340 hit (product) | **no** |
| writer 0x2F4E82 hit | **no** |
| SVC #0xAB (product) | **no** |
| SVC #0xAB (CF ALL) | **yes** (observe-only trap) |
| DRAW | no |
| audit | ok |
| jjfb hash | unchanged |

## Line A — dispatcher (TARGET_OBSERVED)

### Static path

```
0x300158 (many callers)
  → BL @0x3002C0
0x300714  loads *(R9+(0x800+0xD0)); saves incoming arg in r4
  → if state == 38: 0x300816 → 0x300EF0 → 0x30103A
0x30103A  MOVS r0,r4
0x30103C  BL → 0x3020C8
0x3020C8  MOVS r4,r0; CMP r4,#13/#2/#5/#8/#12/#17/#18/#20 …
  → arm reaches 0x302340 / 0x302362 → BL 0x2F4E64 → C44 writer 0x2F4E82
```

Important: `0x30103C` is a **BL site inside** dispatcher `0x300714`, not a standalone function.  
`r4` inside `0x3020C8` is the **original argument** to `0x300714`.

### Live product (tick 25)

- All 8 dispatcher-chain BPs: **NEVER** (`disp_hit=0`)
- Tick1 snap: `R9+(0x800+0xD0) = 0` (state word cold)
- No DRAW

So the C44 bootstrap writer stays cold because **the entire dispatcher `0x300714` never runs**, not because a branch inside it failed after entry.

## Line B — SVC #0xAB (observe-only)

### Static

- Only **one** `SVC #0xAB` in robotol: `0x2D92AE` inside stub `0x2D92A4`
- Stub ABI:
  - `STRB r0,[sp]` — original request byte
  - `MOVS r0,#3` — service selector
  - `MOV r1,sp` — arg block
  - `SVC #0xAB`
- Upstream BL callers: `0x2D91E0`, `0x2D91EE`, `0x2D9202`, `0x2D920E`  
  Reconstructed original `r0` at callers ≈ `#10`

### Live classify

| Path | SVC #0xAB |
|------|-----------|
| Product (flags natural) | NEVER through tick 25 |
| CF ALL (`C44=C9D=CF5=1`) | **HIT** via CODE trap @`0x2D92AE` |

CF trap dump (COUNTERFACTUAL_ONLY):

- `pc=0x2D92AE` `lr=0x2D91E5` `r0=3` `r1=sp` `sp_byte=0x0A`
- arg block first byte `0x0A` matches reconstructed caller request `#10`
- Trap **stopped** without fake success (`JJFB_E8H_SVC_AB_STOP`)

Classification: **`SVC_AB_POST_GATE_REQUIRED`** — needed after idle flags unlock, not on the normal product idle path.

## Minimal handler proposal (not implemented)

Still deferred. Evidence so far (HYPOTHESIS pending callsite decode):

| Field | Observed |
|-------|----------|
| service selector `r0` | always rewritten to `3` by stub |
| request byte `[sp]` / `[r1]` | `0x0A` at live CF callsite |
| return policy | **unknown** — must not blind-return success |

Next implementation must be justified by decoding what `#10` / service `3` mean across all four stub callers — not by “return 0 and continue”.

## Next gap

1. **Who should call `0x300158` / `0x300714`?** Parent has many upstream BLs; product never enters. Discriminate app-init / resource-ready / network / platform return.
2. **What writes `R9+(0x800+0xD0)` to 38?** Required for the `0x30103C` arm once dispatcher runs.
3. **Justified `SVC #0xAB` host bridge** only after request-code taxonomy (`sp_byte` / service `3`) is derived — still post-gate relative to product idle.

## Artifacts

- `tools/e8h_dispatcher_svc_xref.py`
- `RUN_E8H_DISPATCHER_SVC.ps1`
- `src/runtime/robotol_flag_writer_trace.c` (E8H dispatcher BP + SVC trap)
- `out/e8h_tmp/dispatcher_svc_xref.md`
- `logs/stage_e8h_jjfb_stdout.txt`
- `logs/stage_e8h_cf_all_stdout.txt`
