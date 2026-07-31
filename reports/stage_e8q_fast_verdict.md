# Stage E8Q-Fast Verdict

**Verdict:** `R1_20_SUCCESS_REACHED_NEXT_GAP`

**Static co-claim:** ``C44_NONZERO_WRITER_FOUND_NEXT_GAP`` — sole ``MOVS#1→C44`` site is ``0x2FC8CE`` in ``0x2FC8C0``.

**NOT product success.** Do not treat ``0x2F4E82`` as unlock. Success arm does not call the unlock writer.

## One-line

R1=20 success arm is live under FAST: ``0x30213E → STRB#9@1A8 → 0x301848 → 0x301864 → 0x304558(plat 0x1E213)``; state stays ``0x26`` (38); no C44 nonzero, no DRAW, no SVC.

## Static facts

- C44 unlock writer: ``0x2FC8CE`` (``STRB #1``), fn ``0x2FC8C0``.
- Unlock callers: ``0x2DB9DC``, ``0x2E4840``..``0x2E4B06`` UI-init cluster, ``0x30DDE2``.
- Historical ``0x2FC8B8`` is a different adjacent fn (writes state ``0x10D``), not STRB#1.
- ``0x301848``: gate on ``R9+0x858``; may ``BL 0x301864``; does **not** call ``0x2FC8C0``.
- ``0x304558``: plat bridge via ``BLX``; success arm uses plat code ``0x1E213`` (r1=2).
- ``0x2F4E82`` = C44 reset (write 0); never hit on this success-arm matrix.

## Matrix

| Run | 30213E | 301848 | 304558 | 2FC8C0 | 2FC8CE | 2F4E82 | C44T | DRAW | SVC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A_success | True | True | True | False | False | False | False | False | False |
| B_success_eec7c | True | True | True | False | False | False | False | False | False |
| C_10165_success | True | True | True | False | False | False | False | False | False |
| D_310_success | True | True | True | False | False | False | False | False | False |
| E_10165_310_success | True | True | True | False | False | False | False | False | False |
| F_success_5e6 | True | True | True | False | False | False | False | False | False |

## Evidence (A_success)

- `[JJFB_E8Q_ARM] tag=p301848 pc=0x301848 r0=0x14 r1=0x2 r2=0x0 r3=0x0 lr=0x301047 state=0x26 C44=0x0 R9_1A8=0x9 C6C22=0x1 EEC7C=0x0 DEC30=0x1 note=FAST_OR_OBSERVE evidence=OBSERVED`
- `[JJFB_E8Q_ARM] tag=p304558 pc=0x304558 r0=0x1E213 r1=0x2 r2=0x0 r3=0x0 lr=0x302173 state=0x26 C44=0x0 R9_1A8=0x9 C6C22=0x1 EEC7C=0x0 DEC30=0x1 note=FAST_OR_OBSERVE evidence=OBSERVED`
- `[JJFB_E8F_FLAG_SNAP] reason=after_sibling r9=0x2B1858 C44=0x0 C9D=0x0 CF5=0x0 B7D=0x0 FE8=0x0 queue_depth=0x0 R9_state=0x26 E6C=0x0 R9_7D8=0x10000 EEC7C=0x0 DEC30=0x1 evidence=OBSERVED`
- `[JJFB_FAST_FIRE_DONE] case156_r1=0x14 state=0x26 svc_hits=0 svc_continues=0 note=FAST_ASSIST_not_product evidence=OBSERVED`

## Interpretation

- FAST gates ``C6C+0x22=1`` + ``DEC+0x30!=0`` open the R1=20 success arm as predicted by E8P.
- Success arm keeps state=38 and never falls into ``0x302360`` / ``0x2F4E82`` reset.
- ``0x301848`` / ``0x304558`` are **not** on the path to C44 unlock; unlock requires separate callers of ``0x2FC8C0``.
- EEC7C / 10165 / case310 do not unlock C44 on this matrix (same success-arm ceiling).
- insn_limit 5e6 (F) same ceiling as 2e6 (A): not an instruction-budget gap.

## Next gap (E8R candidate)

1. Reach a natural/diagnostic caller of ``0x2FC8C0`` (UI-init ``0x2E4840`` cluster / ``0x30DDE2`` / ``0x2DB9DC``).
2. Naturalize ``C6C+0x22`` + ``DEC+0x30`` writers (product backfill).
3. Naturalize state writer + ``0x10102`` case156 delivery.
4. Do not prioritize SVC; do not force C44/C9D/CF5.

## Clean backfill

- Product still needs natural case156, state=38, embedded field writers, and unlock-path entry.
- Fast-assisted progress is not product success.

## Artifacts

- ``out/e8q_tmp/e8q_deps.md``
- ``logs/stage_e8q_*_stdout.txt``
- ``RUN_E8Q_FAST.ps1``

