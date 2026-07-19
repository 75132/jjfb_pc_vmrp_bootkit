# E8M 0x300158 parent → 0x300714 prerequisites (static)

- fn `0x300158` .. ~`0x3004F6`
- sole `BL 0x300714` at `0x3002C0`

## State load (critical)

- LDR r1, =0x7D8; ADD r1,r9
- MOVS r0,#1; STRB r0,[r1,#2]  — touch queue/base flag
- LDR r0, =0x7D8; ADD r0,r9; ADDS r0,#0x80; LDR r0,[r0,#0x78]
- => r0 = *[R9+0x7D8+0x80+0x78] = *[R9+0x8D0]  (state word)
- MOV r4, <incoming R0> saved before clobber

- switch_subject: **R9+0x8D0 state word (NOT incoming event code)**
- why E8L missed: Parent switch keys off R9+0x8D0; probes left state=0; state0 arm does plat 0x304558 then return — no BL 0x300714

## Path to 0x300714

```
{
  "only_bl_site": 3146432,
  "gate": {
    "pc": 3146426,
    "insns": [
      "CMP r0, #20",
      "BLT 0x3001E2 (epilogue path)",
      "MOV r0,r4",
      "BL 0x300714"
    ],
    "meaning": "state word must be >= 20 (signed) to call dispatcher"
  },
  "reach_gate_via": "default arm 0x300272 \u2192 0x3002BA when state matches no special case",
  "state0_arm": {
    "cmp": "CMP r0,#0; BEQ \u2192 0x30026E \u2192 0x3004C8",
    "effect": "state==0 never reaches 0x3002BA / 0x300714",
    "evidence": "E8L probes all had R9_state=0 \u2192 this arm"
  },
  "why_e8l_missed_714": "Parent switch keys off R9+0x8D0; probes left state=0; state0 arm does plat 0x304558 then return \u2014 no BL 0x300714"
}
```

## Special state arms

- state=0: reaches_714=False {'state': 0, 'arm': '0x3004C8', 'reaches_714': False}
- state=1: reaches_714=False {'state': 1, 'arm': '0x3004E0', 'reaches_714': False}
- state=4: reaches_714=maybe {'state': 4, 'note': 'falls toward default if NE path', 'reaches_714': 'maybe'}
- state=5: reaches_714=False {'state': 5, 'arm': '0x3001A0 early POP', 'reaches_714': False}
- state=20: reaches_714=True {'state': 20, 'note': 'CMP#20 at gate: EQ not LT → may call 714 if default', 'reaches_714': True}
- state=38: reaches_714=True {'state': 38, 'note': 'unlisted → default; 38>=20 → BL 300714 with r4', 'reaches_714': True}

## Event code (r4) vs switch

{
  "r4": "incoming R0 (case156 delivery R1 = 0/18/20)",
  "used_when": "arms that MOV r0,r4 before BL helpers / before BL 0x300714",
  "vs_switch": "does NOT select switch arm; switch uses state word",
  "e8l_diff_hyp": "R0=0/18/20 at parent entry only differ after an arm restores r4; with state=0 all take same state0 arm \u2014 paths converge"
}

## BL targets (8)

`0x2F4BE0`, `0x300714`, `0x301098`, `0x301374`, `0x301650`, `0x301828`, `0x301848`, `0x304558`

## E6C base writers (STR [R9+E6C,#0]): 6

- LDR@`0x2E5F90` STR@`0x2E5FA2`
- LDR@`0x2E5F90` STR@`0x2E5FAE`
- LDR@`0x2FA59C` STR@`0x2FA5B0`
- LDR@`0x30C220` STR@`0x30C22E`
- LDR@`0x30C220` STR@`0x30C23E`
- LDR@`0x30C220` STR@`0x30C24E`

BP: `p:0x300158,p:0x300182,p:0x300194,p:0x30026E,p:0x3004C8,p:0x3002BA,p:0x3002C0,p:0x300714,p:0x3001B8,e:0x2DFC3C,e:0x2DFCAC,e:0x30D300,q:0x2DC80C`

