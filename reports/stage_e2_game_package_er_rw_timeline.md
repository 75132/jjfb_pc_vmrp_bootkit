# Stage E2 鈥?game_package ER_RW timeline samples

```
[JJFB_CFN_P_SLOT] package=? module=? helper=0xA4178 P=0x2803E4 slot=0x80004 lr=0x80034 reason=fixr9_c_function_p_image_plus4 evidence=DOCUMENTED wrote=1
[JJFB_GAME_P_TIMELINE] package=? module=? event=CFN_P_SLOT_PUBLISH P=0x2803E4 p0=0 p4=0 registry_base=0x0 registry_len=0 slot=0x80004 evidence=DOCUMENTED
[JJFB_HELPER_RETARGET] old=0xA4178 new=0x2AEB48 P=0x2AC8DC origin=LOG_PARSE evidence=DOCUMENTED source=mythroad.c:_mr_c_function_new
[JJFB_CFN_P_SLOT] package=gwy/jjfb.mrp module=mrc_loader.ext helper=0x2AEB48 P=0x2AC8DC slot=0x2AEB08 lr=0x0 reason=fixr9_c_function_p_image_plus4 evidence=DOCUMENTED wrote=1
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=mrc_loader.ext event=CFN_P_SLOT_PUBLISH P=0x2AC8DC p0=0 p4=0 registry_base=0x0 registry_len=0 slot=0x2AEB08 evidence=DOCUMENTED
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=mrc_loader.ext event=P_ZERO_SKIP P=0x2AC8DC p0=0x0 p4=0x0 registry_base=0x0 registry_len=0x0 r9=0x0 pc=0x0 note=await_guest_fill evidence=OBSERVED
[JJFB_GAME_ER_RW_SOURCE_MISSING] package=gwy/jjfb.mrp module=mrc_loader.ext needed=module_map_or_mrpgcmap_metadata map_rw=0x0 P=0x2AC8DC evidence=TARGET_OBSERVED
[JJFB_HELPER_RETARGET] old=0x2AEB48 new=0x304AED P=0x2AC8DC origin=LOG_PARSE evidence=DOCUMENTED source=mythroad.c:_mr_c_function_new
[JJFB_CFN_P_SLOT] package=gwy/jjfb.mrp module=robotol.ext helper=0x304AED P=0x2AC8DC slot=0x2D8DF8 lr=0x0 reason=fixr9_c_function_p_image_plus4 evidence=DOCUMENTED wrote=1
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=robotol.ext event=CFN_P_SLOT_PUBLISH P=0x2AC8DC p0=0 p4=0 registry_base=0x0 registry_len=0 slot=0x2D8DF8 evidence=DOCUMENTED
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=robotol.ext event=P_ZERO_SKIP P=0x2AC8DC p0=0x0 p4=0x0 registry_base=0x0 registry_len=0x0 r9=0x0 pc=0x0 note=await_guest_fill evidence=OBSERVED
[JJFB_GAME_ER_RW_SOURCE_MISSING] package=gwy/jjfb.mrp module=robotol.ext needed=module_map_or_mrpgcmap_metadata map_rw=0x0 P=0x2AC8DC evidence=TARGET_OBSERVED
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x303B92 image_base=0x2D8DF4 evidence=OBSERVED
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x303B92 method=0 r0=0x0 r9=0x280400 route=bootstrap_first_pc evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_ER_RW_BIND] package=gwy/jjfb.mrp module=robotol.ext module_id=3 P=0x2AC8DC p_base=0x2B1854 p_len=0x151C registry_base=0x2B1854 registry_len=0x151C reason=mr_c_function_st_metadata_bind evidence=DOCUMENTED
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=robotol.ext event=ER_RW_BOUND P=0x2AC8DC p0=0x2B1854 p4=0x151C registry_base=0x2B1854 registry_len=0x151C r9=0x0 pc=0x0 note=mr_c_function_st_metadata_bind evidence=OBSERVED
[JJFB_R9_SWITCH_ATTEMPT] package=gwy/jjfb.mrp module=robotol.ext module_id=3 callee_er_rw=0x2B1854 caller_r9=0x280400 call_kind=RUNTIME_ENTRY reason=platform_er_rw_publication_restore evidence=DOCUMENTED
[JJFB_R9_SWITCH_OK] package=gwy/jjfb.mrp module=robotol.ext module_id=3 r9=0x2B1854 er_rw_len=0x151C evidence=DOCUMENTED
[JJFB_INIT_SEQ] queued=1 reason=retry_after_context_restore evidence=DOCUMENTED source=mythroad.c:mr_doExt/start.mr
[JJFB_GAME_P_TIMELINE] package=gwy/jjfb.mrp module=robotol.ext event=BIND_REFRESH P=0x2AC8DC p0=0x2B1854 p4=0x151C registry_base=0x2B1854 registry_len=0x151C r9=0x0 pc=0x0 note=mr_c_function_st_metadata_bind evidence=OBSERVED
[JJFB_INIT_SEQ] action=deliver_after_code_1 helper=0x304AED version=2011 appinfo=0x682AA4 evidence=DOCUMENTED source=mythroad_mini.c:mr_doExt
[JJFB_INIT_SEQ] helper=0x304AED code=6 P=0x2AC8DC input=0x0 len=2011 er_rw=0x2B1858 evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x304AEC image_base=0x2D8DF4 evidence=OBSERVED
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x304AEC method=6 r0=0x2AC8DC r9=0x2B1858 route=bootstrap_first_pc evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_INIT_SEQ] helper=0x304AED code=8 P=0x2AC8DC input=0x682AA4 len=16 er_rw=0x2B1858 evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x304AEC image_base=0x2D8DF4 evidence=OBSERVED
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x304AEC method=8 r0=0x2AC8DC r9=0x2B1858 route=bootstrap_first_pc evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_INIT_SEQ] helper=0x304AED code=0 P=0x2AC8DC input=0x0 len=2011 er_rw=0x2B1858 evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x304AEC image_base=0x2D8DF4 evidence=OBSERVED
[JJFB_MRC_INIT_ATTEMPT] module=robotol.ext pc=0x304AEC method=0 r0=0x2AC8DC r9=0x2B1858 route=helper_code0_candidate evidence=OBSERVED note=first_pc_not_proof_of_mrc_init
[JJFB_MRC_INIT] module=robotol.ext helper=0x304AED method=0 ret=-1 route=mr_extHelper evidence=DOCUMENTED source=mythroad.c:case_801
[JJFB_INIT_SEQ] delivered ret6=0 ret8=0 ret0=-1 evidence=OBSERVED
```
