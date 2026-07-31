# Stage E8G — Bootstrap Caller Provenance + Second-Gate Fault

## Verdict

`BOOTSTRAP_CALLER_NEVER_ENTERED`

Secondary diagnostic (COUNTERFACTUAL_ONLY): `COUNTERFACTUAL_SECOND_GATE_CONTEXT_MISSING`

## Gates

| Gate | Result |
|------|--------|
| bootstrap_caller_xref.md | yes (15 caller sites) |
| fault_2d92b0.md | yes (SVC #0xAB refined) |
| handler_10102_abi.md | yes (jump-table style; no direct BL to bootstrap set) |
| caller BP armed | yes (n=26) |
| caller HIT (normal) | **no** — `caller_hit=0` / `caller_never=26` at tick 25 |
| second-gate fault dump | yes (CF ALL) |
| DRAW | no |
| audit | ok |
| jjfb hash | unchanged |

## Line A — bootstrap callers never enter (TARGET_OBSERVED)

Priority callers all NEVER during product long-run:

- `0x302340` / `0x302362` (C44 writer `0x2F4E82` path; fn uses R9+`0xC6C`/`0xEEC`/`0x11D0`, CMP r4 cases; upstream BL `0x30103C`)
- `0x2FC048` (→ `0x2FED14` family for `0x2FEDFA`)
- `0x2E32A2` (CF5 `0x2E7DBC`)
- `0x2EFF1C` / `0x2F08A4` / `0x2F0D6A` / `0x2F1FF0` (C9D `0x2F097A`)
- `0x30D9EE`, `0x30AF8A`, `0x30DF78`, …

So writers stay cold because **their callers never run**, not because STR predicates fail after entry.

## Line B — counterfactual second gate (TARGET_OBSERVED)

Forcing `C44=C9D=CF5=1` → next 10140 faults:

- pc=`0x2D92B0`, r0=`3`, lr=`0x2D91E5`, `UC_ERR_EXCEPTION`
- Stub at `0x2D92A4` does `MOVS r0,#3; MOV r1,sp; **SVC #0xAB**`
- Class: **unimplemented software interrupt / missing platform SVC**, not a missing byte flag

Therefore flags are a real idle gate, but post-gate progress needs **SVC/platform (or the real init that avoids this trap)** — confirming “flags ≠ complete state”.

## 0x10102 (static)

- Handler `0x30D301` is a large PUSH-frame with table/branch flavor (`ADD pc,r3` style region).
- Static scan: **no** direct BL into the prioritized bootstrap caller set.
- Do not fire guessed 10102 cases until table index ABI is derived (`MISSING_10102_INIT_CASE` not claimed).

## Next gap

1. Trace **why `0x30103C` never calls into `0x3020C8`/`0x302340`** (bootstrap dispatcher for C44 sample).
2. Decide product handling for **SVC #0xAB** (document / implement / prove alternate path that never issues it).
3. Optional CF matrix (C44-only …) via `RUN_E8G_BOOTSTRAP_CALLER.ps1 -Matrix` — ALL already shows the SVC second gate.

## Artifacts

- `tools/e8g_bootstrap_caller_xref.py`
- `RUN_E8G_BOOTSTRAP_CALLER.ps1`
- `out/e8g_tmp/bootstrap_caller_xref.md`
- `out/e8g_tmp/fault_2d92b0.md`
- `out/e8g_tmp/handler_10102_abi.md`
- `logs/stage_e8g_jjfb_stdout.txt`
- `logs/stage_e8g_cf_all_stdout.txt`
