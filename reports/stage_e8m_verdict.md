# Stage E8M Verdict

**Verdict:** `PARENT_BRANCH_CONDITION_UNMET`

Evidence class: observe-only — **not** product success. Product still lacks natural `0x10102` delivery.

## One-line

Case156 already wakes `0x300158`, but parent switches on **`R9+0x8D0` (state)**; with `state==0` it takes the early `0x3004C8` arm and **never** `BL 0x300714`.

## Static (TARGET_OBSERVED)

| Fact | Detail |
| --- | --- |
| Switch subject | `LDR` via `R9+0x7D8+0x80+0x78` → **`R9+0x8D0`** |
| Incoming R0 (event code) | Saved in `r4`; does **not** select switch arm |
| Sole `BL 0x300714` | `0x3002C0` after `CMP r0,#20; BLT` on **state** |
| `state==0` | `BEQ` → `0x30026E` → **`0x3004C8`** → plat `0x304558` → POP |
| Gate for 714 | Default arm → `0x3002BA` with **state ≥ 20** |

Full dump: `out/e8m_tmp/parent_prereq.md`

## Live path traces (case156, R1=0/18/20)

All three identical control flow (46 insns):

```text
0x300158 entry (r4 = event code 0 / 0x12 / 0x14)
→ 0x300182 switch_first_cmp  (r0=state=0)
→ 0x300194 cmp_state_eq0
→ 0x30026E state0_branch_land
→ 0x3004C8 state0_arm
→ 0x3001B8 epilogue_pop
```

| Probe | r4 | state0_arm | gate_cmp20 | bl_300714 | DRAW |
| --- | --- | --- | --- | --- | --- |
| path_r1_0 | 0 | HIT | NEVER | NEVER | no |
| path_r1_18 | 0x12 | HIT | NEVER | NEVER | no |
| path_r1_20 | 0x14 | HIT | NEVER | NEVER | no |

**Path diff:** same path; R1=18/20 only changes `r4`. No semantic effect on reaching `0x300714` while state=0.

## Sequence probes A–F (observe-only)

| Seq | FE8 | B7D | E6C | R9_state | state0_arm | bl_300714 |
| --- | --- | --- | --- | --- | --- | --- |
| A 10165→156:18 | set | 1 | 0 | 0 | HIT | NEVER |
| B 10165→156:20 | set | 1 | 0 | 0 | HIT | NEVER |
| C 310→156:18 | 0 | 0 | 0 | 0 | HIT* | NEVER |
| D 310→156:20 | 0 | 0 | 0 | 0 | HIT* | NEVER |
| E 10165+310→156:18 | set | 1 | 0 | 0 | HIT* | NEVER |
| F 10165+310→156:20 | set | 1 | 0 | 0 | HIT* | NEVER |

\*Parent still entered with `state=0`; C/D/E/F logs show parent HIT / state=0; some runs truncated mid-156 by stop pattern after first FIRE_DONE, but no run hit gate/`0x300714`.

**Conclusion:** FE8/B7D from 10165 and case310 prep do **not** raise `R9+0x8D0` or fill `E6C`. Sequence alone does not unlock `0x300714`.

## Case310 / E6C (secondary)

- Live: all probes leave **`E6C=0`** → case310 keeps E6C-null arm `0x2DFCAC`.
- Static: **6** STR-to-`[R9+0xE6C,#0]` base writers (see `parent_prereq.json`); none observed hit in these diagnostics.

## Prerequisites for `0x300158 → 0x300714`

| Check | Role | Live after probes |
| --- | --- | --- |
| `R9+0x8D0` | **switch / gate** | **0 — blocking** |
| `R9+0x7D8` | base used to load state | touched (`0x10000` after parent) |
| `R9+0xFE8` | optional queue token | set only after 10165; not sufficient |
| `R9+0xB7D` | status | 1 after 10165; not sufficient |
| `R9+0xE6C` | case310 object | always 0 |
| event code r4 | arg to 714 **if** gate passed | 0/18/20 present but unused for branch |

## Verdict selection

| Candidate | Result |
| --- | --- |
| `PARENT_BRANCH_CONDITION_UNMET` | **ACCEPTED** — `state==0` → state0 arm |
| `PARENT_REQUIRES_QUEUE_SETUP` | secondary label only (7D8 path exists but subject is 8D0) |
| `PARENT_REQUIRES_FE8_TOKEN` | rejected — FE8 set, still state0 |
| `PARENT_REQUIRES_B7D_STATUS` | rejected — B7D=1, still state0 |
| `PARENT_REQUIRES_E6C_OBJECT` | not required for 714 path; needed for case310 main arm |
| `CASE_SEQUENCE_REQUIRED` | rejected — A–F do not unlock 714 |
| `PARENT_REACHED_300714_NEXT_GAP` | no |
| `DRAW_REACHED` | no |
| `MISSING_NATURAL_10102_DELIVERY` | still true for **product**; not the E8M diagnostic verdict |

## Ranking after E8M

1. **Who writes `R9+0x8D0` to a non-zero / ≥20 value before parent re-entry?** (chicken-egg with dispatcher)
2. Natural `0x10102` delivery (product)
3. E6C object init (case310 secondary)

## Forbidden (held)

- no force of `R9+0x8D0` / C44/C9D/CF5 as product
- no blind SVC `#0xAB`
- no treating diagnostic probes as boot success

## Artifacts

- `out/e8m_tmp/parent_prereq.md` / `parent_prereq.json`
- `logs/stage_e8m_path_*` / `logs/stage_e8m_seq_*`
- `tools/e8m_parent_prereq.py` / `RUN_E8M_PARENT_PREREQ.ps1`
- runtime: `JJFB_E8M_PARENT_TRACE`, `JJFB_E8M_SEQ`
