# Stage D4 — why gamelist timer does not call native_shell runapp

- **task:** RESEARCH_ONLY → SCHEDULER/RUNTIME gate finding
- **verdict:** `COMPLETE` (discriminating cause found); Stage D runapp still open (`PARTIAL`)
- **evidence:** TARGET_OBSERVED + CROSS_TARGET strings; MasterPlan Stage C/D

## Classification

| Claim | Level |
|-------|--------|
| Periodic timer FIRE path ≠ runapp | TARGET_OBSERVED |
| `gamelist.ext` has `gwy/cfg.bin` + cfg36 format strings, no `runapp`/`startGame` literals | TARGET_OBSERVED |
| Post-init: 0 opens of `cfg.bin` / `jjfb.mrp` | TARGET_OBSERVED |
| `cfg.bin` exists on host (`game_files/.../gwy/cfg.bin`, 20728 bytes) | CROSS_TARGET resource |
| Static callgraph: timer handler `0x2E7754` does **not** reach cfg-open callees | TARGET_OBSERVED |
| Cfg-open parents `0x2D7CE4` / `0x2E0F5C` / `0x2E1520` never appear in Full Boot PC log | TARGET_OBSERVED |
| Gate near cfg path compares `r0` to `0xC` (12 = napptype) @ `0x2D9CBC` | TARGET_OBSERVED / HYPOTHESIS on field meaning |

## Proven

1. Timer cycles (D2/D3) only: `0x10204` / `0x10500` + re-arm `period_ms=5000` — no export/runapp.
2. After `After app init`: `cfg.bin` opens = **0**, `jjfb.mrp` opens = **0**, guest-built `[JJFB_GAMELIST_CFG36_BUILD] param=` = **0** (only format_string_mapped at map time).
3. `gamelist.ext` strings include `gwy/cfg.bin`, `napptype=%d_..._nmrpname=%s_gwyblink`, `gwy/%s.mrp`; **zero** `runapp` / `startGame` / `lib.run`.
4. Callgraph: registered `0x10102` handlers + timer `0x2E7754` → `reaches_cfg=False`. Cfg path entered only via `0x2D7CE4` / `0x2E0F5C` / `0x2E1520` (never hit in boot log).
5. Launch param is passed into `start_dsm` (`GWY_LAUNCH_PARAM` / entry) but is **not** proof that guest sprintf’d/consumed cfg36 for jjfb.

## Not yet / blocker

- No `[JJFB_RUNAPP] source=native_shell` / `[JJFB_SHELL_EXPORT_CALL]`.
- Cfg/no_update/post_update branch never runs; MasterPlan Stage C logs absent as *guest-observed* (only host update stub).
- Side notes (not proven gates): `gb16.uc2` MISS; `mr_plat(1327)` → `MR_IGNORE`; init continues after both.

## Next smallest experiment (D5)

See `phase_d5_cfg_gate_verdict.md`: PC probes show cfg-gate and cmd-dispatch **never** enter under timer-only Full Boot (4× `FIRE_EXT_DONE`, still 0 `cfg.bin` opens).
