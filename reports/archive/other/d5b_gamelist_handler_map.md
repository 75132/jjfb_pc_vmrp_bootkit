# D5b — gamelist 0x10102 handler map (static)

evidence: TARGET_OBSERVED (Full Boot slot registrations)

| event_code | handler_va | module_offset | reaches_cmd_or_cfg | nearest_string |
|---|---|---|---|---|
| 0x10600 | `0x2E74AD` | `0x13158` | yes | `IyD@` |
| 0x10601 | `0x2E03E1` | `0xC08C` | yes | `` |
| 0x10602 | `0x2E0421` | `0xC0CC` | yes | `` |
| 0x10603 | `0x2E0359` | `0xC004` | yes | `MpkMD+h` |
| 0x10604 | `0x2E0445` | `0xC0F0` | yes | ``OD8`` |
| 0x10605 | `0x2E0361` | `0xC00C` | yes | `MpkMD+h` |
| 0x10606 | `0x2DFC61` | `0xB90C` | yes | `HHD@j` |
| 0x1060A | `0x2DFC59` | `0xB904` | yes | `HHD@j` |
| 0x10608 | `0x2DF699` | `0xB344` | yes | `IIDT1` |
| 0x10609 | `0x2E0405` | `0xC0B0` | yes | `` |
| timer@FIRE | `0x2E7754` | `0x13400` | yes | ` IDH`pG` |

## Notes

- `reaches_cmd_or_cfg` is a loose BL-window heuristic (not proof of dynamic path).
- Runtime enter/ret logs override this for trigger evidence.

## Runtime (45s quiet boot)

| event_code | live handler | HANDLER_ENTER |
|---|---|---|
| 0x10600–0x10609 / 0x1060A | registered via `0x10102` | **0** |

Conclusion: handlers exist but are **not** the timer keepalive path; no post-init enter observed.

