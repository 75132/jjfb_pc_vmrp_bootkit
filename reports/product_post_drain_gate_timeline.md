# Post-Drain Gate Timeline

- **run_id:** ffp_event_20260724_155025_4016
- **er_rw:** 0x2B1854
- **enter_30CBBC:** 0
- **enter_2E2520:** 1
- **enter_2DC4D8:** 0
- **store_15D:** 0
- **store_B71:** 0
- **15D_writer_grade:** candidate_unproven
- **B71_writer_grade:** candidate_entered_no_store
- **successor_status:** POST_DRAIN_SUCCESSOR_BLOCKED

## Anchors

| seq | name | pc | insn |
|-----|------|----|------|
| 1 | drain_2DC80C | 0x2DC80C | 1 |
| 3 | drain_2DC80C | 0x2DC80C | 2 |
| 7 | pop_312C0C | 0x312C0C | 5 |
| 9 | pop_312C0C | 0x312C0C | 6 |
| 13 | pop_312C0C | 0x312C0C | 9 |
| 15 | pop_312C0C | 0x312C0C | 10 |
| 17 | gate_305EC2 | 0x305EC2 | 11 |
| 19 | gate_305EC2 | 0x305EC2 | 12 |
| 21 | drain_2DC80C | 0x2DC80C | 13 |
| 23 | drain_2DC80C | 0x2DC80C | 14 |

## Watch events (first 16)

| seq | kind | tag | pc | before | after |
|-----|------|-----|----|--------|-------|
| 2 | ANCHOR | drain_2DC80C | 0x2DC80C | 0 | 0 |
| 4 | ANCHOR | drain_2DC80C | 0x2DC80E | 0 | 0 |
| 5 | CODE_ENTER | b71_upstream_2E2520 | 0x2E2520 | 0 | 0 |
| 6 | CODE_ENTER | b71_upstream_2E2520 | 0x2E2522 | 0 | 0 |
| 8 | ANCHOR | pop_312C0C | 0x312C0C | 0 | 0 |
| 10 | ANCHOR | pop_312C0C | 0x312C0E | 0 | 0 |
| 11 | CODE_ENTER | b71_upstream_2E2520 | 0x2E2520 | 0 | 0 |
| 12 | CODE_ENTER | b71_upstream_2E2520 | 0x2E2522 | 0 | 0 |
| 14 | ANCHOR | pop_312C0C | 0x312C0C | 0 | 0 |
| 16 | ANCHOR | pop_312C0C | 0x312C0E | 0 | 0 |
| 18 | ANCHOR | gate_305EC2 | 0x305EC2 | 0 | 0 |
| 20 | ANCHOR | gate_305EC2 | 0x305EC4 | 0 | 0 |
| 22 | ANCHOR | drain_2DC80C | 0x2DC80C | 0 | 0 |
| 24 | ANCHOR | drain_2DC80C | 0x2DC80E | 0 | 0 |
| 25 | CODE_ENTER | b71_upstream_2E2520 | 0x2E2520 | 0 | 0 |
| 26 | CODE_ENTER | b71_upstream_2E2520 | 0x2E2522 | 0 | 0 |

## Discipline

- Observe-only: no writes to 15D/B71/UI_MODE.
- Proven natural writer requires CODE_ENTER + ER_RW store to needed value.
