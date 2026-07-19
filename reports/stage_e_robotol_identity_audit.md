# Stage E 鈥?Robotol Identity Audit

- **purpose:** prove ROBOTOL_ENTER is not applied to mrc_loader/DSM
- **reject_count:** 2
- **true_robotol_entry:** yes
- **fake_bootstrap_robotol_enter:** no

## Sample lines

```
[JJFB_MODULE_IDENTITY] package=gwy/jjfb.mrp module=dsm:cfunction.ext module_id=5 image_base=0x80000 image_size=0x51154 entry_pc=0xA4178 entry_in_range=yes sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 source=descriptor_launcher
[JJFB_ROBOTOL_ENTER_REJECT] reason=dsm_context module=dsm:cfunction.ext package=gwy/jjfb.mrp pc=0xA4178 module_id=5 evidence=OBSERVED
[HELPER_ABI] stage=MODULE_ENTER nested_enter_module=dsm:cfunction.ext module=dsm:cfunction.ext module_id=5 module_offset=0x24178 pc=0xA4178 helper=0xA4178 target=0xA4178 origin=HOST_BRIDGE r0=0x2803E4 r1=0x1 r2=0x2803B4 r3=0xC r9=0x280400 sp=0x280000 lr=0x0 cpsr=0x0 caller_module=unknown caller_offset=0x0
[JJFB_MODULE_IDENTITY] package=gwy/jjfb.mrp module=mrc_loader.ext module_id=2 image_base=0x2AEB04 image_size=0x13C entry_pc=0x2AEB34 entry_in_range=yes sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 source=descriptor_launcher
[JJFB_ROBOTOL_ENTER_REJECT] reason=loader_context module=mrc_loader.ext package=gwy/jjfb.mrp pc=0x2AEB34 module_id=2 evidence=OBSERVED
[HELPER_ABI] stage=MODULE_ENTER nested_enter_module=mrc_loader.ext module=mrc_loader.ext module_id=2 module_offset=0x30 pc=0x2AEB34 helper=0x2AEB48 target=0x2AEB34 origin=GUEST_NESTED r0=0x0 r1=0x2AEB0C r2=0x3A r3=0x27FB5C r9=0x280400 sp=0x27FD88 lr=0xA2548 cpsr=0x600001D3 caller_module=dsm:cfunction.ext caller_offset=0x22548
[JJFB_MODULE_IDENTITY] package=gwy/jjfb.mrp module=robotol.ext module_id=3 image_base=0x2D8DF4 image_size=0x3DE2C entry_pc=0x303B92 entry_in_range=yes sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 source=descriptor_launcher
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x303B92 image_base=0x2D8DF4 evidence=OBSERVED
[HELPER_ABI] stage=ROBOTOL_ENTER nested_enter_module=robotol.ext module=robotol.ext module_id=3 module_offset=0x2AD9E pc=0x303B92 helper=0x304AED target=0x303B92 origin=GUEST_NESTED r0=0x0 r1=0x2D8DFC r2=0x3A r3=0x27FB4C r9=0x280400 sp=0x27FD78 lr=0xA2548 cpsr=0x600001F3 caller_module=dsm:cfunction.ext caller_offset=0x22548
[JJFB_MODULE_IDENTITY] package=gwy/jjfb.mrp module=robotol.ext module_id=3 image_base=0x2D8DF4 image_size=0x3DE2C entry_pc=0x304AEC entry_in_range=yes sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 source=descriptor_launcher
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x304AEC image_base=0x2D8DF4 evidence=OBSERVED
[HELPER_ABI] stage=ROBOTOL_ENTER nested_enter_module=robotol.ext module=robotol.ext module_id=3 module_offset=0x2BCF8 pc=0x304AEC helper=0x304AED target=0x304AEC origin=HOST_BRIDGE r0=0x2AC8DC r1=0x6 r2=0x0 r3=0x7DB r9=0x2B1858 sp=0x280000 lr=0x0 cpsr=0x0 caller_module=unknown caller_offset=0x0
[JJFB_MODULE_IDENTITY] package=gwy/jjfb.mrp module=robotol.ext module_id=3 image_base=0x2D8DF4 image_size=0x3DE2C entry_pc=0x304AEC entry_in_range=yes sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 source=descriptor_launcher
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x304AEC image_base=0x2D8DF4 evidence=OBSERVED
[HELPER_ABI] stage=ROBOTOL_ENTER nested_enter_module=robotol.ext module=robotol.ext module_id=3 module_offset=0x2BCF8 pc=0x304AEC helper=0x304AED target=0x304AEC origin=HOST_BRIDGE r0=0x2AC8DC r1=0x8 r2=0x69EEB4 r3=0x10 r9=0x2B1858 sp=0x280000 lr=0x0 cpsr=0x0 caller_module=unknown caller_offset=0x0
[JJFB_MODULE_IDENTITY] package=gwy/jjfb.mrp module=robotol.ext module_id=3 image_base=0x2D8DF4 image_size=0x3DE2C entry_pc=0x304AEC entry_in_range=yes sha256=52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036 source=descriptor_launcher
[JJFB_ROBOTOL_ENTRY_CALLED] package=gwy/jjfb.mrp module=robotol.ext module_id=3 pc=0x304AEC image_base=0x2D8DF4 evidence=OBSERVED
[HELPER_ABI] stage=ROBOTOL_ENTER nested_enter_module=robotol.ext module=robotol.ext module_id=3 module_offset=0x2BCF8 pc=0x304AEC helper=0x304AED target=0x304AEC origin=GUEST_NESTED r0=0x2AC8DC r1=0x0 r2=0x0 r3=0x7DB r9=0x2B1858 sp=0x280000 lr=0x0 cpsr=0x0 caller_module=unknown caller_offset=0x0
```
