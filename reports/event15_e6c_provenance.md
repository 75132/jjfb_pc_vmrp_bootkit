# Event 15 / E6C provenance (Task 13 / G1–G5)

## Status

**`E6C_NATURAL_STORE` still open.** G1–G5 corrected the producer map: family 4F / `30F45C` is **not** the E6C path.

## Corrected producer map (static + live G2/G5)

| Path | Queue | Consumer | E6C? |
|------|-------|----------|------|
| Family `0x4F` → `30E120` → `30F45C` → `30EE50` | **B50** (lit `0xB50`) | `305EB8` → `2DC985` → `2D9F50` | **No** — UI/"prmv" ingest; may write `UI_MODE=0x45` at `0x2FC448` without E6C/B70 |
| Path-A `101AB` → `30D2F9` → `2E4D6C` → `312A60` | **B54** (lit `0xB54`) | `305EB8` → `2DC80C` → `2E2520` | **Yes only if event_code=15** → `2E4020` → `2E5E60` → `STR` @ `0x2E5FAE` |

Only BL to `0x2E5E60` in `robotol.ext`: `0x2E4022` (case code15).

### `2E5E60` gate (G5)

Reads 7× BE u32 from event payload via `308D98`. If field7 **≥ 0** (signed): `BL 0x3105B4` → `2F68E4` until BE `0xFFFFFFFF`. B50/30EE50 payload (`size=0x28`, starts like `prmv`) never terminates → alloc storm; **no** `E6C_NATURAL_STORE`.

## Live runs (`out/path_a_record_task13/`)

| Run | Result |
|-----|--------|
| G1 | Frame + leave + B71 + `DEFER_FAMILY_C0_E6C_NULL` |
| G2 | Family 4F + `LRT_E6C_PRODUCER`; UI45; E6C=0; code15 on **B50** |
| G3 | B50 drained via `2DC985` / no `event_code=15` on B54 |
| G4 | Host `2E2520` even addr → `uc_err` |
| G5 | Thumb `2E2521`: `LRT_EVENT15` + `LRT_E6C_ALLOC` then `2F68E4` hang |

## Product defaults after G5

- `JJFB_FAMILY_4F_FOR_E6C` default **off** (set `=1` for forensics only)
- B50→`2E2520` default **skipped** (`JJFB_B50_2E2520=1` forensic only)
- Forbidden unchanged: host enqueue 15 / hardwrite E6C / FAST assists

## Next for `E6C_NATURAL_STORE`

1. Observe B54 for a guest-built **code=15** entry (`2E4D6C` / other B54 poster) with an E6C-shaped body (field7 path completes or `blt` skip + calloc).
2. Current Path-A `platform_101ab_fill_path_a` only embeds **code=5** (`downVersion`) — not E6C.
3. Do not re-dispatch B50 code15 through `2E2520`.

Mark only when natural: `EVENT15_NATURAL` (B54), `E6C_NATURAL_STORE`.
