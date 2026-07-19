# Stage E8J — Dispatcher Parent Caller Upstream Reachability

## Verdict

`PARENT_CALLER_CLUSTER_NEVER_REACHED`

Co-claims (probe-discriminated):

- `MISSING_APP_INIT_DISPATCH` — event-switch `0x30D300` (sole static case `BL 0x2DFC3C` in that table) never entered
- `QUEUE_CONSUMER_NEVER_REACHED` — external B7D drain path never hit after 10165 FE8/B7D side effects

## Gates (product)

| Gate | Result |
|------|--------|
| upstream_l2.md | yes |
| queue_consumer.md | yes |
| cluster BP armed | yes (n=111, hooks=108) |
| state watch | yes |
| FE8/B7D read watch | yes |
| hot cluster entry (`0x2DFC3C` / `0x2E0E00` / `0x2DC778`) | **no** |
| L2 upstream (non-enqueue) | **no** |
| queue consumer BP (external) | **no** |
| FE8/B7D mem-read watch hits | **no** |
| direct BL to `0x300158` | **no** |
| `0x300158` / `0x300714` | **no** |
| state writes | **0** (stays 0) |
| state == 38 | no |
| DRAW | no |
| max tick observed | ~249 (wall ~200s; summaries through tick_100 all-zero) |
| jjfb hash | unchanged |
| audit | ok (after spelling fix) |

## Probe run (10165 structured, observe-only)

| Gate | Result |
|------|--------|
| probe done | yes (`R0_EVENTCODE_2` ok=1) |
| FE8 write | yes (`writer_pc=0x30D262`, `fe8_after=0x85A9`) |
| B7D after | 1 |
| enqueue `0x30D24C` | **HIT** (tick1) |
| long-path `0x30D28C` | **HIT** (tick1) |
| enqueue-local queue sites `0x30D268` / `0x30D294` | **HIT** |
| hot cluster entry | **no** |
| event-switch `0x30D300` | **NEVER** |
| legacy B7D-drain gate | **NEVER** |
| parent `0x300158` | **NEVER** |
| state writes | 0 |

## Static (TARGET_OBSERVED)

See `out/e8j_tmp/upstream_l2.md` / `queue_consumer.md`:

- 85 BL → `0x300158` across ~50 enclosing fns after hot-fn resolve
- Hottest: `0x2E0E00` (many R0=#18), `0x2DFC3C` (R0=#18), `0x2DC778` (R0=#20)
- **FE8 has no external reader** — only enqueue core `0x30D24C` reloads its own store
- B7D readers: 8 sites; drain-ish path goes through `0x2DC80C` (timer/callback upstream)
- `0x30D730 BL 0x2DFC3C` is a **case arm inside event-switch `0x30D300`**, not inside 10165 enqueue body
- 10165 long path (`0x30D28C`…) ends via plat helpers / `0x2E4D6C` — does **not** BL hot parent clusters

## Live product

```
[JJFB_E8J_SUMMARY] tick_25/40/100:
  entry_hits=0 upstream_hits=0 queue_bp_hits=0 bl_hits=0 qread_hits=0
```

Entire L2 caller graph for parent dispatcher stayed cold through the observe window.

## Probe discrimination (critical)

10165 **does** execute and can take the long path, and **does** write FE8/B7D — but that is **not** sufficient to wake:

- `0x2DFC3C` / `0x2E0E00` / `0x2DC778`
- event-switch `0x30D300`
- external B7D consumer / drain gate
- `0x300158`

So the missing boot step is **earlier/elsewhere than “fire 10165 with a better event code.”**

## Ranked hypotheses (post-live)

1. **`MISSING_APP_INIT_DISPATCH`** — primary: who should enter event-switch `0x30D300` or otherwise call hot clusters after mrc_init / robotol entry?
2. **`QUEUE_CONSUMER_NEVER_REACHED`** — external B7D/`0x2DC80C` drain never runs (enqueue-local FE8 reload is not a dispatcher bridge)
3. **`MISSING_PLATFORM_SIDE_EFFECT_STATE_38`** — state word still never written (downstream of cold parent path)
4. **`MISSING_RESOURCE_READY_DISPATCH` / `MISSING_NETWORK_READY_DISPATCH`** — still HYPOTHESIS; no live cluster hit to promote
5. **`QUEUE_CONSUMER_BRANCH_UNMET`** — not selected: external consumer never entered (branch inside consumer not reached)

## What this rules out

- Not “force R9+state=38” (still forbidden; parent callers still cold)
- Not “another random 10165/10102 spray” as default next step — 10165 already proven insufficient to wake parent cluster
- Not SVC `#0xAB` (still POST_GATE only)
- Not “FE8 lacks a consumer” as the whole story — FE8 was never an external dispatch token; the real gap is who calls `0x30D300` / hot clusters / B7D drain

## Next gap (E8K candidate)

Trace **who should call event-switch `0x30D300`** (and/or other upstreams of `0x2DFC3C` / `0x2E0E00`):

- method-table / app-init after mrc_init
- registered non-10140 platform handlers
- timer/callback that should enter B7D drain (`0x2DC80C`) **after** a real enqueue producer fills the queue

Smallest discriminating experiment: census BL/callers of `0x30D300` + live BP on those upstreams during product boot.

## Forbidden kept

No product force of state word / C44/C9D/CF5, no blind SVC success, no fake DRAW/UI, no MRP/EXT edits, no return to 21002/25B/gamelist unless xref proves network/update dependency.

## Artifacts

- `tools/e8j_caller_upstream_reach.py`
- `RUN_E8J_CALLER_UPSTREAM.ps1`
- `out/e8j_tmp/upstream_l2.md` / `queue_consumer.md` / `e8j_bp_spec.txt`
- `logs/stage_e8j_product_stdout.txt`
- `logs/stage_e8j_probe10165_stdout.txt`
- `src/runtime/robotol_flag_writer_trace.c` (E8J role-tagged BP + FE8/B7D read watch)
