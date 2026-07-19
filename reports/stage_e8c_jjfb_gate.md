# Stage E8C — jjfb gate

| Gate | Result |
|------|--------|
| target | gwy/jjfb.mrp |
| flag_map | `out/e8c_tmp/flag_map.json` |
| watch_offsets | 0xC44,0xC9D,0xCD1,0xCF5,0x11B0 |
| idle watch armed | yes (offset_base=0x2B1858) |
| flag snap | yes (40 ticks, all 0) |
| flag transition | no |
| helper fx | yes (watched_writes=0) |
| lifecycle ok=1 | yes |
| DRAW | no |
| decision | ROBOTOL_STATE_FLAG_NEVER_SET |

See `reports/stage_e8c_verdict.md`.
