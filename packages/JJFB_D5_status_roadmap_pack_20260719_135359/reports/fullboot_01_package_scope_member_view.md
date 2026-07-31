# Full Boot 01 — Package Scope / Member View

## PACKAGE_SCOPE

- `[JJFB_PACKAGE_SCOPE] package=gwy/gbrwcore.mrp primary=gbrwcore.ext class=shell_core cload=scoped evidence=CROSS_TARGET`
- `[JJFB_PACKAGE_SCOPE] package=gwy/gamelist.mrp primary=gamelist.ext class=shell_core cload=scoped evidence=CROSS_TARGET`
## MEMBER_VIEW

- `[MRP_MEMBER_VIEW] ready target=gwy/jjfb.mrp aliases=1`
- `[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/gbrwcore.mrp primary=gbrwcore.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/shell_gbrwcore_cfunction.mrp original_sha256=dc7c2134d214e0667174d1806227ea88b41357269e92845d15afdb7f10267b44 vfs_remap=enabled evidence=CROSS_TARGET`
- `[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/gamelist.mrp primary=gamelist.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/shell_gamelist_cfunction.mrp original_sha256=4b24babfc82be19015fdf5eb9e2468c7ea030130f61f617a8644ab1e7bf7d956 vfs_remap=enabled evidence=CROSS_TARGET`
- `[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/gbrwshell.mrp primary=gbrwshell.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/shell_gbrwshell_cfunction.mrp original_sha256=3cc99e87680b7ebb94dc6f4c3d765d4480a0595650ee97e3c5463400d1819916 vfs_remap=enabled evidence=CROSS_TARGET`
- `[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/jjfb.mrp primary=robotol.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/game_jjfb_cfunction.mrp original_sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 vfs_remap=enabled evidence=CROSS_TARGET`
- `[MRP_MEMBER_VIEW] game_primary_mode=all_shell_and_game package=gwy/jjfb.mrp`
- `[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/wxjwq.mrp primary=mmochat.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/game_wxjwq_cfunction.mrp original_sha256=6ec628419bc4c0ca1f8fba37b0c5179961220cd53591fc55eba26735defbd02d vfs_remap=enabled evidence=CROSS_TARGET`
- `[MRP_MEMBER_VIEW] game_primary_mode=all_shell_and_game package=gwy/wxjwq.mrp`
## REG_PRIMARY

- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/jjfb.mrp primary=robotol.ext evidence=CROSS_TARGET source=reg.ext+first_ext_member`
- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/gbrwcore.mrp primary=gbrwcore.ext evidence=CROSS_TARGET source=reg.ext+package_stem`
- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/gamelist.mrp primary=gamelist.ext evidence=CROSS_TARGET source=reg.ext+embedded_package_stem`
- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/gbrwshell.mrp primary=gbrwshell.ext evidence=CROSS_TARGET source=reg.ext+package_stem`
- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/jjfb.mrp primary=robotol.ext evidence=CROSS_TARGET source=reg.ext+first_ext_member`
- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/wxjwq.mrp primary=mmochat.ext evidence=CROSS_TARGET source=reg.ext+first_ext_member`
