# E8F long path / 0x101AB

- **enqueue_core**: 0x30D24C
- **short_path_ret**: 0x30D28A
- **long_path_entry**: 0x30D28C
- **queue_base_off**: 0x7D8
- **queue_depth_off**: 0x7D8+0x6C=0x844
- **long_path_gate**: BLE when [R9+0x844] <= 0
- **plat_101ab_lit_pc**: 0x30D2AA
- **plat_101ab_call**: BL 0x304558 with r0=0x101AB r1=saved_R0 r3=2
- **fe8_store_pc**: 0x30D262
- **b7d_store_pc**: 0x30D284
- **note**: Normal R0_EVENTCODE_2 took short path (b7d=1); long path needs depth<=0 after FE8 store
- **evidence**: TARGET_OBSERVED
