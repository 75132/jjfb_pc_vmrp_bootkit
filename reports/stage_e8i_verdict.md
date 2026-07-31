# Stage E8I — Dispatcher Parent Caller Census + State Provenance

## Verdict

`DISPATCHER_PARENT_CALLER_NEVER_REACHED`

Co-claim (same idle window): `R9_8D0_WRITER_NEVER_REACHED`

## Gates

| Gate | Result |
|------|--------|
| parent_census.md | yes (85 callers) |
| parent BP armed | yes (n=92 = chain + all BL sites) |
| state watch armed | yes (`R9+(0x800+0xD0)` @ `0x2B2128`) |
| 0x300158 hit | **no** |
| 0x300714 hit | **no** |
| 0x30103C hit | **no** |
| any parent-chain BL hit | **no** (`parent_hits=0`) |
| state writes | **0** through tick ≥100 (run reached tick 229) |
| state == 38 | **no** (stays 0) |
| DRAW | no |
| audit | ok (after audit-safe spelling) |
| jjfb hash | unchanged |

## Line A — who should call `0x300158`?

### Static census (TARGET_OBSERVED)

- **85** upstream BL callers of `0x300158`
- R0 before BL is almost always a **constant event/state code**:
  - `#18` × 75
  - `#20` × 6
  - **no** caller passes R0=`38`
- Important split:
  - Parent **argument** R0 ∈ {18,20,…} feeds `0x3020C8`’s `CMP r4,#…` cases
  - Gate value **38** is `*(R9+(0x800+0xD0))`, not the BL argument

### Dispatcher table (TARGET_OBSERVED)

Inside `0x300714`:

- loads state word from `R9+(0x800+0xD0)`
- `CMP r0, #38` → `BEQ 0x300816` → `0x30103A` / `0x30103C` → `BL 0x3020C8`

### Live product

- 92 CODE BPs armed (parent entry + dispatcher chain + all 85 callers)
- Through tick 25 / 40 / 100 / 229: **`parent_hits=0`**
- So the missing step is **not** “wrong R0 once inside parent” — **nothing calls parent at all** during product idle

## Line B — who writes state=`38`?

### Static writers

- ~96 LDR`=(0x800+0xD0)` + nearby STR* candidates
- No adjacent `MOVS #38` / `LDR #38` pattern found next to those stores
- Value 38 is likely **computed / table-driven / copied**, not a naked immediate at the store site

### Live watch

- Armed before/at tick1: `addr=0x2B2128` init=`0`
- `state_writes=0` for the entire observe window
- Snaps remain `val=0` after mrc_init, tick1, and every lifecycle fire sampled

Therefore: state stays cold because **no writer runs**, not because a writer runs and chooses a non-38 value.

## Ranked hypotheses (post-live)

1. **`MISSING_APP_INIT_DISPATCH`** — primary: none of 85 callers of `0x300158` fire after robotol entry / mrc_init / 10140 idle loop
2. **`MISSING_PLATFORM_SIDE_EFFECT_STATE_38`** / **`R9_8D0_WRITER_NEVER_REACHED`** — co-primary: state word never written
3. **`MISSING_QUEUE_CONSUMER_TO_DISPATCHER`** — still plausible: 10165 FE8/B7D path never shown to reach `0x300158` (not proven this stage)
4. **`MISSING_RESOURCE_READY_DISPATCH` / `MISSING_NETWORK_READY_DISPATCH`** — still HYPOTHESIS only; no live caller hit to promote

## What this rules out

- Not “need to force state=38 as product” (forbidden; also wouldn’t call parent by itself)
- Not “pick another 10165/10102 event” as next default
- Not SVC `#0xAB` (still POST_GATE only)

## Next gap (E8J candidate)

Trace **reachability into the 85 callers** from:

- registered platform handlers (10140 / other registered codes)
- robotol entry / method table after mrc_init
- any queue consumer after FE8/B7D

Smallest discriminating experiment: pick the hottest enclosing functions for R0=`18`/`20` callers (e.g. `0x2DFC3C`, `0x2E0E00`, `0x2DC778`) and BP **their** entries / upstream — find which boot path should unlock that cluster.

## Artifacts

- `tools/e8i_dispatcher_parent_census.py`
- `RUN_E8I_DISPATCHER_PARENT.ps1`
- `out/e8i_tmp/parent_census.md` / `parent_census_slim.json`
- `logs/stage_e8i_jjfb_stdout.txt`
- `src/runtime/robotol_flag_writer_trace.c` (E8I parent BP + state watch)
