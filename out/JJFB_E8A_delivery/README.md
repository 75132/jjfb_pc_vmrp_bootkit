# JJFB E8A Delivery Pack

One tree under `out/` for GPT inputs + E8A outputs. Canonical repo copies remain in `logs/` and `reports/`.

## Layout

| Dir | Contents |
|-----|----------|
| `01_gpt_requested_input` | `jjfb.mrp` `wxjwq.mrp` `stage_e7_jjfb_lifecycle_stdout.txt` `main.exe` `unicorn.dll` `build-info.json` |
| `02_mrp_extracted` | Key members + inventories (not full BMP dumps) |
| `03_static_analysis` | Handler map + dual disasm |
| `04_unicorn_probe` | Probe `.c` + traces |
| `05_runtime_logs` | E8A jjfb forensic stdout |
| `06_verdict_and_build` | Verdict markdown |

## Verdict

`THUMB_STATE_LOST` → fix → `HANDLER_RETURNED_NEXT_PLATFORM_GAP`
