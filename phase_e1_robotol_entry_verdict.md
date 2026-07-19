# Stage E1 — PRODUCT robotol.ext ENTRY_CALLED

- **task:** RUNTIME / EXT — identity-gated product path `descriptor_launcher` → `robotol.ext` ENTRY_CALLED
- **verdict:** `COMPLETE` (E1)
- **source tag:** `descriptor_launcher` (not `native_shell`)

## Proven

| Gate | Result |
|------|--------|
| Identity: DSM/mrc_loader `ROBOTOL_ENTER` → `JJFB_ROBOTOL_ENTER_REJECT` | PASS (2 rejects) |
| `JJFB_CLOAD_SCOPE` package=gwy/jjfb.mrp → robotol.ext | PASS |
| `MRP_MEMBER_VIEW` game_package install | PASS |
| robotol EXTRACTED → MAPPED → REGISTERED → ENTRY_CALLED | PASS |
| `[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext` | PASS |
| jjfb sha256 unchanged | PASS |
| `audit_launcher_core.py` | ok |
| no gamelist / no host_runapp_equiv | PASS |

## Not yet (E2+)

| Gate | Result |
|------|--------|
| `[JJFB_MRC_INIT] module=robotol.ext` | **0** |
| Natural DRAW/REFRESH | open |
| wxjwq positive control (E4) | not run this step |

## Discriminating notes (TARGET_OBSERVED)

- Entry returned quickly: `[EXT_ENTRY] module_id=3 method=0 return=0`
- `R9_SWITCH_BLOCKED` on mrc_loader and robotol (`CALLEE_ER_RW_NOT_AVAILABLE`) while entry still executed with caller r9
- `JJFB_EXTCHUNK_PUBLISH` for robotol seen; P+0/+4 still `0` at continuation snapshot → ER_RW bind incomplete (Stage E-B / E-D)

## Artifacts

- `RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1`
- `reports/stage_e_product_robotol_boot.md`
- `reports/stage_e_robotol_identity_audit.md`
- `reports/stage_e_mrc_loader_to_robotol_handoff.md`
- `logs/stage_e_product_robotol_stdout.txt`

## Next (exactly one)

Stage E-D / E-B: keep product track; diagnose why robotol entry returns without `[JJFB_MRC_INIT]` — prefer generic `game_package` ER_RW bind (P+0/+4) over UI/shell. Optional parallel: same runner `-Target gwy/wxjwq.mrp`.
