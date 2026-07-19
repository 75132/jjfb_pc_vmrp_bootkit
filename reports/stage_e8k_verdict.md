# Stage E8K — Event-Switch 0x30D300 Upstream + B7D Drain

## Verdict

`EVENT_SWITCH_CASE_DERIVED_NEXT_GAP`

Co-claims:

- `EVENT_SWITCH_CALLER_NEVER_REACHED` — product never naturally enters `0x30D300`
- `B7D_DRAIN_CALLER_NEVER_REACHED` — drain `0x2DC80C` / timer-callback gate never hit (product + 10165)
- `MISSING_APP_INIT_DISPATCH` — `0x10102` family handler is registered but host never delivers into it

## Gates (product)

| Gate | Result |
|------|--------|
| event_switch_xref.md | yes |
| BP armed | yes (n=16) |
| `0x30D300` entry | **no** |
| hot clusters | **no** |
| drain `0x2DC80C` | **no** |
| drain gate callback | **no** |
| parent `0x300158` | **no** |
| state writes | **0** |
| DRAW | no |
| `0x10102` after register | **only REGISTER** (census count=2 at tick1; never delivered) |
| jjfb hash | unchanged |
| audit | ok |

## 10165 probe

| Gate | Result |
|------|--------|
| probe done | yes (FE8 write, B7D=1) |
| `0x30D300` | **no** |
| drain `0x2DC80C` | **no** |
| drain gate | **no** |
| hot cluster | **no** |

Confirms E8J: 10165 FE8/B7D is not the bridge into event-switch or B7D drain.

## Case-310 probe (observe-only `0x10102` with R0=310 / `0x136`)

| Gate | Result |
|------|--------|
| fire | yes (`handler=0x30D301`) |
| `0x30D300` entry | **HIT** (`r0=0x136`) |
| case arm `0x30D72E` / `0x30D730` | **HIT** |
| hot `0x2DFC3C` | **HIT** (`lr=0x30D735`) |
| parent `0x300158` | no (this case does not BL parent) |
| state / C44/C9D/CF5 | still 0 after fire |
| DRAW | no |

This is **COUNTERFACTUAL / observe-only** (not claimed as product success). It proves the derived switch index is real: delivering the registered family handler with R0=case310 reaches the hot cluster that product never wakes.

## Static (TARGET_OBSERVED)

See `out/e8k_tmp/event_switch_xref.md`:

- **0 in-module BL callers** of `0x30D300`; **0** absolute function-pointer literals in ext/MRP
- **Caller identity:** plat `0x10102` register saves Thumb `0x30D301` as family handler (`family=0x1E200`); `host_drain=no`
- Switch: index=**R0**, bound=`0x157`, table `@0x30D324`
  - **case 310 (`0x136`) → `BL 0x2DFC3C`**
  - **case 156 (`0x9C`) → `BL 0x300158`**
- B7D drain: sole BL into `0x2DC80C` is from the timer/callback gate function; upstream BL site `0x2F5734`

## Live product meaning

The missing wake is not “wrong R0 inside `0x30D300`” on the product path — **`0x30D300` is never called at all** because nothing delivers `0x10102` into the registered handler after REGISTER.

Host lifecycle only drains `0x10140`. Family handler `0x10102` stays inert.

## Ranked hypotheses (post-live)

1. **`MISSING_APP_INIT_DISPATCH` / who delivers `0x10102`?** — primary product gap: Mythroad/app-init/event source that should invoke `0x30D301` with a case index (e.g. 310 / 156 / others)
2. **`EVENT_SWITCH_CASE_DERIVED_NEXT_GAP`** — case table proven; next is natural producer ABI (R1–R3 / payload) and which cases boot needs (310 alone did not unlock state/DRAW)
3. **`B7D_DRAIN_CALLER_NEVER_REACHED`** — timer/callback gate into `0x2DC80C` also cold; parallel queue-consumer gap
4. **`MISSING_PLATFORM_SIDE_EFFECT_STATE_38`** — still downstream of cold parent dispatcher
5. **`MISSING_RESOURCE_READY_DISPATCH` / `MISSING_NETWORK_READY_DISPATCH`** — still HYPOTHESIS only

## What this rules out

- Not “force state=38 / idle flags”
- Not “keep spraying 10165 event codes”
- Not “FE8 external consumer” (still none)
- Not “`0x30D300` needs an in-module BL we missed” — registration path is the caller

## Next gap (E8L candidate)

1. Find **who should deliver into `0x10102` / `0x30D301`** on real devices (app-init, resume, ext chunk events, Mythroad `sendAppEvent` producers) — without blind spray; prefer xref of guest sites that call plat `0x10102` with a *handler invocation* shape vs REGISTER shape
2. Derive full ABI for family delivery (R0=case already known; R1/R2/R3 / stack args at `0x30D300` prologue)
3. Probe **case 156** observe-only (direct path to `0x300158`) as discrimination — still not product
4. Parallel: why timer/callback gate never enters B7D drain

## Forbidden kept

No product force of state/idle flags, no blind SVC `#0xAB`, no fake DRAW/UI, no MRP/EXT edits, no return to 21002/25B/gamelist unless xref proves network/update dependency.

## Artifacts

- `tools/e8k_event_switch_provenance.py`
- `RUN_E8K_EVENT_SWITCH.ps1`
- `out/e8k_tmp/event_switch_xref.md` / `e8k_bp_spec.txt`
- `logs/stage_e8k_product_stdout.txt`
- `logs/stage_e8k_probe10165_stdout.txt`
- `logs/stage_e8k_case310_stdout.txt`
- `src/runtime/robotol_flag_writer_trace.c` (`JJFB_E8K_10102_CASE`)
- `src/runtime/robotol_idle_watch.c` (10102 trampoline note)
