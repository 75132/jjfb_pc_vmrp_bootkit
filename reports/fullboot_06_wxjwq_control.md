# Full Boot 06 — WXJWQ Control

## wxjwq / mmochat

- `[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\240x320/gwy/wxjwq.mrp primary=mmochat.ext evidence=CROSS_TARGET source=reg.ext+first_ext_member`
- `[MRP_VIEW_LOOKUP] requested=cfunction.ext resolved=mmochat.ext insert_at=791 entry_size=30 index_end=791 offset=92949 stored_size=206148 result=HIT`
- `[MRP_MEMBER_REQUEST] package=gwy/wxjwq.mrp requested=mmochat.ext scope=mrp_member`
- `[EXT_RESOLVE] package=gwy/wxjwq.mrp requested=mmochat.ext resolved=mmochat.ext strategy=exact scope=mrp_member result=HIT`
- `[MODULE_REGISTRY] module_id=7 requested=mmochat.ext resolved=mmochat.ext origin=MRP_MEMBER strategy=exact state=EXTRACTED stored_size=206148 unpacked_size=320292 extracted_size=320292 helper=0x0`
- `[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/wxjwq.mrp primary=mmochat.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/game_wxjwq_cfunction.mrp original_sha256=6ec628419bc4c0ca1f8fba37b0c5179961220cd53591fc55eba26735defbd02d vfs_remap=enabled evidence=CROSS_TARGET`
- `[MRP_MEMBER_VIEW] game_primary_mode=all_shell_and_game package=gwy/wxjwq.mrp`
- `[EXT_LOAD] module_id=7 load_id=8 package=gwy/wxjwq.mrp requested=mmochat.ext resolved=mmochat.ext origin=MRP_MEMBER stage=EXTRACTED size=320292`
