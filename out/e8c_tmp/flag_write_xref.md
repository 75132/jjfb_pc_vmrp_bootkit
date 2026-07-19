# E8C flag write xref
ext=C:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit\out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext
code_base=0x2D8DF4

Classification is coarse (TARGET_OBSERVED heuristics). Writers set flag bytes via
`LDR lit; ADD rN,r9; STRB/STR`. Load-only sites are the idle CMP readers.

## offset 0xC44 (site 0x3066AC)
- load_width: s8 cmp_imm: 1
- load_sites: 8 store_sites: 8
  - WRITE strb_indexed store_pc=0x2F4E82 ldr=0x2F4E7E class=unknown
  - WRITE str_imm store_pc=0x2F7F56 ldr=0x2F7F4A class=unknown
  - WRITE strb_indexed store_pc=0x2FB286 ldr=0x2FB282 class=robotol_internal
  - WRITE str_imm store_pc=0x2FC8CA ldr=0x2FC8C6 class=robotol_internal
  - WRITE strb_imm store_pc=0x2FEDFA ldr=0x2FEDF4 class=robotol_internal
  - WRITE strb_indexed store_pc=0x2FEE4E ldr=0x2FEE4A class=robotol_internal
  - WRITE strb_indexed store_pc=0x30CC72 ldr=0x30CC6C class=unknown
  - WRITE strb_indexed store_pc=0x311C3E ldr=0x311C36 class=unknown
  - READ ldr_use ldr=0x2E87BE
  - READ ldr_use ldr=0x2E87D6
  - READ ldr_use ldr=0x2E87E8
  - READ ldrsb_use ldr=0x2F5522
  - READ load_only ldr=0x2F5902
  - READ load_only ldr=0x2F5942
  - READ ldrsb_use ldr=0x3066AC
  - READ ldr_use ldr=0x3112A0

## offset 0xC9D (site 0x3066BC)
- load_width: s8 cmp_imm: 1
- load_sites: 1 store_sites: 0
  - no STR/STRB sites found for this literal (ROBOTOL_STATE_FLAG_NEVER_SET candidate)
  - READ ldrsb_use ldr=0x3066BC

## offset 0xCD1 (site 0x3066C8)
- load_width: s8 cmp_imm: 0
- load_sites: 1 store_sites: 1
  - WRITE str_imm store_pc=0x2F76D8 ldr=0x2F76CC class=unknown
  - READ ldrsb_use ldr=0x3066C8

## offset 0xCF5 (site 0x3066D2)
- load_width: s8 cmp_imm: 1
- load_sites: 2 store_sites: 2
  - WRITE str_imm store_pc=0x2E7DBC ldr=0x2E7DB8 class=robotol_internal
  - WRITE str_imm store_pc=0x2F7F2C ldr=0x2F7F20 class=unknown
  - READ ldrsb_use ldr=0x2F596E
  - READ ldrsb_use ldr=0x3066D2

## offset 0x11B0 (site 0x3066DC)
- load_width: u32 cmp_imm: 0
- load_sites: 2 store_sites: 1
  - WRITE str_imm store_pc=0x2F72D8 ldr=0x2F72CC class=unknown
  - READ ldr_use ldr=0x2F597A
  - READ ldr_use ldr=0x3066DC
