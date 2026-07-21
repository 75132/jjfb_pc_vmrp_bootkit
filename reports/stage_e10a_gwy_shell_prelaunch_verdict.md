# Stage E10A-Fix GWY Shell Prelaunch Verdict

- **Mode**: badpath
- **Primary**: `SHELL_GAMELIST_REACHED`
- **Product success**: **NO** (`NOT_PRODUCT`)

## Focus
Reconstruct **gbrwcore -> gamelist -> cfg36 -> update/no-update -> runapp -> jjfb -> splash**.
Do **not** treat robotol resource-ready evt as shell contract.

## Cases
- **badpath** (108.8s): `SHELL_GAMELIST_REACHED` gbrw=True continue=True gamelist=True runapp=False splash=False ac8_nz=False

## Artifacts
| Kind | Path |
|------|------|
| Shell inventory | `reports/e10a_gwy_shell_inventory.csv` |
| cfg records | `reports/e10a_gwy_cfg_records.csv` |
| strings | `reports/e10a_shell_strings.csv` |
| dependencies | `reports/e10a_shell_file_dependencies.csv` |
| transition graph | `out/e10a_shell/launch_transition_graph.md` |
| phase trace | `reports/e10a_shell_phase_trace.csv` |
| vfs trace | `reports/e10a_shell_vfs_trace.csv` |
| vfs badpath | `reports/e10a_shell_vfs_badpath_trace.csv` |
| file API ABI | `reports/e10a_shell_file_api_abi.md` |
| event loop | `reports/e10a_shell_event_loop_trace.csv` |
| update contract | `reports/e10a_shell_update_contract_trace.csv` |
| ac8 trace | `reports/e10a_shell_ac8_trace.csv` |
| log | `logs/e10a_shell_trace_stdout.txt` |