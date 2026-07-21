# Stage E10A GWY Shell Prelaunch Verdict

- **Mode**: direct
- **Primary**: `DIRECT_PATH_AC8_ZERO`
- **Product success**: **NO** (`NOT_PRODUCT`)

## Cases
- **direct** (94.8s): `DIRECT_PATH_AC8_ZERO` launch=descriptor_direct splash=True ac8_nz=False shell_gbrw=False shell_gl=False post_update=False runapp=False

## Artifacts
- trace: `reports/e10a_shell_ac8_trace.csv`
- log: `logs/e10a_gwy_shell_prelaunch_stdout.txt`

## Notes
- E10A compares `descriptor_direct` vs `gwy_shell_core_continue` without synthetic resource-ready or AC8 poke.
- If both paths AC8=0 at splash, blocker remains `AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL` (E9Z).