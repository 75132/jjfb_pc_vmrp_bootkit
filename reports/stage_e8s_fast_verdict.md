# Stage E8S-Fast Verdict (SpeedPatch)

**Verdict:** `POST_C44_BLOCKED_BY_C9D`

**Static co-claim:** `POST_C44_REQUIRES_UI_INIT` — UI-init `0x2E4788` never hit; it also **rejects state=38**.

**Mode:** quick=1 (2 cases) timeout=75s insn=500000 — wall ~112s total.

**NOT product success.**

## SpeedPatch

- Default quick: only `B_unlock_before` + `D_unlock_10165`.
- Full matrix: `JJFB_E8S_FULL_MATRIX=1` or `-FullMatrix`.
- Deep: `JJFB_E8S_DEEP=1` (tick 600 / insn 5e6).
- Each case prints `== E8S_CASE_DONE ...`; outer kill at timeout+15s.

## Quick matrix

| Case | Verdict | Elapsed | C44=1 | C9D | CF5 | UI | DRAW | SVC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| B_unlock_before | POST_C44_BLOCKED_BY_C9D | 63.8s | True | False | False | False | False | False |
| D_unlock_10165 | POST_C44_BLOCKED_BY_C9D | 44.3s | True | False | False | False | False | False |

## Evidence

- `[JJFB_FAST_UNLOCK_DONE] ... C44_after=0x1`
- `[JJFB_E8R_C44_UNLOCKED] C44=1 via=0x2FC8C0`
- `[JJFB_E8F_FLAG_SNAP] reason=after_fast_unlock ... C44=0x1 C9D=0x0 CF5=0x0`
- No C9D/CF5 transition; no UI-init; no DRAW; no SVC.

## Interpretation

- After real FAST `C44=1`, next blocker is **C9D (and CF5) still 0**.
- 10165 before unlock does not flip C9D/CF5.
- Natural UI-init still required for product path; `state=38` is rejected by `0x2E4788`.
- Do not prioritize SVC.

## Static (Lane B/C)

- CF5 write_1: `0x2E7DC2` (fn `0x2E7DA8`, caller `0x2E32A2`).
- C9D write_1 via lit `0xC9C`+imm: `0x2F097A` (C9C+1), `0x2FB008`, `0x30AA42`.
- UI-init `0x2E4788`: rejects `{38,46,69,252,300}`; needs ED8!=0, CA3==1; then BL `0x2FC8C0` x7.

## Next (only if needed)

- Deep one case with `JJFB_E8S_DEEP=1` if tick2 persistence needed.
- Or `FAST_UI_INIT_CALL` with `UI_STATE!=38` (already coded; not in quick default).
- Full matrix only with `-FullMatrix`.

## Artifacts

- `reports/stage_e8s_speed_summary.jsonl`
- `logs/stage_e8s_B_unlock_before_stdout.txt`
- `logs/stage_e8s_D_unlock_10165_stdout.txt`
- `out/e8s_tmp/e8s_deps.md`
- `RUN_E8S_FAST.ps1`
