# Stage E8P-Fast Verdict

**Verdict:** `FAST_REACHED_C44_WRITER_NEXT_GAP`

**Co-claim:** `C44_DEP_REQUIRES_MULTI_OBJECTS` (for R1=20 non-clear arm: `C6C+0x22` + `DEC+0x30`)

**NOT product success.** FAST_ASSIST / field poke only — no MRP/EXT edits, no fake DRAW.

---

## Headline

1. **`R9+0xC6C / 0xEEC / 0x11D0` are R9-embedded struct bases**, not heap pointer slots. Do not allocate guest objects for them.
2. With raised insn budget, **`0x2F4E64` / `0x2F4E82` are reached** on state=38+case156 (R1=18 via `0x302340`; R1=20 fail via `0x302362`).
3. **`0x2F4E82` writes C44=0** (reset). No `FLAG_TRANSITION`, no DRAW, no SVC.
4. R1=20 early gate is **not** `EEC+0x7C`; it is **`LDRSB` from `R9+0xC6C+0x22` == 1**, then **`*(R9+0xDEC+0x30) != 0`**.

---

## Static map (corrected)

### Function head

```
r1 = R9+0xC6C          ; struct base
r6 = R9+0xC6C+0x20
r5 = R9+0xC6C+0x60
r0 = *(R9+0xEEC+0x7C)  ; field (early case 2/12 null-check)
r7 = R9+0x11D0         ; struct base
```

### R1=18 (`r4=18`)

```
→ 0x302174 … cleanup …
→ STR state @ 0x3021FA (38→0)
→ fall-through cleanup …
→ 0x302340 BL 0x2F4E64 → 0x2F4E82 (C44:=0)
```

(Needs insn_limit ≳ 2e6; 400k used to stop mid-helper at `0x94EAC`.)

### R1=20 (`r4=20`)

```
0x302124:
  LDRSB r0, [r6, #2]     ; byte at R9+C6C+0x22
  CMP r0, #1
  BNE → 0x30221E → 0x302356 → clear state @0x302360 → BL 0x2F4E64 @0x302362

  BEQ → 0x30213E:
    LDR *(R9+DEC+0x30); CMP #0
    BEQ → same fail arm (0x30221E)
    else: STRB #9 @R9+0x1A8; BL 0x301848; BL 0x304558; exit 0x302114
         (state preserved; NO 0x302340 / NO C44 writer)
```

### `0x2F4E64`

- Touches C6C fields; on byte test, **`STRB #0 → R9+C44`** at `0x2F4E82`.
- Then clears `*(EEC+0x7C)` among other EEC fields.

---

## Live matrix (PARENT_HIT)

| Run | 302340 | 302360 | 302362/2F4E64 | 2F4E82 | state after | C44* | DRAW |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A s38 R1=18 | Y | N | Y (via 340) | Y | 0 | N | N |
| B s38 R1=20 | N | Y | Y | Y | 0 | N | N |
| C 10165+R1=20 | N | Y | Y | Y | 0 | N | N |
| D 310+R1=20 | N | Y | Y | Y | 0 | N | N |
| E full+R1=20 | N | Y | Y | Y | 0 | N | N |
| F EEC7C=1 | N | Y | Y | Y | 0 | N | N |
| G DEC30=1 | N | Y | Y | Y | 0 | N | N |
| H EEC7C+DEC30 | N | Y | Y | Y | 0 | N | N |
| J C6C22=1 | N | Y | Y | Y | 0 | N | N |
| K C6C22+DEC30 | **N** | **N** | **N** | **N** | **38 kept** | N | N |

\* no `FLAG_TRANSITION` (writer stores 0 into already-0).

**K evidence:** success arm taken — `after_sibling R9_state=0x26 DEC30=0x1`, no `0x302360` / no `0x2F4E82`.

---

## What this means

| Question | Answer |
| --- | --- |
| Missing heap objects for C6C/EEC/11D0? | **No** — embedded structs |
| Next gap after writer hit? | **C44 unlock semantics** (who writes C44≠0), and/or post-writer platform path |
| R1=20 “happy” arm useful for DRAW? | Unclear — it avoids clear + skips C44 writer; calls `0x301848` / `0x304558` |
| Natural writers to chase | `*(C6C+0x22)`, `*(DEC+0x30)`, `*(EEC+0x7C)` (`0x2F0F64` sets 7C; `0x2F4E92` clears) |
| SVC `#0xAB`? | Still not hit — keep deprioritized |

---

## Clean backfill (B 线)

1. Naturalize **`R9+0x8D0` state writer** + `0x10102` case156 delivery (still product-required).
2. Naturalize **`C6C+0x22` / `DEC+0x30`** writers if product wants R1=20 success arm.
3. Trace **who writes C44 to non-zero** (not only `0x2F4E82` reset-to-0).
4. Do not invent pointer objects for C6C/EEC/11D0.

---

## Artifacts

- `out/e8p_tmp/e8p_deps.md` / `e8p_deps.json`
- `logs/stage_e8p_*_stdout.txt` (incl. `J_c6c22`, `K_c6c22_dec30`)
- `RUN_E8P_FAST.ps1`
- Env: `JJFB_FAST_EEC7C`, `JJFB_FAST_DEC30`, `JJFB_FAST_C6C22`, `JJFB_FAST_INSN_LIMIT`

---

## Note

Auto matrix table in the first script pass was wrong (pattern noise); this document is the corrected recount from the same logs.
