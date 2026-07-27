# Stage E 鈥?mrc_loader 鈫?robotol handoff

- **mrc_loader ENTRY_CALLED:** yes
- **robotol EXTRACTED/MAPPED/REGISTERED/ENTRY:** True / True / True / True
- **CLOAD_SCOPE seen:** yes

## Gate separation

| Gate | Proven |
|------|--------|
| mrc_loader ENTRY_CALLED | yes |
| cfunction reg_primary resolved | yes |
| robotol mapped/register | yes |
| robotol ENTRY_CALLED | yes |

## Sample

```
[CALLBACK_FRAME] boundary=GUEST_BEFORE_HOST_CALLBACK callback_id=1 module_id=0 module=? slot=0x280068 slot_name=_mr_c_function_new call_pc=0x280068 continuation_pc=0x80034 continuation_file_offset=0x0 r0=0xA4178 r1=0x14 r2=0x280068 r3=0x0 r4=0x80008 r5=0x0 r6=0x0 r7=0x0 r8=0x0 r9=0x0 r10=0x0 r11=0x0 r12=0x0 sp=0x27FFF0 lr=0x80034 cpsr=0x600001D3 thumb=no guest_callback_frame_gate=blocked module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=blocked evidence=OBSERVED note=observe_only_no_reg_restore
[CALLBACK_FRAME] boundary=HOST_CALLBACK_RETURN callback_id=1 module_id=0 module=? slot=0x280068 slot_name=_mr_c_function_new call_pc=0x280068 continuation_pc=0x80034 continuation_file_offset=0x0 r0=0x0 r1=0x14 r2=0x280068 r3=0x0 r4=0x80008 r5=0x0 r6=0x0 r7=0x0 r8=0x0 r9=0x0 r10=0x0 r11=0x0 r12=0x0 sp=0x27FFF0 lr=0x80034 cpsr=0x600001D3 thumb=no guest_callback_frame_gate=blocked module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=blocked evidence=OBSERVED note=observe_only_no_reg_restore
[CALLBACK_R9_DELTA] callback_id=1 module= cfn_path=HOST_MAP_FUNC pre=0x0 leave=0x0 resume=0x0 return_r0=0x0 guest_callback_frame_gate=blocked module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=blocked evidence=OBSERVED
[CALLBACK_CALLEE_SAVED_DELTA] callback_id=1 changed=no r4_pre=0x80008 r4_post=0x80008 r9_pre=0x0 r9_post=0x0 r10_pre=0x0 r10_post=0x0 sp_pre=0x27FFF0 sp_post=0x27FFF0 cpsr_t_pre=0 cpsr_t_post=0 guest_callback_frame_gate=blocked module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=blocked evidence=OBSERVED
[CALLBACK_FRAME] boundary=GUEST_CONTINUATION_RESUME callback_id=1 module_id=0 module=? slot=0x280068 slot_name=_mr_c_function_new call_pc=0x280068 continuation_pc=0x80034 continuation_file_offset=0x0 r0=0x0 r1=0x14 r2=0x280068 r3=0x0 r4=0x80008 r5=0x0 r6=0x0 r7=0x0 r8=0x0 r9=0x0 r10=0x0 r11=0x0 r12=0x0 sp=0x27FFF0 lr=0x80034 cpsr=0x600001D3 thumb=no guest_callback_frame_gate=blocked module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=blocked evidence=OBSERVED note=observe_only_no_reg_restore
[CALLBACK_CONTINUATION] relation=CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW callback_id=1 module= module_id=0 cfn_path=HOST_MAP_FUNC call_pc=0x280068 continuation_pc=0x80034 continuation_file_offset=0x0 helper=0xA4178 helper_ne_continuation=yes identity=CONFIRMED superseded_by_callback_continuation_identity=yes causal=false note=not_module_entry guest_callback_frame_gate=blocked module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=blocked evidence=OBSERVED
[R9_SCOPE] stage=NOOP scope_id=0 owns_frame=no requested_by=bridge_mr_extHelper requested_frame_id=0 actual_top_frame_id=0 top_frame_kind=NONE leave_action=NOOP emu_exit_reason=NORMAL_GUEST_RETURN depth_before=0 depth_after=0 old_r9=0x0 new_r9=0x0 generation=0 module_r9_switch_gate=open nested_r9_scope_gate=blocked allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=DOCUMENTED
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=start.mr module_id=1 requested=start.mr resolved= origin=MRP_MEMBER strategy=unknown state=DISCOVERED stored_size=0 unpacked_size=0 extracted_size=0 helper=0x0
[EXT_RESOLVE] package=gwy/jjfb.mrp requested=start.mr resolved=start.mr strategy=exact scope=mrp_member result=HIT
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=start.mr module_id=1 requested=start.mr resolved=start.mr origin=MRP_MEMBER strategy=exact state=EXTRACTED stored_size=1514 unpacked_size=3787 extracted_size=3787 helper=0x0
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=mrc_loader.ext module_id=2 requested=mrc_loader.ext resolved= origin=MRP_MEMBER strategy=unknown state=DISCOVERED stored_size=0 unpacked_size=0 extracted_size=0 helper=0x0
[MRP_MEMBER_REQUEST] package=gwy/jjfb.mrp requested=mrc_loader.ext scope=mrp_member
[EXT_RESOLVE] package=gwy/jjfb.mrp requested=mrc_loader.ext resolved=mrc_loader.ext strategy=exact scope=mrp_member result=HIT
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=mrc_loader.ext module_id=2 requested=mrc_loader.ext resolved=mrc_loader.ext origin=MRP_MEMBER strategy=exact state=EXTRACTED stored_size=219 unpacked_size=232 extracted_size=232 helper=0x0
[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\320x480/gwy/jjfb.mrp primary=robotol.ext evidence=CROSS_TARGET source=reg.ext+first_ext_member
[EXT_RESOLVE] package=gwy/jjfb.mrp requested=cfunction.ext resolved=robotol.ext strategy=reg_primary scope=mrp_member result=HIT reason=reg_ext_primary
[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\320x480/gwy/jjfb.mrp primary=robotol.ext evidence=CROSS_TARGET source=reg.ext+first_ext_member
[MRP_VIEW_LOOKUP] requested=cfunction.ext resolved=robotol.ext insert_at=1904 entry_size=30 index_end=1904 offset=231624 stored_size=161178 result=HIT
[MRP_MEMBER_REQUEST] package=gwy/jjfb.mrp requested=robotol.ext scope=mrp_member
[EXT_RESOLVE] package=gwy/jjfb.mrp requested=robotol.ext resolved=robotol.ext strategy=exact scope=mrp_member result=HIT
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=robotol.ext module_id=3 requested=robotol.ext resolved=robotol.ext origin=MRP_MEMBER strategy=exact state=EXTRACTED stored_size=161178 unpacked_size=253420 extracted_size=253420 helper=0x0
[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/jjfb.mrp primary=robotol.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/game_jjfb_cfunction.mrp original_sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 vfs_remap=enabled evidence=CROSS_TARGET
[REG_PRIMARY] package=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\game_files\mythroad\320x480/gwy/wxjwq.mrp primary=mmochat.ext evidence=CROSS_TARGET source=reg.ext+first_ext_member
[EXT_RESOLVE] package=gwy/wxjwq.mrp requested=mmochat.ext resolved=mmochat.ext strategy=exact scope=mrp_member result=HIT
[MODULE_REGISTRY] package=gwy/wxjwq.mrp module=mmochat.ext module_id=4 requested=mmochat.ext resolved=mmochat.ext origin=MRP_MEMBER strategy=exact state=EXTRACTED stored_size=206148 unpacked_size=320292 extracted_size=320292 helper=0x0
[MRP_MEMBER_VIEW] reg_primary_installed package=gwy/wxjwq.mrp primary=mmochat.ext view=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\vmrp_run\overlay/mrp_member_view/game_wxjwq_cfunction.mrp original_sha256=6ec628419bc4c0ca1f8fba37b0c5179961220cd53591fc55eba26735defbd02d vfs_remap=enabled evidence=CROSS_TARGET
[JJFB_PACKAGE_SCOPE] package=gwy/jjfb.mrp primary=robotol.ext class=mrc_loader_game cload=scoped evidence=CROSS_TARGET
[JJFB_CLOAD_SCOPE] request=cfunction.ext package=gwy/jjfb.mrp resolved=robotol.ext reason=package_reg_primary evidence=CROSS_TARGET
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=dsm:cfunction.ext module_id=5 requested=dsm:cfunction.ext resolved= origin=MRP_MEMBER strategy=unknown state=DISCOVERED stored_size=0 unpacked_size=0 extracted_size=0 helper=0x0
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=cfunction.ext module_id=5 requested=dsm:cfunction.ext resolved=cfunction.ext origin=DSM strategy=unknown state=MAPPED stored_size=0 unpacked_size=0 extracted_size=0 helper=0xA4178
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=cfunction.ext module_id=5 requested=dsm:cfunction.ext resolved=cfunction.ext origin=DSM strategy=unknown state=REGISTERED stored_size=0 unpacked_size=0 extracted_size=0 helper=0xA4178
[EXT_LOAD] module_id=2 load_id=3 package=gwy/jjfb.mrp requested=mrc_loader.ext resolved=mrc_loader.ext origin=MRP_MEMBER stage=EXTRACTED size=232
[EXT_LOAD] module_id=3 load_id=4 package=gwy/jjfb.mrp requested=robotol.ext resolved=robotol.ext origin=MRP_MEMBER stage=EXTRACTED size=253420
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=cfunction.ext module_id=5 requested=dsm:cfunction.ext resolved=cfunction.ext origin=DSM strategy=unknown state=ENTRY_CALLED stored_size=0 unpacked_size=0 extracted_size=0 helper=0xA4178
[JJFB_ROBOTOL_ENTER_REJECT] reason=dsm_context module=dsm:cfunction.ext package=gwy/jjfb.mrp pc=0xA4178 module_id=5 evidence=OBSERVED
[R9_WRITE] event_seq=1 old=0x0 new=0x280400 reason=MODULE_R9_SWITCH_ENTER frame_id=1 scope_id=1 host_callsite=bridge_mr_extHelper guest_pc=0x0 guest_module=cfunction.ext depth_before=0 depth_after=1 evidence=OBSERVED
[R9_SWITCH] stage=ENTER from=? to=cfunction.ext old_r9=0x0 new_r9=0x280400 call_kind=MR_HELPER depth=1 er_rw_size=9680 evidence=DOCUMENTED module_r9_switch_gate=open allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY phase6b_b_gate=blocked note=r9_only
[R9_SCOPE] stage=ENTER scope_id=1 frame_id=1 owns_frame=yes requested_kind=MR_HELPER caller=? callee=cfunction.ext current_r9=0x0 target_r9=0x280400 result=SWITCHED frame_pushed=yes depth_before=0 depth_after=1 top_frame_id=1 top_frame_kind=MR_HELPER generation=1 module_r9_switch_gate=open nested_r9_scope_gate=open allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=DOCUMENTED
[R9_SWITCH] stage=DSM_R9_GUARD_ATTACH code=[0x80000,0xD1154) module_id=5 evidence=DOCUMENTED note=block_hook_r9_only
[EXT_LOAD] module_id=2 load_id=3 package=gwy/jjfb.mrp requested=mrc_loader.ext resolved=mrc_loader.ext origin=MRP_MEMBER stage=PENDING size=0
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=mrc_loader.ext module_id=2 requested=mrc_loader.ext resolved=mrc_loader.ext origin=MRP_MEMBER strategy=exact state=MAPPED stored_size=219 unpacked_size=232 extracted_size=232 helper=0x0
[JJFB_SHELL_EXT] package=mrc_loader.ext member=mrc_loader.ext loaded=yes base=0x2AEB04 size=316 evidence=DOCUMENTED note=raw_base_refine
[R9_SWITCH_BLOCKED] reason=CALLEE_ER_RW_NOT_AVAILABLE module=mrc_loader.ext call_kind=BOOTSTRAP_ENTRY caller=cfunction.ext callee_id=2 depth=1 module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked r9_policy=OBSERVE_ONLY evidence=DOCUMENTED note=no_dsm_r9_fallback
[R9_SCOPE] stage=ENTER scope_id=2 frame_id=0 owns_frame=no requested_kind=BOOTSTRAP_ENTRY caller=cfunction.ext callee=mrc_loader.ext current_r9=0x0 target_r9=0x0 result=BLOCKED_NO_ER_RW frame_pushed=no depth_before=1 depth_after=1 top_frame_id=1 top_frame_kind=MR_HELPER generation=1 module_r9_switch_gate=open nested_r9_scope_gate=open allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=DOCUMENTED
[CROSS_MODULE_CALL] transfer_id=1 hop=DSM_TO_LOADER from_module=dsm:cfunction.ext from_module_id=5 from_pc=0x89B78 to_module=mrc_loader.ext to_module_id=2 target=0x2AEB0C target_offset=0x8 r0=0x0 r1=0x2AEB0C lr=0x89B78 evidence=OBSERVED
[R0_LAYER] stage=DSM_TO_DISPATCH r0=0x0 dispatch_module=mrc_loader.ext
[R9_SWITCH] stage=ENTER already_switched=1 from=mrc_loader.ext to=cfunction.ext r9=0x280400 call_kind=RUNTIME_ENTRY depth=1 evidence=DOCUMENTED note=dsm_return_side_stack
[R9_SCOPE] stage=ENTER scope_id=3 frame_id=1 owns_frame=no requested_kind=RUNTIME_ENTRY caller=mrc_loader.ext callee=cfunction.ext current_r9=0x280400 target_r9=0x280400 result=ALREADY_SWITCHED frame_pushed=no depth_before=1 depth_after=1 top_frame_id=1 top_frame_kind=MR_HELPER generation=1 module_r9_switch_gate=open nested_r9_scope_gate=open allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=DOCUMENTED
[R9_SWITCH] stage=ARM_DSM_RETURN caller_id=2 call_pc=0x2AEB30 return_pc=0x2AEB34 frame_id=0 owns_r9_pop=0 side_depth=1 evidence=DOCUMENTED note=mrp_dsm_helper_blx
[CROSS_MODULE_CALL] transfer_id=2 hop=OTHER from_module=mrc_loader.ext from_module_id=2 from_pc=0x2AEB30 to_module=dsm:cfunction.ext to_module_id=5 target=0x89CF4 target_offset=0x9CF4 r0=0x2AEB48 r1=0x14 lr=0x2AEB34 evidence=OBSERVED
[R9_SCOPE] stage=NOOP scope_id=3 owns_frame=no requested_by=CROSS_MODULE_MRP_TO_DSM requested_frame_id=1 actual_top_frame_id=1 top_frame_kind=MR_HELPER leave_action=NOOP emu_exit_reason=YIELD_TO_NESTED_GUEST depth_before=1 depth_after=1 old_r9=0x280400 new_r9=0x280400 generation=1 module_r9_switch_gate=open nested_r9_scope_gate=open allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY evidence=DOCUMENTED
[R9_SCOPE_BALANCE] stage=NOOP requested_by=CROSS_MODULE_MRP_TO_DSM owns_frame=no top_frame_id=1 top_frame_kind=MR_HELPER restore_r9=0x280400 emu_exit_reason=YIELD_TO_NESTED_GUEST nested_r9_scope_gate=open next_allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY guest_callback_frame_gate=blocked bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked evidence=DOCUMENTED note=tokenized_leave_keeps_outer_frame
[JJFB_CFN_P_SLOT] package=gwy/jjfb.mrp module=mrc_loader.ext helper=0x2AEB48 P=0x2AC8DC slot=0x2AEB08 lr=0x0 reason=fixr9_c_function_p_image_plus4 evidence=DOCUMENTED wrote=1
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=mrc_loader.ext event=CFN_P_SLOT_PUBLISH P=0x2AC8DC p0=0 p4=0 registry_base=0x0 registry_len=0 slot=0x2AEB08 evidence=DOCUMENTED
[JJFB_EXTCHUNK_CONTRACT] struct=mrc_extChunk_st check_off=0x0 init_func_off=0x4 event_off=0x8 sendAppEvent_off=0x28 check_magic=0x7FD854EB size=0x40 source=mr_helper.h+doc/ext_important evidence=DOCUMENTED
[JJFB_E10A31B] milestone=GAMELIST_CHUNK_CREATED chunk=0x682A5C P=0x2AC8DC helper=0x2AEB48 module=mrc_loader.ext module_id=2 reason=mr_c_function_new_contract evidence=OBSERVED
[JJFB_EXTCHUNK_ALLOC] module=mrc_loader.ext module_id=2 guest=0x682A5C size=0x40 helper=0x2AEB48 evidence=DOCUMENTED
[JJFB_EXTCHUNK_SLOT] module=mrc_loader.ext off=0x04 value=0x2AEB0C meaning=init_func evidence=DOCUMENTED
[JJFB_EXTCHUNK_SLOT] module=mrc_loader.ext off=0x08 value=0x2AEB48 meaning=event_helper evidence=DOCUMENTED
[JJFB_EXTCHUNK_SLOT] module=mrc_loader.ext off=0x28 value=0x280058 meaning=sendAppEvent evidence=DOCUMENTED
[JJFB_EXTCHUNK_PUBLISH] module=mrc_loader.ext P=0x2AC8DC off=0x0C old=0x0 new=0x682A5C reason=mr_c_function_new_contract evidence=DOCUMENTED
[CALLBACK_FRAME] boundary=GUEST_BEFORE_HOST_CALLBACK callback_id=0 module_id=0 module=mrc_loader.ext slot=0x0 slot_name= call_pc=0x0 continuation_pc=0x0 continuation_file_offset=0x0 r0=0x0 r1=0x0 r2=0x0 r3=0x0 r4=0x0 r5=0x0 r6=0x0 r7=0x0 r8=0x0 r9=0x0 r10=0x0 r11=0x0 r12=0x0 sp=0x0 lr=0x0 cpsr=0x0 thumb=no guest_callback_frame_gate=blocked module_r9_switch_gate=open bootstrap_entry_r9_gate=blocked phase6b_b_gate=blocked er_rw_metadata_timing_gate=blocked nested_r9_scope_gate=open evidence=OBSERVED note=observe_only_no_reg_restore
[JJFB_EXTCHUNK_ALLOC] module=mrc_loader.ext module_id=2 guest=0x682A5C size=0x40 helper=0x2AEB48 evidence=DOCUMENTED
[JJFB_EXTCHUNK_SLOT] module=mrc_loader.ext off=0x04 value=0x2AEB0C meaning=init_func evidence=DOCUMENTED
[JJFB_EXTCHUNK_SLOT] module=mrc_loader.ext off=0x08 value=0x2AEB48 meaning=event_helper evidence=DOCUMENTED
[JJFB_EXTCHUNK_SLOT] module=mrc_loader.ext off=0x28 value=0x280058 meaning=sendAppEvent evidence=DOCUMENTED
[JJFB_EXTCHUNK_PUBLISH] module=mrc_loader.ext P=0x2AC8DC off=0x0C old=0x682A5C new=0x682A5C reason=ext_register_contract evidence=DOCUMENTED
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=mrc_loader.ext module_id=2 requested=mrc_loader.ext resolved=mrc_loader.ext origin=MRP_MEMBER strategy=exact state=REGISTERED stored_size=219 unpacked_size=232 extracted_size=232 helper=0x2AEB48
[EXT_LOAD] module_id=2 load_id=3 package=gwy/jjfb.mrp requested=mrc_loader.ext resolved=mrc_loader.ext origin=MRP_MEMBER stage=REGISTERED size=232
[CONTEXT_CANDIDATE] kind=mrc_extChunk_ptr chunk=0x682A5C from=P+0x0C
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=mrc_loader.ext event=P_ZERO_SKIP P=0x2AC8DC p0=0x0 p4=0x0 registry_base=0x0 registry_len=0x0 r9=0x0 pc=0x0 note=await_guest_fill evidence=OBSERVED
[JJFB_GAME_ER_RW_SOURCE_MISSING] package=gwy/jjfb.mrp module=mrc_loader.ext needed=module_map_or_mrpgcmap_metadata map_rw=0x0 P=0x2AC8DC evidence=TARGET_OBSERVED
[R9_SWITCH] stage=RESTORE_AFTER_DSM to_caller_id=2 pc=0x2AEB34 return_pc=0x2AEB34 call_pc=0x2AEB30 cur_r9=0x280400 lr=0xA2548 executed=1 owns_r9_pop=0 side_depth=0 frame_id=0 evidence=DOCUMENTED note=mrp_dsm_helper_return
[MODULE_REGISTRY] package=gwy/jjfb.mrp module=mrc_loader.ext module_id=2 requested=mrc_loader.ext resolved=mrc_loader.ext origin=MRP_MEMBER strategy=exact state=ENTRY_CALLED stored_size=219 unpacked_size=232 extracted_size=232 helper=0x2AEB48
[ENTRY_TRANSFER] dispatch_target=0x2AEB34 dispatch_module=mrc_loader.ext dispatch_offset=0x30 callee_module=mrc_loader.ext first_executed_pc=0x2AEB34 r0=0x0 stage=FIRST_EXECUTED_PC nested_enter=1
[ENTRY_RECONCILE] module=mrc_loader.ext module_id=2 image_base=0x2AEB04 header_raw=0x2AEB0C header_norm=0x2AEB0C chunk_field_04_raw=0x2AEB0C chunk_field_04_norm=0x2AEB0C helper_raw=0x2AEB48 helper_norm=0x2AEB48 observed_first_pc_raw=0x2AEB34 observed_norm=0x2AEB34 relation=HEADER_ENTRY_WRONG dsm_select_correct=unknown chunk_field_04_writer=NONE_BEFORE_SELECT
[JJFB_MODULE_IDENTITY] package=gwy/jjfb.mrp module=mrc_loader.ext module_id=2 image_base=0x2AEB04 image_size=0x13C entry_pc=0x2AEB34 entry_in_range=yes sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 source=descriptor_launcher
[JJFB_ROBOTOL_ENTER_REJECT] reason=loader_context module=mrc_loader.ext package=gwy/jjfb.mrp pc=0x2AEB34 module_id=2 evidence=OBSERVED
[HELPER_ABI] stage=MODULE_ENTER nested_enter_module=mrc_loader.ext module=mrc_loader.ext module_id=2 module_offset=0x30 pc=0x2AEB34 helper=0x2AEB48 target=0x2AEB34 origin=GUEST_NESTED r0=0x0 r1=0x2AEB0C r2=0x3A r3=0x27FB5C r9=0x280400 sp=0x27FD88 lr=0xA2548 cpsr=0x600001D3 caller_module=dsm:cfunction.ext caller_offset=0x22548
[CROSS_MODULE_CALL] transfer_id=3 hop=DSM_TO_LOADER from_module=dsm:cfunction.ext from_module_id=5 from_pc=0xA2548 to_module=mrc_loader.ext to_module_id=2 target=0x2AEB34 target_offset=0x30 r0=0x0 r1=0x2AEB0C lr=0xA2548 evidence=OBSERVED
```
