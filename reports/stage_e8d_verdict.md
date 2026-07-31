# Stage E8D — Robotol Flag Source / Event Delivery Provenance

## Verdict

**MISSING_10165_EVENT_DELIVERY**

`0x10165` is registered as enqueue handler `0x30D2F9` (docs/06). Host never drains it in normal product loop — only `0x10140` lifecycle ticks. An observe-only `ZERO_ARGS` fire of `0x30D2F9` returns `ok=1` but **does not** set blocking flags `R9+0xC44/C9D/CF5` (still 0 through tick 40). Two unrelated bytes at `R9+0xFE8` changed during the probe. Real event payload / ABI is still missing — not a blank “handler never callable.”

## Evidence

| Item | Result |
|------|--------|
| Handler map | `0x10140` → `0x30630D` (lifecycle_period); `0x10165` → `0x30D2F9` (enqueue_event); also `0x10162`/`0x10102` |
| Early watch | armed; ER_RW flags 0 at `after_mrc_init`, `first_10140_tick`, `after_10165_probe`, `tick_40` |
| FLAG_WRITE / TRANSITION | 0 |
| 10165 ZERO_ARGS | `FIRE` + `FIRE_DONE ok=1 stop_at_base`; flags unchanged |
| ERW window diff | `after_10165_probe changed=2 first_off=0xFE8` (not C44/C9D/CF5) |
| C9D static | no direct STRB; wider/memcpy heuristics in `out/e8d_tmp/flag_write_xref_v2.md` |
| DRAW | no |
| audit + jjfb hash | clean / unchanged |

## Interpretation

```text
10140 tick  = alive poller (idle loop)
10165       = registered enqueue path, never fed real events
ZERO_ARGS   = proves handler runnable; proves wrong/empty event does not unlock idle flags
```

Next discriminating step (not E8D): derive enqueue ABI / event codes from `0x30D2F9` disasm + callers, then one candidate at a time (`APP_START` / resource / network) without forcing flags.

## Artifacts

- `RUN_E8D_FLAG_SOURCE.ps1`
- `logs/stage_e8d_jjfb_stdout.txt`
- `out/e8d_tmp/flag_write_xref_v2.md`, `writer_class.md`, `handler_10165_disasm.txt`
- `reports/stage_e8d_jjfb_gate.md`

## Forbidden (held)

No forced `C44/C9D/CF5=1`, no `0x1E209` ret mutation, no fake DRAW, no MRP/EXT edits.
