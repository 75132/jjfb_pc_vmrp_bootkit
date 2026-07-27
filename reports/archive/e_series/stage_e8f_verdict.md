# Stage E8F — Idle Flag Writer Callpath / Upstream Conditions

## Verdict

`WRITER_FUNCTION_NEVER_ENTERED`

Secondary diagnostic (COUNTERFACTUAL_ONLY): `FLAG_GATE_CONFIRMED_NEXT_GAP`

## Gates

| Gate | Result |
|------|--------|
| static writer_callpath.md | yes (21 writers) |
| writer BP armed | yes (n=23) |
| writer HIT live | **no** (0 hits through tick 25) |
| writer NEVER | **yes** (all 21 writers + longpath/fe8 hooks) |
| longpath 0x30D28C HIT | no (10165 not fired this stage) |
| queue_depth R9+0x844 | 0x0 (long path would be eligible if enqueue ran) |
| sibling 10162 ZERO_ARGS | ok=1, **no** C44/C9D/CF5/FE8/B7D change |
| sibling 10102 ZERO_ARGS | ok=1, **no** flag change |
| counterfactual ALL | poked C44=C9D=CF5=1 |
| CF next 10140 | **UC_ERR_EXCEPTION** @ `0x2D92B0` (r0=3) — not DRAW |
| DRAW | no |
| audit_launcher_core | ok |
| jjfb.mrp SHA-256 | unchanged |

## Primary finding (TARGET_OBSERVED)

All top E8D writer PCs for `C44/C9D/CF5` stayed **NEVER_ENTERED** across a full lifecycle window (tick≥25), including while:

- host drained only `0x10140`
- observe-only `0x10162` / `0x10102` ZERO_ARGS fires completed

So idle flags stay 0 because **their writer functions never run**, not because 10165 event-code probing failed.

### Notable callpath detail

- C44 site `0x2E87F2` lives in `0x2E87B4..0x2E87F8`; sole BL caller `0x3066B8` sits in the **idle poll** after `LDRSB [R9+C44]; CMP #1; BNE`.
  - That BL runs only when **C44 is already 1** → this path is a post-set consumer/maintainer, not the bootstrap setter.
- Other C44/C9D/CF5 writers have robotol-internal callers (e.g. `0x2F4E82` ← `0x302340`/`0x302362`) that also never execute in product long-run.

### Sibling handlers

- `0x10162` @ `0x30D249` is a **stub**: `MOVS r0,#0; BX lr` (even `0x30D248`). It is not an enqueue twin of `0x30D24C`.
- `0x10102` @ `0x30D301` is a larger PUSH-frame routine; ZERO_ARGS fire returned ok with **no** idle-flag side effects.

## Counterfactual (COUNTERFACTUAL_ONLY — not product)

Run: `JJFB_E8F_COUNTERFACTUAL=ALL` after tick1.

| Step | Observation |
|------|-------------|
| poke | C44=1, C9D=1, CF5=1 |
| next 10140 (tick2) | `ok=0` `UC_ERR_EXCEPTION` pc=`0x2D92B0` |
| DRAW | no |

Interpretation: the three idle bytes **are a real gate** (forced values change control flow), but the taken path is **incomplete / second-layer missing** → fault. Do **not** ship flag forcing.

## Long path / 0x101AB

Static notes in `out/e8f_tmp/long_path_101ab.md`. Live: queue depth was 0, but E8F did not fire 10165; longpath/fe8 CODE hooks therefore also NEVER (expected). E8E already showed FE8/B7D side effects under 10165 probe.

## Next gap

Trace **who should call the bootstrap writers** (not the C44==1 idle maintainer):

1. Callers of `0x2F4E82` / `0x2FEDFA` / `0x2E7DBC` / C9D wider-store sites — why never entered.
2. Preconditions on those callers (plat return, resource, session struct init covering `0xC9D`).
3. Exception site `0x2D92B0` after forced flags — what second gate it assumes.

## Artifacts

- `tools/e8f_writer_callpath.py`
- `src/runtime/robotol_flag_writer_trace.c`
- `RUN_E8F_IDLE_FLAG_WRITER.ps1`
- `out/e8f_tmp/writer_callpath.md` / `.json`
- `out/e8f_tmp/sibling_handler_disasm.md`
- `out/e8f_tmp/long_path_101ab.md`
- `logs/stage_e8f_jjfb_stdout.txt` (primary)
- `logs/stage_e8f_cf_all_stdout.txt` (counterfactual)
