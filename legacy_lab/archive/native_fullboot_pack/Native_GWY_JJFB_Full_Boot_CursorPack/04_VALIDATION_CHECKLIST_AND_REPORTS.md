# Validation Checklist and Reports

## Required logs

```text
[JJFB_PACKAGE_SCOPE]
[MRP_MEMBER_VIEW]
[REG_PRIMARY]
[EXT_REGISTER]
[JJFB_EXTCHUNK_PUBLISH]
[JJFB_ER_RW_BIND]
[JJFB_R9_SWITCH_OK]
[JJFB_GAMELIST_CFG36_BUILD]
[JJFB_GAMELIST_POST_UPDATE]
[JJFB_SHELL_EXPORT_RESOLVE]
[JJFB_SHELL_EXPORT_CALL]
[JJFB_RUNAPP] source=native_shell
[JJFB_FILEOPEN] guest="gwy/jjfb.mrp"
[JJFB_STRCOM] code=601/800/801
[JJFB_MRC_LOADER]
[JJFB_MRC_INIT]
[JJFB_RESOURCE_REQUEST]
[JJFB_DRAW]
[JJFB_REFRESH]
```

## Required reports

```text
reports/fullboot_00_environment.md
reports/fullboot_01_package_scope_member_view.md
reports/fullboot_02_shell_module_context.md
reports/fullboot_03_gamelist_cfg36_no_update.md
reports/fullboot_04_native_export_runapp.md
reports/fullboot_05_jjfb_mrc_loader_robotol.md
reports/fullboot_06_wxjwq_control.md
reports/fullboot_07_slot_calls_if_any.md
reports/fullboot_08_fileopen_resource_chain.md
reports/fullboot_09_visual_natural_chain.md
reports/fullboot_10_final_verdict.md
```

## Stop conditions

Do not stop at:

```text
gamelist started only
file open only
export string found only
host equivalent runapp
P+0xC published only
R9_SWITCH_OK only
```

Stop only when one of these happens:

```text
native shell opens jjfb.mrp
jjfb mrc_init reached
natural visual output appears
new concrete blocker with full report
```

## Contradiction control

If summary report and detailed report conflict:

1. Trust stdout tags.
2. Trust focused reports over aggregate summary.
3. Mark stale report explicitly.
4. Never claim high success when `export_call/runapp/jjfb_open` are false.
