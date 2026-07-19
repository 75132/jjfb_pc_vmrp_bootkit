# E8N R9+0x8D0 state writer xref + ladder (static)

- STATE_OFF = `0x8D0` (= 0x800+0xD0)
- direct writers: **75**
- imm nonzero / >=20 / ==38: 58 / 47 / 0
- via 7D8 alias writers: 0
- E6C writers: 6
- shared state∩E6C fns: (none)

## State0 arm `0x3004C8`

{
  "entry": 3146952,
  "bls": [
    {
      "pc": 3146970,
      "target": 3163480
    }
  ],
  "summary": "MOVS args; BL 0x304558 (plat helper); B epilogue \u2014 no STR to state, no BL 0x300714, no E6C write",
  "side_effect": "platform/helper call only (0x304558)",
  "advances_state": false
}

## Ladder hypothesis

- {'from': 0, 'to': 'special arms (1,5,...) or stay', 'via': 'state0 arm 0x3004C8 does NOT advance; needs external writer'}
- {'from': 'unlisted >=20 (e.g. 20..37,39..)', 'to': 'BL 0x300714', 'via': 'default arm + CMP#20 not LT'}
- {'from': 38, 'to': '0x30103C path inside dispatcher', 'via': '0x300714 CMP #38'}
- {'note': 'Do not assume 0→38 single step; E8N maps writers that can produce intermediate values'}

## Imm#38 writer sites (if any)

- (none with adjacent MOVS #38 — value likely computed/copied)

## Direct writers (first 25)

- STR@`0x2DC668` w=1 fn`0x2DC620` val={'kind': 'imm8', 'imm': 1, 'pc': 2999894, 'can_nonzero': True, 'can_ge20': False, 'can_eq38': False} callers=1 nz=True ge20=False eq38=False
- STR@`0x2DC66E` w=1 fn`0x2DC620` val={'kind': 'imm8', 'imm': 1, 'pc': 2999894, 'can_nonzero': True, 'can_ge20': False, 'can_eq38': False} callers=1 nz=True ge20=False eq38=False
- STR@`0x2DC94E` w=4 fn`0x2DC80C` val={'kind': 'imm8', 'imm': 46, 'pc': 3000652, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=1 nz=True ge20=True eq38=False
- STR@`0x2DD440` w=4 fn`0x2DD068` val={'kind': 'imm8', 'imm': 37, 'pc': 3003452, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=2 nz=True ge20=True eq38=False
- STR@`0x2DD478` w=4 fn`0x2DD068` val={'kind': 'imm8', 'imm': 30, 'pc': 3003508, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=2 nz=True ge20=True eq38=False
- STR@`0x2DD512` w=4 fn`0x2DD068` val={'kind': 'imm8', 'imm': 20, 'pc': 3003662, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=2 nz=True ge20=True eq38=False
- STR@`0x2DD51E` w=4 fn`0x2DD068` val={'kind': 'imm8', 'imm': 255, 'pc': 3003672, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=2 nz=True ge20=True eq38=False
- STR@`0x2DD646` w=4 fn`0x2DD068` val={'kind': 'imm8', 'imm': 27, 'pc': 3003970, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=2 nz=True ge20=True eq38=False
- STR@`0x2E052C` w=4 fn`0x2E0526` val={'kind': 'imm8', 'imm': 255, 'pc': 3015976, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=0 nz=True ge20=True eq38=False
- STR@`0x2E248E` w=4 fn`0x2E2458` val={'kind': 'imm8', 'imm': 255, 'pc': 3023978, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=2 nz=True ge20=True eq38=False
- STR@`0x2E2F60` w=4 fn`0x2E2F58` val={'kind': 'unknown'} callers=0 nz=unknown ge20=unknown eq38=unknown
- STR@`0x2E3254` w=1 fn`0x2E3236` val={'kind': 'imm8', 'imm': 0, 'pc': 3027538, 'can_nonzero': False, 'can_ge20': False, 'can_eq38': False} callers=0 nz=False ge20=False eq38=False
- STR@`0x2E346E` w=4 fn`0x2E3466` val={'kind': 'unknown'} callers=0 nz=unknown ge20=unknown eq38=unknown
- STR@`0x2E39EC` w=4 fn`0x2E39E4` val={'kind': 'unknown'} callers=0 nz=unknown ge20=unknown eq38=unknown
- STR@`0x2E3A04` w=4 fn`0x2E39E4` val={'kind': 'unknown'} callers=0 nz=unknown ge20=unknown eq38=unknown
- STR@`0x2E3F8C` w=4 fn`0x2E3F84` val={'kind': 'unknown'} callers=0 nz=unknown ge20=unknown eq38=unknown
- STR@`0x2E3FCC` w=4 fn`0x2E3FC6` val={'kind': 'imm8', 'imm': 20, 'pc': 3030984, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=0 nz=True ge20=True eq38=False
- STR@`0x2E55B8` w=4 fn`0x2E5428` val={'kind': 'imm8', 'imm': 255, 'pc': 3036594, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=1 nz=True ge20=True eq38=False
- STR@`0x2E6986` w=4 fn`0x2E68F0` val={'kind': 'unknown'} callers=1 nz=unknown ge20=unknown eq38=unknown
- STR@`0x2E698E` w=1 fn`0x2E68F0` val={'kind': 'imm8', 'imm': 1, 'pc': 3041674, 'can_nonzero': True, 'can_ge20': False, 'can_eq38': False} callers=1 nz=True ge20=False eq38=False
- STR@`0x2E69C8` w=4 fn`0x2E69BE` val={'kind': 'imm8', 'imm': 0, 'pc': 3041680, 'can_nonzero': False, 'can_ge20': False, 'can_eq38': False} callers=0 nz=False ge20=False eq38=False
- STR@`0x2E69D0` w=4 fn`0x2E69BE` val={'kind': 'unknown'} callers=0 nz=unknown ge20=unknown eq38=unknown
- STR@`0x2E69D8` w=4 fn`0x2E69BE` val={'kind': 'imm8', 'imm': 0, 'pc': 3041748, 'can_nonzero': False, 'can_ge20': False, 'can_eq38': False} callers=0 nz=False ge20=False eq38=False
- STR@`0x2E796E` w=4 fn`0x2E7934` val={'kind': 'imm8', 'imm': 32, 'pc': 3045738, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=1 nz=True ge20=True eq38=False
- STR@`0x2FAEF0` w=4 fn`0x2FAE98` val={'kind': 'imm8', 'imm': 26, 'pc': 3124972, 'can_nonzero': True, 'can_ge20': True, 'can_eq38': False} callers=2 nz=True ge20=True eq38=False

BP: `p:0x300158,p:0x3004C8,p:0x3002BA,p:0x3002C0,p:0x300714,p:0x30103C,e:0x2DFC3C,e:0x2DFCAC,e:0x30D300,u:0x2DC668,u:0x2DC66E,u:0x2DC94E,u:0x2DD440,u:0x2DD478,u:0x2DD512,u:0x2DD51E,u:0x2DD646,u:0x2E052C,u:...`

