# Task 13 — C0 Reset/Start Context Object (in progress)

## Verdict

**Natural leave+B71 restored (B15); first-frame gate PASS (F1).** Case 5 E6C still open for lifecycle. Product default now prefers DrawFP frame over leave-fast chrome skips.

| Variant | Result |
|---------|--------|
| **B12** skip `2FC3BE` | leave → B71 → defer C0; E6C=0 |
| **B14** run `2FC3BE` (pre-fix) | hang in wrong-pack `304BF0` scan (`topleft!15!5.bmp`) |
| **B15** run `2FC3BE` + sibling `default2` entry_complete | **leave** after topleft/topright; B71; **E6C still 0** |
| **F1** `chrome_skip_draw=0` (product default) | **DrawFP + FIRST_REAL_FRAME + screenshot**; leave may be 0 |

See also: [`reports/stage_launch_first_frame.md`](stage_launch_first_frame.md).

## B15 proof (V75-aligned chrome epilogue)

```
2FC3BE → 2D92E4(r0=0, r1=topleft!15!5.bmp)
  → 304BF0 entry_complete via jjfbol/default2.mrp
2FC3D4 → 2D92E4(topright!12!4.bmp) → same
LRT_2FC26C_LEAVE UI_MODE=0x3 E6C=0x0
PATH_A_SECOND → B71_NATURAL → DEFER_FAMILY_C0_E6C_NULL
```

Completing `2D92E4` does **not** write E6C. Unique remaining gap = **natural event15 → `0x2E5E60` → `E6C_NATURAL_STORE`**.

## Fixes shipped

1. Defer family C0 while `E6C==0`
2. Path-A default PRIMING_EMPTY
3. V68 under `2FC26C`: early-ret `311FD4`; nop `2F449C` only under `2FC26C`
4. **`platform_mrp_resource`:** try `gwy/jjfbol/default2.mrp` sibling; **304BF0 entry_complete** on exact archive hit (no force-equal strcmp)
5. **`2FC3BE`:** default **run** (skip only if `JJFB_SKIP_2FC3BE=1`)

## Still open (Case 5)

**Corrected map (G5):** E6C is **B54 code15** → `2E2520` → `2E4020` → `2E5E60` → `STR` @ `0x2E5FAE`.

Family `0x4F` / `30F45C` / `30EE50` posts code15 onto **B50** only (`2D9F50` ingest). Forcing B50→`2E2520` hangs in `2F68E4` (G5). See [`reports/event15_e6c_provenance.md`](event15_e6c_provenance.md).

Live Path-A `101AB` still only frames **`event_code=5`**; no B54 code15 / `E6C_NATURAL_STORE`.

Next:

1. Find guest (or protocol) path that posts **B54** code15 with an E6C-shaped body
2. Then C0 past `2FEC3C` → `B70_NATURAL_STORE` → `UI_MODE_NATURAL_45` (UI45 during family 4F alone is **not** success)

Forbidden: host enqueue 15 / hardwrite E6C / FAST assists / B50→`2E2520`.

## Evidence

- `out/path_a_record_task13/B15_default2/` — natural leave + default2 loads
- `out/path_a_record_task13/B14_2d92e4/` — pre-fix hang
- `reports/event15_e6c_provenance.md`

## Completion gate

`B70_NATURAL_STORE` + `UI_MODE_NATURAL_45`
