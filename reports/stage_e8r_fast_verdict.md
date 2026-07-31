# Stage E8R-Fast Verdict

**Verdict:** `FAST_REACHED_C44_TRANSITION_NEXT_GAP`

**Static co-claim:** `C44_UNLOCK_REQUIRES_UI_INIT_CLUSTER` — natural entry is UI-init `0x2E4788` / BLs `0x2E4840..0x2E4B06` (depends `R9+0x8D0`); alt `0x30DDE2` inside `0x30D300`.

**NOT product success.** `JJFB_FAST_UNLOCK_CALL` invokes real `0x2FC8C0` (not a C44 memory poke).

## One-line

FAST can set `C44=1` via real writer `0x2FC8CE`; natural callers never hit on product/fast matrices; next gap is post-C44 (`C9D`/`CF5`/DRAW still idle).

## Static facts

- Unlock fn `0x2FC8C0`: `BL 0x3046A8` then `STRB #1 @ C44`; stores helper ret @ `C44+4`; clears `C44+8/+C`.
- Entry args unused (R9-only). Thumb entry must be `0x2FC8C1`.
- Callers (9): `0x2DB9DC`; UI cluster `0x2E4840..0x2E4B06` (fn `0x2E4788`, reads state `0x8D0`); `0x30DDE2` in `0x30D300` (10102 handler).
- Classes: `APP_INIT_UI_CLUSTER` (primary), `EVENT_SWITCH_CASE_UI_INIT`, `PLATFORM_SIDE_EFFECT_UI_INIT`.

## Matrix

| Run | UI caller | 2DB9DC | 30DDE2 | 2FC8C0 | 2FC8CE | C44=1 | C9D/CF5 | DRAW | SVC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A_product | False | False | False | False | False | False | False | False | False |
| B_fast_success | False | False | False | False | False | False | False | False | False |
| C_10165_success | False | False | False | False | False | False | False | False | False |
| D_310_success | False | False | False | False | False | False | False | False | False |
| E_10165_310_success | False | False | False | False | False | False | False | False | False |
| F_unlock_before | False | False | False | True | True | True | False | False | False |
| G_unlock_after | False | False | False | True | True | True | False | False | False |

## Evidence (F_unlock_before)

- `[JJFB_FAST_UNLOCK_DONE] ok=1 end=stop_at_base pc_after=0x80000 C44_before=0x0 C44_after=0x1 note=FAST_ASSIST_not_product`
- `[JJFB_E8R_C44_UNLOCKED] C44=1 via=0x2FC8C0 note=FAST_ASSIST_real_writer`
- `[JJFB_E8F_FLAG_SNAP] reason=after_fast_unlock ... C44=0x1 C9D=0x0 CF5=0x0 R9_state=0x26`
- Success arm still reached after unlock (`p301848`); no DRAW / no SVC.

## Interpretation

- `0x301848` / `0x304558` still do not call unlock (E8Q confirmed).
- Product + E8Q-style FAST never enter UI-init / `0x30DDE2` / unlock.
- Direct FAST call of `0x2FC8C0` proves writer is sufficient once reached; helper `0x3046A8` works under current plat bind.
- After `C44=1`, `C9D`/`CF5` stay 0 — next gap is post-unlock gate / UI path, not the writer itself.
- Natural missing trigger: UI-init cluster upstream (`0x2E2F50`..`0x2E3F7C`) and/or state ladder into `0x2E4788`.

## Next gap (E8S candidate)

1. Trace upstream of `0x2E4788` (why UI-init never enters).
2. After FAST `C44=1`, watch what still blocks DRAW (`C9D`/`CF5` / graphics / queue).
3. Keep product backfill: case156, state=38, `C6C+0x22`, `DEC+0x30`, UI-init entry.
4. Do not prioritize SVC; do not force C44/C9D/CF5 as product.

## Clean backfill

- Fast unlock != product success.
- Product still needs natural unlock caller entry (UI-init preferred).

## Artifacts

- `out/e8r_tmp/e8r_deps.md`
- `logs/stage_e8r_*_stdout.txt`
- `RUN_E8R_FAST.ps1`
- `tools/e8r_c44_unlock_caller.py`
