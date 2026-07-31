# Product Direct JJFB Verdict

- **run_id:** p2_20260801_040032_63208
- **runtime:** Gwy+stubs (Mode=Gwy 鈫?launcher_core + research stubs)
- **seconds:** 90
- **process_exit:** 
- **main_exe_sha256:** beababbcede839e7430c41be77a7896cdc4423ef7bee38b8d069c66e271c4190
- **strong_success:** YES
- **forbidden_hits:** none
- **post_callback:** NATURAL_CALLBACK_IDLE_WAIT
- **manifest:** C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\reports\product_direct_jjfb_manifest_p2_20260801_040032_63208.txt

## Required gates

| Gate | OK |
|------|----|
| DESCRIPTOR_FROZEN | yes |
| TARGET_HASH_VERIFIED | yes |
| START_MR_ENTERED | yes |
| MRC_LOADER_RESOLVED_EXACT | yes |
| ROBOTOL_RESOLVED_BY_PROFILE_ALIAS | yes |
| ROBOTOL_BOOTSTRAP_RETURN | yes |
| EXT_VERSION_RETURN_ZERO | yes |
| EXT_APPINFO_RETURN_ZERO | yes |
| ROBOTOL_INIT_RETURN_ZERO | yes |
| ROBOTOL_HANDLER_REGISTERED | yes |
| SCHEDULER_NATURAL_CALLBACK | yes |

## Forbidden (must be absent)

| Item | Present |
|------|---------|
| gamelist_fast | clean |
| method0_smscfg_write | clean |
| fixed_pc_jump | clean |
| host_fake_ui | clean |
| forced_callback | clean |

## Notes

- Partial progress (e.g. ENTRY_CALLED without init=0) is **not** product success.
- Gates require structured markers with evidence=OBSERVED (current run).
- Research E10A / shell runners: `RUN_RESEARCH_GWY_SHELL.ps1`
