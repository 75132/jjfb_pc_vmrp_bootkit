# Stage E8N Verdict

**Verdict:** `STATE_LADDER_DERIVED_NEXT_GAP`

Co-claim (product / natural path): `R9_8D0_WRITER_NEVER_REACHED`

CF state pokes and case156 fires are **COUNTERFACTUAL_ONLY / observe-only — not product success**.

## One-line

Legal ladder is now mapped: **`state>=20` unlocks `BL 0x300714`; `state==38` unlocks `0x30103C`**. Natural writers still never run; strongest static candidates sit on the cold **B7D drain** path (`0x2DC80C`).

## Static writers (`R9+0x8D0`)

| Metric | Value |
| --- | --- |
| Direct STR writers | **75** |
| Adjacent imm nonzero / ≥20 / ==38 | 58 / 47 / **0** |
| Via `7D8+0x80+0x78` alias stores | 0 |
| E6C base writers | 6 |
| Shared state∩E6C enclosing fns | **none** |

No adjacent `MOVS #38` at store sites — **38 is not a naked immediate store**; likely computed/copied inside dispatcher or a colder routine.

### High-value writer candidates (TARGET_OBSERVED)

| STR PC | fn | imm | Notes |
| --- | --- | --- | --- |
| `0x2DC94E` | **`0x2DC80C`** (B7D drain) | **46** | ≥20 → would unlock 714 |
| `0x2DD512` | `0x2DD068` | **20** | exact gate floor |
| `0x2DD440` / `0x2DD478` / `0x2DD646` | `0x2DD068` | 37 / 30 / 27 | ≥20 |
| others | various | 1 / 255 / … | bootstrap or sentinel |

E8K/E8M already showed **`0x2DC80C` NEVER** on product and diagnostic sequences → explains cold `R9+0x8D0`.

Full list: `out/e8n_tmp/state_writers.md` / `state_writers.json`

## State0 arm `0x3004C8` (static + live)

- `BL 0x304558` (plat helper) then epilogue
- **Does not** write state, call `0x300714`, or touch E6C
- Pure “state still idle” handler — expects an **external** writer before the next parent entry

## CF ladder (COUNTERFACTUAL_ONLY + case156 R1=18)

| CF state | Parent path | `0x300714` | `0x30103C` |
| --- | --- | ---: | ---: |
| 1 | special arm (no gate BP) | no | no |
| 2 | default → gate; **BLT** (2&lt;20) | no | no |
| 19 | default → gate; **BLT** (19&lt;20) | no | no |
| **20** | gate → **`BL 0x300714`** | **HIT** | no |
| **38** | gate → 714 → **`0x30103C`** | **HIT** | **HIT** |

Derived ladder:

```text
0      → state0 arm 0x3004C8 (no 714)
1      → dedicated arm (no 714)
2..19  → may reach CMP#20 but BLT skip
>=20   → BL 0x300714   (event code in r4)
38     → inside 0x300714 → 0x30103C
```

**Do not** product-force 0→38; CF only maps the ladder.

Live evidence (cf_state_20):

```text
[JJFB_E8N_CF_STATE] old=0x0 new=0x14 ...
[JJFB_E8M_PARENT_PATH] ... note=gate_cmp20
[JJFB_E8M_PARENT_PATH] ... note=bl_300714
[JJFB_E8I_PARENT_HIT] tag=p300714 ...
```

## Natural / sequence path

| Run | R9_state | state writes | 714 | notes |
| --- | --- | --- | --- | --- |
| E8I product (prior) | 0 | 0 through tick≥100 | no | writers never entered |
| E8M seq A–F | 0 | 0 | no | FE8/B7D ≠ state |
| E8N seq_E 10165+310+156 | 0 | 0 | no | case310 still E6C-null; `0x2DC80C` NEVER |

## E6C vs state

- No shared enclosing function between E6C base writers and state writers
- Object init and state bootstrap are **separate** cold paths
- case310 before case156 does not raise state

## Verdict selection

| Candidate | Result |
| --- | --- |
| `STATE_LADDER_DERIVED_NEXT_GAP` | **ACCEPTED** — CF maps ≥20→714 and 38→30103C |
| `R9_8D0_WRITER_NEVER_REACHED` | **co-claim** for product/natural |
| `R9_8D0_WRITER_BRANCH_UNMET` | not chosen — writers never entered, not failed mid-fn |
| `R9_8D0_REQUIRES_OBJECT_INIT` | rejected as required for state (no shared fn) |
| `R9_8D0_REQUIRES_QUEUE_SETUP` | secondary — drain/`0x2DC80C` still cold after 10165 FE8 |
| `STATE_REACHED_300714_NEXT_GAP` | CF only; not natural |
| `DRAW_REACHED` | no |

## Ranking after E8N

1. **Reach a real state writer** — especially `0x2DC80C` (imm 46) / `0x2DD068` (imm 20…)
2. Why B7D drain never runs after 10165 sets B7D=1
3. Natural `0x10102` delivery (still missing on product)
4. E6C object init (parallel, not state-shared)

## Forbidden (held)

- CF poke is **not** product success
- no force C44/C9D/CF5, no SVC `#0xAB`, no fake DRAW
- no treating diagnostic case156 as boot success

## Artifacts

- `out/e8n_tmp/state_writers.md` / `state_writers.json` / `e8n_bp_spec.txt`
- `logs/stage_e8n_cf_state_*_stdout.txt`
- `logs/stage_e8n_seq_E_10165_310_156_18_stdout.txt`
- `tools/e8n_state_ladder.py` / `RUN_E8N_STATE_LADDER.ps1`
- runtime: `JJFB_E8N_CF_STATE`, enhanced `JJFB_E8N_STATE_WRITE` / old value on watch
