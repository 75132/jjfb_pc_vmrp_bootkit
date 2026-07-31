# E8P-Fast: 0x3020C8 → C44 dependency static map

## Function head (R9 slot address formation)

```
0x3020C8: PUSH 0xB5F0
0x3020CA: LDR r1, [pc,#0x3D0] ; =0xC6C
0x3020CC: MOVS r4, r0
0x3020CE: ADD r1, r9
0x3020D0: MOVS r0, r1
0x3020D2: ADDS r0, #112
0x3020D4: SUB sp, #0x14
0x3020D6: STR r0, [sp, #0x10]
0x3020D8: LDR r0, [pc,#0x3C4] ; =0xEEC
0x3020DA: MOVS r5, r1
0x3020DC: MOVS r6, r1
0x3020DE: LDR r7, [pc,#0x3B8] ; =0x11D0
0x3020E0: ADDS r6, #32
0x3020E2: ADDS r5, #96
0x3020E4: ADD r0, r9
0x3020E6: LDR r0, [r0, #0x7C]
0x3020E8: CMP r4, #13
0x3020EA: ADD r7, r9
0x3020EC: BEQ 0x3021D2
0x3020EE: BGT 0x302118
0x3020F0: CMP r4, #2
0x3020F2: BEQ 0x302100
0x3020F4: CMP r4, #5
0x3020F6: BEQ 0x302124
0x3020F8: CMP r4, #8
0x3020FA: BEQ 0x3021D2
0x3020FC: CMP r4, #12
0x3020FE: BNE 0x302114
0x302100: CMP r0, #0
0x302102: BEQ 0x302114
0x302104: MOVS r1, #4
0x302106: BL 0x2F5B38
0x30210A: MOVS r3, #0
0x30210C: MOVS r2, r7
0x30210E: MOVS r1, #0
0x302110: BL 0x2DA424
0x302114: ADD sp, #0x14
0x302116: POP 0xBDF0
0x302118: CMP r4, #17
0x30211A: BEQ 0x302124
0x30211C: CMP r4, #18
0x30211E: BEQ 0x302174
0x302120: CMP r4, #20
0x302122: BNE 0x302114
0x302124: LDR r1, [pc,#0x374] ; =0xC6C
0x302126: MOVS r3, #2
0x302128: raw 0x56F0
0x30212A: MOVS r3, #6
0x30212C: ADD r1, r9
0x30212E: raw 0x56C9
0x302130: CMP r0, #1
0x302132: STR r1, [sp, #0xC]
0x302134: BNE 0x30221E
0x302136: CMP r4, #20
0x302138: BEQ 0x30213E
0x30213A: CMP r4, #5
0x30213C: BNE 0x30221E
0x30213E: LDR r0, [pc,#0x364] ; =0xDEC
```

## Symbolic events (slot/obj)

- `0x3020CA` `load_offset_lit` {'rd': 1, 'off': 3180}
- `0x3020CE` `form_r9_addr` {'rd': 1, 'off': 3180}
- `0x3020D8` `load_offset_lit` {'rd': 0, 'off': 3820}
- `0x3020DE` `load_offset_lit` {'rd': 7, 'off': 4560}
- `0x3020E4` `form_r9_addr` {'rd': 0, 'off': 3820}
- `0x3020E6` `load_from_r9_addr` {'rt': 0, 'off': 3820, 'field': 124}
- `0x3020E8` `cmp_imm` {'rn': 4, 'imm': 13, 'sym': None}
- `0x3020EA` `form_r9_addr` {'rd': 7, 'off': 4560}
- `0x3020EC` `bcond` {'cond': 0, 'tgt': 3154386}
- `0x3020EE` `bcond` {'cond': 12, 'tgt': 3154200}
- `0x3020F0` `cmp_imm` {'rn': 4, 'imm': 2, 'sym': None}
- `0x3020F2` `bcond` {'cond': 0, 'tgt': 3154176}
- `0x3020F4` `cmp_imm` {'rn': 4, 'imm': 5, 'sym': None}
- `0x3020F6` `bcond` {'cond': 0, 'tgt': 3154212}
- `0x3020F8` `cmp_imm` {'rn': 4, 'imm': 8, 'sym': None}
- `0x3020FA` `bcond` {'cond': 0, 'tgt': 3154386}
- `0x3020FC` `cmp_imm` {'rn': 4, 'imm': 12, 'sym': None}
- `0x3020FE` `bcond` {'cond': 1, 'tgt': 3154196}
- `0x302100` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('addr', 3820)}
- `0x302102` `bcond` {'cond': 0, 'tgt': 3154196}
- `0x302106` `bl` {'tgt': 3103544, 'r0': ('addr', 3820), 'r1': ('addr', 3180), 'r4': None}
- `0x302110` `bl` {'tgt': 2991140, 'r0': ('addr', 3820), 'r1': ('addr', 3180), 'r4': None}
- `0x302118` `cmp_imm` {'rn': 4, 'imm': 17, 'sym': None}
- `0x30211A` `bcond` {'cond': 0, 'tgt': 3154212}
- `0x30211C` `cmp_imm` {'rn': 4, 'imm': 18, 'sym': None}
- `0x30211E` `bcond` {'cond': 0, 'tgt': 3154292}
- `0x302120` `cmp_imm` {'rn': 4, 'imm': 20, 'sym': None}
- `0x302122` `bcond` {'cond': 1, 'tgt': 3154196}
- `0x302124` `load_offset_lit` {'rd': 1, 'off': 3180}
- `0x30212C` `form_r9_addr` {'rd': 1, 'off': 3180}
- `0x302130` `cmp_imm` {'rn': 0, 'imm': 1, 'sym': ('addr', 3820)}
- `0x302134` `bcond` {'cond': 1, 'tgt': 3154462}
- `0x302136` `cmp_imm` {'rn': 4, 'imm': 20, 'sym': None}
- `0x302138` `bcond` {'cond': 0, 'tgt': 3154238}
- `0x30213A` `cmp_imm` {'rn': 4, 'imm': 5, 'sym': None}
- `0x30213C` `bcond` {'cond': 1, 'tgt': 3154462}
- `0x302144` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 3564)}
- `0x302146` `bcond` {'cond': 0, 'tgt': 3154462}
- `0x302152` `bl` {'tgt': 3151944, 'r0': ('imm_off', 3564), 'r1': ('imm_off', 424), 'r4': None}
- `0x302156` `load_offset_lit` {'rd': 0, 'off': 4560}
- `0x30215C` `form_r9_addr` {'rd': 0, 'off': 4560}
- `0x302166` `load_from_r9_addr` {'rt': 2, 'off': 4560, 'field': 104}
- `0x302168` `load_from_r9_addr` {'rt': 3, 'off': 4560, 'field': 108}
- `0x30216E` `bl` {'tgt': 3163480, 'r0': ('imm_off', 123411), 'r1': ('imm_off', 424), 'r4': None}
- `0x302172` `b` {'tgt': 3154196}
- `0x302174` `load_offset_lit` {'rd': 0, 'off': 3820}
- `0x30217A` `form_r9_addr` {'rd': 0, 'off': 3820}
- `0x302180` `store_r9` {'off': 3820, 'field': 84, 'rt': 4}
- `0x302184` `load_offset_lit` {'rd': 7, 'off': 3180}
- `0x30218C` `form_r9_addr` {'rd': 7, 'off': 3180}
- `0x302190` `cmp_imm` {'rn': 0, 'imm': 2, 'sym': ('addr', 3820)}
- `0x302192` `bcond` {'cond': 1, 'tgt': 3154326}
- `0x30219A` `cmp_imm` {'rn': 0, 'imm': 1, 'sym': ('addr', 3820)}
- `0x30219C` `bcond` {'cond': 1, 'tgt': 3154356}
- `0x3021B0` `bl` {'tgt': 3163480, 'r0': ('imm_off', 123415), 'r1': ('imm_off', 424), 'r4': None}
- `0x3021CA` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 3564)}
- `0x3021CC` `bcond` {'cond': 0, 'tgt': 3154408}
- `0x3021D0` `b` {'tgt': 3154388}
- `0x3021D2` `b` {'tgt': 3156184}
- `0x3021E4` `bl` {'tgt': 3163480, 'r0': ('imm_off', 123396), 'r1': ('imm_off', 424), 'r4': None}
- `0x302200` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 424)}
- `0x302202` `bcond` {'cond': 0, 'tgt': 3154466}
- `0x302212` `bl` {'tgt': 3209404, 'r0': ('imm_off', 2868), 'r1': ('imm_off', 2256), 'r4': None}
- `0x302218` `bl` {'tgt': 3169800, 'r0': ('imm_off', 2868), 'r1': ('imm_off', 2256), 'r4': None}
- `0x30221C` `b` {'tgt': 3154464}
- `0x30221E` `b` {'tgt': 3154774}
- `0x302228` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 2868)}
- `0x30222A` `bcond` {'cond': 0, 'tgt': 3154502}
- `0x30223A` `bl` {'tgt': 3209404, 'r0': ('imm_off', 2852), 'r1': ('imm_off', 2256), 'r4': None}
- `0x302240` `bl` {'tgt': 3169800, 'r0': ('imm_off', 2852), 'r1': ('imm_off', 2256), 'r4': None}
- `0x30224C` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 2852)}
- `0x30224E` `bcond` {'cond': 0, 'tgt': 3154538}
- `0x30225E` `bl` {'tgt': 3209404, 'r0': ('imm_off', 2860), 'r1': ('imm_off', 2256), 'r4': None}
- `0x302264` `bl` {'tgt': 3169800, 'r0': ('imm_off', 2860), 'r1': ('imm_off', 2256), 'r4': None}
- `0x302270` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 2860)}
- `0x302272` `bcond` {'cond': 0, 'tgt': 3154574}
- `0x302282` `bl` {'tgt': 3209404, 'r0': ('imm_off', 2844), 'r1': ('imm_off', 2256), 'r4': None}
- `0x302288` `bl` {'tgt': 3169800, 'r0': ('imm_off', 2844), 'r1': ('imm_off', 2256), 'r4': None}
- `0x302294` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 2844)}
- `0x302296` `bcond` {'cond': 0, 'tgt': 3154610}
- `0x3022A6` `bl` {'tgt': 3209404, 'r0': ('imm_off', 2828), 'r1': ('imm_off', 2256), 'r4': None}
- `0x3022AC` `bl` {'tgt': 3169800, 'r0': ('imm_off', 2828), 'r1': ('imm_off', 2256), 'r4': None}
- `0x3022B8` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 2828)}
- `0x3022BA` `bcond` {'cond': 0, 'tgt': 3154646}
- `0x3022CA` `bl` {'tgt': 3209404, 'r0': ('imm_off', 2836), 'r1': ('imm_off', 2256), 'r4': None}
- `0x3022D0` `bl` {'tgt': 3169800, 'r0': ('imm_off', 2836), 'r1': ('imm_off', 2256), 'r4': None}
- `0x3022DC` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 2836)}
- `0x3022DE` `bcond` {'cond': 0, 'tgt': 3154682}
- `0x3022EE` `bl` {'tgt': 3209404, 'r0': ('imm_off', 2820), 'r1': ('imm_off', 2256), 'r4': None}
- `0x3022F4` `bl` {'tgt': 3169800, 'r0': ('imm_off', 2820), 'r1': ('imm_off', 2256), 'r4': None}
- `0x3022FE` `cmp_imm` {'rn': 0, 'imm': 1, 'sym': ('imm_off', 2820)}
- `0x302300` `bcond` {'cond': 1, 'tgt': 3154742}
- `0x302308` `bl` {'tgt': 3134408, 'r0': ('imm_off', 79138), 'r1': ('imm_off', 2256), 'r4': None}
- `0x30230C` `load_offset_lit` {'rd': 1, 'off': 3180}
- `0x302310` `form_r9_addr` {'rd': 1, 'off': 3180}
- `0x302316` `bl` {'tgt': 3204444, 'r0': ('imm_off', 79138), 'r1': ('addr', 3180), 'r4': None}
- `0x302320` `bl` {'tgt': 3205044, 'r0': ('imm_off', 79138), 'r1': ('addr', 3180), 'r4': None}
- `0x30232C` `cmp_imm` {'rn': 0, 'imm': 0, 'sym': ('imm_off', 79138)}
- `0x30232E` `bcond` {'cond': 0, 'tgt': 3154742}
- `0x302330` `bl` {'tgt': 3169800, 'r0': ('imm_off', 79138), 'r1': ('addr', 3180), 'r4': None}
- `0x30233C` `bl` {'tgt': 3161192, 'r0': ('imm_off', 79138), 'r1': ('addr', 3180), 'r4': None}
- `0x302340` `bl` {'tgt': 3100260, 'r0': ('imm_off', 79138), 'r1': ('addr', 3180), 'r4': None}
- `0x302348` `cmp_imm` {'rn': 0, 'imm': 1, 'sym': ('imm_off', 79138)}
- `0x30234A` `bcond` {'cond': 0, 'tgt': 3154766}
- `0x30234C` `b` {'tgt': 3154196}
- `0x302350` `bl` {'tgt': 3146072, 'r0': ('imm_off', 79138), 'r1': ('addr', 3180), 'r4': None}
- `0x302354` `b` {'tgt': 3154196}
- `0x302362` `bl` {'tgt': 3100260, 'r0': ('imm_off', 424), 'r1': ('imm_off', 2256), 'r4': None}
- `0x30236C` `load_offset_lit` {'rd': 0, 'off': 3180}
- `0x30236E` `cmp_imm` {'rn': 1, 'imm': 1, 'sym': ('imm_off', 2256)}
- `0x302370` `form_r9_addr` {'rd': 0, 'off': 3180}
- `0x302372` `bcond` {'cond': 0, 'tgt': 3154764}
- `0x30237E` `cmp_imm` {'rn': 1, 'imm': 2, 'sym': ('imm_off', 3564)}
- `0x302380` `bcond` {'cond': 1, 'tgt': 3154850}

## Null checks on C6C/EEC/11D0 pointer loads

- (none matched heuristic LDR-slot → CMP #0 on same rt)

## Case r4=18 decisions (CMP/Bcond)

- CMP `r4,#13` @ `0x3020E8` → Bcond@0x3020EC tgt `0x3021D2` taken=False
- CMP `r4,#2` @ `0x3020F0` → Bcond@0x3020F2 tgt `0x302100` taken=False
- CMP `r4,#5` @ `0x3020F4` → Bcond@0x3020F6 tgt `0x302124` taken=False
- CMP `r4,#8` @ `0x3020F8` → Bcond@0x3020FA tgt `0x3021D2` taken=False
- CMP `r4,#12` @ `0x3020FC` → Bcond@0x3020FE tgt `0x302114` taken=True
- CMP `r4,#17` @ `0x302118` → Bcond@0x30211A tgt `0x302124` taken=False
- CMP `r4,#18` @ `0x30211C` → Bcond@0x30211E tgt `0x302174` taken=True
- CMP `r4,#20` @ `0x302120` → Bcond@0x302122 tgt `0x302114` taken=True
- CMP `r4,#20` @ `0x302136` → Bcond@0x302138 tgt `0x30213E` taken=False
- CMP `r4,#5` @ `0x30213A` → Bcond@0x30213C tgt `0x30221E` taken=True

## Case r4=20 decisions

- CMP `r4,#13` @ `0x3020E8` → Bcond@0x3020EC tgt `0x3021D2` taken=False
- CMP `r4,#2` @ `0x3020F0` → Bcond@0x3020F2 tgt `0x302100` taken=False
- CMP `r4,#5` @ `0x3020F4` → Bcond@0x3020F6 tgt `0x302124` taken=False
- CMP `r4,#8` @ `0x3020F8` → Bcond@0x3020FA tgt `0x3021D2` taken=False
- CMP `r4,#12` @ `0x3020FC` → Bcond@0x3020FE tgt `0x302114` taken=True
- CMP `r4,#17` @ `0x302118` → Bcond@0x30211A tgt `0x302124` taken=False
- CMP `r4,#18` @ `0x30211C` → Bcond@0x30211E tgt `0x302174` taken=False
- CMP `r4,#20` @ `0x302120` → Bcond@0x302122 tgt `0x302114` taken=False
- CMP `r4,#20` @ `0x302136` → Bcond@0x302138 tgt `0x30213E` taken=True
- CMP `r4,#5` @ `0x30213A` → Bcond@0x30213C tgt `0x30221E` taken=True

## Region near R1=18 clear (0x302170..)

```
0x302170: raw 0xF9F3
0x302172: B 0x302114
0x302174: LDR r0, [pc,#0x328] ; =0xEEC
0x302176: MOVS r4, #0
0x302178: STRB r4, [r6, #0xE]
0x30217A: ADD r0, r9
0x30217C: ADDS r0, #128
0x30217E: STRB r4, [r6, #0xA]
0x302180: STR r4, [r0, #0x54]
0x302182: LDR r1, [sp, #0x10]
0x302184: LDR r7, [pc,#0x314] ; =0xC6C
0x302186: STRB r4, [r1, #0x0]
0x302188: STRB r4, [r5, #0x6]
0x30218A: MOVS r3, #16
0x30218C: ADD r7, r9
0x30218E: raw 0x56F8
0x302190: CMP r0, #2
0x302192: BNE 0x302196
0x302194: STRB r4, [r7, #0x10]
0x302196: MOVS r3, #0
0x302198: raw 0x56E8
0x30219A: CMP r0, #1
0x30219C: BNE 0x3021B4
0x30219E: MOVS r2, #0
0x3021A0: MOVS r1, #0
0x3021A2: STR r2, [sp, #0x8]
0x3021A4: raw 0x4243
0x3021A6: MOVS r2, #3
0x3021A8: STR r1, [sp, #0x0]
0x3021AA: STR r1, [sp, #0x4]
0x3021AC: MOVS r1, #4
0x3021AE: LDR r0, [pc,#0x300] ; =0x1E217
0x3021B0: BL 0x304558
0x3021B4: STRB r4, [r5, #0xF]
0x3021B6: STRB r4, [r6, #0x4]
0x3021B8: LDR r0, [pc,#0x2E8] ; =0xDEC
0x3021BA: STRB r4, [r6, #0x5]
0x3021BC: STRB r4, [r5, #0x8]
0x3021BE: ADD r0, r9
0x3021C0: STR r4, [r0, #0x30]
0x3021C2: STRB r4, [r7, #0x16]
0x3021C4: STRB r4, [r7, #0x17]
0x3021C6: MOVS r3, #87
0x3021C8: raw 0x56F8
0x3021CA: CMP r0, #0
0x3021CC: BEQ 0x3021E8
0x3021CE: MOVS r2, #0
0x3021D0: B 0x3021D4
0x3021D2: B 0x3028D8
0x3021D4: MOVS r1, #0
0x3021D6: STR r2, [sp, #0x8]
0x3021D8: MOVS r2, #9
0x3021DA: STR r1, [sp, #0x0]
0x3021DC: STR r1, [sp, #0x4]
0x3021DE: MOVS r3, r4
0x3021E0: MOVS r1, #29
0x3021E2: LDR r0, [pc,#0x2D0] ; =0x1E204
0x3021E4: BL 0x304558
0x3021E8: STRB r4, [r5, #0xA]
0x3021EA: MOVS r0, #0
0x3021EC: STRB r0, [r5, #0xB]
0x3021EE: LDR r0, [pc,#0x2B8] ; =0x1A8
0x3021F0: LDR r1, [pc,#0x2C4] ; =0x8D0
0x3021F2: ADD r0, r9
0x3021F4: LDR r0, [r0, #0x54]
0x3021F6: LDR r6, [pc,#0x2C4] ; =0xAB4
0x3021F8: ADD r1, r9
0x3021FA: STR r0, [r1, #0x0]
0x3021FC: ADD r6, r9
0x3021FE: LDR r0, [r6, #0x0]
0x302200: CMP r0, #0
0x302202: BEQ 0x302222
0x302204: LDR r0, [pc,#0x2B8] ; =0xB38
0x302206: MOVS r2, #6
0x302208: ADD r0, r9
0x30220A: LDR r1, [r0, #0x0]
0x30220C: LDR r0, [pc,#0x2B4] ; =0xB34
0x30220E: ADD r0, r9
0x302210: LDR r0, [r0, #0x0]
0x302212: BL 0x30F8BC
0x302216: LDR r0, [r6, #0x0]
0x302218: BL 0x305E08
0x30221C: B 0x302220
0x30221E: B 0x302356
```

## Region near R1=20 / C44 arm (0x302300..)

```
0x302300: BNE 0x302336
0x302302: LDR r0, [pc,#0x20C] ; =0x13522
0x302304: STRB r4, [r5, #0xC]
0x302306: ADD r0, r15
0x302308: BL 0x2FD3C8
0x30230C: LDR r1, [pc,#0x18C] ; =0xC6C
0x30230E: MOVS r0, #1
0x302310: ADD r1, r9
0x302312: ADDS r1, #128
0x302314: STRB r0, [r1, #0x1]
0x302316: BL 0x30E55C
0x30231A: MOVS r1, #0
0x30231C: raw 0x43C9
0x30231E: MOVS r0, #0
0x302320: BL 0x30E7B4
0x302324: LDR r6, [pc,#0x17C] ; =0xDEC
0x302326: ADD r6, r9
0x302328: SUBS r6, #128
0x30232A: LDR r0, [r6, #0x48]
0x30232C: CMP r0, #0
0x30232E: BEQ 0x302336
0x302330: BL 0x305E08
0x302334: STR r4, [r6, #0x48]
0x302336: MOVS r2, #0
0x302338: MOVS r1, #0
0x30233A: MOVS r0, #0
0x30233C: BL 0x303C68
0x302340: BL 0x2F4E64
0x302344: MOVS r3, #9
0x302346: raw 0x56E8
0x302348: CMP r0, #1
0x30234A: BEQ 0x30234E
0x30234C: B 0x302114
0x30234E: MOVS r0, #18
0x302350: BL 0x300158
0x302354: B 0x302114
0x302356: LDR r0, [pc,#0x150] ; =0x1A8
0x302358: LDR r1, [pc,#0x15C] ; =0x8D0
0x30235A: ADD r0, r9
0x30235C: LDR r0, [r0, #0x54]
0x30235E: ADD r1, r9
0x302360: STR r0, [r1, #0x0]
0x302362: BL 0x2F4E64
0x302366: MOVS r7, #0
0x302368: STRB r7, [r6, #0xE]
0x30236A: LDR r1, [sp, #0xC]
0x30236C: LDR r0, [pc,#0x12C] ; =0xC6C
0x30236E: CMP r1, #1
0x302370: ADD r0, r9
0x302372: BEQ 0x30234C
0x302374: LDR r1, [pc,#0x12C] ; =0xDEC
0x302376: MOVS r3, #16
0x302378: ADD r1, r9
0x30237A: STR r7, [r1, #0x30]
0x30237C: raw 0x56C1
0x30237E: CMP r1, #2
```

## 0x2F4E64 head

```
0x2F4E64: PUSH 0xB538
0x2F4E66: LDR r0, [pc,#0x44] ; =0xC6C
0x2F4E68: MOVS r4, #0
0x2F4E6A: MOVS r1, #34
0x2F4E6C: ADD r0, r9
0x2F4E6E: raw 0x540C
0x2F4E70: MOVS r1, #67
0x2F4E72: raw 0x540C
0x2F4E74: STRB r4, [r0, #0x6]
0x2F4E76: MOVS r3, #87
0x2F4E78: raw 0x56C0
0x2F4E7A: CMP r0, #0
0x2F4E7C: BNE 0x2F4E84
0x2F4E7E: LDR r0, [pc,#0x30] ; =0xC44
0x2F4E80: ADD r0, r9
0x2F4E82: STRB r4, [r0, #0x0]
0x2F4E84: LDR r5, [pc,#0x2C] ; =0xEEC
0x2F4E86: ADD r5, r9
0x2F4E88: LDR r0, [r5, #0x7C]
0x2F4E8A: CMP r0, #0
0x2F4E8C: BEQ 0x2F4E92
0x2F4E8E: BL 0x30A424
0x2F4E92: STR r4, [r5, #0x7C]
0x2F4E94: LDR r0, [r5, #0x78]
0x2F4E96: CMP r0, #0
0x2F4E98: BEQ 0x2F4E9E
0x2F4E9A: BL 0x305E08
0x2F4E9E: LDR r1, [pc,#0x18] ; =0x228
0x2F4EA0: MOVS r0, #0
0x2F4EA2: raw 0x43C0
0x2F4EA4: STR r4, [r5, #0x78]
0x2F4EA6: ADD r1, r9
0x2F4EA8: STR r0, [r1, #0x14]
0x2F4EAA: POP 0xBD38
0x2F4EAC: raw 0x0C6C
0x2F4EAE: raw 0x0000
0x2F4EB0: raw 0x0C44
0x2F4EB2: raw 0x0000
0x2F4EB4: raw 0x0EEC
0x2F4EB6: raw 0x0000
0x2F4EB8: raw 0x0228
0x2F4EBA: raw 0x0000
0x2F4EBC: PUSH 0xB510
0x2F4EBE: SUB sp, #0x10
0x2F4EC0: MOVS r1, #0
0x2F4EC2: MOVS r2, #0
0x2F4EC4: LDR r0, [pc,#0xC8] ; =0xBEC
0x2F4EC6: STR r2, [sp, #0x8]
0x2F4EC8: STR r1, [sp, #0x0]
0x2F4ECA: STR r1, [sp, #0x4]
0x2F4ECC: ADD r0, r9
0x2F4ECE: LDR r1, [r0, #0x48]
0x2F4ED0: MOVS r4, #0
0x2F4ED2: MOVS r3, r4
0x2F4ED4: LDR r0, [pc,#0xBC] ; =0x10105
0x2F4ED6: BL 0x304558
0x2F4EDA: MOVS r1, #0
0x2F4EDC: MOVS r2, #0
0x2F4EDE: STR r2, [sp, #0x8]
0x2F4EE0: MOVS r3, r4
0x2F4EE2: STR r1, [sp, #0x0]
```

## Offset uses / stores

### 0xC6C uses (163)
- LDR@0x2D9CD6 r5 use@0x2D9CDE: `CMP r0, #1`
- LDR@0x2DA948 r5 use@0x2DA950: `CMP r5, #0`
- LDR@0x2DA9E6 r0 use@0x2DA9F0: `CMP r0, #0`
- LDR@0x2DAB18 r0 use@0x2DAB28: `CMP r1, #0`
- LDR@0x2DAB36 r6 use@0x2DAB46: `CMP r0, #0`
- LDR@0x2DBA6E r7 use@0x2DBA76: `CMP r0, #1`
- LDR@0x2DBB00 r7 use@0x2DBB0A: `CMP r0, #1`
- LDR@0x2DBB7E r7 use@0x2DBB88: `CMP r0, #1`
- LDR@0x2DBC46 r7 use@0x2DBC50: `CMP r0, #1`
- LDR@0x2DC77A r7 use@0x2DC788: `LDR r0, [r5, #0x58]`
- LDR@0x2DC790 r0 use@0x2DC7A4: `STR r0, [r1, #0x48]`
- LDR@0x2DC7BE r0 use@0x2DC7C4: `STR r5, [r0, #0x4C]`
- LDR@0x2DC7E4 r1 use@0x2DC7EC: `STR r0, [r1, #0x4C]`
- LDR@0x2DCA20 r6 use@0x2DCA28: `CMP r0, #1`
- LDR@0x2DE836 r7 use@0x2DE83E: `CMP r0, #1`
- LDR@0x2DE8C6 r0 use@0x2DE8D6: `CMP r0, #1`
- LDR@0x2DEA4A r7 use@0x2DEA52: `CMP r0, #1`
- LDR@0x2DEADE r7 use@0x2DEAE6: `CMP r0, #1`
- LDR@0x2DED08 r6 use@0x2DED12: `CMP r0, #1`
- LDR@0x2DFB68 r7 use@0x2DFB70: `CMP r0, #1`
### 0xC6C stores (54)
- `0x2DC782` `STRB r0, [r7, #0x1]` fn=`0x2DC778` callers=['0x2DD3D0']
- `0x2DC7C4` `STR r5, [r0, #0x4C]` fn=`0x2DC778` callers=['0x2DD3D0']
- `0x2DC7EC` `STR r0, [r1, #0x4C]` fn=`0x2DC778` callers=['0x2DD3D0']
- `0x2DFC8C` `STRB r6, [r5, #0x11]` fn=`0x2DFC3C` callers=['0x2DC650', '0x2F52DA', '0x300606', '0x30061C', '0x3006EE', '0x300704']
- `0x2DFD7A` `STRB r6, [r0, #0x11]` fn=`0x2DFC3C` callers=['0x2DC650', '0x2F52DA', '0x300606', '0x30061C', '0x3006EE', '0x300704']
- `0x2E012E` `STRB r6, [r0, #0x11]` fn=`0x2DFC3C` callers=['0x2DC650', '0x2F52DA', '0x300606', '0x30061C', '0x3006EE', '0x300704']
- `0x2E0152` `STRB r6, [r0, #0x11]` fn=`0x2DFC3C` callers=['0x2DC650', '0x2F52DA', '0x300606', '0x30061C', '0x3006EE', '0x300704']
- `0x2E12EE` `STRB r0, [r1, #0x8]` fn=`0x2E0E00` callers=['0x3013B8', '0x3013CE']
- `0x2E6F1C` `STRB r1, [r0, #0x4]` fn=`0x2E6ED8` callers=['0x2DD350', '0x2E2B54', '0x2E473A', '0x2E4FA2', '0x2E4FD8', '0x2E6DE6']
- `0x2EA2A6` `STR r0, [r7, #0x70]` fn=`0x2EA254` callers=['0x2F0124']
- `0x2ECE30` `STRB r0, [r1, #0x4]` fn=`0x2EC9C0` callers=['0x2ECE64', '0x2ED044', '0x2EF0C2', '0x2F0946', '0x2F0F94', '0x2F1954']
- `0x2F4E74` `STRB r4, [r0, #0x6]` fn=`0x2F4E64` callers=['0x302340', '0x302362']
- `0x2FA812` `STRB r0, [r7, #0xC]` fn=`0x2FA808` callers=['0x2F57E0', '0x2F5844', '0x3011B6', '0x3011C8']
- `0x2FA826` `STRH r0, [r1, #0x2C]` fn=`0x2FA808` callers=['0x2F57E0', '0x2F5844', '0x3011B6', '0x3011C8']
- `0x2FA996` `STR r1, [r0, #0x18]` fn=`0x2FA808` callers=['0x2F57E0', '0x2F5844', '0x3011B6', '0x3011C8']
- `0x2FAA54` `STRB r6, [r7, #0xC]` fn=`0x2FAA44` callers=['0x2F584E', '0x2F585A', '0x3011E8', '0x3011FA']
- `0x2FAA62` `STRH r0, [r1, #0x2C]` fn=`0x2FAA44` callers=['0x2F584E', '0x2F585A', '0x3011E8', '0x3011FA']
- `0x2FAC3E` `STR r1, [r0, #0x18]` fn=`0x2FAA44` callers=['0x2F584E', '0x2F585A', '0x3011E8', '0x3011FA']
- `0x2FAC94` `STRB r0, [r6, #0xC]` fn=`0x2FAC84` callers=['0x2F57AE', '0x2F57BA', '0x30124C', '0x30125E']
- `0x2FB274` `STRB r6, [r0, #0x0]` fn=`0x2FB248` callers=['0x2DA64A', '0x2DE7EC', '0x30FCEE']
- `0x2FCAF6` `STRB r7, [r0, #0x5]` fn=`0x2FCA80` callers=['0x2E5290', '0x2E52C8']
- `0x2FE1B0` `STRB r3, [r0, #0x16]` fn=`0x2FE17E` callers=[]
- `0x2FE238` `STRH r0, [r1, #0x3A]` fn=`0x2FE17E` callers=[]
- `0x2FE28A` `STRB r3, [r1, #0x9]` fn=`0x2FE17E` callers=[]
- `0x2FE2AC` `STRB r3, [r0, #0xD]` fn=`0x2FE17E` callers=[]
### 0xEEC uses (145)
- LDR@0x2DA486 r2 use@0x2DA496: `CMP r0, #17`
- LDR@0x2DB002 r1 use@0x2DB008: `STR r0, [r1, #0x14]`
- LDR@0x2DD3C2 r0 use@0x2DD3C6: `LDR r0, [r0, #0x68]`
- LDR@0x2E0BCA r1 use@0x2E0BD0: `STR r0, [r1, #0x2C]`
- LDR@0x2E8B06 r0 use@0x2E8B0A: `LDR r1, [r0, #0xC]`
- LDR@0x2E8B50 r1 use@0x2E8B56: `LDR r1, [r1, #0x8]`
- LDR@0x2E8B6C r1 use@0x2E8B70: `LDR r0, [r1, #0xC]`
- LDR@0x2E8B88 r1 use@0x2E8B8C: `LDR r0, [r1, #0xC]`
- LDR@0x2E8DF6 r0 use@0x2E8DFA: `LDR r0, [r0, #0x6C]`
- LDR@0x2E8E1C r0 use@0x2E8E20: `LDR r0, [r0, #0x6C]`
- LDR@0x2E8E26 r7 use@0x2E8E2C: `LDR r0, [r7, #0xC]`
- LDR@0x2E8E4C r7 use@0x2E8E54: `STR r4, [r7, #0x10]`
- LDR@0x2E8E88 r0 use@0x2E8E94: `LDR r0, [r0, #0x6C]`
- LDR@0x2E8E96 r7 use@0x2E8EA6: `LDR r1, [r7, #0x28]`
- LDR@0x2E8F0E r1 use@0x2E8F16: `LDR r1, [r1, #0x28]`
- LDR@0x2E8F18 r0 use@0x2E8F28: `LDR r0, [r0, #0x6C]`
- LDR@0x2E8F20 r1 use@0x2E8F28: `LDR r0, [r0, #0x6C]`
- LDR@0x2E8F78 r1 use@0x2E8F80: `LDR r1, [r1, #0x24]`
- LDR@0x2E8F8E r2 use@0x2E8F96: `LDR r2, [r2, #0x28]`
- LDR@0x2E8F98 r0 use@0x2E8FA0: `LDR r0, [r0, #0x6C]`
### 0xEEC stores (38)
- `0x2DB008` `STR r0, [r1, #0x14]` fn=`0x2DAFF0` callers=['0x300CAA', '0x311A50']
- `0x2E0BD0` `STR r0, [r1, #0x2C]` fn=`0x2E0BD0` callers=[]
- `0x2E8B86` `STR r7, [r1, #0xC]` fn=`0x2E8A74` callers=['0x2F0DF0']
- `0x2E8E54` `STR r4, [r7, #0x10]` fn=`0x2E8DCC` callers=['0x2EB2F0', '0x2F160E', '0x3070B0', '0x30DDDA']
- `0x2F0F64` `STR r0, [r2, #0x7C]` fn=`0x2F0D74` callers=['0x306F2C']
- `0x2F4E92` `STR r4, [r5, #0x7C]` fn=`0x2F4E64` callers=['0x302340', '0x302362']
- `0x2FAF94` `STR r7, [r5, #0x78]` fn=`0x2FAF04` callers=['0x2DA836', '0x2DA842', '0x2DB01C', '0x2DB8C2', '0x2DBAB0', '0x2DBB44']
- `0x2FB4EA` `STR r0, [r7, #0x28]` fn=`0x2FB360` callers=['0x301160', '0x30B4F8', '0x30E054']
- `0x2FB52C` `STR r0, [r1, #0x18]` fn=`0x2FB360` callers=['0x301160', '0x30B4F8', '0x30E054']
- `0x2FB5D8` `STR r0, [r6, #0x24]` fn=`0x2FB5BC` callers=['0x2DA540', '0x2DDE98', '0x2DFCC0', '0x2E0378', '0x2E0432', '0x2E04D8']
- `0x2FC002` `STR r0, [r7, #0x7C]` fn=`0x2FBFB4` callers=['0x2DB928', '0x2DB9E2', '0x2DDBD4', '0x2DDC44', '0x2DE1C0', '0x2E0090']
- `0x2FC804` `STR r1, [r0, #0x7C]` fn=`0x2FC7D8` callers=['0x2DE5A2', '0x2E78E6']
- `0x2FCA0E` `STR r7, [r0, #0x1C]` fn=`0x2FC984` callers=['0x30D742']
- `0x2FE39E` `STR r0, [r4, #0x48]` fn=`0x2FE384` callers=['0x2FBDD0']
- `0x3005CE` `STR r0, [r1, #0x18]` fn=`0x300560` callers=['0x301068', '0x302C16']
- `0x3005E4` `STR r0, [r1, #0x18]` fn=`0x300560` callers=['0x301068', '0x302C16']
- `0x30060C` `STR r0, [r4, #0x54]` fn=`0x300560` callers=['0x301068', '0x302C16']
- `0x300694` `STR r0, [r1, #0x2C]` fn=`0x300628` callers=['0x301076', '0x302C1E']
- `0x3006A8` `STR r0, [r1, #0x2C]` fn=`0x300628` callers=['0x301076', '0x302C1E']
- `0x3006C6` `STR r0, [r4, #0x24]` fn=`0x300628` callers=['0x301076', '0x302C1E']
- `0x3006F4` `STR r0, [r4, #0x54]` fn=`0x300628` callers=['0x301076', '0x302C1E']
- `0x302180` `STR r4, [r0, #0x54]` fn=`0x3020C8` callers=['0x30103C']
- `0x3075D8` `STR r0, [r4, #0x6C]` fn=`0x3075A4` callers=['0x2E8E18', '0x2FE690', '0x30D630']
- `0x3075E2` `STR r5, [r7, #0x58]` fn=`0x3075A4` callers=['0x2E8E18', '0x2FE690', '0x30D630']
- `0x307666` `STR r0, [r1, #0x6C]` fn=`0x3075A4` callers=['0x2E8E18', '0x2FE690', '0x30D630']
### 0x11D0 uses (3)
- LDR@0x3020DE r7 use@0x3020F0: `CMP r4, #2`
- LDR@0x302156 r0 use@0x302166: `LDR r2, [r0, #0x68]`
- LDR@0x3023DA r0 use@0x3023E0: `LDR r1, [r0, #0x74]`
### 0x11D0 stores (0)

## BL sites inside 0x3020C8..0x302400

- `0x302106` → `0x2F5B38`
- `0x302110` → `0x2DA424`
- `0x302152` → `0x301848`
- `0x30216E` → `0x304558`
- `0x3021B0` → `0x304558`
- `0x3021E4` → `0x304558`
- `0x302212` → `0x30F8BC`
- `0x302218` → `0x305E08`
- `0x30223A` → `0x30F8BC`
- `0x302240` → `0x305E08`
- `0x30225E` → `0x30F8BC`
- `0x302264` → `0x305E08`
- `0x302282` → `0x30F8BC`
- `0x302288` → `0x305E08`
- `0x3022A6` → `0x30F8BC`
- `0x3022AC` → `0x305E08`
- `0x3022CA` → `0x30F8BC`
- `0x3022D0` → `0x305E08`
- `0x3022EE` → `0x30F8BC`
- `0x3022F4` → `0x305E08`
- `0x302308` → `0x2FD3C8`
- `0x302316` → `0x30E55C`
- `0x302320` → `0x30E7B4`
- `0x302330` → `0x305E08`
- `0x30233C` → `0x303C68`
- `0x302340` → `0x2F4E64`
- `0x302350` → `0x300158`
- `0x302362` → `0x2F4E64`
- `0x30238E` → `0x30E55C`
- `0x30239C` → `0x30E7F8`
- `0x3023B2` → `0x301848`
- `0x3023B6` → `0x30E55C`
- `0x3023C0` → `0x30E7F8`
- `0x3023D0` → `0x301848`
- `0x3023D6` → `0x30E55C`
- `0x3023E8` → `0x30F908`
