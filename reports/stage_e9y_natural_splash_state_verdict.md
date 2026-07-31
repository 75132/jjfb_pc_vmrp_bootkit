# Stage E9Y Natural Splash State Verdict

- **Stage**: E9Y — natural splash state (AC8 / workbuf / event)
- **Product success**: **NO** (`NOT_PRODUCT`)
- **Evidence**: OBSERVED

## Cases run

| Case | Mode | Verdict | Elapsed |
|------|------|---------|---------|
| state_ac8_chain | state | `AC8_STILL_DISPLAYFIRST_ONLY` (analyzer initially tagged C9D; CSV shows C9D=0 + AC8=0 at splash) | ~100s |
| workbuf_30CBBC | workbuf | **`WORKBUF_30CD82_ALLOC_REACHED`** | ~125s |

## Lane A — AC8 runtime state

- DEBUG AC8: **off** (forbidden in E9Y)
- Natural AC8>0: **not found**
- Splash `0x2EF86C` reached with AC8=0 → loading-only at `0x2EF8AE`
- State chain (CSV): `pre_splash` → `ui_init_2FC418` → `splash_enter` → `ac8_branch(loading_only)`
- Snapshot at splash: `ui=0x45 C44=1 C9D=0 CF5=0 AC8=0 8D8=0`
- AC8 mem write observed once: `writer_pc=0x2FE84C` **new=0** (clear / keep-zero), not a >0 event source
- `JJFB_FAST_REAL_SPLASH_STATE_EVENT` flag present; real event that sets AC8>0 **not yet identified**

**Verdict (Lane A):** `AC8_STILL_DISPLAYFIRST_ONLY`

## Lane C — workbuf natural alloc

With `JJFB_PLATFORM_WORKBUF_ALLOC=1` (call real `0x30CBBC`):

- `WORKBUF_30CBBC_ALLOC_REACHED`
- `WORKBUF_30CD82_ALLOC_REACHED` — `ptr=0x2AC8FC` via guest `0x2d99ac`
- Later state shows `8D8=0x2AC8FC` (no seed)
- Seed path blocked: `WORKBUF_SEED_BLOCKED_NEED_30CD82`

**Verdict (Lane C):** `WORKBUF_30CD82_ALLOC_REACHED` (+ natural 8D8 populated after store)

## Lane D — no_debug full splash

Not yet green: without DEBUG AC8, logo branch still blocked even when 8D8 is natural.

## Lane E — timer (secondary)

During workbuf case, **`PROGRESS_TIMER_2F55FA_REACHED`** (`lr=0x3056D5`) while AC8 still 0 — timer alone does not raise AC8.

## Artifacts

| Kind | Path |
|------|------|
| State CSV | `reports/e9y_ac8_state_chain_trace.csv` |
| Workbuf CSV | `reports/e9y_workbuf_alloc_trace.csv` |
| Event/timer CSV | `reports/e9y_event_timer_trace.csv` |
| Validation | `reports/e9y_no_debug_validation.csv` |
| Log | `logs/e9y_natural_splash_state_stdout.txt` |
| Runner | `RUN_E9Y_NATURAL_SPLASH_STATE.ps1` |

## Next (still E9Y)

1. Find real event/init that sets **AC8>0** (not STR scan; follow post-`0x2FE84C` / update-check / lifecycle).
2. Wire `JJFB_FAST_REAL_SPLASH_STATE_EVENT` to that real fn when ABI is proven.
3. Run `-Mode no_debug` for `SPLASH_LOGO_BRANCH_WITHOUT_DEBUG_AC8` / full parity without seed.

## Forbidden (held)

- No AC8 poke / 8D8 seed as success
- No BA0+0x2C / C9D poke as success
- No show1/slogo hardcode / fake UI / MRP edits
