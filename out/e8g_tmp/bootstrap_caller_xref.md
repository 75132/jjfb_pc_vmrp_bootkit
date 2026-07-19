# E8G bootstrap caller provenance
code_base=0x2D8DF4

## C44_2F4E82 writer 0x2F4E82
### caller 0x302340
- function: `0x3020C8` .. `0x302118`
- upstream BL callers (1): 0x30103C
- BL to writer/fn: none-in-fn-scan
- plat helpers: []
- R9 offset lits: [{'pc': '0x3020CA', 'off': '0xC6C'}, {'pc': '0x3020D8', 'off': '0xEEC'}, {'pc': '0x3020DE', 'off': '0x11D0'}]
- key predicates:
  - 0x3020CA: LDR r1,[pc,#0x3D0] =0xC6C
  - 0x3020CE: ADD r1, r9
  - 0x3020D8: LDR r0,[pc,#0x3C4] =0xEEC
  - 0x3020DE: LDR r7,[pc,#0x3B8] =0x11D0
  - 0x3020E4: ADD r0, r9
  - 0x3020E8: CMP r4, #13
  - 0x3020EA: ADD r7, r9
  - 0x3020EC: BEQ -> 0x3021D2
  - 0x3020EE: BGT -> 0x302118
  - 0x3020F0: CMP r4, #2
  - 0x3020F2: BEQ -> 0x302100
  - 0x3020F4: CMP r4, #5

### caller 0x302362
- function: `0x3020C8` .. `0x302118`
- upstream BL callers (1): 0x30103C
- BL to writer/fn: none-in-fn-scan
- plat helpers: []
- R9 offset lits: [{'pc': '0x3020CA', 'off': '0xC6C'}, {'pc': '0x3020D8', 'off': '0xEEC'}, {'pc': '0x3020DE', 'off': '0x11D0'}]
- key predicates:
  - 0x3020CA: LDR r1,[pc,#0x3D0] =0xC6C
  - 0x3020CE: ADD r1, r9
  - 0x3020D8: LDR r0,[pc,#0x3C4] =0xEEC
  - 0x3020DE: LDR r7,[pc,#0x3B8] =0x11D0
  - 0x3020E4: ADD r0, r9
  - 0x3020E8: CMP r4, #13
  - 0x3020EA: ADD r7, r9
  - 0x3020EC: BEQ -> 0x3021D2
  - 0x3020EE: BGT -> 0x302118
  - 0x3020F0: CMP r4, #2
  - 0x3020F2: BEQ -> 0x302100
  - 0x3020F4: CMP r4, #5


## C44_2FEDFA writer 0x2FEDFA
### caller 0x2FC048
- function: `0x2FC03C` .. `0x2FC064`
- upstream BL callers (2): 0x2DAE24, 0x2FECAA
- BL to writer/fn: [{'pc': '0x2FC048', 'target': '0x2FED14'}]
- plat helpers: []
- R9 offset lits: [{'pc': '0x2FC03E', 'off': '0x11EC'}, {'pc': '0x2FC04C', 'off': '0x1A8'}]
- key predicates:
  - 0x2FC03E: LDR r4,[pc,#0x24] =0x11EC
  - 0x2FC040: ADD r4, r9
  - 0x2FC04C: LDR r1,[pc,#0x18] =0x1A8
  - 0x2FC050: ADD r1, r9
  - 0x2FC05A: LDR r0,[pc,#0x10] =0x18664
  - 0x2FC05C: ADD r0, r15


## C44_2FEE4E writer 0x2FEE4E
### caller 0x2FC048
- function: `0x2FC03C` .. `0x2FC064`
- upstream BL callers (2): 0x2DAE24, 0x2FECAA
- BL to writer/fn: [{'pc': '0x2FC048', 'target': '0x2FED14'}]
- plat helpers: []
- R9 offset lits: [{'pc': '0x2FC03E', 'off': '0x11EC'}, {'pc': '0x2FC04C', 'off': '0x1A8'}]
- key predicates:
  - 0x2FC03E: LDR r4,[pc,#0x24] =0x11EC
  - 0x2FC040: ADD r4, r9
  - 0x2FC04C: LDR r1,[pc,#0x18] =0x1A8
  - 0x2FC050: ADD r1, r9
  - 0x2FC05A: LDR r0,[pc,#0x10] =0x18664
  - 0x2FC05C: ADD r0, r15


## CF5_2E7DBC writer 0x2E7DBC
### caller 0x2E32A2
- function: `0x2E32A2` .. `0x2E3AA4`
- upstream BL callers (0): none
- BL to writer/fn: [{'pc': '0x2E32A2', 'target': '0x2E7DA8'}]
- plat helpers: [{'pc': '0x2E32BA', 'helper': '0x304558'}, {'pc': '0x2E32D2', 'helper': '0x304558'}, {'pc': '0x2E330E', 'helper': '0x304558'}, {'pc': '0x2E3340', 'helper': '0x304558'}, {'pc': '0x2E3358', 'helper': '0x304558'}, {'pc': '0x2E3370', 'helper': '0x304558'}, {'pc': '0x2E33FE', 'helper': '0x304558'}, {'pc': '0x2E342E', 'helper': '0x304558'}, {'pc': '0x2E3446', 'helper': '0x304558'}, {'pc': '0x2E345E', 'helper': '0x304558'}, {'pc': '0x2E34DA', 'helper': '0x304558'}, {'pc': '0x2E35B8', 'helper': '0x304558'}, {'pc': '0x2E3644', 'helper': '0x304558'}, {'pc': '0x2E365C', 'helper': '0x304558'}, {'pc': '0x2E3674', 'helper': '0x304558'}, {'pc': '0x2E368C', 'helper': '0x304558'}, {'pc': '0x2E36CC', 'helper': '0x304558'}, {'pc': '0x2E36E4', 'helper': '0x304558'}, {'pc': '0x2E36FC', 'helper': '0x304558'}, {'pc': '0x2E3722', 'helper': '0x304558'}, {'pc': '0x2E373A', 'helper': '0x304558'}, {'pc': '0x2E3798', 'helper': '0x304558'}, {'pc': '0x2E381E', 'helper': '0x304558'}, {'pc': '0x2E3902', 'helper': '0x304558'}, {'pc': '0x2E391A', 'helper': '0x304558'}, {'pc': '0x2E3932', 'helper': '0x304558'}, {'pc': '0x2E394C', 'helper': '0x304558'}, {'pc': '0x2E396C', 'helper': '0x304558'}, {'pc': '0x2E3984', 'helper': '0x304558'}, {'pc': '0x2E3A24', 'helper': '0x304558'}, {'pc': '0x2E3A3C', 'helper': '0x304558'}]
- R9 offset lits: [{'pc': '0x2E339E', 'off': '0xB5C'}, {'pc': '0x2E33A6', 'off': '0xB60'}, {'pc': '0x2E33B6', 'off': '0x11C0'}, {'pc': '0x2E3464', 'off': '0xBE0'}, {'pc': '0x2E3466', 'off': '0x8D0'}, {'pc': '0x2E34F0', 'off': '0x1378'}, {'pc': '0x2E34FC', 'off': '0x1374'}, {'pc': '0x2E3508', 'off': '0x137C'}]
- key predicates:
  - 0x2E32B8: LDR r0,[pc,#0x1F0] =0x1E206
  - 0x2E32D0: LDR r0,[pc,#0x1D8] =0x1E206
  - 0x2E330C: LDR r0,[pc,#0x1A0] =0x1E501
  - 0x2E333E: LDR r0,[pc,#0x174] =0x1E217
  - 0x2E3356: LDR r0,[pc,#0x160] =0x1E216
  - 0x2E336E: LDR r0,[pc,#0x148] =0x1E216
  - 0x2E339E: LDR r1,[pc,#0xF8] =0xB5C
  - 0x2E33A2: ADD r1, r9
  - 0x2E33A6: LDR r1,[pc,#0xF4] =0xB60
  - 0x2E33AA: ADD r1, r9
  - 0x2E33B6: LDR r0,[pc,#0xD8] =0x11C0
  - 0x2E33B8: ADD r0, r9


## C9D_2F097A writer 0x2F097A
### caller 0x2EFF1C
- function: `0x2EFD90` .. `0x2EFF24`
- upstream BL callers (6): 0x3064E0, 0x306518, 0x30657C, 0x30659C, 0x3065AC, 0x3065EC
- BL to writer/fn: [{'pc': '0x2EFF1C', 'target': '0x2F096C'}]
- plat helpers: [{'pc': '0x2EFDE6', 'helper': '0x304558'}, {'pc': '0x2EFE10', 'helper': '0x304558'}, {'pc': '0x2EFE28', 'helper': '0x304558'}]
- R9 offset lits: [{'pc': '0x2EFD94', 'off': '0x8D0'}, {'pc': '0x2EFDB0', 'off': '0x106C'}, {'pc': '0x2EFE42', 'off': '0xE6C'}, {'pc': '0x2EFE62', 'off': '0x818'}, {'pc': '0x2EFE6C', 'off': '0x81C'}, {'pc': '0x2EFE7A', 'off': '0x1A8'}, {'pc': '0x2EFEC4', 'off': '0x818'}, {'pc': '0x2EFECE', 'off': '0x81C'}]
- key predicates:
  - 0x2EFD94: LDR r0,[pc,#0x18C] =0x8D0
  - 0x2EFD98: ADD r0, r9
  - 0x2EFD9C: CMP r0, #239
  - 0x2EFD9E: BEQ -> 0x2EFE86
  - 0x2EFDA0: CMP r0, #242
  - 0x2EFDA2: BEQ -> 0x2EFE86
  - 0x2EFDA4: LDR r5,[pc,#0x180] =0x24942
  - 0x2EFDA6: ADD r5, r15
  - 0x2EFDA8: LDR r6,[pc,#0x180] =0x25F5E
  - 0x2EFDAA: ADD r6, r15
  - 0x2EFDAC: CMP r0, #209
  - 0x2EFDAE: BNE -> 0x2EFDCE

### caller 0x2F08A4
- function: `0x2F0810` .. `0x2F0956`
- upstream BL callers (1): 0x306A90
- BL to writer/fn: [{'pc': '0x2F08A4', 'target': '0x2F096C'}]
- plat helpers: []
- R9 offset lits: [{'pc': '0x2F0812', 'off': '0x10EC'}, {'pc': '0x2F0888', 'off': '0x1A8'}, {'pc': '0x2F0892', 'off': '0x10EC'}, {'pc': '0x2F08FE', 'off': '0x7C8'}, {'pc': '0x2F0900', 'off': '0x7D0'}]
- key predicates:
  - 0x2F0812: LDR r6,[pc,#0x144] =0x10EC
  - 0x2F0816: ADD r6, r9
  - 0x2F083C: LDR r5,[pc,#0x11C] =0x24DF6
  - 0x2F083E: ADD r5, r15
  - 0x2F0888: LDR r2,[pc,#0xD4] =0x1A8
  - 0x2F088E: ADD r2, r9
  - 0x2F0892: LDR r2,[pc,#0xC4] =0x10EC
  - 0x2F0896: ADD r2, r9
  - 0x2F08B0: CMP r1, #0
  - 0x2F08B2: BEQ -> 0x2F094A
  - 0x2F08BA: BEQ -> 0x2F08E6
  - 0x2F08FE: LDR r4,[pc,#0x64] =0x7C8

### caller 0x2F0D6A
- function: `0x2F0D5C` .. `0x2F0D72`
- upstream BL callers (1): 0x30E336
- BL to writer/fn: [{'pc': '0x2F0D6A', 'target': '0x2F096C'}]
- plat helpers: []
- R9 offset lits: []
- key predicates:

### caller 0x2F1FF0
- function: `0x2F1FA0` .. `0x2F2000`
- upstream BL callers (1): 0x306D28
- BL to writer/fn: [{'pc': '0x2F1FF0', 'target': '0x2F096C'}]
- plat helpers: []
- R9 offset lits: [{'pc': '0x2F1FA2', 'off': '0x10EC'}, {'pc': '0x2F1FDC', 'off': '0x10EC'}]
- key predicates:
  - 0x2F1FA2: LDR r7,[pc,#0x5C] =0x10EC
  - 0x2F1FA6: ADD r7, r9
  - 0x2F1FB6: LDR r0,[pc,#0x4C] =0x23686
  - 0x2F1FBE: ADD r0, r15
  - 0x2F1FDC: LDR r2,[pc,#0x20] =0x10EC
  - 0x2F1FE0: ADD r2, r9


## C9D_2FB008 writer 0x2FB008
### caller 0x30D9EE
- function: `0x30D9EE` .. `0x30E1F0`
- upstream BL callers (0): none
- BL to writer/fn: [{'pc': '0x30D9EE', 'target': '0x2FAFFC'}]
- plat helpers: [{'pc': '0x30E13C', 'helper': '0x304558'}, {'pc': '0x30E158', 'helper': '0x304558'}]
- R9 offset lits: [{'pc': '0x30E0D8', 'off': '0x858'}, {'pc': '0x30E0EA', 'off': '0x858'}]
- key predicates:
  - 0x30DAA4: CMP r1, #1
  - 0x30DAA6: BEQ -> 0x30DAB4
  - 0x30DAA8: CMP r1, #2
  - 0x30DAAA: BEQ -> 0x30DABA
  - 0x30DAAC: CMP r1, #4
  - 0x30DAB0: BNE -> 0x30DAC0
  - 0x30E0D8: LDR r6,[pc,#0x3A4] =0x858
  - 0x30E0DA: ADD r6, r9
  - 0x30E0EA: LDR r1,[pc,#0x394] =0x858
  - 0x30E0EE: ADD r1, r9
  - 0x30E13A: LDR r0,[pc,#0x348] =0x10106
  - 0x30E156: LDR r0,[pc,#0x330] =0x10107


## C9D_30AA42 writer 0x30AA42
### caller 0x30AF8A
- function: `0x30AED2` .. `0x30B42C`
- upstream BL callers (0): none
- BL to writer/fn: [{'pc': '0x30AF8A', 'target': '0x30A9EC'}]
- plat helpers: [{'pc': '0x30AF0A', 'helper': '0x304558'}, {'pc': '0x30B040', 'helper': '0x304558'}, {'pc': '0x30B060', 'helper': '0x304558'}, {'pc': '0x30B0F0', 'helper': '0x304558'}]
- R9 offset lits: [{'pc': '0x30AEE0', 'off': '0xC6C'}, {'pc': '0x30AF12', 'off': '0xC6C'}, {'pc': '0x30AF4A', 'off': '0xC6C'}, {'pc': '0x30AF5A', 'off': '0xC6C'}, {'pc': '0x30AF5C', 'off': '0x1A8'}, {'pc': '0x30AF6E', 'off': '0xE6C'}, {'pc': '0x30AF80', 'off': '0xE6C'}, {'pc': '0x30AF9E', 'off': '0x8A0'}]
- key predicates:
  - 0x30AED6: ADD r0, r9
  - 0x30AEDC: CMP r1, #0
  - 0x30AEDE: BEQ -> 0x30AFC4
  - 0x30AEE0: LDR r2,[pc,#0x3D4] =0xC6C
  - 0x30AEE4: ADD r2, r9
  - 0x30AEF8: CMP r0, #1
  - 0x30AEFA: BNE -> 0x30AF0E
  - 0x30AF08: LDR r0,[pc,#0x3B0] =0x1E204
  - 0x30AF12: LDR r5,[pc,#0x3A4] =0xC6C
  - 0x30AF16: ADD r5, r9
  - 0x30AF20: CMP r0, #0
  - 0x30AF22: BEQ -> 0x30AF2A

### caller 0x30DF78
- function: `0x30DF78` .. `0x30E550`
- upstream BL callers (0): none
- BL to writer/fn: [{'pc': '0x30DF78', 'target': '0x30A9EC'}]
- plat helpers: [{'pc': '0x30E13C', 'helper': '0x304558'}, {'pc': '0x30E158', 'helper': '0x304558'}]
- R9 offset lits: [{'pc': '0x30E0D8', 'off': '0x858'}, {'pc': '0x30E0EA', 'off': '0x858'}, {'pc': '0x30E236', 'off': '0x858'}, {'pc': '0x30E242', 'off': '0x858'}, {'pc': '0x30E4F8', 'off': '0x820'}, {'pc': '0x30E522', 'off': '0xB5C'}, {'pc': '0x30E524', 'off': '0xB60'}]
- key predicates:
  - 0x30E0D8: LDR r6,[pc,#0x3A4] =0x858
  - 0x30E0DA: ADD r6, r9
  - 0x30E0EA: LDR r1,[pc,#0x394] =0x858
  - 0x30E0EE: ADD r1, r9
  - 0x30E13A: LDR r0,[pc,#0x348] =0x10106
  - 0x30E156: LDR r0,[pc,#0x330] =0x10107
  - 0x30E214: CMP r1, #1
  - 0x30E216: BEQ -> 0x30E220
  - 0x30E218: CMP r1, #2
  - 0x30E21A: BNE -> 0x30E224
  - 0x30E236: LDR r0,[pc,#0x248] =0x858
  - 0x30E238: ADD r0, r9


## C44_2FB286 writer 0x2FB286
### caller 0x2DA64A
- function: `0x2DA624` .. `0x2DA650`
- upstream BL callers (0): none
- BL to writer/fn: [{'pc': '0x2DA64A', 'target': '0x2FB248'}]
- plat helpers: [{'pc': '0x2DA63C', 'helper': '0x305604'}]
- R9 offset lits: [{'pc': '0x2DA634', 'off': '0x7D8'}, {'pc': '0x2DA640', 'off': '0x8D8'}]
- key predicates:
  - 0x2DA628: ADD r1, r9
  - 0x2DA62C: ADD r2, r9
  - 0x2DA634: LDR r0,[pc,#0x18] =0x7D8
  - 0x2DA636: ADD r0, r9
  - 0x2DA640: LDR r0,[pc,#0x14] =0x8D8
  - 0x2DA642: ADD r0, r9
  - 0x2DA646: LDR r0,[pc,#0x14] =0x394EC
  - 0x2DA648: ADD r0, r15

### caller 0x2DE7EC
- function: `0x2DE7BC` .. `0x2DE7F8`
- upstream BL callers (1): 0x2E4076
- BL to writer/fn: [{'pc': '0x2DE7EC', 'target': '0x2FB248'}]
- plat helpers: []
- R9 offset lits: [{'pc': '0x2DE7BE', 'off': '0xB5C'}, {'pc': '0x2DE7C4', 'off': '0xB60'}]
- key predicates:
  - 0x2DE7BE: LDR r7,[pc,#0x38] =0xB5C
  - 0x2DE7C4: LDR r5,[pc,#0x34] =0xB60
  - 0x2DE7C6: ADD r7, r9
  - 0x2DE7CC: ADD r5, r9
  - 0x2DE7E6: BEQ -> 0x2DE7F6

### caller 0x30FCEE
- function: `0x30FCAC` .. `0x30FCF4`
- upstream BL callers (1): 0x2DC9CE
- BL to writer/fn: [{'pc': '0x30FCEE', 'target': '0x2FB248'}]
- plat helpers: []
- R9 offset lits: [{'pc': '0x30FCAE', 'off': '0xB64'}, {'pc': '0x30FCB8', 'off': '0xB68'}, {'pc': '0x30FCC4', 'off': '0xB7D'}]
- key predicates:
  - 0x30FCAE: LDR r5,[pc,#0x44] =0xB64
  - 0x30FCB0: ADD r5, r9
  - 0x30FCB4: CMP r0, #0
  - 0x30FCB6: BEQ -> 0x30FCF2
  - 0x30FCB8: LDR r7,[pc,#0x3C] =0xB68
  - 0x30FCBC: ADD r7, r9
  - 0x30FCC0: CMP r1, #0
  - 0x30FCC2: BLE -> 0x30FCCA
  - 0x30FCC4: LDR r2,[pc,#0x34] =0xB7D
  - 0x30FCC6: ADD r2, r9
  - 0x30FCE4: CMP r4, #0
  - 0x30FCE6: BNE -> 0x30FCF2


